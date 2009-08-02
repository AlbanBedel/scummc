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
 * @file scvm_op.c
 * @ingroup scvm
 * @brief SCVM op-code implementation
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"
#include "scc_box.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

static int scvm_op_dummy_v(scvm_t* vm, scvm_thread_t* thread);

static int scvm_op_dummy_vv(scvm_t* vm, scvm_thread_t* thread);

static int scvm_op_dummy_s(scvm_t* vm, scvm_thread_t* thread);

// Basic stack ops

int scvm_push(scvm_t* vm,int val) {
  if(vm->stack_ptr + 1 >= vm->stack_size) {
    scc_log(LOG_ERR,"Stack overflow!\n");
    return SCVM_ERR_STACK_OVERFLOW;
  }
  scc_log(LOG_MSG,"Push %d\n",val);
  vm->stack[vm->stack_ptr] = val;
  vm->stack_ptr++;
  return 0;
}

int scvm_pop(scvm_t* vm,unsigned* val) {
  if(vm->stack_ptr < 1) {
    scc_log(LOG_ERR,"Stack underflow!\n");
    return SCVM_ERR_STACK_UNDERFLOW;
  }
  vm->stack_ptr--;
  scc_log(LOG_MSG,"Pop %d\n",vm->stack[vm->stack_ptr]);
  if(val) *val = vm->stack[vm->stack_ptr];
  return 0;
}

int scvm_spop(scvm_t* vm,int* val) {
  return scvm_pop(vm,(unsigned*)val);
}

int scvm_vpop(scvm_t* vm, ...) {
  va_list ap;
  unsigned* val;
  int r;
  va_start(ap,vm);
  while((val = va_arg(ap,unsigned*)))
    if((r = scvm_pop(vm,val))) return r;
  va_end(ap);
  return 0;
}

int scvm_peek(scvm_t* vm,int* val) {
  if(vm->stack_ptr < 1) {
    scc_log(LOG_ERR,"Stack underflow!\n");
    return SCVM_ERR_STACK_UNDERFLOW;
  }
  if(val) *val = vm->stack[vm->stack_ptr-1];
  return 0;
}

int scvm_thread_read_var(scvm_t* vm, scvm_thread_t* thread,
                         uint16_t addr, unsigned* val) {
  // decode the address
  if(addr & 0x8000) { // bit variable
    addr &= 0x7FFF;
    if(addr >= vm->num_bitvar) return SCVM_ERR_BAD_ADDR;
    *val = (vm->bitvar[addr>>3]>>(addr&7))&1;
    scc_log(LOG_MSG,"Read bit var %d: %d\n",addr,*val);
  } else if(addr & 0x4000) { // thread local variable
    addr &= 0x3FFF;
    if(!thread || addr >= thread->num_var) return SCVM_ERR_BAD_ADDR;
    *val = thread->var[addr];
    scc_log(LOG_MSG,"Read local var %d: %d\n",addr,*val);
  } else { // global variable
    addr &= 0x3FFF;
    if(addr >= vm->num_var) return SCVM_ERR_BAD_ADDR;
    if(addr < 0x100 && vm->get_var[addr])
      *val = vm->get_var[addr](vm,addr);
    else
      *val = vm->var_mem[addr];
    scc_log(LOG_MSG,"Read global var %d: %d\n",addr,*val);
  }
  return 0;
}

int scvm_thread_write_var(scvm_t* vm, scvm_thread_t* thread,
                          uint16_t addr, unsigned val) {
  // decode the address
  if(addr & 0x8000) { // bit variable
    addr &= 0x7FFF;
    if(addr >= vm->num_bitvar) return SCVM_ERR_BAD_ADDR;
    vm->bitvar[addr>>3] &= ~(1<<(addr&7));
    vm->bitvar[addr>>3] |= (val&1)<<(addr&7);
    scc_log(LOG_MSG,"Write bit var %d: %d\n",addr,val);
  } else if(addr & 0x4000) { // thread local variable
    addr &= 0x3FFF;
    if(!thread || addr >= thread->num_var) return SCVM_ERR_BAD_ADDR;
    thread->var[addr] = val;
    scc_log(LOG_MSG,"Write local var %d: %d\n",addr,val);
  } else { // global variable
    addr &= 0x3FFF;
    if(addr >= vm->num_var) return SCVM_ERR_BAD_ADDR;
    if(addr < 0x100 && vm->set_var[addr])
      vm->set_var[addr](vm,addr,val);
    else
      vm->var_mem[addr] = val;
    scc_log(LOG_MSG,"Write global var %d: %d\n",addr,val);
  }
  return 0;
}

int scvm_thread_read_svar(scvm_t* vm, scvm_thread_t* thread,
                          uint16_t addr, int* val) {
  return scvm_thread_read_var(vm,thread,addr,(unsigned*)val);
}

scvm_array_t* scvm_thread_get_array_var(scvm_t* vm, scvm_thread_t* thread,
                                        uint16_t addr) {
  unsigned id;
  if(scvm_thread_read_var(vm,vm->current_thread,addr,&id) ||
     id >= vm->num_array)
    return NULL;
  return vm->array[id].size ? &vm->array[id] : NULL;
}

unsigned char* scvm_thread_get_string_var(scvm_t* vm, scvm_thread_t* thread,
                                          uint16_t addr) {
  scvm_array_t* array = scvm_thread_get_array_var(vm,thread,addr);
  return array ? array->data.byte : NULL;
}


int scvm_read_array(scvm_t* vm, unsigned addr, unsigned x, unsigned y, int* val) {
  unsigned idx;
  if(addr >= vm->num_array) return SCVM_ERR_BAD_ADDR;
  idx = x+y*vm->array[addr].line_size;
  if(idx >= vm->array[addr].size)
    return SCVM_ERR_ARRAY_BOUND;
  switch(vm->array[addr].type) {
  case SCVM_ARRAY_WORD:
    *val = vm->array[addr].data.word[idx];
    break;
  case SCVM_ARRAY_BYTE:
  //case SCVM_ARRAY_CHAR:
    *val = vm->array[addr].data.byte[idx];
    break;
  case SCVM_ARRAY_NIBBLE:
    *val = (vm->array[addr].data.byte[idx>>2]>>((idx&1)<<2))&0xF;
    break;
  case SCVM_ARRAY_BIT:
    *val = (vm->array[addr].data.byte[idx>>3]>>(idx&7))&1;
    break;
  default:
    return SCVM_ERR_ARRAY_TYPE;
  }
  scc_log(LOG_MSG,"Read array %d[%d][%d]: %d\n",addr,x,y,*val);
  return 0;
}

int scvm_write_array(scvm_t* vm, unsigned addr, unsigned x, unsigned y, int val) {
  unsigned idx;
  scc_log(LOG_MSG,"Write array %d[%d][%d]: %d\n",addr,x,y,val);
  if(addr >= vm->num_array) return SCVM_ERR_BAD_ADDR;
  idx = x+y*vm->array[addr].line_size;
  if(idx >= vm->array[addr].size)
    return SCVM_ERR_ARRAY_BOUND;
  switch(vm->array[addr].type) {
  case SCVM_ARRAY_WORD:
    vm->array[addr].data.word[idx] = val;
    break;
  case SCVM_ARRAY_BYTE:
  //case SCVM_ARRAY_CHAR:
    val = vm->array[addr].data.byte[idx] = val;
    break;
  case SCVM_ARRAY_NIBBLE:
    vm->array[addr].data.byte[idx>>2] &= 0xF<<((idx&1)<<2);
    vm->array[addr].data.byte[idx>>2] |= (val&0xF)<<((idx&1)<<2);
    break;
  case SCVM_ARRAY_BIT:
    vm->array[addr].data.byte[idx>>3] &= ~(1<<(idx&7));
    vm->array[addr].data.byte[idx>>3] |= (val&1)<<(idx&7);
    break;
  default:
    return SCVM_ERR_ARRAY_TYPE;
  }
  return 0;
}


int scvm_alloc_array(scvm_t* vm, unsigned type, unsigned x, unsigned y) {
  scvm_array_t* array;
  int addr;
  
  switch(type) {
  case SCVM_ARRAY_WORD:
  case SCVM_ARRAY_BYTE:
    break;
  case SCVM_ARRAY_NIBBLE:
    x = (x+1)&~1;
    break;
  case SCVM_ARRAY_BIT:
    x = (x+7)&~7;
    break;
  default:
    return SCVM_ERR_ARRAY_TYPE;
  }
  
  for(addr = 1 ; addr < vm->num_array ; addr++)
      if(vm->array[addr].size == 0) break;
  if(addr >= vm->num_array) return SCVM_ERR_OUT_OF_ARRAY;
  array = &vm->array[addr];
  array->type = type;

  if(y) {
    array->line_size = x;
    array->size = x*y;
  } else {
    array->line_size = 0;
    array->size = x;
  }
  switch(type) {
  case SCVM_ARRAY_WORD:
    array->data.word = calloc(2,array->size);
    break;
  case SCVM_ARRAY_BYTE:
    array->data.byte = calloc(1,array->size);
    break;
  case SCVM_ARRAY_NIBBLE:
    array->data.byte = calloc(1,array->size>>1);
    break;
  case SCVM_ARRAY_BIT:
    array->data.byte = calloc(1,array->size>>3);
    break;
  }
  return addr;
}

static int scvm_nuke_array(scvm_t* vm, unsigned addr) {
  if(!addr || addr >= vm->num_array) return SCVM_ERR_BAD_ADDR;
  if(vm->array[addr].data.byte) free(vm->array[addr].data.byte);
  vm->array[addr].size = vm->array[addr].line_size = 0;
  return 0;
}

// SCUMM ops

// 0x00
static int scvm_op_push_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint8_t byte;
  if((r=scvm_thread_r8(thread,&byte))) return r;
  return scvm_push(vm,byte);
}

// 0x01
static int scvm_op_push_word(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  int16_t word;
  if((r=scvm_thread_r16s(thread,&word))) return r;
  return scvm_push(vm,word);
}

// 0x02
static int scvm_op_var_read_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  return scvm_push(vm,val);
}

// 0x03
static int scvm_op_var_read_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  return scvm_push(vm,val);
}

// 0x06
static int scvm_op_array_read_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  return scvm_push(vm,val);
}

// 0x07
static int scvm_op_array_read_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  return scvm_push(vm,val);
}

// 0x0A
static int scvm_op_array2_read_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x,y;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&x,&y,NULL)) ||
     (r=scvm_read_array(vm,addr,x,y,&val)))
    return r;
  return scvm_push(vm,val);
}

// 0x0B
static int scvm_op_array2_read_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x,y;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&x,&y,NULL)) ||
     (r=scvm_read_array(vm,addr,x,y,&val)))
    return r;
  return scvm_push(vm,val);
}

// 0x0C
static int scvm_op_dup(scvm_t* vm, scvm_thread_t* thread) {
  int r,a;
  if((r=scvm_peek(vm,&a))) return r;
  return scvm_push(vm,a);
}

// 0x0D
static int scvm_op_not(scvm_t* vm, scvm_thread_t* thread) {
  int r,a;
  if((r=scvm_spop(vm,&a))) return r;
  return scvm_push(vm,!a);
}

#define SCVM_BIN_OP(name,op) \
static int scvm_op_ ##name (scvm_t* vm, scvm_thread_t* thread) { \
  int r,a,b;                                              \
  if((r=scvm_vpop(vm,&b,&a,NULL)))                        \
    return r;                                             \
  return scvm_push(vm,(a op b));                          \
}

// 0x0E ... 0x19
SCVM_BIN_OP(eq,==)
SCVM_BIN_OP(neq,!=)
SCVM_BIN_OP(gt,>)
SCVM_BIN_OP(lt,<)
SCVM_BIN_OP(ge,>=)
SCVM_BIN_OP(le,<=)
SCVM_BIN_OP(add,+)
SCVM_BIN_OP(sub,-)
SCVM_BIN_OP(mul,*)
SCVM_BIN_OP(div,/)
SCVM_BIN_OP(land,&&)
SCVM_BIN_OP(lor,||)
// 0xD6 0xD7
SCVM_BIN_OP(band,&)
SCVM_BIN_OP(bor,|)

// 0x1A
static int scvm_op_pop(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,NULL);
}

// 0x42
static int scvm_op_var_write_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_spop(vm,&val))) return r;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x43
static int scvm_op_var_write_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_spop(vm,&val))) return r;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x46
static int scvm_op_array_write_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&val,&x,NULL)))
    return r;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x47
static int scvm_op_array_write_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&val,&x,NULL)))
    return r;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x4A
static int scvm_op_array2_write_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x,y;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&val,&x,&y,NULL)))
    return r;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x4B
static int scvm_op_array2_write_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x,y;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_vpop(vm,&val,&x,&y,NULL)))
    return r;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x4E
static int scvm_op_var_inc_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  val++;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x4F
static int scvm_op_var_inc_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  val++;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x52
static int scvm_op_array_inc_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  val++;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x53
static int scvm_op_array_inc_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  val++;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x56
static int scvm_op_var_dec_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  val--;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x57
static int scvm_op_var_dec_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_svar(vm,thread,addr,&val))) return r;
  val--;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x5A
static int scvm_op_array_dec_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  val--;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x5B
static int scvm_op_array_dec_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint16_t vaddr;
  if((r=scvm_thread_r16(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_read_array(vm,addr,x,0,&val)))
    return r;
  val--;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x5C
static int scvm_op_jmp_not_zero(scvm_t* vm, scvm_thread_t* thread) {
  int r,ptr,val;
  int16_t off;
  if((r=scvm_thread_r16s(thread,&off)) ||
     (r=scvm_spop(vm,&val))) return r;
  ptr = thread->code_ptr+off;
  if(ptr < 0 || ptr >= thread->script->size)
    return SCVM_ERR_JUMP_BOUND;
  if(val) thread->code_ptr = ptr;
  return 0;
}

// 0x5D
static int scvm_op_jmp_zero(scvm_t* vm, scvm_thread_t* thread) {
  int r,ptr,val;
  int16_t off;
  if((r=scvm_thread_r16s(thread,&off)) ||
     (r=scvm_spop(vm,&val))) return r;
  ptr = thread->code_ptr+off;
  if(ptr < 0 || ptr >= thread->script->size)
    return SCVM_ERR_JUMP_BOUND;
  if(!val) thread->code_ptr = ptr;
  return 0;
}

// 0x5E
static int scvm_op_start_script(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned num_args,script,flags;
  if((r=scvm_pop(vm,&num_args))) return r;
  else {
    unsigned args[num_args+1];
    args[0] = num_args;
    while(num_args > 0) {
      if((r=scvm_pop(vm,&args[num_args]))) return r;
      num_args--;
    }
    if((r=scvm_vpop(vm,&script,&flags,NULL)) ||
       (r=scvm_start_script(vm,flags,script,args)) < 0)
      return r;
    vm->next_thread = &vm->thread[r];
    return SCVM_START_SCRIPT;
  }    
}

// 0x5F
static int scvm_op_start_script0(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned num_args,script;
  if((r=scvm_pop(vm,&num_args))) return r;
  else {
    unsigned args[num_args+1];
    args[0] = num_args;
    while(num_args > 0) {
      if((r=scvm_pop(vm,&args[num_args]))) return r;
      num_args--;
    }
    if((r=scvm_pop(vm,&script)) ||
       (r=scvm_start_script(vm,0,script,args)) < 0)
      return r;
    vm->next_thread = &vm->thread[r];
    return SCVM_START_SCRIPT;
  }    
}

// 0xBF
static int scvm_op_start_script_recursive(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned num_args,script;
  if((r=scvm_pop(vm,&num_args))) return r;
  else {
    unsigned args[num_args+1];
    args[0] = num_args;
    while(num_args > 0) {
      if((r=scvm_pop(vm,&args[num_args]))) return r;
      num_args--;
    }
    if((r=scvm_pop(vm,&script)) ||
       (r=scvm_start_script(vm,SCVM_THREAD_RECURSIVE,script,args)) < 0)
      return r;
    vm->next_thread = &vm->thread[r];
    return SCVM_START_SCRIPT;
  }    
}

// 0x60
static int scvm_op_start_object_script(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned num_args,flags,obj,verb;
  if((r=scvm_pop(vm,&num_args))) return r;
  else {
    unsigned args[num_args+1];
    args[0] = num_args;
    while(num_args > 0) {
      if((r=scvm_pop(vm,&args[num_args]))) return r;
      num_args--;
    }
    if((r=scvm_vpop(vm,&verb,&obj,&flags,NULL)) ||
       (r=scvm_start_object_script(vm,obj,verb,flags,args)) < 0)
      return r;
    vm->next_thread = &vm->thread[r];
    return SCVM_START_SCRIPT;
  }
}


// 0x61
static int scvm_op_draw_object(scvm_t* vm, scvm_thread_t* thread) {
  int r,obj_id,state;
  if((r = scvm_vpop(vm,&state,&obj_id,NULL)))
    return r;
  if(!vm->room) return SCVM_ERR_NO_ROOM;
  // the lower address are actors
  if(obj_id < 0x10 || obj_id >= vm->num_object)
    return SCVM_ERR_BAD_OBJECT;
  vm->object_pdata[obj_id].state = state > 0 ? state : 1;
  return 0;
}

// 0x65, 0x66
static int scvm_op_stop_thread(scvm_t* vm, scvm_thread_t* thread) {
  scvm_stop_thread(vm,thread);
  return 0;
}

// 0x6B9C
static int scvm_op_init_charset(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned chset_id;
  struct scc_charmap_st* chset;
  if((r = scvm_pop(vm,&chset_id)))
    return r;
  if(!(chset = scvm_load_res(vm,SCVM_RES_CHARSET,chset_id)))
    return SCVM_ERR_BAD_RESOURCE;
  vm->current_charset = chset;
  return 0;
}

// 0x6C
static int scvm_op_break_script(scvm_t* vm, scvm_thread_t* thread) {
  thread->cycle = vm->cycle+1;
  return 0;
}

// 0x6D
static int scvm_op_is_object_of_class(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r = scvm_pop(vm,&len))) return r;
  else {
    unsigned obj,i,vals[len];
    for(i = 0 ; i < len ; i++)
      if((r = scvm_pop(vm,&vals[i]))) return r;
    if((r = scvm_pop(vm,&obj))) return r;
    if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
    for(i = 0 ; i < len ; i++)
      if(((vals[i] & 0x80) &&
          !(vm->object_pdata[obj].klass & (1 << (vals[i]&0x7F)))) ||
         (!(vals[i] & 0x80) &&
          (vm->object_pdata[obj].klass & (1 << (vals[i]&0x7F))))) {
        r = 1;
        break;
      }
  }
  return scvm_push(vm,!r);
}

// 0x6E
static int scvm_op_set_object_class(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r = scvm_pop(vm,&len))) return r;
  else {
    unsigned obj,i,vals[len];
    for(i = 0 ; i < len ; i++)
      if((r = scvm_pop(vm,&vals[i]))) return r;
    if((r = scvm_pop(vm,&obj))) return r;
    if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
    for(i = 0 ; i < len ; i++) {
      if(vals[i] & 0x80)
        vm->object_pdata[obj].klass |= 1 << (vals[i]&0x7F);
      else
        vm->object_pdata[obj].klass &= ~(1 << (vals[i]&0x7F));
    }
  }
  return 0;
}

// 0x6F
static int scvm_op_get_object_state(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned obj;
  if((r = scvm_pop(vm,&obj))) return r;
  if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
  return scvm_push(vm,vm->object_pdata[obj].state);
}

// 0x70
static int scvm_op_set_object_state(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned obj, state;
  if((r = scvm_vpop(vm,&state,&obj,NULL))) return r;
  if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
  vm->object_pdata[obj].state = state;
  return 0;
}

// 0x71
static int scvm_op_set_object_owner(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned obj, owner;
  if((r = scvm_vpop(vm,&owner,&obj,NULL))) return r;
  if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
  vm->object_pdata[obj].owner = owner;
  return 0;
}

// 0x72
static int scvm_op_get_object_owner(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned obj;
  if((r = scvm_pop(vm,&obj))) return r;
  if(obj >= vm->num_object) return SCVM_ERR_BAD_OBJECT;
  return scvm_push(vm,vm->object_pdata[obj].owner);
}

// 0x73
static int scvm_op_jmp(scvm_t* vm, scvm_thread_t* thread) {
  int r,ptr;
  int16_t off;
  if((r=scvm_thread_r16s(thread,&off))) return r;
  ptr = thread->code_ptr+off;
  if(ptr < 0 || ptr >= thread->script->size)
    return SCVM_ERR_JUMP_BOUND;
  thread->code_ptr = ptr;
  return 0;
}

// 0x78
static int scvm_op_pan_camera_to(scvm_t* vm, scvm_thread_t* thread) {
  int r,x;
  if((r=scvm_spop(vm,&x))) return r;
  scvm_pan_camera_to(vm,x);
  return 0;
}

// 0x79
static int scvm_op_camera_follow_actor(scvm_t* vm, scvm_thread_t* thread) {
  unsigned a;
  int r;
  if((r=scvm_pop(vm,&a))) return r;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  vm->view->follow = a;
  return SCVM_CAMERA_FOLLOW_ACTOR;
}

// 0x7A
static int scvm_op_set_camera_at(scvm_t* vm, scvm_thread_t* thread) {
  int r,x;
  if((r=scvm_spop(vm,&x)) ||
     (r=scvm_set_camera_at(vm,x)) < 0) return r;

  // Stop following actors and panning
  vm->view->follow = 0;
  vm->view->flags &= ~ SCVM_VIEW_PAN;

  if(r > 0) {
    vm->next_thread = &vm->thread[r-1];
    return SCVM_START_SCRIPT;
  }
  return 0;
}

// 0x7B
static int scvm_op_start_room(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  
  if((r=scvm_pop(vm,&vm->next_room))) return r;
  return SCVM_OPEN_ROOM;
}

// 0x7C
static int scvm_op_stop_script(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned id;
  if((r=scvm_pop(vm,&id))) return r;
  scvm_stop_script(vm,id);
  return 0;
}

// 0x7D
static int scvm_op_walk_actor_to_object(scvm_t* vm, scvm_thread_t* thread) {
  int r,x,y,dist;
  unsigned a,o;
  scvm_object_t* obj;

  if((r=scvm_vpop(vm,&dist,&o,&a,NULL)))
    return r;

  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  if(!(obj = scvm_get_object(vm,o))) return SCVM_ERR_BAD_OBJECT;

  x = obj->x, y = obj->y;
  if(obj->hotspot) {
    unsigned hs = obj->pdata->state;
    if(hs > obj->num_hotspot) hs = obj->num_hotspot-1;
    x += obj->hotspot[hs*2+0];
    y += obj->hotspot[hs*2+1];
  }
  if(dist) {
    if(vm->actor[a].x <= x)
      x -= dist;
    else
      x += dist;
  }
  scvm_actor_walk_to(&vm->actor[a],x,y,obj->actor_dir);
  return 0;
}

// 0x7E
static int scvm_op_walk_actor_to(scvm_t* vm, scvm_thread_t* thread) {
  int r,a,x,y;

  if((r=scvm_vpop(vm,&y,&x,&a,NULL)))
    return r;

  if(a < 0 || a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  scvm_actor_walk_to(&vm->actor[a],x,y,-1);
  return 0;
}


// 0x7F
static int scvm_op_put_actor_at(scvm_t* vm, scvm_thread_t* thread) {
  int r,a,x,y,room;

  if((r=scvm_vpop(vm,&room,&y,&x,&a,NULL)))
    return r;
  return scvm_put_actor_at(vm,a,x,y,room);
}

// 0x82
static int scvm_op_actor_animate(scvm_t* vm, scvm_thread_t* thread) {
  int r,a,f;
  if((r = scvm_vpop(vm,&f,&a,NULL)))
    return r;
  if(a < 0 || a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  scvm_actor_animate(&vm->actor[a],f);
  return 0;
}

// 0x83
static int scvm_op_do_sentence(scvm_t* vm, scvm_thread_t* thread) {
  int r,vrb,obj_a,dummy,obj_b;
  if((r = scvm_vpop(vm,&obj_b,&dummy,&obj_a,&vrb,NULL)))
    return r;
  scvm_do_sentence(vm,vrb,obj_a,obj_b);
  return 0;
}

// 0x87
static int scvm_op_get_random_number(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned max;
  if((r=scvm_pop(vm,&max))) return r;
  vm->var->random_num = scvm_random(vm,0,max);
  return scvm_push(vm,vm->var->random_num);
}

// 0x88
static int scvm_op_get_random_number_range(scvm_t* vm, scvm_thread_t* thread) {
  int r,min,max;
  if((r=scvm_vpop(vm,&max,&min,NULL)))
    return r;
  vm->var->random_num = scvm_random(vm,min,max);
  return scvm_push(vm,vm->var->random_num);
}

// 0x8B
static int scvm_op_is_script_running(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned id;
  if((r=scvm_pop(vm,&id))) return r;
  r = scvm_is_script_running(vm,id);
  return scvm_push(vm,r);
}

// 0x8C
static int scvm_op_get_actor_room(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a < 0 || a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,vm->actor[a].room);
}

// 0x8D
static int scvm_op_get_object_x(scvm_t* vm, scvm_thread_t* thread) {
  int r,x;
  unsigned o;
  if((r=scvm_pop(vm,&o)) ||
     (r=scvm_get_object_position(vm,o,&x,NULL)) < 0) return r;
  return scvm_push(vm,x);
}

// 0x8E
static int scvm_op_get_object_y(scvm_t* vm, scvm_thread_t* thread) {
  int r,y;
  unsigned o;
  if((r=scvm_pop(vm,&o)) ||
     (r=scvm_get_object_position(vm,o,NULL,&y)) < 0) return r;
  return scvm_push(vm,y);
}

// 0x8F
static int scvm_op_get_object_direction(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a < 0 || a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,vm->actor[a].direction);
}

// 0x90
static int scvm_op_get_actor_walk_box(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,vm->actor[a].box);
}

// 0x93
static int scvm_op_get_inventory_count(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,0);
}

// 0x94
static int scvm_op_get_verb_at(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned x,y;
  scvm_verb_t* verb;
  if((r=scvm_vpop(vm,&y,&x,NULL))) return r;
  verb = scvm_get_verb_at(vm,x,y);
  return scvm_push(vm,verb ? verb->id : 0);
  
}

// 0x95
static int scvm_op_begin_override(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  // there should be a jump there
  if(thread->code_ptr + 3 >= thread->script->size)
    return SCVM_ERR_SCRIPT_BOUND;
  if((r=scvm_thread_begin_override(vm,thread)))
    return r;
  // skip the jump
  thread->code_ptr += 3;
  return 0;
}

// 0x96
static int scvm_op_end_override(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_thread_end_override(vm,thread);
}

// 0x9B
static int scvm_op_resource(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned res;
  uint8_t op;
  
  if((r=scvm_thread_r8(thread,&op)) ||
     (r=scvm_pop(vm,&res))) return r;
  
  if(op == 0x65) return 0; // ignore sounds for now
  
  if(op >= 0x64 && op <= 0x67) {
    if(!scvm_load_res(vm,op-0x64,res))
      return SCVM_ERR_BAD_RESOURCE;
    return 0;
  } else if(op >= 0x6C && op <= 0x6F) {
    if(!scvm_lock_res(vm,op-0x6C,res))
      return SCVM_ERR_BAD_RESOURCE;
    return 0;
  } else if(op >= 0x70 && op <= 0x73) {
    if(!scvm_unlock_res(vm,op-0x70,res))
      return SCVM_ERR_BAD_RESOURCE;
    return 0;
  }
  
  switch(op) {    
  case 0x75: // load charset
    if(!scvm_load_res(vm,SCVM_RES_CHARSET,res))
      return SCVM_ERR_BAD_RESOURCE;
    return 0;
  case 0x77: // load fl object
    return scvm_pop(vm,NULL);
  }
  return SCVM_ERR_NO_OP;
}

// 0x9CAC
static int scvm_op_set_scrolling(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_vpop(vm,&vm->view->scroll_right,
                  &vm->view->scroll_left,NULL)))
    return r;
  return 0;
}

// 0x9CAE
static int scvm_op_set_screen(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_vpop(vm,&vm->view->room_end,
                  &vm->view->room_start,NULL)))
    return r;
  return 0;
}

// 0x9CAF
static int scvm_op_set_room_color(scvm_t* vm, scvm_thread_t* thread) {
  int r,red,green,blue,color;
  if((r=scvm_vpop(vm,&color,&blue,&green,&red,NULL)))
    return r;
  if(color < 0 || color > 0xFF) return 0;
  vm->view->palette[color].r = red;
  vm->view->palette[color].g = green;
  vm->view->palette[color].b = blue;
  vm->view->flags |= SCVM_VIEW_PALETTE_CHANGED;
  return 0;
}

// 0x9CB0
static int scvm_op_shake_on(scvm_t* vm, scvm_thread_t* thread) {
  vm->view->flags |= SCVM_VIEW_SHAKE;
  return 0;
}

// 0x9CB1
static int scvm_op_shake_off(scvm_t* vm, scvm_thread_t* thread) {
  vm->view->flags &= ~SCVM_VIEW_SHAKE;
  return 0;
}

// 0x9CB3
static int scvm_op_set_room_intensity(scvm_t* vm, scvm_thread_t* thread) {
  int r,scale,start,end;
  if((r=scvm_vpop(vm,&end,&start,&scale,NULL)))
    return r;
  if(!vm->room) return SCVM_ERR_NO_ROOM;
  scvm_view_scale_palette(vm->view,vm->room->current_palette,
                          scale,scale,scale,start,end);
  return 0;
}

// 0x9CB5
static int scvm_op_set_transition_effect(scvm_t* vm, scvm_thread_t* thread) {
    return scvm_pop(vm,&vm->view->effect);
}

// 0x9CB6
static int scvm_op_set_rgb_intensity(scvm_t* vm, scvm_thread_t* thread) {
  int r,red,green,blue,start,end;
  if((r=scvm_vpop(vm,&end,&start,&blue,&green,&red,NULL)))
    return r;
  if(!vm->room) return SCVM_ERR_NO_ROOM;
  scvm_view_scale_palette(vm->view,vm->room->current_palette,
                          red,green,blue,start,end);
  return 0;
}

// 0x9CD5
static int scvm_op_set_palette(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned p;
  if((r=scvm_pop(vm,&p))) return r;
  if(!vm->room) return SCVM_ERR_NO_ROOM;
  if(p < 0 || p >= vm->room->num_palette)
    return SCVM_ERR_BAD_PALETTE;
  vm->room->current_palette = vm->room->palette[p];
  memcpy(vm->view->palette,vm->room->current_palette,
         sizeof(scvm_palette_t));
  vm->view->flags |= SCVM_VIEW_PALETTE_CHANGED;
  return 0;
}


// 0x9DC5
static int scvm_op_set_current_actor(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  vm->current_actor = &vm->actor[a];
  return 0;
}

// 0x9D4C
static int scvm_op_set_actor_costume(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  scc_cost_t* cost = NULL;
  if((r=scvm_pop(vm,&a))) return r;
  if(a && !(cost = scvm_load_res(vm,SCVM_RES_COSTUME,a)))
    return SCVM_ERR_BAD_COSTUME;
  if(vm->current_actor)
    scvm_actor_set_costume(vm->current_actor,cost);
  return 0;
}

// 0x9D4D
static int scvm_op_set_actor_walk_speed(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_vpop(vm,&vm->current_actor->walk_speed_y,
                   &vm->current_actor->walk_speed_x,NULL);
}

// 0x9D4F
static int scvm_op_set_actor_walk_frame(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->walk_frame);
}

// 0x9D50
static int scvm_op_set_actor_talk_frame(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_vpop(vm,&vm->current_actor->talk_end_frame,
                   &vm->current_actor->talk_start_frame,NULL);
}

// 0x9D51
static int scvm_op_set_actor_stand_frame(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->stand_frame);
}

// 0x9D53
static int scvm_op_actor_init(scvm_t* vm, scvm_thread_t* thread) {
  if(vm->current_actor)
    scvm_actor_init(vm->current_actor);
  return 0;
}

// 0x9D54
static int scvm_op_set_actor_elevation(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->elevation);
}

// 0x9D54
static int scvm_op_set_actor_default_frames(scvm_t* vm,
                                            scvm_thread_t* thread) {
  if(vm->current_actor)
    scvm_actor_set_default_frames(vm->current_actor);
  return 0;
}


// 0x9D57
static int scvm_op_set_actor_talk_color(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->talk_color);
}

// 0x9D58
static int scvm_op_set_actor_name(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_thread_get_string(thread,&vm->current_actor->name);
}

// 0x9D59
static int scvm_op_set_actor_init_frame(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->init_frame);
}

// 0x9D5B
static int scvm_op_set_actor_width(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->width);
}

// 0x9D5C
static int scvm_op_set_actor_scale(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_pop(vm,&vm->current_actor->scale_x))) return r;
  vm->current_actor->scale_y = vm->current_actor->scale_x;
  return 0;
}

// 0x9D5F
static int scvm_op_set_actor_ignore_boxes(scvm_t* vm, scvm_thread_t* thread) {
  if(!vm->current_actor) return 0;
  vm->current_actor->flags |= SCVM_ACTOR_IGNORE_BOXES;
  vm->current_actor->box = 0;
  return 0;
}

// 0x9D60
static int scvm_op_set_actor_follow_boxes(scvm_t* vm, scvm_thread_t* thread) {
  if(!vm->current_actor) return 0;
  vm->current_actor->flags &= ~SCVM_ACTOR_IGNORE_BOXES;
  if(vm->current_actor->room == vm->room->id)
    scvm_put_actor_at(vm,vm->current_actor->id,
                      vm->current_actor->x,
                      vm->current_actor->y,
                      vm->current_actor->room);
  return 0;
}

// 0x9D61
static int scvm_op_set_actor_anim_speed(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->anim_speed);
}

// 0x9D63
static int scvm_op_set_actor_talk_pos(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_vpop(vm,&vm->current_actor->talk_y,
                   &vm->current_actor->talk_x,NULL);
}


// 0x9DE3
static int scvm_op_set_actor_layer(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->layer);
}

// 0x9DE4
static int scvm_op_set_actor_walk_script(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->walk_script);
}

// 0x9DE6
static int scvm_op_set_actor_direction(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->direction);
}

// 0x9DEB
static int scvm_op_set_actor_talk_script(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,&vm->current_actor->talk_script);
}

// 0x9E7D
static int scvm_op_set_verb_name(scvm_t* vm, scvm_thread_t* thread) {
  unsigned char** p_name = vm->current_verb ? &vm->current_verb->name : NULL;
  return scvm_thread_get_string(thread,p_name);
}

// 0x9E7E
static int scvm_op_set_verb_color(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned c;
  if((r=scvm_pop(vm,&c))) return r;
  if(c >= 0x100) return SCVM_ERR_BAD_COLOR;
  if(!vm->current_verb) return 0;
  vm->current_verb->color = c;
  return 0;
}

// 0x9E7F
static int scvm_op_set_verb_hi_color(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned c;
  if((r=scvm_pop(vm,&c))) return r;
  if(c >= 0x100) return SCVM_ERR_BAD_COLOR;
  if(!vm->current_verb) return 0;
  vm->current_verb->hi_color = c;
  return 0;
}

// 0x9E80
static int scvm_op_set_verb_xy(scvm_t* vm, scvm_thread_t* thread) {
  int r,x,y;
  if((r = scvm_vpop(vm,&y,&x,NULL))) return r;
  if(vm->current_verb) {
    vm->current_verb->y = y;
    vm->current_verb->x = x;
  }
  return 0;
}

// 0x9E81
static int scvm_op_set_verb_on(scvm_t* vm, scvm_thread_t* thread) {
  if(vm->current_verb)
    vm->current_verb->mode = SCVM_VERB_SHOW;
  return 0;
}

// 0x9E82
static int scvm_op_set_verb_off(scvm_t* vm, scvm_thread_t* thread) {
  if(vm->current_verb)
    vm->current_verb->mode = SCVM_VERB_HIDE;
  return 0;
}

// 0x9E83
static int scvm_op_kill_verb(scvm_t* vm, scvm_thread_t* thread) {
  if(!vm->current_verb_id) return 0;
  scvm_kill_verb(vm,vm->current_verb_id,0);
  return 0;
}

// 0x9E84
static int scvm_op_init_verb(scvm_t* vm, scvm_thread_t* thread) {
  if(!vm->current_verb_id) return SCVM_ERR_BAD_VERB;
  if(!(vm->current_verb = scvm_new_verb(vm,vm->current_verb_id)))
    return SCVM_ERR_TOO_MANY_VERB;
  return 0;
}

// 0x9E85
static int scvm_op_set_verb_dim_color(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned c;
  if((r=scvm_pop(vm,&c))) return r;
  if(c >= 0x100) return SCVM_ERR_BAD_COLOR;
  if(vm->current_verb)
    vm->current_verb->dim_color = c;
  return 0;
}

// 0x9E86
static int scvm_op_set_verb_dim(scvm_t* vm, scvm_thread_t* thread) {
  if(vm->current_verb)
    vm->current_verb->mode = SCVM_VERB_DIM;
  return 0;
}

// 0x9E87
static int scvm_op_set_verb_key(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,vm->current_verb ? &vm->current_verb->key : NULL);
}

// 0x9E88
static int scvm_op_set_verb_center(scvm_t* vm, scvm_thread_t* thread) {
  if(vm->current_verb)
    vm->current_verb->flags |= SCVM_VERB_CENTER;
  return 0;
}

// 0x9E8B
static int scvm_op_set_verb_object_image(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned room, obj;
  if((r = scvm_vpop(vm,&room,&obj,NULL))) return r;
  if(!vm->current_verb) return 0;
  return scvm_set_verb_image(vm,vm->current_verb_id,room,obj);
}

// 0x9EC4
static int scvm_op_set_current_verb(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned v;
  if((r=scvm_pop(vm,&v))) return r;
  if(!v) return SCVM_ERR_BAD_VERB;
  vm->current_verb_id = v;
  vm->current_verb = scvm_get_verb(vm,v,0);
  return 0;
}

// 0xA0
static int scvm_op_get_object_at(scvm_t* vm, scvm_thread_t* thread) {
  int r,x,y;
  scvm_object_t* obj;
  if((r = scvm_vpop(vm,&y,&x,NULL))) return r;
  obj = scvm_get_object_at(vm,x,y);
  return scvm_push(vm,obj ? obj->id : 0);
}

// 0xA3
static int scvm_op_get_object_verb_entry_point(scvm_t* vm,
                                               scvm_thread_t* thread) {
  int r;
  unsigned obj,verb,entry;
  if((r = scvm_vpop(vm,&verb,&obj,NULL)) ||
     (r = scvm_get_object_verb_entry_point(vm,obj,verb,&entry)) < 0)
    return r;
  return scvm_push(vm,entry);
}

// 0xA4CD
static int scvm_op_array_write_string(scvm_t* vm, scvm_thread_t* thread) {
  int r,addr;
  uint16_t vaddr;
  unsigned len,x;
  
  if((r=scvm_pop(vm,&x)) ||
     (r=scvm_thread_r16(thread,&vaddr)) ||
     (r = scvm_thread_strlen(thread,&len))) return r;
  if((addr = scvm_alloc_array(vm,SCVM_ARRAY_BYTE,
                              x+len+1,0)) < 0)
    return addr;
  if(len > 0)
    memcpy(vm->array[addr].data.byte+x,
           thread->script->code+thread->code_ptr,len);
  thread->code_ptr += len+1;
  vm->array[addr].data.byte[len] = 0;
  return scvm_thread_write_var(vm,thread,vaddr,addr);
}

// 0xA4D0
static int scvm_op_array_write_list(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint16_t vaddr;
  unsigned addr,val,len,x;
  
  if((r=scvm_pop(vm,&x)) ||
     (r=scvm_thread_r16(thread,&vaddr)) ||
     (r = scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r = scvm_pop(vm,&len))) return r;
  if(addr <= 0) {
    if((addr = scvm_alloc_array(vm,SCVM_ARRAY_WORD, x+len,0)) <= 0)
      return addr;
    scvm_thread_write_var(vm,thread,vaddr,addr);
  }
  while(len > 0) {
    if((r = scvm_pop(vm,&val)) ||
       (r=scvm_write_array(vm,addr,x,0,val))) return r;
    len--, x++;
  }
  return 0;
}

// 0xA4D4
static int scvm_op_array_write_list2(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint16_t vaddr;
  unsigned addr,len,x,y;
  
  if((r=scvm_pop(vm,&x)) ||
     (r=scvm_thread_r16(thread,&vaddr)) ||
     (r = scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r = scvm_pop(vm,&len))) return r;
  if(!addr || addr >= vm->num_array) return SCVM_ERR_BAD_ADDR;
  else {
    unsigned i,list[len];
    for(i = 0 ; i < len ; i++)
      if((r = scvm_pop(vm,list+i))) return r;
    if((r = scvm_pop(vm,&y))) return r;
    for(i = 0 ; i < len ; i++)
      if((r = scvm_write_array(vm,addr,x+i,y,list[i]))) return r;
  }
  return 0;
}

// 0xA58D
static int scvm_op_save_verbs(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned from,to,save_id,vrb;
  if((r=scvm_vpop(vm,&save_id,&to,&from,NULL)))
      return r;
  for(vrb = from ; vrb <= to ; vrb++)
      scvm_save_verb(vm,vrb,save_id);
  return 0;
}

// 0xA58E
static int scvm_op_restore_verbs(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned from,to,save_id,vrb;
  if((r=scvm_vpop(vm,&save_id,&to,&from,NULL)))
      return r;
  for(vrb = from ; vrb <= to ; vrb++)
      scvm_restore_verb(vm,vrb,save_id);
  return 0;
}

// 0xA58F
static int scvm_op_kill_verbs(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned from,to,save_id,vrb;
  if((r=scvm_vpop(vm,&save_id,&to,&from,&vrb,NULL)))
      return r;
  for(vrb = from ; vrb <= to ; vrb++)
      scvm_kill_verb(vm,vrb,save_id);
  return 0;
}


// 0xA9A8
static int scvm_op_wait_for_actor(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  int16_t off;
  if((r=scvm_pop(vm,&a)) ||
     (r=scvm_thread_r16s(thread,&off))) return r;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  if(vm->actor[a].walking) {
      thread->code_ptr += off;
      return scvm_op_break_script(vm,thread);
  }
  return 0;
}

// 0xA9A8 A9E2 A9E8
static int scvm_op_wait_for(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned x;
  uint16_t off;
  if((r=scvm_pop(vm,&x)) ||
     (r=scvm_thread_r16(thread,&off))) return r;
  return scvm_op_break_script(vm,thread);
}

// 0xA9A9 A9AA A9AB
static int scvm_op_wait(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_op_break_script(vm,thread);  
}

// 0xAA
static int scvm_op_get_actor_x_scale(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return a;
  if(a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,vm->actor[a].scale_x);
}

// 0xAB
static int scvm_op_get_actor_anim_counter(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned a;
  if((r=scvm_pop(vm,&a))) return r;
  if(a < 0 || a >= vm->num_actor) return SCVM_ERR_BAD_ACTOR;
  return scvm_push(vm,vm->actor[a].costdec.anim_counter);
}


// 0xAC
static int scvm_op_sound_kludge(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned num;
  // drop the list for now
  r=scvm_pop(vm,&num);
  while(!r && num > 0) {
    r=scvm_pop(vm,NULL);
    num--;
  }
  return r;
}

// 0xAD
static int scvm_op_is_any_of(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r=scvm_pop(vm,&len))) return r;
  else {
    unsigned i,val,list[len];
    for(i = len-1 ; i != -1 ; i--)
      if((r=scvm_pop(vm,list+i))) return r;
    if((r=scvm_pop(vm,&val))) return r;
    for(i = 0 ; i < len ; i++)
      if(val == list[i])
        return scvm_push(vm,1);
  }
  return scvm_push(vm,0);
}

// 0xAE9F
static int scvm_op_pause(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pause(vm);
}

// 0xAEA0
static int scvm_op_quit(scvm_t* vm, scvm_thread_t* thread) {
  return SCVM_QUIT;
}

// 0xB0
static int scvm_op_delay(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned d;
  if((r=scvm_pop(vm,&d))) return r;
  thread->delay = d*16;
  thread->state = SCVM_THREAD_DELAYED;
  return 0;
}

// 0xB1
static int scvm_op_delay_seconds(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned d;
  if((r=scvm_pop(vm,&d))) return r;
  thread->delay = d*1000;
  thread->state = SCVM_THREAD_DELAYED;
  return 0;
}

// 0xB2
static int scvm_op_delay_minutes(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned d;
  if((r=scvm_pop(vm,&d))) return r;
  thread->delay = d*60000;
  thread->state = SCVM_THREAD_DELAYED;
  return 0;
}

// 0xB5
static int scvm_op_dbg_print(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint8_t op;
  if((r = scvm_thread_r8(thread,&op))) return r;
  switch(op) {
  case 0x41: // at
    return scvm_op_dummy_vv(vm,thread);
  case 0x42: // color
  case 0x43: // clipped
    return scvm_op_dummy_v(vm,thread);
  case 0x45: // center
  case 0x47: // left
  case 0x48: // overhead
  case 0x4A: // mumble
  case 0xFE: // begin
  case 0xFF: // end
    return 0;
  case 0x4B:
    {
      unsigned char buffer[1024];
      if(!scvm_string_expand(vm,thread->script->code+thread->code_ptr,
                             buffer,sizeof(buffer)))
        printf("%s\n",(char*)buffer);
      if((r = scvm_thread_get_string(thread,NULL)))
        return r;
      return 0;
    }
  }
  return SCVM_ERR_NO_OP;
}

// 0xB8
static int scvm_op_actor_print(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint8_t op;
  if((r = scvm_thread_r8(thread,&op))) return r;
  switch(op) {
  case 0x41: // at
    return scvm_op_dummy_vv(vm,thread);
  case 0x42: // color
  case 0x43: // clipped
    return scvm_op_dummy_v(vm,thread);
  case 0x45: // center
  case 0x47: // left
  case 0x48: // overhead
  case 0x4A: // mumble
    return 0;
  case 0xFE: // begin
    return scvm_op_dummy_v(vm,thread);
  case 0xFF: // end
    return 0;
  case 0x4B:
    return scvm_op_dummy_s(vm,thread);
  }
  return SCVM_ERR_NO_OP;
}

// 0xBA
static int scvm_op_actor_say(scvm_t* vm, scvm_thread_t* thread) {
    int r;
    unsigned actor,len;
    if((r = scvm_pop(vm,&actor))) return r;

    if((r = scvm_thread_strlen(thread,&len))) return r;
    thread->code_ptr += len+1;

    return 0;
}

// 0xBB
static int scvm_op_ego_say(scvm_t* vm, scvm_thread_t* thread) {
    scvm_push(vm, vm->var->ego);
    return scvm_op_actor_say(vm, thread);
}

static int scvm_op_dim_array(scvm_t* vm, scvm_thread_t* thread, int dim) {
  int r;
  unsigned addr,size, size_y = 0;
  uint8_t op;
  uint16_t vaddr;
  
  if((r=scvm_thread_r8(thread,&op)) ||
     (r=scvm_thread_r16(thread,&vaddr))) return r;
  
  if(op == 0xCC) { // UNDIM
    if((r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
       (r=scvm_nuke_array(vm,addr)))
      return r;
    return scvm_thread_write_var(vm,thread,vaddr,0);
  }
  
  if(op < 0xC7 || op > 0xCB) return SCVM_ERR_NO_OP;

  if((r=scvm_pop(vm,&size))) return r;
  if(dim > 1 && (r=scvm_pop(vm,&size_y))) return r;
  
  switch(op) {
  case 0xC7:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_WORD,size,size_y);
    break;
  case 0xC8:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_BIT,size,size_y);
    break;
  case 0xC9:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_NIBBLE,size,size_y);
    break;
  case 0xCA: // byte
  case 0xCB: // char
    addr = scvm_alloc_array(vm,SCVM_ARRAY_BYTE,size,size_y);
    break;
  }
  if(addr < 0) return addr;
  return scvm_thread_write_var(vm,thread,vaddr,addr);
}

// 0xBC
static int scvm_op_dim(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_op_dim_array(vm,thread,1);
}


// 0xC0
static int scvm_op_dim2(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_op_dim_array(vm,thread,2);
}

// 0xC5
static int scvm_op_get_distance_obj_obj(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned objA, objB;
  int ax,ay, bx,by;

  if((r = scvm_vpop(vm,&objB,&objA,NULL)) ||
     (r = scvm_get_object_position(vm,objA,&ax,&ay)) < 0 ||
     (r = scvm_get_object_position(vm,objB,&bx,&by)) < 0) return r;
  return scvm_push(vm,hypot(bx-ax,by-ay));
}

// 0xCA
static int scvm_op_break_script_n_times(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned n;
  if((r=scvm_pop(vm,&n))) return r;
  thread->cycle = vm->cycle+n;
  return 0;
}

static int scvm_op_dummy(scvm_t* vm, scvm_thread_t* thread) {
  return 0;
}

static int scvm_op_dummy_v(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_pop(vm,NULL);
}

static int scvm_op_dummy_vv(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_pop(vm,NULL))) return r;
  return scvm_pop(vm,NULL);
}

static int scvm_op_dummy_vvv(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL))) return r;
  return scvm_pop(vm,NULL);
}

static int scvm_op_dummy_vvvv(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL))) return r;
  return scvm_pop(vm,NULL);
}

static int scvm_op_dummy_vvvvv(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  if((r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL)) ||
     (r=scvm_pop(vm,NULL))) return r;
  return scvm_pop(vm,NULL);
}

static int scvm_op_dummy_l(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r=scvm_pop(vm,&len))) return r;
  while(len > 0) {
    if((r=scvm_pop(vm,NULL))) return r;
    len--;
  }
  return 0;
}

static int scvm_op_dummy_vl(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r=scvm_pop(vm,&len))) return r;
  while(len > 0) {
    if((r=scvm_pop(vm,NULL))) return r;
    len--;
  }
  return scvm_pop(vm,NULL);
}


static int scvm_op_dummy_s(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  unsigned len;
  if((r = scvm_thread_strlen(thread,&len))) return r;
  thread->code_ptr += len+1;
  return 0;
}

static int scvm_op_dummy_print(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  uint8_t op;
  if((r = scvm_thread_r8(thread,&op))) return r;
  switch(op) {
  case 0x41: // at
    return scvm_op_dummy_vv(vm,thread);
  case 0x42: // color
  case 0x43: // clipped
    return scvm_op_dummy_v(vm,thread);
  case 0x45: // center
  case 0x47: // left
  case 0x48: // overhead
  case 0x4A: // mumble
  case 0xFE: // begin
  case 0xFF: // end
    return 0;
  case 0x4B:
    return scvm_op_dummy_s(vm,thread);
  }
  return SCVM_ERR_NO_OP;
}

static int scvm_op_dummy_get_at(scvm_t* vm, scvm_thread_t* thread) {
  int r,x,y;
  if((r = scvm_vpop(vm,&y,&x,NULL))) return r;
  return scvm_push(vm,0);
}


static int scvm_op_subop(scvm_t* vm, scvm_thread_t* thread) {
  return scvm_thread_do_op(vm,thread,vm->suboptable);
}

scvm_op_t scvm_optable[0x100] = {
  // 00
  { scvm_op_push_byte, "push" },
  { scvm_op_push_word, "push" },
  { scvm_op_var_read_byte, "read var" },
  { scvm_op_var_read_word, "read var" },
  // 04
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array_read_byte, "read array" },
  { scvm_op_array_read_word, "read array" },
  // 08
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array2_read_byte, "read array2" },
  { scvm_op_array2_read_word, "read array2" },
  // 0C
  { scvm_op_dup, "dup" },
  { scvm_op_not, "not" },
  { scvm_op_eq, "eq" },
  { scvm_op_neq, "neq" },
  // 10
  { scvm_op_gt, "gt" },
  { scvm_op_lt, "lt" },
  { scvm_op_le, "le" },
  { scvm_op_ge, "ge" },
  // 14
  { scvm_op_add, "add" },
  { scvm_op_sub, "sub" },
  { scvm_op_mul, "mul" },
  { scvm_op_div, "div" },
  // 18
  { scvm_op_land, "land" },
  { scvm_op_lor, "lor" },
  { scvm_op_pop, "pop" },
  { NULL, NULL },
  // 1C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 20
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 24
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 28
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 2C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 30
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 34
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 38
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 3C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 40
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_var_write_byte, "write var" },
  { scvm_op_var_write_word, "write var" },
  // 44
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array_write_byte, "write array" },
  { scvm_op_array_write_word, "write array" },
  // 48
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array2_write_byte, "write array2" },
  { scvm_op_array2_write_word, "write array2" },
  // 4C
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_var_inc_byte, "inc var" },
  { scvm_op_var_inc_word, "inc var" },
  // 50
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array_inc_byte, "inc array" },
  { scvm_op_array_inc_word, "inc array" },
  // 54
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_var_dec_byte, "dec var" },
  { scvm_op_var_dec_word, "dec var" },
  // 58
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_array_dec_byte, "dec array" },
  { scvm_op_array_dec_word, "dec array" },
  // 5C
  { scvm_op_jmp_not_zero, "jump if not zero" },
  { scvm_op_jmp_zero, "jump if zero" },
  { scvm_op_start_script, "start script" },
  { scvm_op_start_script0, "start script0" },
  // 60
  { scvm_op_start_object_script, "start object script" },
  { scvm_op_draw_object, "draw object" },
  { NULL, NULL },
  { NULL, NULL },
  // 64
  { NULL, NULL },
  { scvm_op_stop_thread, "stop thread" },
  { scvm_op_stop_thread, "stop thread" },
  { scvm_op_dummy, "end thread" },
  // 68
  { scvm_op_dummy_l, "begin cutscene" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_subop, "interface op" },
  // 6C
  { scvm_op_break_script, "break script" },
  { scvm_op_is_object_of_class, "is object of class" },
  { scvm_op_set_object_class, "set class" },
  { scvm_op_get_object_state, "get object state" },
  // 70
  { scvm_op_set_object_state, "set object state" },
  { scvm_op_set_object_owner, "set object owner" },
  { scvm_op_get_object_owner, "get object owner" },
  { scvm_op_jmp, "jump" },
  // 74
  { scvm_op_dummy_v, "start sound" },
  { scvm_op_dummy_v, "stop sound" },
  { scvm_op_dummy_v, "start music" },
  { NULL, NULL },
  // 78
  { scvm_op_pan_camera_to, "pan camera to" },
  { scvm_op_camera_follow_actor, "camera follow actor" },
  { scvm_op_set_camera_at, "set camera at" },
  { scvm_op_start_room, "start room" },
  // 7C
  { scvm_op_stop_script, "stop script" },
  { scvm_op_walk_actor_to_object, "walk actor to object" },
  { scvm_op_walk_actor_to, "walk actor to" },
  { scvm_op_put_actor_at, "put actor at" },
  // 80
  { scvm_op_dummy_vvv,  "put actor at object" },
  { scvm_op_dummy_vv, "actor face" },
  { scvm_op_actor_animate, "actor animate" },
  { scvm_op_do_sentence, "do sentence" },
  // 84
  { scvm_op_dummy_vv, "pickup object" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_get_random_number, "get random number" },
  // 88
  { scvm_op_get_random_number_range, "get random number range" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_is_script_running, "is script running" },
  // 8C
  { scvm_op_get_actor_room, "get actor room" },
  { scvm_op_get_object_x, "get object x" },
  { scvm_op_get_object_y, "get object y" },
  { scvm_op_get_object_direction, "get object direction" },
  // 90
  { scvm_op_get_actor_walk_box, "get actor walk box" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_get_inventory_count, "get inventory count" },
  // 94
  { scvm_op_get_verb_at, "get verb at" },
  { scvm_op_begin_override, "begin override" },
  { scvm_op_end_override, "end override" },
  { NULL, NULL },
  // 98
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_resource, "resource op" },
  // 9C
  { scvm_op_subop, "view op" },
  { scvm_op_subop, "actor op" },
  { scvm_op_subop, "verb op" },
  { scvm_op_dummy_get_at, "get actor at" },
  // A0
  { scvm_op_get_object_at, "get object at" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_get_object_verb_entry_point, "get object verb entry point" },
  // A4
  { scvm_op_subop, "array write" },
  { scvm_op_subop, "save-load verbs" },
  { NULL, NULL },
  { NULL, NULL },
  // A8
  { NULL, NULL },
  { scvm_op_subop, "wait" },
  { scvm_op_get_actor_x_scale, "get actor x scale" },
  { scvm_op_get_actor_anim_counter, "get actor anim counter" },
  // AC
  { scvm_op_sound_kludge, "sound kludge" },
  { scvm_op_is_any_of, "is any of" },
  { scvm_op_subop, "system ops" },
  { NULL, NULL },
  // B0
  { scvm_op_delay, "delay" },
  { scvm_op_delay_seconds, "delay seconds" },
  { scvm_op_delay_minutes, "delay minutes"  },
  { scvm_op_dummy, "stop sentence" },
  // B4
  { scvm_op_dummy_print, "print" },
  { scvm_op_dummy_print, "cursor print" },
  { scvm_op_dbg_print, "debug print" },
  { scvm_op_dummy_print, "sys print" },
  // B8
  { scvm_op_actor_print, "actor print" },
  { scvm_op_dummy_print, "ego print" },
  { scvm_op_actor_say, "actor say" },
  { scvm_op_ego_say, "ego say" },
  // BC
  { scvm_op_dim, "dim" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_start_script_recursive, "start script recursive" },
  // C0
  { scvm_op_dim2, "dim 2d" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // C4
  { NULL, NULL },
  { scvm_op_get_distance_obj_obj, "get dist obj obj" },
  { NULL, NULL },
  { NULL, NULL },
  // C8
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_break_script_n_times, "break script n times" },
  { NULL, NULL },
  // CC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // D0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // D4
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_band, "bit and" },
  { scvm_op_bor, "bit or" },
  // D8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // DC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // E0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // E4
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // E8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // EC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F4
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // FC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL }
};

scvm_op_t scvm_suboptable[0x100] = {
  // 00
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 04
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 08
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 0C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 10
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 14
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 18
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 1C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 20
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 24
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 28
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 2C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 30
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 34
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 38
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 3C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 40
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 44
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 48
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 4C
  { scvm_op_set_actor_costume, "set costume" },
  { scvm_op_set_actor_walk_speed, "set walk speed" },
  { scvm_op_dummy_l, "set sounds" },
  { scvm_op_set_actor_walk_frame, "set walk frame" },
  // 50
  { scvm_op_set_actor_talk_frame, "set talk frame" },
  { scvm_op_set_actor_stand_frame, "set stand frame" },
  { NULL, NULL },
  { scvm_op_actor_init, "init" },
  // 54
  { scvm_op_set_actor_elevation, "set elevation" },
  { scvm_op_set_actor_default_frames, "default frames" },
  { NULL, NULL },
  { scvm_op_set_actor_talk_color, "set talk color" },
  // 58
  { scvm_op_set_actor_name, "set name" },
  { scvm_op_set_actor_init_frame, "set init frame" },
  { NULL, NULL },
  { scvm_op_set_actor_width, "set width" },
  // 5C
  { scvm_op_set_actor_scale, "set scale" },
  { scvm_op_dummy, "never z clip" },
  { NULL, NULL },
  { scvm_op_set_actor_ignore_boxes, "ignore boxes" },
  // 60
  { scvm_op_set_actor_follow_boxes, "follow boxes" },
  { scvm_op_set_actor_anim_speed, "set anim speed" },
  { scvm_op_dummy_v, "set shadow mode" },
  { scvm_op_set_actor_talk_pos, "set talk pos" },
  // 64
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 68
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 6C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 70
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 74
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 78
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 7C
  { scvm_op_dummy_v, "set image" },
  { scvm_op_set_verb_name, "set name" },
  { scvm_op_set_verb_color, "set color" },
  { scvm_op_set_verb_hi_color, "set hi-color" },
  // 80
  { scvm_op_set_verb_xy, "set XY" },
  { scvm_op_set_verb_on, "set on" },
  { scvm_op_set_verb_off, "set off" },
  { scvm_op_kill_verb, "kill" },
  // 84
  { scvm_op_init_verb, "init" },
  { scvm_op_set_verb_dim_color, "set dim-color" },
  { scvm_op_set_verb_dim, "dim" },
  { scvm_op_set_verb_key, "set key" },
  // 88
  { scvm_op_set_verb_center, "set center" },
  { scvm_op_dummy_v, "set name string" },
  { NULL, NULL },
  { scvm_op_set_verb_object_image, "set object" },
  // 8C
  { scvm_op_dummy_v, "set back color" },
  { scvm_op_save_verbs, "save verbs" },
  { scvm_op_restore_verbs, "restore verbs" },
  { scvm_op_kill_verbs, "kill verbs" },
  // 90
  { scvm_op_dummy, "cursor on" },
  { scvm_op_dummy, "cursor off" },
  { scvm_op_dummy, "user put on" },
  { scvm_op_dummy, "user put off" },
  // 94
  { scvm_op_dummy, "soft cursor on" },
  { scvm_op_dummy, "soft curosr off" },
  { scvm_op_dummy, "soft user put on" },
  { scvm_op_dummy, "soft user put off" },
  // 98
  { NULL, NULL },
  { scvm_op_dummy_vv, "set cursor image" },
  { scvm_op_dummy_vv, "set cursor hotspot" },
  { NULL, NULL },
  // 9C
  { scvm_op_init_charset, "init charset" },
  { scvm_op_dummy_l, "set charset colors" },
  { scvm_op_dummy, "restart" },
  { scvm_op_pause, "pause" },
  // A0
  { scvm_op_quit, "quit" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // A4
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // A8
  { scvm_op_wait_for_actor, "wait actor" },
  { scvm_op_wait, "wait msg" },
  { scvm_op_wait, "wait camera" },
  { scvm_op_wait, "wait sentence" },
  // AC
  { scvm_op_set_scrolling, "set scrolling" },
  { NULL, NULL },
  { scvm_op_set_screen, "set screen" },
  { scvm_op_set_room_color, "room color" },
  // B0
  { scvm_op_shake_on, "shake on" },
  { scvm_op_shake_off, "shake off" },
  { NULL, NULL },
  { scvm_op_set_room_intensity, "room intensity" },
  // B4
  { scvm_op_dummy_vv, "save/load thing" },
  { scvm_op_set_transition_effect, "transition effect" },
  { scvm_op_set_rgb_intensity, "rgb intensitiy" },
  { scvm_op_dummy_vvvvv, "room shadow" },
  // B8
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_dummy_vvvv, "pal manip" },
  { scvm_op_dummy_vv, "cycle delay" },
  // BC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // C0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // C4
  { scvm_op_set_current_verb, "set current verb" },
  { scvm_op_set_current_actor, "set current actor" },
  { scvm_op_dummy_vv, "set anim var" },
  { NULL, NULL },
  // C8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // CC
  { NULL, NULL },
  { scvm_op_array_write_string, "array write string" },
  { NULL, NULL },
  { NULL, NULL },
  // D0
  { scvm_op_array_write_list, "array write list" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // D4
  { scvm_op_array_write_list2, "array write list2" },
  { scvm_op_set_palette, "set palette" },
  { NULL, NULL },
  { scvm_op_dummy_v, "set cursor transparency" },
  // D8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // DC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // E0
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_wait_for, "wait anim" },
  { scvm_op_set_actor_layer, "set layer" },
  // E4
  { scvm_op_set_actor_walk_script, "set walk script" },
  { scvm_op_dummy, "set standing" },
  { scvm_op_set_actor_direction, "set direction" },
  { scvm_op_dummy_v, "turn to direction" },
  // E8
  { scvm_op_wait_for, "wait turn" },
  { scvm_op_dummy, "freeze" },
  { scvm_op_dummy, "unfreeze" },
  { scvm_op_set_actor_talk_script, "set talk script" },
  // EC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F4
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // F8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // FC
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_dummy, "redraw" }
};
