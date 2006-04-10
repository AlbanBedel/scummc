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
#include "scc_cost.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"


int scvm_thread_r8(scvm_thread_t* thread, uint8_t* ret) {
  if(thread->code_ptr + 1 > thread->script->size) return SCVM_ERR_SCRIPT_BOUND;
  *ret = thread->script->code[thread->code_ptr];
  thread->code_ptr++;
  return 0;
}

int scvm_thread_r16(scvm_thread_t* thread, uint16_t *ret) {
  if(thread->code_ptr + 2 > thread->script->size) return SCVM_ERR_SCRIPT_BOUND;
  *ret = thread->script->code[thread->code_ptr];
  *ret |= (thread->script->code[thread->code_ptr+1]) << 8;
  thread->code_ptr += 2;
  return 0;
}

int scvm_thread_r32(scvm_thread_t* thread, uint32_t *ret) {
  if(thread->code_ptr + 4 > thread->script->size) return SCVM_ERR_SCRIPT_BOUND;
  *ret = thread->script->code[thread->code_ptr];
  *ret |= (thread->script->code[thread->code_ptr+1]) << 8;
  *ret |= (thread->script->code[thread->code_ptr+2]) << 16;
  *ret |= (thread->script->code[thread->code_ptr+3]) << 24;
  thread->code_ptr += 4;
  return 0;
}

int scvm_thread_strlen(scvm_thread_t* thread,unsigned *ret) {
  unsigned len = 0;
 
  while(thread->script->size > thread->code_ptr+len) {
    if(!thread->script->code[thread->code_ptr+len]) break;
    if(thread->script->code[thread->code_ptr+len] == 0xFF) {
      int type = thread->script->code[thread->code_ptr+len+1];
      len += 2;
      if((type < 1 || type > 3) && type != 8)
        len += 2;
    } else
      len++;
  }
  *ret = len;
  if(len >= thread->script->size-thread->code_ptr)
    return SCVM_ERR_STRING_BOUND;
  return 0;
}

int scvm_thread_begin_override(scvm_t* vm, scvm_thread_t* thread) {
  if(thread->override_ptr >= SCVM_MAX_OVERRIDE)
    return SCVM_ERR_OVERRIDE_OVERFLOW;
  thread->override[thread->override_ptr] = thread->code_ptr;
  thread->override_ptr++;
  vm->var->override = 0;
  return 0;
}

int scvm_thread_end_override(scvm_t* vm, scvm_thread_t* thread) {
  if(thread->override_ptr < 1)
    return SCVM_ERR_OVERRIDE_UNDERFLOW;
  thread->override_ptr--;
  vm->var->override = 0;
  return 0;
}

int scvm_stop_thread(scvm_t* vm, scvm_thread_t* thread) {
  thread->state = SCVM_THREAD_STOPPED;
  return 0;
}

int scvm_is_script_running(scvm_t* vm, unsigned id) {
  int i;
  for(i = 0 ; i < vm->num_thread ; i++) {
    if(vm->thread[i].state == SCVM_THREAD_STOPPED ||
       !vm->thread[i].script ||
       vm->thread[i].script->id != id)
      continue;
    return 1;
  }
  return 0;
}

int scvm_stop_script(scvm_t* vm, unsigned id) {
  int i,n=0;
  for(i = 0 ; i < vm->num_thread ; i++) {
    if(vm->thread[i].state == SCVM_THREAD_STOPPED ||
       !vm->thread[i].script ||
       vm->thread[i].script->id != id)
      continue;
    scvm_stop_thread(vm,&vm->thread[i]);
    n++;
  }
  return n;
}

int scvm_start_thread(scvm_t* vm, scvm_script_t* scr, unsigned code_ptr,
                      unsigned flags, unsigned* args) {
  int i;
  scvm_thread_t* thread;
  
  // find a free thread
  for(i = 0 ; i < vm->num_thread ; i++)
    if(vm->thread[i].state == SCVM_THREAD_STOPPED) break;
  if(i >= vm->num_thread) {
    scc_log(LOG_ERR,"No thread left to start script %d\n",scr->id);
    return -1; // fixme
  }
  thread = &vm->thread[i];
  
  thread->state = SCVM_THREAD_RUNNING;
  thread->flags = flags;
  thread->script = scr;
  thread->code_ptr = code_ptr;
  thread->cycle = vm->cycle;
  thread->parent = NULL;
  if(!thread->var) {
    thread->num_var = 16;
    thread->var = calloc(sizeof(int),vm->thread[i].num_var);
  } else
    memset(thread->var,0,sizeof(int)*vm->thread[i].num_var);
  if(args) {
    int j;
    for(j = 0 ; j < args[0] && j < 16 ; j++)
      thread->var[j] = args[j+1];
  }
  return thread->id;
}

int scvm_start_script(scvm_t* vm, unsigned flags, unsigned num, unsigned* args) {
  scvm_script_t* scr;
  
  // get the script
  // local scripts, this 200 shouldn't be hardcoded
  // but where do we get it from?
  if(num >= 200) {
    if(!vm->room || num-200 >= vm->room->num_script ||
       !(scr = vm->room->script[num-200]))
      return SCVM_ERR_BAD_ADDR;
  } else if(!(scr = scvm_load_res(vm,SCVM_RES_SCRIPT,num)))
      return SCVM_ERR_BAD_RESOURCE;
  
  // if it's not recursive kill the script if it run
  if(!(flags & SCVM_THREAD_RECURSIVE))
    scvm_stop_script(vm,num);

  return scvm_start_thread(vm,scr,0,flags,args);
}

int scvm_thread_do_op(scvm_t* vm, scvm_thread_t* thread, scvm_op_t* optable) {
  int r;
  uint8_t op;
  
  if((r=scvm_thread_r8(thread,&op))) return r;
  
  scc_log(LOG_MSG,"Do op %s (0x%x)\n",optable[op].name,op);

  if(!optable[op].op) {
    scc_log(LOG_WARN,"Op %s (0x%x) is missing\n",optable[op].name,op);
    return SCVM_ERR_NO_OP; // not implemented/existing
  }
  return optable[op].op(vm,thread);
}


int scvm_thread_run(scvm_t* vm, scvm_thread_t* thread) {
  int r=0;
  while(thread->state == SCVM_THREAD_RUNNING &&
        thread->cycle <= vm->cycle) {
    thread->op_start = thread->code_ptr;
    r = scvm_thread_do_op(vm,thread,vm->optable);
    if(r < 0) return r;
    if(r > 0) break;
  }
  return r;
}

// Debug helper
int scvm_find_op(scvm_op_t* optable,char* name) {
  int i;
  for(i = 0 ; i < 0x100 ; i++)
    if(!strcmp(optable[i].name,name)) return i;
  return -1;
}
