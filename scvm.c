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

/**
 * @file scvm.c
 * @ingroup scvm
 * @brief A primitive SCUMM VM implementation.
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
#include <signal.h>

#include <SDL.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_param.h"
#include "scc_cost.h"
#include "scc_box.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

#include "scvm_help.h"

#define SCVM_ERR_MAX 0x100

static const char* scvm_error[SCVM_ERR_MAX] = {
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
  "bad object",
  "no room",
  "bad palette",
  "uninitialized vm",
  "failed to set video mode",
  "bad verb",
  "bad color",
  "too many verb",
  "sentence overflow",
  NULL
};

static const char* scvm_not_error[SCVM_ERR_MAX] = {
  "interrupted",
  "breakpoint",
  "quit",
  NULL
};

extern scvm_op_t scvm_optable[0x100];
extern scvm_op_t scvm_suboptable[0x100];

const char* scvm_strerror(int errn) {
  const char** names = scvm_error;
  if(errn < 0) errn = -errn;
  if(errn > SCVM_NOT_ERR_BASE) {
    errn -= SCVM_NOT_ERR_BASE;
    names = scvm_not_error;
  }
  if(errn >= SCVM_ERR_MAX) return NULL;
  return names[errn];
}

char* scvm_state_name(unsigned state) {
  switch(state) {
  case SCVM_UNINITED:
    return "uninited";
  case SCVM_BOOT:
    return "boot";
  case SCVM_PAUSE:
    return "pause";
  case SCVM_BEGIN_CYCLE:
    return "begin cycle";
  case SCVM_RUNNING:
    return "running";
  case SCVM_FINISHED_SCRIPTS:
    return "finished scripts";
  case SCVM_FINISH_CYCLE:
    return "finish cycle";
  case SCVM_START_SCRIPT:
    return "start script";
  case SCVM_OPEN_ROOM:
    return "open room";
  case SCVM_RUN_PRE_EXIT:
    return "run pre exit";
  case SCVM_RUN_EXCD:
    return "run excd";
  case SCVM_RUN_POST_EXIT:
    return "run post exit";
  case SCVM_SETUP_ROOM:
    return "setup room";
  case SCVM_RUN_PRE_ENTRY:
    return "run pre entry";
  case SCVM_RUN_ENCD:
    return "run encd";
  case SCVM_RUN_POST_ENTRY:
    return "run post entry";
  case SCVM_OPENED_ROOM:
    return "opened room";
  case SCVM_CAMERA_FOLLOW_ACTOR:
    return "camera follow actor";
  case SCVM_CAMERA_START_FOLLOWING_ACTOR:
    return "camera start following actor";
  case SCVM_CAMERA_START_FOLLOWING_ACTOR_IN_ROOM:
    return "camera start following actor in room";
  case SCVM_RUN_INVENTORY:
    return "run inventory";
  case SCVM_CHECK_SENTENCE:
    return "check sentence";
  case SCVM_REMOVE_SENTENCE:
    return "remove sentence";
  case SCVM_CHECKED_SENTENCE:
    return "checked sentence";
  case SCVM_CHECK_INPUT:
    return "check input";
  case SCVM_CHECKED_INPUT:
    return "checked input";
  case SCVM_MOVE_CAMERA:
    return "move camera";
  case SCVM_WALK_ACTORS:
    return "walk actors";
  case SCVM_RESTART:
    return "restart";
  case SCVM_QUIT:
    return "quit";
  }
  return NULL;
}

static int scvm_default_random(struct scvm_backend_priv* be,int min,int max) {
  int diff = max-min;
  int r = (int)((diff+1.0)*rand()/(RAND_MAX+1.0));
  return min+r;
}

static void scvm_vars_init(scvm_vars_t* var) {
  // init some engine variables
  // soundcard: 0 = none
  //            1 = pc speaker
  //            3 = adlib
  var->soundcard = 0;
  // video mode: 4  = CGA
  //             13 = EGA
  //             19 = VGA?
  //             30 = Hercule
  //             42 = FMTowns
  //             50 = MAC
  //             82 = Amiga
  var->videomode = 19;
  // Heap size
  var->heap_space = 1400;
  // Playing from HD
  var->fixed_disk = 1;
  // Input mode: 0 = keyboard
  //             1 = joystick
  //             3 = mouse
  var->input_mode = 3;
  // EMS size
  var->ems_space = 10000;
  // Set an initial room size
  var->room_width = 320;
  var->room_height = 200;
  // Subtitle speed
  var->charinc = 4;
}

scvm_t *scvm_new(scvm_backend_t* be, char* path,char* basename, uint8_t key) {
  scc_fd_t* fd;
  scvm_t* vm;
  int i,num,len = (path ? strlen(path) + 1 : 0) + strlen(basename);
  char idx_file[len+1];
  uint32_t type;

  if(!be) {
    scc_log(LOG_ERR,"No backend?\n");
    return NULL;
  }

  if(be->init && !be->init(be)) {
      scc_log(LOG_ERR,"Backend initialization failed.\n");
      return NULL;
  }

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
  vm->backend = be;
  // vars
  vm->num_var = scc_fd_r16le(fd);
  vm->var_mem = calloc(vm->num_var,sizeof(int));
  vm->var = (scvm_vars_t*)vm->var_mem;
  scvm_vars_init(vm->var);
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
  vm->num_verb = scc_fd_r16le(fd);
  vm->verb = calloc(vm->num_verb,sizeof(scvm_verb_t));
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
        scc_log(LOG_ERR,"LOFF block has bad room number: %d\n",rno);
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
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != MKID('D','O','B','J') || len < 10) {
    scc_log(LOG_ERR,"Invalid index file, bad DOBJ block.\n");
    return NULL;
  }
  num = scc_fd_r16le(fd);
  if(len != 8+2+5*num || num > vm->num_object) {
    scc_log(LOG_ERR,"Invalid index file, bad DOBJ block.\n");
    return NULL;
  }
  for(i = 0 ; i < num ; i++) {
    uint8_t owner_state = scc_fd_r8(fd);
    vm->object_pdata[i].state = owner_state>>4;
    vm->object_pdata[i].owner = owner_state&0x0F;
  }
  for(i = 0 ; i < num ; i++)
    vm->object_pdata[i].klass = scc_fd_r32le(fd);

  // arrays

  // actors
  vm->num_actor = 16;
  vm->actor = calloc(vm->num_actor,sizeof(scvm_actor_t));
  for(i = 0 ; i < vm->num_actor ; i++) {
    vm->actor[i].id = i;
    scvm_actor_init(&vm->actor[i]);
    scc_cost_dec_init(&vm->actor[i].costdec);
  }
  vm->current_actor = vm->actor;
  
  // view
  vm->view = calloc(1,sizeof(scvm_view_t));
  vm->view->screen_width = 320;
  vm->view->screen_height = 200;
  
  // threads
  vm->state = SCVM_BOOT;
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
  
  // set default system callbacks
  if(!vm->backend->random)
    vm->backend->random = scvm_default_random;
  
  return vm;
}

int scvm_add_breakpoint(scvm_t* vm, unsigned room_id,
                        unsigned script_id, unsigned pos) {
  scvm_debug_t* dbg;
  scvm_breakpoint_t* bp;
  int id;

  if(script_id < 200 && script_id >= vm->res[SCVM_RES_SCRIPT].num)
    return -1;
  // TODO: check room script id if the room is loaded
  if(room_id >= vm->res[SCVM_RES_ROOM].num)
    return -1;

  if(!vm->dbg)
    vm->dbg = calloc(1,sizeof(scvm_debug_t));

  dbg = vm->dbg;
  id = dbg->num_breakpoint;
  if((id & 3) == 0)
    dbg->breakpoint = realloc(dbg->breakpoint,
                              (id+4)*sizeof(scvm_breakpoint_t));
  bp = dbg->breakpoint + id;
  dbg->num_breakpoint++;

  bp->id = id;
  bp->room_id = room_id;
  bp->script_id = script_id;
  bp->pos = pos;
  return bp->id;
}

unsigned scvm_get_time(scvm_t* vm) {
  return vm->backend->get_time(vm->backend->priv);
}

int scvm_random(scvm_t* vm, int min, int max) {
  return vm->backend->random(vm->backend->priv,min,max);
}

int scvm_init_video(scvm_t* vm, unsigned w, unsigned h, unsigned bpp) {
  return vm->backend->init_video(vm->backend->priv,w,h,bpp);
}

void scvm_update_palette(scvm_t* vm, scvm_color_t* pal) {
  vm->backend->update_palette(vm->backend->priv,pal);
}

void scvm_draw(scvm_t* vm, scvm_view_t* view) {
  vm->backend->draw(vm->backend->priv,vm,view);
}

void scvm_sleep(scvm_t* vm, unsigned delay) {
  vm->backend->sleep(vm->backend->priv,delay);
}

void scvm_flip(scvm_t* vm) {
  vm->backend->flip(vm->backend->priv);
}

void scvm_uninit_video(scvm_t* vm) {
  vm->backend->uninit_video(vm->backend->priv);
}

void scvm_check_events(scvm_t* vm) {
  vm->backend->check_events(vm->backend->priv,vm);
}

unsigned scvm_pause(scvm_t* vm) {
  if(vm->state == SCVM_PAUSE) {
    vm->state = vm->pause_state;
    vm->pause_state = 0;
  } else {
    vm->pause_state = vm->state;
    vm->state = SCVM_PAUSE;
  }
  return vm->state;
}

void scvm_press_key(scvm_t* vm, uint8_t key) {
  if(vm->state == SCVM_PAUSE  ||
     (vm->var->pause_key != -1 && key == vm->var->pause_key))
    scvm_pause(vm);
  vm->keypress = key;
  vm->key_state[key>>3] |= 1<<(key&7);
}

void scvm_release_key(scvm_t* vm, uint8_t key) {
  vm->key_state[key>>3] &= ~(1<<(key&7));
}

void scvm_press_button(scvm_t* vm, int btn) {
  if(btn > 3) return;
  vm->btnpress = btn;
  vm->btn_state |= 1<<btn;
}

void scvm_release_button(scvm_t* vm, int btn) {
  if(btn > 3) return;
  vm->btn_state &= ~(1<<btn);
}

void scvm_set_mouse_position(scvm_t* vm, int x, int y) {
    if(x < 0)
        x = 0;
    else if(x >= vm->view->screen_width)
        x = vm->view->screen_width-1;
    if(y < 0)
        y = 0;
    else if(y >= vm->view->screen_height)
        y = vm->view->screen_height-1;
    vm->var->mouse_x = x;
    vm->var->mouse_y = y;
}

static void scvm_switch_to_thread(scvm_t* vm,unsigned thid, unsigned next_state) {
  if(vm->current_thread)
    vm->current_thread->state = SCVM_THREAD_PENDED;
  vm->thread[thid].parent = vm->current_thread;
  vm->thread[thid].next_state = next_state;
  vm->current_thread = &vm->thread[thid];
  vm->state = SCVM_RUNNING;
}

int scvm_run_threads(scvm_t* vm,unsigned cycles) {
  int i=0,r;
  unsigned delta,now;
  scvm_thread_t* parent;
  
  while(cycles > 0) {
    scc_log(LOG_MSG,"VM run threads state: %d\n",vm->state);
    switch(vm->state) {
    case SCVM_BOOT:
      if(!scvm_init_video(vm,640,480,8)) {
       scc_log(LOG_ERR,"Failed to open video output.\n");
        return SCVM_ERR_VIDEO_MODE;
      }
      if((r=scvm_start_script(vm,0,1,NULL)) < 0) {
        scc_log(LOG_MSG,"Failed to start boot script: %s\n",scvm_strerror(r));
        return r;
      }
      vm->state = SCVM_BEGIN_CYCLE;

    case SCVM_BEGIN_CYCLE:
      // Reschedule the delayed threads
      now = scvm_get_time(vm);
      if(now < vm->time) now = vm->time;
      delta = now - vm->time;
      for(i = 0 ; i < vm->num_thread ; i++) {
        if(vm->thread[i].state != SCVM_THREAD_DELAYED) continue;
        if(!(vm->thread[i].flags & SCVM_THREAD_DELAY))
          vm->thread[i].flags |= SCVM_THREAD_DELAY;
        else if(vm->thread[i].delay > delta)
          vm->thread[i].delay -= delta;
        else {
          vm->thread[i].delay = 0;
          vm->thread[i].flags &= ~SCVM_THREAD_DELAY;
          vm->thread[i].state = SCVM_THREAD_RUNNING;
        }
      }
      // Update the timers
      vm->var->timer = 0;
      vm->var->timer1 += vm->var->timer_next;
      vm->var->timer2 += vm->var->timer_next;
      vm->var->timer3 += vm->var->timer_next;
      // Update the mouse virtual position
      {
          int x = vm->var->mouse_x, y = vm->var->mouse_y;
          if(!scvm_abs_position_to_virtual(vm,&x,&y)) {
              vm->var->virtual_mouse_x = x;
              vm->var->virtual_mouse_y = y;
          } else {
              vm->var->virtual_mouse_x = -1;
              vm->var->virtual_mouse_y = -1;
          }
      }
      vm->var->camera_pos_x = vm->view->camera_x;
      // Run the scripts      
      vm->state = SCVM_RUNNING;
      vm->time = now;
      break;
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
          vm->state = SCVM_FINISHED_SCRIPTS;
          break;
        }
        vm->current_thread = &vm->thread[i];
      }
      scc_log(LOG_MSG,"\n == VM enter thread %d / script %d @ 0x%x / room %d ==\n",
              vm->current_thread->id,vm->current_thread->script->id,
              vm->current_thread->code_ptr,
              vm->room ? vm->room->id : -1);
      r = scvm_thread_run(vm,vm->current_thread);
      scc_log(LOG_MSG," == VM leave thread %d / script %d @ 0x%x / room %d ==\n\n",
              vm->current_thread->id,vm->current_thread->script->id,
              vm->current_thread->code_ptr,
              vm->room ? vm->room->id : -1);
      if(r < 0) return r; // error
      if(r > 0) {         // switch state
        vm->state = r;
        break;
      }
      // Done with this thread for this cycle
      if(vm->current_thread->cycle <= vm->cycle)
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
      vm->var->new_room = vm->next_room;
    case SCVM_RUN_PRE_EXIT:
      if(vm->var->pre_exit_script) {
        if((r = scvm_start_script(vm,0,vm->var->pre_exit_script,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_RUN_EXCD);
        break;
      }
      vm->state = SCVM_RUN_EXCD;
    case SCVM_RUN_EXCD:
      if(vm->room && vm->room->exit) {
        if((r = scvm_start_thread(vm,vm->room->exit,0,0,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_RUN_POST_EXIT);
        break;
      }
      vm->state = SCVM_RUN_POST_EXIT;
    case SCVM_RUN_POST_EXIT:
      if(vm->var->post_exit_script) {
        if((r = scvm_start_script(vm,0,vm->var->post_exit_script,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_SETUP_ROOM);
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
        // copy the room palette
        if(room->num_palette > 0) {
          room->current_palette = room->palette[0];
          memcpy(vm->view->palette,room->palette[0],sizeof(scvm_palette_t));
          vm->view->flags |= SCVM_VIEW_PALETTE_CHANGED;
        }
        // set the camera min/max
        vm->var->camera_min_x = vm->view->screen_width/2;
        vm->var->camera_max_x = room->width - vm->var->camera_min_x;
        vm->var->room = room->id;
        vm->room = room;
        vm->view->camera_x = vm->var->camera_min_x;
        vm->view->camera_x = vm->var->camera_min_x;
        vm->var->camera_pos_x = vm->view->camera_x;
        scvm_put_actors(vm);
        vm->state = SCVM_RUN_PRE_ENTRY;
      }
    case SCVM_RUN_PRE_ENTRY:
      if(vm->var->pre_entry_script) {
        if((r = scvm_start_script(vm,0,vm->var->pre_entry_script,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_RUN_ENCD);
        break;
      }
      vm->state = SCVM_RUN_ENCD;
    case SCVM_RUN_ENCD:
      if(vm->room->entry) {
        if((r = scvm_start_thread(vm,vm->room->entry,0,0,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_RUN_POST_ENTRY);
        break;
      }
      vm->state = SCVM_RUN_POST_ENTRY;
    case SCVM_RUN_POST_ENTRY:
      if(vm->var->post_entry_script) {
        if((r = scvm_start_script(vm,0,vm->var->post_entry_script,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,0);
        break;
      }      
      if(vm->next_room_state) {
        vm->state = vm->next_room_state;
        vm->next_room_state = 0;
      } else
        vm->state = SCVM_RUNNING;
      break;

    case SCVM_CAMERA_FOLLOW_ACTOR:
      if(vm->actor[vm->view->follow].room != vm->room->id) {
        vm->next_room = vm->actor[vm->view->follow].room;
        vm->next_room_state = SCVM_CAMERA_START_FOLLOWING_ACTOR_IN_ROOM;
        vm->state = SCVM_OPEN_ROOM;
      } else
        vm->state = SCVM_CAMERA_START_FOLLOWING_ACTOR;
      break;

    case SCVM_CAMERA_START_FOLLOWING_ACTOR:
      // TODO: We should only call set camera if needed here
      // if(noNeedToSetCamera) {
      //   vm->state = SCVM_RUN_INVENTORY;
      //   break;
      // }

    case SCVM_CAMERA_START_FOLLOWING_ACTOR_IN_ROOM:
      if((r = scvm_set_camera_at(vm,vm->actor[vm->view->follow].x)) < 0)
        return r;
      if(r > 0) {
        scvm_switch_to_thread(vm,r-1,SCVM_RUN_INVENTORY);
        break;
      }
      vm->state = SCVM_RUN_INVENTORY;


    case SCVM_RUN_INVENTORY:
      if(vm->var->inventory_script) {
        if((r = scvm_start_script(vm,0,vm->var->inventory_script,NULL)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,0);
        break;
      }
      vm->state = SCVM_RUNNING;
      break;

    case SCVM_FINISHED_SCRIPTS:
      vm->state = SCVM_CHECK_SENTENCE;


    case SCVM_CHECK_SENTENCE:
      if(!vm->var->sentence_script) {
        vm->state = SCVM_REMOVE_SENTENCE;
        break;
      }
      if(!vm->num_sentence ||
         scvm_is_script_running(vm,vm->var->sentence_script)) {
        vm->state = SCVM_CHECKED_SENTENCE;
        break;
      } else {
        unsigned args[] = { 3, vm->sentence->verb,
                            vm->sentence->object_a,
                            vm->sentence->object_b };
        if((r = scvm_start_script(vm,0,vm->var->sentence_script,args)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_REMOVE_SENTENCE);
        break;
      }
      vm->state = SCVM_CHECKED_SENTENCE;
      break;

    case SCVM_REMOVE_SENTENCE:
      if(vm->num_sentence > 1)
        memmove(vm->sentence,vm->sentence+1,
                (vm->num_sentence-1)*sizeof(scvm_sentence_t));
      vm->num_sentence -= 1;
      vm->state = SCVM_CHECKED_SENTENCE;

    case SCVM_CHECKED_SENTENCE:
      vm->state = SCVM_CHECK_INPUT;


    case SCVM_CHECK_INPUT:
      if(!vm->var->verb_script /*|| !vm->userput*/) {
        vm->state = SCVM_CHECKED_INPUT;
        break;
      }
      if(vm->keypress) {
        int args[] = { 3, SCVM_INPUT_KEY, vm->keypress, 1 };
        vm->keypress = 0;
        // TODO: check verb keys
        if((r = scvm_start_script(vm,0,vm->var->verb_script,args)) < 0)
          return r;
        // Keep the same state to also check mouse button
        // press in the same frame. ScummVM don't do that,
        // but that seems more correct to me. Dunno what the
        // original engine did.
        scvm_switch_to_thread(vm,r,SCVM_CHECK_INPUT);
        break;
      }
      if(vm->btnpress) {
        int args[] = { 3, SCVM_INPUT_VERB, 0, vm->btnpress };
        scvm_verb_t* verb;
        vm->btnpress = 0;
        if((verb = scvm_get_verb_at(vm,vm->var->mouse_x,
                                    vm->var->mouse_y))) {
          args[2] = verb->id;
        } else if(vm->var->virtual_mouse_x >= 0)
          args[1] = SCVM_INPUT_ROOM;
        if((r = scvm_start_script(vm,0,vm->var->verb_script,args)) < 0)
          return r;
        scvm_switch_to_thread(vm,r,SCVM_CHECKED_INPUT);
        break;
      }
    case SCVM_CHECKED_INPUT:
      vm->state = SCVM_MOVE_CAMERA;

    case SCVM_MOVE_CAMERA:
      if((r = scvm_move_camera(vm)) < 0)
        return r;
      if(r > 0) {
        scvm_switch_to_thread(vm,r-1,SCVM_WALK_ACTORS);
        break;
      }
      vm->state = SCVM_WALK_ACTORS;

    case SCVM_WALK_ACTORS:
      scvm_step_actors(vm);
      vm->state = SCVM_FINISH_CYCLE;

    case SCVM_FINISH_CYCLE:
      scc_log(LOG_MSG,"\n == VM finished cycle %d == \n\n",vm->cycle);
      cycles--;
      vm->cycle++;
      vm->state = SCVM_BEGIN_CYCLE;
      break;

    case SCVM_PAUSE:
      return 0;

    case SCVM_QUIT:
      return SCVM_ERR_QUIT;

    default:
      scc_log(LOG_ERR,"Invalid VM state: %d\n",vm->state);
      return SCVM_ERR_BAD_STATE;
    }
  }
  
  return 0;    
}

void scvm_cycle_palette(scvm_t* vm) {
  int i,step;
  scvm_color_t color;
  scvm_cycle_t* cycle;
  if(!vm->room || !vm->room->cycle) return;
  step = vm->var->timer;
  if(step < vm->var->timer_next)
    step = vm->var->timer_next;
  for(i = 0 ; i < vm->room->num_cycle ; i++) {
    cycle = &vm->room->cycle[i];
    if(!cycle->delay) continue;
    cycle->counter += step;
    if(cycle->counter < cycle->delay) continue;
    // backward
    if(cycle->flags & 2) {
      color = vm->view->palette[cycle->start];
      if(cycle->end-cycle->start > 0)
        memmove(&vm->view->palette[cycle->start],
                &vm->view->palette[cycle->start+1],
                (cycle->end-cycle->start)*sizeof(scvm_color_t));
      vm->view->palette[cycle->end] = color;
    } else { // forward
      color = vm->view->palette[cycle->end];
      if(cycle->end-cycle->start > 0)
        memmove(&vm->view->palette[cycle->start+1],
                &vm->view->palette[cycle->start],
                (cycle->end-cycle->start)*sizeof(scvm_color_t));
      vm->view->palette[cycle->start] = color;
    }
    vm->view->flags |= SCVM_VIEW_PALETTE_CHANGED;
  }
}

int scvm_do_sentence(scvm_t* vm, unsigned vrb, unsigned obj_a,
                     unsigned obj_b) {
  scvm_sentence_t* st;
  if(vm->num_sentence >= SCVM_MAX_SENTENCE)
    return SCVM_ERR_SENTENCE_OVERFLOW;
  st = &vm->sentence[vm->num_sentence];
  st->verb = vrb;
  st->object_a = obj_a;
  st->object_b = obj_b;
  vm->num_sentence += 1;
  return 0;
}

int scvm_run_once(scvm_t* vm) {
  unsigned start,end,delay;
  int r;

  if(!vm->backend)
    return SCVM_ERR_UNINITED_VM;;

  start = scvm_get_time(vm);
  scvm_check_events(vm);
  r = scvm_run_threads(vm,1);
  if(SCVM_IS_ERR(r)) {
    if(vm->current_thread)
      scc_log(LOG_MSG,"Script error: %s @ %03d:0x%04X\n",
              scvm_strerror(r),vm->current_thread->script->id,
              vm->current_thread->op_start);
    else
      scc_log(LOG_MSG,"Script error: %s\n",scvm_strerror(r));
    return r;
  }

  scvm_cycle_palette(vm);
  if(vm->view->flags & SCVM_VIEW_PALETTE_CHANGED) {
    scvm_update_palette(vm,vm->view->palette);
    vm->view->flags &= ~SCVM_VIEW_PALETTE_CHANGED;
  }

  scvm_draw(vm,vm->view);

  end = scvm_get_time(vm);
  if(end < start) end = start;
  delay = vm->var->timer_next*1000/60;
  if(delay > end-start)
    scvm_sleep(vm,delay-(end-start));

  scvm_flip(vm);

  return r;
}

int scvm_run(scvm_t* vm) {
  int r = 0;
  while(r >= 0)
    r = scvm_run_once(vm);
  return r;
}


/////////////////////

typedef struct scvm_backend_priv {
  int inited_video;
  SDL_Surface* screen;
} scvm_backend_sdl_t;

static int sdl_scvm_init_video(scvm_backend_sdl_t* be, unsigned width,
                               unsigned height, unsigned bpp) {
  if(!be->inited_video) {
    sig_t oldint = signal(SIGINT,SIG_DFL);
    sig_t oldquit = signal(SIGQUIT,SIG_DFL);
    sig_t oldterm = signal(SIGTERM,SIG_DFL);
    if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
      scc_log(LOG_ERR,"SDL video init failed.\n");
      return 0;
    }
    signal(SIGINT,oldint);
    signal(SIGQUIT,oldquit);
    signal(SIGTERM,oldterm);
    // Lame, but this must be called after initing the video
    SDL_EnableUNICODE(1);
    be->inited_video = 1;
  }
  if(!(be->screen =
       SDL_SetVideoMode(width,height,bpp,SDL_HWPALETTE))) {
    scc_log(LOG_ERR,"Failed to create SDL screen.\n");
    return 0;
  }
  return 1;
}

static void sdl_scvm_uninit_video(scvm_backend_sdl_t* be) {
  if(be->inited_video) {
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    be->inited_video = 0;
  }
}


static void sdl_scvm_update_palette(scvm_backend_sdl_t* be, scvm_color_t* pal) {
  SDL_Color colors[256];
  int i;
  for(i = 0 ; i < 256 ; i++) {
    colors[i].r = pal[i].r;
    colors[i].g = pal[i].g;
    colors[i].b = pal[i].b;
  }
  SDL_SetPalette(be->screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256);
}

void sdl_scvm_draw(scvm_backend_sdl_t* be, scvm_t* vm, scvm_view_t* view) {
  if(SDL_MUSTLOCK(be->screen)) SDL_LockSurface(be->screen);
  scvm_view_draw(vm,vm->view,be->screen->pixels,be->screen->pitch,
                 be->screen->w,be->screen->h);
  if(SDL_MUSTLOCK(be->screen)) SDL_UnlockSurface(be->screen);
}

static void sdl_scvm_sleep(scvm_backend_sdl_t* be, unsigned delay) {
  SDL_Delay(delay);
}

static void sdl_scvm_flip(scvm_backend_sdl_t* be) {
  SDL_Flip(be->screen);
}

static unsigned sdl_scvm_get_time(scvm_backend_sdl_t* be) {
  return SDL_GetTicks();
}

static void sdl_scvm_check_events(scvm_backend_sdl_t* be, scvm_t* vm) {
  SDL_Event ev;
  while(SDL_PollEvent(&ev)) {
    switch(ev.type) {
    case SDL_MOUSEBUTTONDOWN:
      scvm_press_button(vm,ev.button.button);
      break;
    case SDL_MOUSEBUTTONUP:
      scvm_release_button(vm,ev.button.button);
      break;
    case SDL_MOUSEMOTION:
      scvm_set_mouse_position(vm,ev.motion.x*
                              vm->view->screen_width/be->screen->w,
                              ev.motion.y*
                              vm->view->screen_height/be->screen->h);
      break;
    case SDL_KEYDOWN:
      if(!ev.key.keysym.unicode ||
         ev.key.keysym.unicode > 255) break;
      scvm_press_key(vm,ev.key.keysym.unicode);
      break;

    case SDL_KEYUP:
      if(!ev.key.keysym.unicode ||
         ev.key.keysym.unicode > 255) break;
      scvm_release_key(vm,ev.key.keysym.unicode);
      break;
    }
  }
}

static void sdl_backend_uninit(scvm_backend_t* be) {
  SDL_Quit();
  if(be->priv)
    free(be->priv);
}

static int sdl_backend_init(scvm_backend_t* be) {
  be->uninit = sdl_backend_uninit;
  be->get_time = sdl_scvm_get_time;
  be->update_palette = sdl_scvm_update_palette;
  be->init_video = sdl_scvm_init_video;
  be->draw = sdl_scvm_draw;
  be->sleep = sdl_scvm_sleep;
  be->flip = sdl_scvm_flip;
  be->uninit_video = sdl_scvm_uninit_video;
  be->check_events = sdl_scvm_check_events;
  be->priv = calloc(1,sizeof(scvm_backend_sdl_t));
  if(SDL_Init(SDL_INIT_NOPARACHUTE) < 0) {
    scc_log(LOG_ERR,"SDL init failed.\n");
    return 0;
  }
  return 1;
}

static char* basedir = NULL;
static int file_key = 0;
static int run_debugger = 0;

static scc_param_t scc_parse_params[] = {
  { "dir", SCC_PARAM_STR, 0, 0, &basedir },
  { "key", SCC_PARAM_INT, 0, 0xFF, &file_key },
  { "dbg", SCC_PARAM_FLAG, 0, 1, &run_debugger },
  { "help", SCC_PARAM_HELP, 0, 0, &scvm_help },
  { NULL, 0, 0, 0, NULL }
};


int main(int argc,char** argv) {
  scvm_t* vm;
  scc_cl_arg_t* files;
  scvm_backend_t backend = { sdl_backend_init };

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  if(!files) scc_print_help(&scvm_help,1);

  vm = scvm_new(&backend,basedir,files->val,file_key);

  if(!vm) {
    scc_log(LOG_ERR,"Failed to create VM.\n");
    return 1;
  }
  scc_log(LOG_MSG,"VM created.\n");

  if(run_debugger)
    scvm_debugger(vm);
  else
    scvm_run(vm);
  return 0;
}
