/* ScummC
 * Copyright (C) 2006  Alban Bedel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_param.h"
#include "scc_cost.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

static char* scvm_error[0x100] = {
  "no error",
  "script bound",
  "op not supported or invalid",
  "stack overflow",
  "stack underflow",
  "bad address",
  "out of array bounds",
  "bad array type",
  "jump out of bounds",
  "out of arrays",
  "out of bound string",
  "bad resource",
  "bad thread",
  "bad VM state",
  "bad actor",
  "bad costume",
  "override overflow",
  "override underflow",
  NULL
};

extern scvm_op_t scvm_optable[0x100];
extern scvm_op_t scvm_suboptable[0x100];

scvm_t *scvm_new(char* path,char* basename, uint8_t key) {
  scc_fd_t* fd;
  scvm_t* vm;
  int i,num,len = (path ? strlen(path) + 1 : 0) + strlen(basename);
  char idx_file[len+1];
  uint32_t type;
  
  if(path)
    sprintf(idx_file,"%s/%s.000",path,basename);
  else
    sprintf(idx_file,"%s.000",basename);
  
  fd = new_scc_fd(idx_file,O_RDONLY,key);
  if(!fd) {
    scc_log(LOG_ERR,"Failed to open index file %s: %s\n",
            idx_file,strerror(errno));
    return NULL;
  }
  
  // Read the initial header
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != MKID('R','N','A','M') || len < 9 ) {
    scc_log(LOG_ERR,"Invalid index file, no RNAM block at the start.\n");
    scc_fd_close(fd);
    return NULL;
  }
  scc_fd_seek(fd,len-8,SEEK_CUR);
  
  // Read the maxs block
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != MKID('M','A','X','S') || len != 8+15*2) {
    scc_log(LOG_ERR,"Invalid index file, bad MAXS block.\n");
    scc_fd_close(fd);
    return NULL;
  }
  vm = calloc(1,sizeof(scvm_t));
  // vars
  vm->num_var = scc_fd_r16le(fd);
  vm->var = calloc(vm->num_var,sizeof(int));
  // unknown
  scc_fd_r16le(fd);
  // bit vars, don't be so stupid as the original vm, round up
  vm->num_bitvar = (scc_fd_r16le(fd)+7)&~7;
  vm->bitvar = calloc(vm->num_bitvar>>3,1);
  // local objs
  vm->num_local_object = scc_fd_r16le(fd);
  // arrays
  vm->num_array = scc_fd_r16le(fd);
  vm->array = calloc(vm->num_array,sizeof(scvm_array_t));
  // unkonwn
  scc_fd_r16le(fd);
  // verbs
  scc_fd_r16le(fd);
  // fl objects
  scc_fd_r16le(fd);
  // inventory
  scc_fd_r16le(fd);
  // rooms
  scvm_res_init(&vm->res[SCVM_RES_ROOM],"room",scc_fd_r16le(fd),
                scvm_load_room,NULL);
  // scripts
  scvm_res_init(&vm->res[SCVM_RES_SCRIPT],"script",scc_fd_r16le(fd),
                scvm_load_script,free);
  // sounds
  scvm_res_init(&vm->res[SCVM_RES_SOUND],"sound",scc_fd_r16le(fd),NULL,NULL);
  // charsets
  scvm_res_init(&vm->res[SCVM_RES_CHARSET],"charset",scc_fd_r16le(fd),
                scvm_load_charset,NULL);
  // costumes
  scvm_res_init(&vm->res[SCVM_RES_COSTUME],"costume",scc_fd_r16le(fd),
                scvm_load_costume,NULL);
  // objects
  scvm_res_init(&vm->res[SCVM_RES_OBJECT],"object",scc_fd_r16le(fd),NULL,NULL);
  vm->num_object = vm->res[SCVM_RES_OBJECT].num;
  vm->object_pdata = calloc(vm->num_object,sizeof(scvm_object_pdata_t));
  
  // load room index
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != MKID('D','R','O','O') || len < 8+2 ||
     (num = scc_fd_r16le(fd))*5+2+8 != len ||
     num != vm->res[SCVM_RES_ROOM].num) {
    scc_log(LOG_ERR,"Invalid index file, bad DROO block.\n");
    scc_fd_close(fd);
    // scvm_free(vm);
    return NULL;
  }
  for(i = 0 ; i < num ; i++) {
    vm->res[SCVM_RES_ROOM].idx[i].file = scc_fd_r8(fd);
    vm->res[SCVM_RES_ROOM].idx[i].room = i;
    if(vm->res[SCVM_RES_ROOM].idx[i].file >= vm->num_file)
      vm->num_file = vm->res[SCVM_RES_ROOM].idx[i].file+1;
  }
  if(!vm->num_file) {
    scc_log(LOG_ERR,"Invalid index file, no file ?\n");
    scc_fd_close(fd);
    // scvm_free(vm);
    return NULL;
  }
  // setup the file stuff
  vm->path = strdup(path ? path : ".");
  vm->basename = strdup(basename);
  vm->file_key = key;
  vm->file = calloc(vm->num_file,sizeof(scc_fd_t*));
  vm->file[0] = fd;
  // all 0 ??
  for(i = 0 ; i < num ; i++)
    vm->res[SCVM_RES_ROOM].idx[i].offset = scc_fd_r32le(fd);
  // load loff
  for(i = 1 ; i < vm->num_file ; i++) {
    int j;
    scc_fd_t* loff_fd = scvm_open_file(vm,i);
    if(!fd) continue;
    type = scc_fd_r32(loff_fd);
    len = scc_fd_r32be(loff_fd);
    if(type != MKID('L','E','C','F') || len < 17 ) {
      scc_log(LOG_ERR,"Invalid LECF block.\n");
      continue;
    }
    type = scc_fd_r32(loff_fd);
    len = scc_fd_r32be(loff_fd);
    if(type != MKID('L','O','F','F') || len < 9 ) {
      scc_log(LOG_ERR,"Invalid LOFF block.\n");
      continue;
    }
    num = scc_fd_r8(loff_fd);
    for(j = 0 ; j < num ; j++) {
      unsigned rno = scc_fd_r8(loff_fd);
      if(rno >= vm->res[SCVM_RES_ROOM].num) {
        scc_log(LOG_ERR,"LOFF block have bad room number: %d\n",rno);
        continue;
      }
      vm->res[SCVM_RES_ROOM].idx[rno].offset = scc_fd_r32le(loff_fd);
    }
    scvm_close_file(vm,i);
  }
  
    
  // script index
  if(!scvm_res_load_index(vm,&vm->res[SCVM_RES_SCRIPT],fd,
                          MKID('D','S','C','R'))) {
    // scvm_free(vm);
    return NULL;
  }
  
  // sound index
  if(!scvm_res_load_index(vm,&vm->res[SCVM_RES_SOUND],fd,
                          MKID('D','S','O','U'))) {
    // scvm_free(vm);
    return NULL;
  }
  
  // costume index
  if(!scvm_res_load_index(vm,&vm->res[SCVM_RES_COSTUME],fd,
                          MKID('D','C','O','S'))) {
    // scvm_free(vm);
    return NULL;
  }
  
  // charset index
  if(!scvm_res_load_index(vm,&vm->res[SCVM_RES_CHARSET],fd,
                          MKID('D','C','H','R'))) {
    // scvm_free(vm);
    return NULL;
  }
  
  // objects
  
  // arrays

  // actors
  vm->num_actor = 16;
  vm->actor = calloc(vm->num_actor,sizeof(scvm_actor_t));
  vm->current_actor = vm->actor;
  
  // view
  vm->view = calloc(1,sizeof(scvm_view_t));
  
  // threads
  vm->state = SCVM_RUNNING;
  vm->num_thread = 16;
  vm->current_thread = NULL;
  vm->thread = calloc(vm->num_thread,sizeof(scvm_thread_t));
  for(i = 0 ; i < vm->num_thread ; i++)
    vm->thread[i].id = i;
  vm->optable = scvm_optable;
  vm->suboptable = scvm_suboptable;
  // stack
  vm->stack_size = 1024;
  vm->stack = calloc(vm->stack_size,sizeof(int));

  // close fd
  scvm_close_file(vm,0);  
  return vm;
}


int scvm_run_threads(scvm_t* vm,unsigned cycles) {
  int i=0,r;
  scvm_thread_t* parent;
  
  while(cycles > 0) {
    scc_log(LOG_MSG,"VM run threads state: %d\n",vm->state);
    switch(vm->state) {
    case SCVM_START_SCRIPT:
      parent = vm->current_thread;
      vm->current_thread = vm->next_thread;
      vm->next_thread = NULL;
      vm->current_thread->parent = parent;
      if(parent) parent->state = SCVM_THREAD_PENDED;
      vm->state = SCVM_RUNNING;
    case SCVM_RUNNING:
      if(!vm->current_thread) {
        for(i = 0 ; i < vm->num_thread ; i++)
          if(vm->thread[i].state == SCVM_THREAD_RUNNING &&
             vm->thread[i].cycle <= vm->cycle) break;
        if(i >= vm->num_thread) {
          cycles--;
          vm->cycle++;
          break;
        }
        vm->current_thread = &vm->thread[i];
      }
      r = scvm_thread_run(vm,vm->current_thread);
      if(r < 0) return r; // error
      if(r > 0) {         // switch state
        vm->state = r;
        break;
      }
      // Done with this thread for this cycle
      vm->current_thread->cycle = vm->cycle+1;
      // Continue a job
      if(vm->current_thread->next_state) {
        vm->state = vm->current_thread->next_state;
        parent = vm->current_thread->parent;
        if(parent) parent->state = SCVM_THREAD_RUNNING;
        vm->current_thread->next_state = 0;
        vm->current_thread->parent = NULL;
        vm->current_thread = parent;
        break;
      }
      // nested call, switch back to the parent
      if(vm->current_thread->parent &&
         vm->current_thread->parent->state == SCVM_THREAD_PENDED) {
        parent = vm->current_thread->parent;
        vm->current_thread->parent = NULL;
        parent->state = SCVM_THREAD_RUNNING;
        vm->current_thread = parent;
        break;
      } else {
        vm->current_thread = NULL;
      }
      break;
      
    case SCVM_OPEN_ROOM:
    case SCVM_RUN_EXCD:
      if(vm->room && vm->room->exit) {
        if((r = scvm_start_thread(vm,vm->room->exit,0,0,NULL)) < 0)
          return r;
        vm->current_thread->state = SCVM_THREAD_PENDED;
        vm->thread[r].parent = vm->current_thread;
        vm->thread[r].next_state = SCVM_SETUP_ROOM;
        vm->current_thread = &vm->thread[r];
        vm->state = SCVM_RUNNING;
        break;
      }
      vm->state = SCVM_SETUP_ROOM;
    case SCVM_SETUP_ROOM:
      if(!vm->next_room) {
        vm->state = SCVM_RUNNING;
        break;
      } else {
        scvm_room_t* room = scvm_load_res(vm,SCVM_RES_ROOM,vm->next_room);
        // load room
        if(!room) return SCVM_ERR_BAD_RESOURCE;
        // cleanup the old room
        vm->room = room;
        vm->state = SCVM_RUN_ENCD;
      }
    case SCVM_RUN_ENCD:
      if(vm->room->entry) {
        if((r = scvm_start_thread(vm,vm->room->exit,0,0,NULL)) < 0)
          return r;
        vm->current_thread->state = SCVM_THREAD_PENDED;
        vm->thread[r].parent = vm->current_thread;
        //vm->thread[r].next_state = SCVM_RUNNING;
        vm->current_thread = &vm->thread[r];
      }
      vm->state = SCVM_RUNNING;
      break;
      
    default:
      scc_log(LOG_ERR,"Invalid VM state: %d\n",vm->state);
      return SCVM_ERR_BAD_STATE;
    }
  }
  
  return 0;    
}

static char* basedir = NULL;
static int file_key = 0;

static scc_param_t scc_parse_params[] = {
  { "dir", SCC_PARAM_STR, 0, 0, &basedir },
  { "key", SCC_PARAM_INT, 0, 0xFF, &file_key },
  { NULL, 0, 0, 0, NULL }
};


int main(int argc,char** argv) {
  scvm_t* vm;
  scc_cl_arg_t* files;
  int r;
  
  if(argc < 2) {
    scc_log(LOG_ERR,"Usage: scvm [-dir dir] [-key nn] basename\n");
    return 1;
  }

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  if(!files) return -1;
  
  vm = scvm_new(basedir,files->val,file_key);
  
  if(!vm) {
    scc_log(LOG_ERR,"Failed to create VM.\n");
    return 1;
  }
  scc_log(LOG_MSG,"VM created.\n");
  
  if((r=scvm_start_script(vm,0,1,NULL)) < 0) {
    scc_log(LOG_MSG,"Failed to start boot script: %s\n",scvm_error[-r]);
    return 1;
  }
  r = scvm_run_threads(vm,1);
  if(r < 0) {
    scc_log(LOG_MSG,"Failed to run boot script: %s\n",scvm_error[-r]);
    if(r == SCVM_ERR_NO_OP &&
       vm->current_thread &&
       vm->current_thread->code_ptr > 0)
      scc_log(LOG_MSG,"Last op: 0x%x @ %x\n",
              vm->current_thread->script->code[vm->current_thread->code_ptr-1],
              vm->current_thread->code_ptr-1);
    return 1;
  }
  
  return 0;
}
