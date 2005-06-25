

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "scc_parse.h"
#include "scc_ns.h"
#include "scc_util.h"
#include "scc_code.h"

#define YYSTYPE int
#include "scc_parse.tab.h"

scc_operator_t scc_bin_op[] = {
  { '+', SCC_OP_ADD, AADD },
  { '-', SCC_OP_SUB, ASUB },
  { '*', SCC_OP_MUL, AMUL },
  { '/', SCC_OP_DIV, ADIV },
  { '&', SCC_OP_BAND, AAND },
  { '|', SCC_OP_BOR,  AOR },
  { LAND, SCC_OP_LAND, 0 },
  { LOR, SCC_OP_LOR ,0 },
  { '<', SCC_OP_L ,0 },
  { LE, SCC_OP_LE ,0 },
  { '>', SCC_OP_G ,0 },
  { GE, SCC_OP_GE ,0 },
  { EQ, SCC_OP_EQ ,0 },
  { NEQ, SCC_OP_NEQ ,0 },
  { 0, 0, 0 }
};

struct scc_loop_st {
  scc_loop_t* next;
  
  int type; 
  int id;   
  char* sym;
};

static scc_loop_t *loop_stack = NULL;

static scc_loop_t* scc_loop_get(char* sym) {
  scc_loop_t* l;
  if(!sym) return loop_stack;
  
  for(l = loop_stack ; l ; l = l->next) {
    if(!l->sym) continue;
    if(!strcmp(l->sym,sym)) return l;
  }
  return NULL;
}

static void scc_loop_push(int type, char* sym) {
  scc_loop_t* l;

  if(sym && scc_loop_get(sym)) {
    printf("Warning there is alredy a loop named %s in the loop stack.\n",
	   sym);
  }

  l = calloc(1,sizeof(scc_loop_t));

  l->id = (loop_stack ? loop_stack->id : 0) + 1;
  l->type = type;
  l->sym = sym;

  l->next = loop_stack;
  loop_stack = l;
}

static scc_loop_t* scc_loop_pop(void) {
  scc_loop_t* l;

  if(!loop_stack) {
    printf("Can't pop empty loop stack.\n");
    return NULL;
  }

  l = loop_stack;
  loop_stack = l->next;
  l->next = NULL;

  return l;
}

static void scc_loop_fix_code(scc_code_t* c,int br, int cont) {
  scc_loop_t* l = scc_loop_pop();
  int pos = 0;

  for( ; c ; c = c->next) {
    pos += c->len;
    if(c->fix != SCC_FIX_BRANCH) continue;
    if(c->data[1] != l->id) continue;
    
    //c->data[0] = 0x73;
    if(c->data[2] == SCC_BRANCH_BREAK)
      SCC_SET_S16LE(c->data,1,br - pos);
    else if(cont >= 0 && c->data[2] == SCC_BRANCH_CONTINUE)
      SCC_SET_S16LE(c->data,1,cont - pos);
    else
      SCC_SET_S16LE(c->data,1,pos);
    c->fix = SCC_FIX_NONE;

    printf("Branch fixed to 0x%x\n",((int16_t*)&c->data[1])[0]);
  }
}

static scc_operator_t* scc_get_bin_op(int op) {
  int i;

  for(i = 0 ; scc_bin_op[i].scc_op ; i++){
    if(scc_bin_op[i].scc_op == op)
      return &scc_bin_op[i];
  }

  return NULL;
}

static scc_operator_t* scc_get_assign_op(int op) {
  int i;

  for(i = 0 ; scc_bin_op[i].scc_op ; i++){
    if(scc_bin_op[i].assign_op == op)
      return &scc_bin_op[i];
  }

  return NULL;
}

static scc_code_t* scc_statement_gen_code(scc_statement_t* st, int ret_val);

scc_code_t* scc_code_new(int len) {
  scc_code_t* c = calloc(1,sizeof(scc_code_t));

  if(len > 0) {
    c->data = malloc(len);
    c->len = len;
  }
  return c;
}

void scc_code_free(scc_code_t* c) {
  if(!c) return;
  if(c->data) free(c->data);
  free(c);
}

void scc_code_free_all(scc_code_t* c) {

  while(c) {
    scc_code_t* n = c->next;
    scc_code_free(c);
    c = n;
  }    

}

static scc_code_t* scc_code_push_val(uint8_t op,uint16_t v) {
  scc_code_t* code;

  if(v <= 0xFF) {
    code = scc_code_new(2);
    code->data[0] = op-1;
    code->data[1] = v;
  } else {
    code = scc_code_new(3);
    code->data[0] = op;
    SCC_SET_16LE(code->data,1,v);
  }

  return code;
}

static scc_code_t* scc_code_push_res(uint8_t op,scc_symbol_t* res) {
  scc_code_t* c,*d;

  // Address is alredy assigned
  if(res->addr >= 0) return scc_code_push_val(op,res->addr);

  if(!res->rid) {
    printf("Ressource %s have no assigned rid !!!!\n",res->sym);
    return NULL;
  }

  // no addr, so code the op and addr separatly
  c = scc_code_new(1);
  c->data[0] = op;
  d = scc_code_new(2);
  SCC_SET_16LE(d->data,0,res->rid);
  d->fix = SCC_FIX_RES + res->type;
  c->next = d;

  return c;
}

static scc_code_t* scc_code_res_addr(int op, scc_symbol_t* res,
				     int ref_type) {
  scc_code_t *c,*o;

  if(res->addr >= 0) {
    if(ref_type != SCC_FA_BREF || res->addr > 0xFF) {
      if(ref_type == SCC_FA_BREF) {
	printf("Ressource address is too big to be coded on 1 byte.\n");
	return NULL;
      }
      c = scc_code_new(2);
      SCC_SET_16LE(c->data,0,res->addr);
    } else {
      c = scc_code_new(1);
      c->data[0] = res->addr;
    }
  } else {
    if(ref_type == SCC_FA_BREF) {
      printf("Ressource with \"dynamic\" address can't be coded on 1 byte.\n");
      return NULL;
    }
    c = scc_code_new(2);
    SCC_SET_16LE(c->data,0,res->rid);
    c->fix = SCC_FIX_RES + res->type;
  }

  if(op < 0) return c;

  if(op > 0xFF) {
    o = scc_code_new(2);
    o->data[0] = op >> 8;
    o->data[1] = op & 0xFF;
  } else {
    o = scc_code_new(1);
    o->data[0] = op;
  }
  o->next = c;
  return o;
}

int scc_code_size(scc_code_t* c) {
  int size = 0;
  while(c) {
    size += c->len;
    c = c->next;
  }
  return size;
}

static scc_code_t* scc_str_gen_code(scc_str_t* s) {
  scc_code_t *code = NULL,*last = NULL,*c;

  while(s) {
    switch(s->type) {
    case SCC_STR_CHAR:
      c = calloc(1,sizeof(scc_code_t));
      c->data = strdup(s->str);
      c->len = strlen(s->str);
      break;
    case SCC_STR_INT:
    case SCC_STR_VERB:
    case SCC_STR_NAME:
    case SCC_STR_STR:
      c = scc_code_res_addr(0xFF00 | (s->type & 0xFF),s->sym,SCC_FA_WREF);
      break;
    case SCC_STR_COLOR:
      c = scc_code_new(4);
      c->data[0] = 0xFF;
      c->data[1] = SCC_STR_COLOR;
      c->data[2] = s->str[0];
      c->data[3] = s->str[1];
      break;
    default:
      printf("Got an unknow string type.\n");
      break;
    }
    SCC_LIST_ADD(code,last,c);
    s = s->next;
  }

  // Append the terminal 0
  c = scc_code_new(1);
  c->data[0] = '\0';
  SCC_LIST_ADD(code,last,c);

  return code;
}

static scc_code_t* scc_statement_gen_ref_code(scc_statement_t* st, int ref_type) {
  scc_code_t* code = NULL;
  uint16_t ptr;

  switch(st->type) {
  case SCC_ARG_AVAR:
    code = scc_code_res_addr(-1,st->val.av.r,ref_type);
    break;
  case SCC_ARG_RES:
    code = scc_code_res_addr(-1,st->val.r,ref_type);
    break;
  case SCC_ARG_VAL:
    ptr = st->val.i;
    if(ref_type == SCC_FA_BREF) {
      if(ptr > 0xFF) printf("Warning pointer value 0x%x can't be encoded as short.\n",ptr);
      code = scc_code_new(1);
      code->data[0] = ptr;
    } else {
      code = scc_code_new(2);
      SCC_SET_16LE(code->data,0,ptr);
    }
    break;
  case SCC_ARG_STR:
    code = scc_str_gen_code(st->val.s);
    break;
  }
  return code;
}


static scc_code_t* scc_call_gen_code(scc_call_t* call, int ret_val) {
  scc_code_t *code = NULL,*last = NULL,*c;
  scc_statement_t* a = NULL;
  int n = 0;

  // Generate arg code
  for(n = 0, a = call->argv ; a ; n++, a = a->next) {
    if(call->func->argt[n] & SCC_FA_REF) continue;
    c = scc_statement_gen_code(a,1);
    SCC_LIST_ADD(code,last,c);
  }
  
  if(call->func->opcode <= 0xFF) {
    c = scc_code_new(1);
    c->data[0] = call->func->opcode;
  } else {
    c = scc_code_new(2);
    c->data[0] = (call->func->opcode & 0xFF00)>>8;
    c->data[1] = call->func->opcode & 0xFF;
  }
  SCC_LIST_ADD(code,last,c);

  // add ref arguments
  for(n = 0, a = call->argv ; a ; n++, a = a->next) {
    if(!(call->func->argt[n] & SCC_FA_REF)) continue;
    c = scc_statement_gen_ref_code(a,call->func->argt[n] & SCC_FA_REF);
    SCC_LIST_ADD(code,last,c);
  }

  for( ; n < call->func->argc+ call->func->hidden_args ; n++) {
    // only hidden arg type atm
    if(call->func->argt[n] != SCC_FA_SELF_OFF) continue;
    c = scc_code_new(2);
    SCC_AT_16(c->data,0) = -scc_code_size(code)-2;
    SCC_LIST_ADD(code,last,c);
  }

  // kick the return val if it's not needed
  if(call->func->ret && (!ret_val)) {
    c = scc_code_new(1);
    c->data[0] = SCC_OP_POP;
    SCC_LIST_ADD(code,last,c);
  }

  return code;
}

static scc_code_t* scc_assign_gen_code(scc_op_t* op, int ret_val) {
  scc_code_t *code = NULL,*last = NULL,*c;
  scc_statement_t *a = op->argv, *b = op->argv->next;
  scc_operator_t* oper;

  switch(a->type) {
  case SCC_ARG_RES:

    if(a->val.r->type != SCC_RES_VAR &&
       a->val.r->type != SCC_RES_LVAR &&
       a->val.r->type != SCC_RES_BVAR) {
      printf("Assignement to a non-variable ressource is invalid !!\n");
      return NULL;
    }

    if(op->op != '=') {
      c = scc_statement_gen_code(a,1);
      SCC_LIST_ADD(code,last,c);
    }
     
    c = scc_statement_gen_code(b,1);
    SCC_LIST_ADD(code,last,c);

    if(op->op != '=') {
      oper = scc_get_assign_op(op->op);

      c = scc_code_new(1);
      c->data[0] = oper->op;
      SCC_LIST_ADD(code,last,c);
    }

    // dup the value to use it as return val
    if(ret_val) {
      c = scc_code_new(1);
      c->data[0] = SCC_OP_DUP;
      SCC_LIST_ADD(code,last,c);
    }

    c = scc_code_push_res(SCC_OP_VAR_WRITE,a->val.r);
    SCC_LIST_ADD(code,last,c);
    break;
  case SCC_ARG_AVAR:
    switch(b->type) {
    case SCC_ARG_LIST:
      
      if(a->val.av.x) {
	c = scc_statement_gen_code(a->val.av.x,1);
	SCC_LIST_ADD(code,last,c);
      }

      c = scc_statement_gen_code(b,1);
      SCC_LIST_ADD(code,last,c);

      c = scc_statement_gen_code(a->val.av.y,1);
      SCC_LIST_ADD(code,last,c);
      
      c = scc_code_res_addr(a->val.av.x ? 0xA4D4 : 0xA4D0,
			    a->val.av.r,SCC_FA_WREF);
      SCC_LIST_ADD(code,last,c);

      // put a dummy return val
      if(ret_val) {
	c = scc_code_push_val(SCC_OP_PUSH,1);
	SCC_LIST_ADD(code,last,c);
      }

      break;
    case SCC_ARG_STR:

      if(a->val.av.x) {
	printf("Strings can't be assigned to 2-dim arrays.\n");
	return NULL;
      }

      c = scc_statement_gen_code(a->val.av.y,1);
      SCC_LIST_ADD(code,last,c);

      c = scc_code_res_addr(0xA4CD,a->val.av.r,SCC_FA_WREF);
      SCC_LIST_ADD(code,last,c);

      c = scc_statement_gen_ref_code(b,SCC_FA_SREF);
      SCC_LIST_ADD(code,last,c);

      // push a dummy return value.
      if(ret_val) {
	c = scc_code_push_val(SCC_OP_PUSH,1);
	SCC_LIST_ADD(code,last,c);
      }
      break;

    default:

      // push the x index
      if(a->val.av.x) {
	c = scc_statement_gen_code(a->val.av.x,1);
	SCC_LIST_ADD(code,last,c);
      }

      // the y
      c = scc_statement_gen_code(a->val.av.y,1);
      SCC_LIST_ADD(code,last,c);

      // If we have an op push a
      if(op->op != '=') {
	c = scc_statement_gen_code(a,1);
	SCC_LIST_ADD(code,last,c);
      }

      // push b
      c = scc_statement_gen_code(b,1);
      SCC_LIST_ADD(code,last,c);

      // put the op
      if(op->op != '=') {
	oper = scc_get_assign_op(op->op);

	c = scc_code_new(1);
	c->data[0] = oper->op;
	SCC_LIST_ADD(code,last,c);
      }
      
      if(a->val.av.x)
	c = scc_code_push_res(SCC_OP_ARRAY2_WRITE,a->val.av.r);
      else
	c = scc_code_push_res(SCC_OP_ARRAY_WRITE,a->val.av.r);
      SCC_LIST_ADD(code,last,c);

      // put a dummy return val
      if(ret_val) {
	c = scc_code_push_val(SCC_OP_PUSH,1);
	SCC_LIST_ADD(code,last,c);
      }
    }
    break;

  default:
    printf("Got unhandled assignement to a %d\n",a->type);
  }
  return code;
}

static scc_code_t* scc_bop_gen_code(scc_op_t* op) {
  scc_code_t *code = NULL,*last = NULL,*c;
  scc_statement_t *a = op->argv, *b = op->argv->next;
  scc_operator_t* oper = scc_get_bin_op(op->op);
  
  if(!oper) {
    printf("Got unhandled binary operator.\n");
    return NULL;
  }

  c = scc_statement_gen_code(a,1);
  SCC_LIST_ADD(code,last,c);
  c = scc_statement_gen_code(b,1);
  SCC_LIST_ADD(code,last,c);
  
  c = scc_code_new(1);
  c->data[0] = oper->op;
  SCC_LIST_ADD(code,last,c);  

  return code;
}

static scc_code_t* scc_uop_gen_code(scc_op_t* op, int ret_val) {
  scc_code_t *code = NULL,*last = NULL,*c;
  int o;

  switch(op->op) {
  case '-':
    if(!ret_val) break;
    c = scc_code_push_val(SCC_OP_PUSH,-1);
    SCC_LIST_ADD(code,last,c);
    c = scc_statement_gen_code(op->argv,1);
    SCC_LIST_ADD(code,last,c);

    c = scc_code_new(1);
    c->data[0] = SCC_OP_MUL;
    SCC_LIST_ADD(code,last,c);
    break;
  case '!':
    if(!ret_val) break;
    c = scc_statement_gen_code(op->argv,1);
    SCC_LIST_ADD(code,last,c);

    c = scc_code_new(1);
    c->data[0] = SCC_OP_NOT;
    SCC_LIST_ADD(code,last,c);
    break;
  case PREINC:
  case PREDEC:
    if(op->argv->type == SCC_ARG_AVAR) {
      o = (op->op == PREINC ? SCC_OP_INC_ARRAY : SCC_OP_DEC_ARRAY);
      // push the index
      c = scc_statement_gen_code(op->argv->val.av.y,1);
      SCC_LIST_ADD(code,last,c);
      // the inc/dec
      c = scc_code_push_res(o,op->argv->val.av.r);
    } else {
      o = (op->op == PREINC ? SCC_OP_INC_VAR : SCC_OP_DEC_VAR);
      c = scc_code_push_res(o,op->argv->val.r);
    }
    SCC_LIST_ADD(code,last,c);
    if(!ret_val) break;
    c = scc_statement_gen_code(op->argv,1);
    SCC_LIST_ADD(code,last,c);
    break;
  case POSTINC:
  case POSTDEC:
    if(ret_val) {
      c = scc_statement_gen_code(op->argv,1);
      SCC_LIST_ADD(code,last,c);
    }
    if(op->argv->type == SCC_ARG_AVAR) {
      o = (op->op == POSTINC ? SCC_OP_INC_ARRAY : SCC_OP_DEC_ARRAY);
      // push the index
      c = scc_statement_gen_code(op->argv->val.av.y,1);
      SCC_LIST_ADD(code,last,c);
      // the inc/dec
      c = scc_code_push_res(o,op->argv->val.av.r);
    } else {
      o = (op->op == POSTINC ? SCC_OP_INC_VAR : SCC_OP_DEC_VAR);
      c = scc_code_push_res(o,op->argv->val.r);
    }
    SCC_LIST_ADD(code,last,c);
    break;
  default:
    printf("Got unhandled unary operator: %c\n",op->op);
    return NULL;
  }

  return code;
}

static scc_code_t* scc_top_gen_code(scc_op_t* op, int ret_val) {
  scc_code_t *code = NULL,*last = NULL,*c;
  scc_code_t *cb,*cc;
  scc_statement_t *a,*x,*y;
  int lb,lc;

  a = op->argv; x = a->next; y = x->next;

  cb = scc_statement_gen_code(x,ret_val);
  lb = scc_code_size(cb);
  cc = scc_statement_gen_code(y,ret_val);
  lc = scc_code_size(cc);

  if(lb + lc == 0) {
    if(ret_val)
      printf("Something went badly wrong.\n");
    return NULL;
  }

  // gen the condition value code
  c = scc_statement_gen_code(a,1);
  SCC_LIST_ADD(code,last,c);

  // if
  c = scc_code_new(3);
  c->data[0] = SCC_OP_JZ;
  SCC_SET_S16LE(c->data,1,scc_code_size(cb) + 3);
  SCC_LIST_ADD(code,last,c);

  SCC_LIST_ADD(code,last,cb);

  c = scc_code_new(3);
  c->data[0] = SCC_OP_JMP;
  SCC_SET_S16LE(c->data,1,scc_code_size(cc));
  SCC_LIST_ADD(code,last,c);
  
  SCC_LIST_ADD(code,last,cc);

  return code;
}

static scc_code_t* scc_op_gen_code(scc_op_t* op, int ret_val) {
  scc_code_t *code = NULL; //,*last = NULL,*c;

  switch(op->type) {
  case SCC_OT_ASSIGN:
    code = scc_assign_gen_code(op,ret_val);
    break;
  case SCC_OT_BINARY:
    if(ret_val) code = scc_bop_gen_code(op);
    break;
  case SCC_OT_UNARY:
    code = scc_uop_gen_code(op,ret_val);
    break;
  case SCC_OT_TERNARY:
    code = scc_top_gen_code(op,ret_val);
    break;
  default:
    printf("Got unhandled op %c (%d)\n",op->op,op->op);
  }

  return code;
}

static scc_code_t* scc_statement_gen_code(scc_statement_t* st, int ret_val) {
  scc_code_t *code = NULL,*last = NULL,*c;
  scc_statement_t* a = NULL;
  int n = 0;

  switch(st->type) {
  case SCC_ARG_VAL:
    if(ret_val) {
      c = scc_code_push_val(SCC_OP_PUSH,st->val.i);
      SCC_LIST_ADD(code,last,c);
    }
    break;
  case SCC_ARG_RES:
    if(ret_val) {
      if(st->val.r->type == SCC_RES_VAR ||
	 st->val.r->type == SCC_RES_BVAR ||
	 st->val.r->type == SCC_RES_LVAR)
	c = scc_code_push_res(SCC_OP_VAR_READ,st->val.r);
      else
	c = scc_code_res_addr(SCC_OP_PUSH,st->val.r,SCC_FA_WREF);
      SCC_LIST_ADD(code,last,c);
    }
    //code = scc_code_push_val(SCC_OP_VAR_READ,scc_var_get_ptr(st->val.v));
    break;
  case SCC_ARG_CALL:
    c = scc_call_gen_code(&st->val.c,ret_val);
    SCC_LIST_ADD(code,last,c);
    break;
  case SCC_ARG_LIST:
    if(!ret_val) break;
    // count the elements
    //for(a = st->val.l ; a ; a = a->next) n++;
    
    // gen their code and count the elements
    for(a = st->val.l ; a ; a = a->next) {
      c = scc_statement_gen_code(a,1);
      SCC_LIST_ADD(code,last,c);
      n++;
    }
    // push the number of element
    c = scc_code_push_val(SCC_OP_PUSH,n);
    SCC_LIST_ADD(code,last,c);
    break;
  case SCC_ARG_AVAR:
    if(!ret_val) break;

    // push x value
    if(st->val.av.x) {
      c = scc_statement_gen_code(st->val.av.x,1);
      SCC_LIST_ADD(code,last,c);
    }
    // push y value
    c = scc_statement_gen_code(st->val.av.y,1);
    SCC_LIST_ADD(code,last,c);
    // op code
    c = scc_code_push_res(SCC_OP_ARRAY_READ,st->val.av.r);
    SCC_LIST_ADD(code,last,c);
    break;
  case SCC_ARG_OP:
    c = scc_op_gen_code(&st->val.o,ret_val);
    SCC_LIST_ADD(code,last,c);
    break;
  case SCC_ARG_CHAIN:
    // gen the code for each element
    // only the last one one should potentialy return a value
    for(a = st->val.l ; a ; a = a->next) {
      c = scc_statement_gen_code(a,a->next ? 0 : ret_val);
      SCC_LIST_ADD(code,last,c);
    }
    break;
  default:
    printf("Got unhandled statement type: %d\n",st->type);
  }

  return code;

}

static scc_code_t* scc_instruct_gen_code(scc_instruct_t* inst);
static scc_code_t* scc_branch_gen_code(scc_instruct_t* inst);

static scc_code_t* scc_if_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;
  scc_code_t *a = NULL;
  int len = 0;

  // gen the condition value code
  c = scc_statement_gen_code(inst->cond,1);
  SCC_LIST_ADD(code,last,c);

  // Optimize out the branch instructions, they all generate a jump
  if(inst->body->type == SCC_INST_BRANCH) {
    // get the code
    c = scc_branch_gen_code(inst->body);
    // set our condition instead of the unconditional jmp
    c->data[0] = inst->subtype ? SCC_OP_JZ : SCC_OP_JNZ;
    SCC_LIST_ADD(code,last,c);
  } else {  
    // gen the if body code so we can know how big it is
    a = scc_instruct_gen_code(inst->body);
    len = scc_code_size(a);
    // if we have an else block we need to add a jump at the
    // end of the first body
    if(inst->body2) len += 3;

    // if
    c = scc_code_new(3);
    c->data[0] = inst->subtype ? SCC_OP_JNZ : SCC_OP_JZ;

    SCC_SET_S16LE(c->data,1,len);
    SCC_LIST_ADD(code,last,c);
    // body 1
    SCC_LIST_ADD(code,last,a);
  }

  // else
  if(inst->body2) {

 
    // we need the size of the else block to jump above it
    // at the end of the if block
    a = scc_instruct_gen_code(inst->body2);

    // If we optimized a branch inst we don't have a first body
    // so we don't need a jump after it
    if(len) {
      // endif :)
      c = scc_code_new(3);
      c->data[0] = SCC_OP_JMP;
      SCC_SET_S16LE(c->data,1,scc_code_size(a));
      SCC_LIST_ADD(code,last,c);
    }

    // body 2
    SCC_LIST_ADD(code,last,a);
  }
  return code;
}

static scc_code_t* scc_for_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;
  scc_code_t *loop,*body,*post;
  int cont;

  // push the loop context
  scc_loop_push(inst->type,inst->sym);

  body = scc_instruct_gen_code(inst->body);
  post = scc_statement_gen_code(inst->post,0);

  // pre
  c = scc_statement_gen_code(inst->pre,0);
  SCC_LIST_ADD(code,last,c);

  // cond
  cont = scc_code_size(c);
  loop = c = scc_statement_gen_code(inst->cond,1);
  SCC_LIST_ADD(code,last,c);

  c = scc_code_new(3);
  c->data[0] = SCC_OP_JZ;
  SCC_SET_S16LE(c->data,1,scc_code_size(body) + scc_code_size(post) + 3);
  SCC_LIST_ADD(code,last,c);

  // body
  SCC_LIST_ADD(code,last,body);
  // post
  SCC_LIST_ADD(code,last,post);

  c = scc_code_new(3);
  c->data[0] = SCC_OP_JMP;
  SCC_LIST_ADD(code,last,c);

  SCC_SET_S16LE(c->data,1, - scc_code_size(loop));

  //  br = scc_code_size(code);

  scc_loop_fix_code(code,scc_code_size(code),cont);

  return code;
}

static scc_code_t* scc_while_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;
  scc_code_t *body;

  // push the loop context
  scc_loop_push(inst->type,inst->sym);

  body = scc_instruct_gen_code(inst->body);

  // cond
  c = scc_statement_gen_code(inst->cond,1);
  SCC_LIST_ADD(code,last,c);

  c = scc_code_new(3);
  c->data[0] = inst->subtype ? SCC_OP_JNZ : SCC_OP_JZ;
  SCC_SET_S16LE(c->data,1, scc_code_size(body) + 3);
  SCC_LIST_ADD(code,last,c);

  // body
  SCC_LIST_ADD(code,last,body);

  c = scc_code_new(3);
  c->data[0] = SCC_OP_JMP;
  SCC_LIST_ADD(code,last,c);

  SCC_SET_S16LE(c->data,1, - scc_code_size(code));

  scc_loop_fix_code(code,scc_code_size(code),0);

  return code;
}


static scc_code_t* scc_do_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;
  int cont;
  // push the loop context
  scc_loop_push(inst->type,inst->sym);

  // body
  c = scc_instruct_gen_code(inst->body);
  SCC_LIST_ADD(code,last,c);
  cont = scc_code_size(c);

  // cond
  c = scc_statement_gen_code(inst->cond,1);
  SCC_LIST_ADD(code,last,c);

  c = scc_code_new(3);
  c->data[0] = inst->subtype ? SCC_OP_JZ : SCC_OP_JNZ;
  SCC_LIST_ADD(code,last,c);

  SCC_SET_S16LE(c->data,1, -scc_code_size(code));

  scc_loop_fix_code(code,scc_code_size(code),cont);

  return code;
}

static scc_code_t* scc_branch_gen_code(scc_instruct_t* inst) {
  scc_code_t* c;
  scc_loop_t* l;

  if(!loop_stack) {
    printf("Branching instruction can't be used outside of loops.\n");
    return NULL;
  }

  l = scc_loop_get(inst->sym);
  if(!l) {
    printf("No loop named %s was found in the loop stack.\n",
	   inst->sym);
    return NULL;
  }

  if(l->type == SCC_INST_SWITCH && inst->subtype == SCC_BRANCH_CONTINUE) {
    printf("Continue is not allowed in swith blocks.\n");
    return NULL;
  }

  c = scc_code_new(3);
  c->fix = SCC_FIX_BRANCH;
  c->data[0] = SCC_OP_JMP;
  c->data[1] = l->id;
  c->data[2] = inst->subtype;
  
  
  return c;
}

static scc_code_t* scc_switch_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;
  scc_code_t *cond_code,*body_code;
  scc_instruct_t *i = inst->body, *i0 = NULL;
  scc_statement_t* cond = i->cond;
  int add_jmp = 0;

  // gen the switched value code, if we have some conditions
  if(cond) {
    c = scc_statement_gen_code(inst->cond,1);
    SCC_LIST_ADD(code,last,c);
  }

  // push the loop context
  scc_loop_push(inst->type,inst->sym);

  // generate the first condition code
  if(cond)
    cond_code = scc_statement_gen_code(cond,1);

  while(cond) {

    // dup the switched value
    c = scc_code_new(1);
    c->data[0] = SCC_OP_DUP;
    SCC_LIST_ADD(code,last,c);
    // put the condition
    SCC_LIST_ADD(code,last,cond_code);
    // if
    c = scc_code_new(4);
    c->data[0] = SCC_OP_NEQ;
    c->data[1] = SCC_OP_JNZ;

    // there's a next condition, so we'll add a jump
    // instead of a real body
    if(cond->next) {
      add_jmp = 1;
      SCC_SET_S16LE(c->data,2,1 /*kill*/ + 3 /*jmp*/);

    } else { // that's the last condition so put the body
#if 1
      // look if the last instruction is a break, if so spare the
      // useless jump.
      // here and under we could probably stop at the first break found
      for(i0 = i->body ; i0 && i0->next ; i0 = i0->next);
      if(i0 && i0->type == SCC_INST_BRANCH && 
	 i0->subtype == SCC_BRANCH_BREAK)
	add_jmp = 0;
      else
#endif
	add_jmp = 1;
      
      body_code = scc_instruct_gen_code(i->body);
      
      SCC_SET_S16LE(c->data,2,scc_code_size(body_code) + 1 + (add_jmp ? 3 : 0));
    }
    SCC_LIST_ADD(code,last,c);

    // kill the switched value before entering the body
    c = scc_code_new(1);
    c->data[0] = SCC_OP_POP;
    SCC_LIST_ADD(code,last,c);

    if(!cond->next) {
      // no next condition, add the body
      SCC_LIST_ADD(code,last,body_code);

      // find the next condition
      i = i->next;
      if(i)
	cond = i->cond;
      else
	cond = NULL;
    } else
      cond = cond->next;

    // generate the code for the next condition so we can get its size
    if(cond) cond_code = scc_statement_gen_code(cond,1);

    // if needed add the jump to the next block
    if(add_jmp) {
      c = scc_code_new(3);
      c->data[0] = SCC_OP_JMP;
      SCC_SET_S16LE(c->data,1,1 + (cond ? scc_code_size(cond_code) + 4 + 1 : 0));
      SCC_LIST_ADD(code,last,c);
    }
    if(!cond && i) break;
  }

  // the "final" kill is not needed if we had no condition at all
  if(inst->body->cond) {
    c = scc_code_new(1);
    c->data[0] = SCC_OP_POP;
    SCC_LIST_ADD(code,last,c);
  }

  // default
  if(i) {
    scc_instruct_t* i1 = NULL;
    for(i0 = i->body ; i0 && i0->next ; i1 = i0, i0 = i0->next);
    if(i0 && i0->type == SCC_INST_BRANCH &&
       i0->subtype == SCC_BRANCH_BREAK) {
      if(i1)
	i1->next = NULL;
      else
	i->body = NULL;
      free(i0); // check me
    }
    
    if(i->body) {
      c = scc_instruct_gen_code(i->body);
      SCC_LIST_ADD(code,last,c);
    }
  }
  
  scc_loop_fix_code(code,scc_code_size(code),-1);


  return code;
}

static scc_code_t* scc_cutscene_gen_code(scc_instruct_t* inst) {
    scc_code_t *code=NULL,*last=NULL,*c;
    scc_statement_t* st;
    int n;

    // generate the argument list
    for(st = inst->cond, n=0 ; st ; st = st->next) {
        c = scc_statement_gen_code(st,1);
        SCC_LIST_ADD(code,last,c);
        n++;
    }
    // push the number of element
    c = scc_code_push_val(SCC_OP_PUSH,n);
    SCC_LIST_ADD(code,last,c);
    // put the cutscene begin op code
    c = scc_code_new(1);
    c->data[0] = SCC_OP_CUTSCENE_BEGIN;
    SCC_LIST_ADD(code,last,c);
    // add the body code
    c = scc_instruct_gen_code(inst->body);
    SCC_LIST_ADD(code,last,c);
    // put the cutscene end op code
    c = scc_code_new(1);
    c->data[0] = SCC_OP_CUTSCENE_END;

    return code;
}

static scc_code_t* scc_instruct_gen_code(scc_instruct_t* inst) {
  scc_code_t *code=NULL,*last=NULL,*c;

  for( ; inst ; inst = inst->next ) {
    switch(inst->type) {
    case SCC_INST_ST:
      c = scc_statement_gen_code(inst->pre,0);
      break;
    case SCC_INST_IF:
      c = scc_if_gen_code(inst);
      break;
    case SCC_INST_FOR:
      c = scc_for_gen_code(inst);
      break;
    case SCC_INST_WHILE:
      c = scc_while_gen_code(inst);
      break;
    case SCC_INST_DO:
      c = scc_do_gen_code(inst);
      break;
    case SCC_INST_BRANCH:
      c = scc_branch_gen_code(inst);
      break;
    case SCC_INST_SWITCH:
      c = scc_switch_gen_code(inst);
      break;
    case SCC_INST_CUTSCENE:
      c = scc_cutscene_gen_code(inst);
      break;
    default:
      printf("Unsupported instruction type: %d\n",inst->type);
      c = NULL;
    }
    SCC_LIST_ADD(code,last,c);
  }

  return code;
}

scc_script_t* scc_script_new(scc_ns_t* ns, scc_instruct_t* inst) {
  scc_code_t* code = scc_instruct_gen_code(inst);
  scc_sym_fix_t* rf = NULL, *rf_last = NULL, *r;
  scc_symbol_t* sym;
  int p,l;
  uint8_t* data;
  scc_script_t* scr;
  
  if(!code) {
    printf("Code generation failed.\n");
    return NULL;
  }

  l = scc_code_size(code) + 1;
  data = malloc(l);

  for(p = 0 ; code ; p+= code->len, code = code->next) {
    memcpy(&data[p],code->data,code->len);
    if(code->fix >= SCC_FIX_RES) {
      uint16_t rid = SCC_AT_16LE(data,p);
      sym = scc_ns_get_sym_with_id(ns,code->fix - SCC_FIX_RES,rid);
      if(!sym) {
	printf("Unable to find ressource %d of type %d\n",
	       rid,code->fix - SCC_FIX_RES);
	continue;
      }
      r = calloc(1,sizeof(scc_sym_fix_t));
      r->off = p;
      r->sym = sym;
      SCC_LIST_ADD(rf,rf_last,r);
    }
  }
  // Add return. The original game seems to use different op
  // for object script and script, but scumvm just use the same
  // code for both, so why bother.
  data[l-1] = 0x65;

  scr = calloc(1,sizeof(scc_script_t));
  scr->code = data;
  scr->code_len = l;
  scr->sym_fix = rf;

  return scr;
}

void scc_script_free(scc_script_t* scr) {

  if(scr->code) free(scr->code);
  free(scr);
}

void scc_script_list_free(scc_script_t* scr) {
  scc_script_t* n;

  while(scr) {
    n = scr->next;
    scc_script_free(scr);
    scr = n;
  }

}
