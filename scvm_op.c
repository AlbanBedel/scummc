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
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

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

int scvm_pop(scvm_t* vm,int* val) {
  if(vm->stack_ptr < 1) {
    scc_log(LOG_ERR,"Stack underflow!\n");
    return SCVM_ERR_STACK_UNDERFLOW;
  }
  vm->stack_ptr--;
  scc_log(LOG_MSG,"Pop %d\n",vm->stack[vm->stack_ptr]);
  if(val) *val = vm->stack[vm->stack_ptr];
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
                         uint16_t addr, int* val) {
  // decode the address
  if(addr & 0x8000) { // bit variable
    addr &= 0x7FFF;
    if(addr >= vm->num_bitvar) return SCVM_ERR_BAD_ADDR;
    *val = (vm->bitvar[addr>>3]>>(addr&7))&1;
  } else if(addr & 0x4000) { // thread local variable
    addr &= 0x3FFF;
    if(addr >= thread->num_var) return SCVM_ERR_BAD_ADDR;
    *val = thread->var[addr];
  } else { // global variable
    addr &= 0x3FFF;
    if(addr >= vm->num_var) return SCVM_ERR_BAD_ADDR;
    if(addr < 0x100 && vm->get_var[addr])
      *val = vm->get_var[addr](vm,addr);
    else
      *val = vm->var[addr];
  }
  return 0;
}

int scvm_thread_write_var(scvm_t* vm, scvm_thread_t* thread,
                          uint16_t addr, int val) {
  // decode the address
  if(addr & 0x8000) { // bit variable
    addr &= 0x7FFF;
    if(addr >= vm->num_bitvar) return SCVM_ERR_BAD_ADDR;
    vm->bitvar[addr>>3] &= ~(1<<(addr&7));
    vm->bitvar[addr>>3] |= (val&1)<<(addr&7);
  } else if(addr & 0x4000) { // thread local variable
    addr &= 0x3FFF;
    if(addr >= thread->num_var) return SCVM_ERR_BAD_ADDR;
    thread->var[addr] = val;
  } else { // global variable
    addr &= 0x3FFF;
    if(addr >= vm->num_var) return SCVM_ERR_BAD_ADDR;
    if(addr < 0x100 && vm->set_var[addr])
      vm->set_var[addr](vm,addr,val);
    else
      vm->var[addr] = val;
  }
  return 0;
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
  return 0;
}

int scvm_write_array(scvm_t* vm, unsigned addr, unsigned x, unsigned y, int val) {
  unsigned idx;
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
  uint16_t word;
  if((r=scvm_thread_r16(thread,&word))) return r;
  return scvm_push(vm,word);
}

// 0x02
static int scvm_op_var_read_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
  return scvm_push(vm,val);
}

// 0x03
static int scvm_op_var_read_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
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
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_pop(vm,&y)) ||
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
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_pop(vm,&y)) ||
     (r=scvm_read_array(vm,addr,x,y,&val)))
    return r;
  return scvm_push(vm,val);
}

// 0x0C
static int scvm_op_dup(scvm_t* vm, scvm_thread_t* thread) {
  int r,a;
  if((r=scvm_peek(vm,&a))) return r;
  return scvm_push(vm,!a);
}

// 0x0D
static int scvm_op_not(scvm_t* vm, scvm_thread_t* thread) {
  int r,a;
  if((r=scvm_pop(vm,&a))) return r;
  return scvm_push(vm,!a);
}

#define SCVM_BIN_OP(name,op) \
static int scvm_op_ ##name (scvm_t* vm, scvm_thread_t* thread) { \
  int r,a,b;                                              \
  if((r=scvm_pop(vm,&b)) ||                               \
     (r=scvm_pop(vm,&a))) return r;                       \
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
     (r=scvm_pop(vm,&val))) return r;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x43
static int scvm_op_var_write_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_pop(vm,&val))) return r;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x46
static int scvm_op_array_write_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  unsigned addr,x;
  uint8_t vaddr;
  if((r=scvm_thread_r8(thread,&vaddr)) ||
     (r=scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
     (r=scvm_pop(vm,&val)) ||
     (r=scvm_pop(vm,&x)))
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
     (r=scvm_pop(vm,&val)) ||
     (r=scvm_pop(vm,&x)))
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
     (r=scvm_pop(vm,&val)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_pop(vm,&y)))
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
     (r=scvm_pop(vm,&val)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_pop(vm,&y)))
    return r;
  return scvm_write_array(vm,addr,x,0,val);
}

// 0x4E
static int scvm_op_var_inc_byte(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint8_t addr;
  if((r=scvm_thread_r8(thread,&addr)) ||
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
  val++;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x4F
static int scvm_op_var_inc_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
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
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
  val--;
  return scvm_thread_write_var(vm,thread,addr,val);
}

// 0x57
static int scvm_op_var_dec_word(scvm_t* vm, scvm_thread_t* thread) {
  int r,val;
  uint16_t addr;
  if((r=scvm_thread_r16(thread,&addr)) ||
     (r=scvm_thread_read_var(vm,thread,addr,&val))) return r;
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
  if((r=scvm_thread_r16(thread,&off)) ||
     (r=scvm_pop(vm,&val))) return r;
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
  if((r=scvm_thread_r16(thread,&off)) ||
     (r=scvm_pop(vm,&val))) return r;
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
    int args[num_args+1];
    args[0] = num_args;
    while(num_args > 0) {
      if((r=scvm_pop(vm,&args[num_args]))) return r;
      num_args--;
    }
    if((r=scvm_pop(vm,&script)) ||
       (r=scvm_pop(vm,&flags)) ||
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
    int args[num_args+1];
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
    int args[num_args+1];
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

// 0x65, 0x66
static int scvm_op_stop_thread(scvm_t* vm, scvm_thread_t* thread) {
  scvm_stop_thread(vm,thread);
  return 0;
}

// 0x6B
static int scvm_op_interface(scvm_t* vm, scvm_thread_t* thread) {
  int r,res;
  uint8_t op;
  
  if((r=scvm_thread_r8(thread,&op))) return r;
  // no args, just ignore atm
  scc_log(LOG_MSG,"Cursor op 0x%x\n",op);
  if(op >= 0x90 && op <= 0x97)
    return 0;
  switch(op) {
  case 0x9C: // init charset
    if((r=scvm_pop(vm,&res))) return r;
    //vm->charset = scvm_load_res(vm,SCVM_RES_CHARSET);
    return 0;
  }
  
  return SCVM_ERR_NO_OP;
}

// 0x7B
static int scvm_op_start_room(scvm_t* vm, scvm_thread_t* thread) {
  int r;
  
  if((r=scvm_pop(vm,&vm->next_room))) return r;
  return SCVM_OPEN_ROOM;
}

// 0x9B
static int scvm_op_resource(scvm_t* vm, scvm_thread_t* thread) {
  int r,res;
  uint8_t op;
  
  if((r=scvm_thread_r8(thread,&op)) ||
     (r=scvm_pop(vm,&res))) return r;
  
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

  }
  return SCVM_ERR_NO_OP;
}

// 0x9C
static int scvm_op_view(scvm_t* vm, scvm_thread_t* thread) {
  int r,a,b,c,d,e;
  uint8_t op;
  
  if((r=scvm_thread_r8(thread,&op))) return r;
  switch(op) {
  case 0xAC: // scrolling
    if((r=scvm_pop(vm,&vm->view->scroll_right)) ||
       (r=scvm_pop(vm,&vm->view->scroll_left))) return r;
    return 0;
  case 0xAE: // screen pos
    if((r=scvm_pop(vm,&vm->view->room_end)) ||
       (r=scvm_pop(vm,&vm->view->room_start))) return r;
    return 0;
  case 0xAF: // room color
    if((r=scvm_pop(vm,&d)) ||
       (r=scvm_pop(vm,&c)) ||
       (r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
    return 0;
  case 0xB0: // shake on
    vm->view->flags |= SCVM_VIEW_SHAKE;
    return 0;
  case 0xB1: // shake off
    vm->view->flags &= ~SCVM_VIEW_SHAKE;
    return 0;
  case 0xB3: // room intensity
    if((r=scvm_pop(vm,&c)) ||
       (r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
    return 0;
  case 0xB4: // save load thing
    if((r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
    return 0;
  case 0xB5: // screen effect
    return scvm_pop(vm,&vm->view->effect);
  case 0xB6: // rgb intensity
  case 0xB7: // room shadow
    if((r=scvm_pop(vm,&e)) ||
       (r=scvm_pop(vm,&d)) ||
       (r=scvm_pop(vm,&c)) ||
       (r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
    return 0;
  case 0xBA: // pal manip
    if((r=scvm_pop(vm,&d)) ||
       (r=scvm_pop(vm,&c)) ||
       (r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
    return 0;
  case 0xBB: // cycle delay
    if((r=scvm_pop(vm,&b)) ||
       (r=scvm_pop(vm,&a))) return r;
  case 0xD5: // room palette
    if((r=scvm_pop(vm,&a))) return r;
    return 0;
  }
  return SCVM_ERR_NO_OP;
}

// 0xA4
static int scvm_op_array_write(scvm_t* vm, scvm_thread_t* thread) {
  int r,val,addr;
  uint8_t op;
  uint16_t vaddr;
  unsigned len,x,y = 0;
  
  if((r=scvm_thread_r8(thread,&op)) ||
     (r=scvm_pop(vm,&x)) ||
     (r=scvm_thread_r16(thread,&vaddr))) return r;
  switch(op) {
  case 0xCD: // STRING
    if((r = scvm_thread_strlen(thread,&len))) return r;
    if((addr = scvm_alloc_array(vm,SCVM_ARRAY_BYTE,
                                x+len+1,0)) < 0)
      return addr;
    if(len > 0)
      memcpy(vm->array[addr].data.byte+x,
             thread->script->code+thread->code_ptr,len);
    thread->code_ptr += len+1;
    vm->array[addr].data.byte[len] = 0;
    return scvm_thread_write_var(vm,thread,vaddr,addr);
    
  case 0xD0: // LIST
    if((r = scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
       (r = scvm_pop(vm,&len))) return r;
    if(addr <= 0 && (addr = scvm_alloc_array(vm,SCVM_ARRAY_WORD,
                                             x+len,0)) <= 0)
      return addr;
    while(len > 0) {
      if((r = scvm_pop(vm,&val)) ||
         (r=scvm_write_array(vm,addr,x,0,val))) return r;
      len--, x++;
    }
    return 0;
  case 0xD4: // LIST 2D
    if((r = scvm_thread_read_var(vm,thread,vaddr,&addr)) ||
       (r = scvm_pop(vm,&len))) return r;
    if(!addr || addr >= vm->num_array) return SCVM_ERR_BAD_ADDR;
    else {
      int i,list[len];
      for(i = 0 ; i < len ; i++)
        if((r = scvm_pop(vm,list+i))) return r;
      if((r = scvm_pop(vm,&y))) return r;
      for(i = 0 ; i < len ; i++)
        if((r = scvm_write_array(vm,addr,x+i,y,list[i]))) return r;
    }
    return 0;
  default:
    return SCVM_ERR_NO_OP;
  }
  return 0;
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

// 0xBC
static int scvm_op_dim(scvm_t* vm, scvm_thread_t* thread) {
  int r,addr;
  unsigned size;
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
  
  switch(op) {
  case 0xC7:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_WORD,size,0);
    break;
  case 0xC8:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_BIT,size,0);
    break;
  case 0xC9:
    addr = scvm_alloc_array(vm,SCVM_ARRAY_NIBBLE,size,0);
    break;
  case 0xCA: // byte
  case 0xCB: // char
    addr = scvm_alloc_array(vm,SCVM_ARRAY_BYTE,size,0);
    break;
  }
  if(addr < 0) return addr;
  return scvm_thread_write_var(vm,thread,vaddr,addr);
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
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 64
  { NULL, NULL },
  { scvm_op_stop_thread, "stop thread" },
  { scvm_op_stop_thread, "stop thread" },
  { NULL, NULL },
  // 68
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_interface, "interface op" },
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
  { scvm_op_start_room, "start room" },
  // 7C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 80
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 84
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 88
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 8C
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 90
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 94
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // 98
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_resource, "resource op" },
  // 9C
  { scvm_op_view, "view op" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // A0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // A4
  { scvm_op_array_write, "array write" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // A8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // AC
  { scvm_op_sound_kludge, "sound kludge" },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // B0
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // B4
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // B8
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  { NULL, NULL },
  // BC
  { scvm_op_dim, "dim" },
  { NULL, NULL },
  { NULL, NULL },
  { scvm_op_start_script_recursive, "start script recursive" },
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
