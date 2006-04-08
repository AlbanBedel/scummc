/* ScummC
 * Copyright (C) 2004-2006  Alban Bedel
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

#define SCC_MAX_ARGS   31
#define SCC_MAX_CLASS  32

// direct integer vals
#define SCC_ST_VAL     0
// ressources (room, actor, verb, etc)
#define SCC_ST_RES     1
// string
#define SCC_ST_STR     2
// function call
#define SCC_ST_CALL    3
// list
#define SCC_ST_LIST    4
// variables
#define SCC_ST_VAR     5
// operation
#define SCC_ST_OP      6
// statement chain
#define SCC_ST_CHAIN   7


#define SCC_VOID        0
#define SCC_VAR_BIT     1
#define SCC_VAR_NIBBLE  2
#define SCC_VAR_BYTE    3
#define SCC_VAR_CHAR    4
#define SCC_VAR_WORD    5

#define SCC_VAR_ARRAY   0x100

typedef struct scc_code_st scc_code_t;
typedef struct scc_func_st scc_func_t;
typedef struct scc_arg_st scc_arg_t;
typedef struct scc_statement_st scc_statement_t;
typedef struct scc_symbol_st scc_symbol_t;
typedef struct scc_scr_arg_st scc_scr_arg_t;
typedef struct scc_script_st scc_script_t;
typedef struct scc_decl_st scc_decl_t;
typedef struct scc_instruct_st scc_instruct_t;
typedef struct scc_loop_st scc_loop_t;
typedef struct scc_call_st scc_call_t;
typedef struct scc_op_st scc_op_t;
typedef struct scc_operator_st scc_operator_t;
typedef struct scc_sym_fix_st scc_sym_fix_t;
typedef struct scc_str_st scc_str_t;
typedef struct scc_verb_script_st scc_verb_script_t;

// func arg type

#define SCC_FA_REF    0xF00
#define SCC_FA_BREF   0x100
#define SCC_FA_WREF   0x200
#define SCC_FA_SREF   0x400

// value, on stack
#define SCC_FA_VAL      0
// list of value, on stack
#define SCC_FA_LIST     1
// string, emmbeded in the code
#define SCC_FA_STR     (2|SCC_FA_SREF)
// array pointers
#define SCC_FA_ARRAY   (3|SCC_FA_WREF)
// offset to the begining of the call
#define SCC_FA_SELF_OFF 4

// op type
#define SCC_OT_ASSIGN  0
#define SCC_OT_UNARY   1
#define SCC_OT_BINARY  2
#define SCC_OT_TERNARY 3

struct scc_code_st {
  scc_code_t* next;

  uint8_t* data;
  int fix,len;
};

struct scc_func_st {
  char* sym;

  uint16_t opcode;

  int ret; // does it return smthg
  int argc; // num args
  int hidden_args; // for wait and perhaps some others
  int argt[SCC_MAX_ARGS]; // args
    
};

struct scc_call_st {
  scc_func_t* func;
  int argc;
  scc_statement_t* argv;
};

struct scc_op_st {
  int type;
  int op;
  int argc;
  scc_statement_t* argv;
};

#define SCC_STR_CHAR 0
// here the values are the one used as code in the string
// don't change them.
#define SCC_STR_INT      4
#define SCC_STR_VERB     5
#define SCC_STR_NAME     6
#define SCC_STR_STR      7
#define SCC_STR_VOICE   10
#define SCC_STR_COLOR   12
#define SCC_STR_FONT    14

struct scc_str_st {
  scc_str_t* next;
  int type;
  scc_symbol_t* sym; // for var
  char* str;
};

struct scc_statement_st {
  scc_statement_t* next;
  int type;
  union {
    // val
    uint16_t i;
    // ressource
    scc_symbol_t* r;
    // string
    scc_str_t* s;
    // call
    scc_call_t c;
    // list / chains
    scc_statement_t* l;
    // variable
    struct {
      scc_symbol_t* r;
      scc_statement_t* x, *y;
    } v;
    // operation
    scc_op_t o;
  } val;
};


#define SCC_INST_ST       0
#define SCC_INST_IF       1
#define SCC_INST_FOR      2
#define SCC_INST_WHILE    3
#define SCC_INST_DO       4
#define SCC_INST_BRANCH   5
#define SCC_INST_SWITCH   6
#define SCC_INST_CASE     7
#define SCC_INST_CUTSCENE 8
#define SCC_INST_OVERRIDE 9


#define SCC_BRANCH_BREAK     0
#define SCC_BRANCH_CONTINUE  1
#define SCC_BRANCH_RETURN    2

#define SCC_FIX_NONE    0
#define SCC_FIX_BRANCH  1
#define SCC_FIX_RETURN  2
#define SCC_FIX_RES     0x100

struct scc_instruct_st {
  scc_instruct_t* next;

  int type,subtype;
  char* sym;

  scc_statement_t* pre;
  scc_statement_t* cond;
  scc_instruct_t* body;
  scc_instruct_t* body2;
  scc_statement_t* post;
};

// global vars
#define SCC_RES_VAR      1
#define SCC_RES_ROOM     2
#define SCC_RES_SCR      3
#define SCC_RES_COST     4
#define SCC_RES_SOUND    5
#define SCC_RES_CHSET    6
#define SCC_RES_LSCR     7
#define SCC_RES_VERB     8
#define SCC_RES_OBJ      9
#define SCC_RES_STATE   10
// local vars
#define SCC_RES_LVAR    11
// bit vars
#define SCC_RES_BVAR    12
// dunno these kind of things might be needed too
#define SCC_RES_PAL     13
#define SCC_RES_CYCL    14
#define SCC_RES_ACTOR   15
#define SCC_RES_BOX     16
#define SCC_RES_CLASS   17
#define SCC_RES_VOICE   18
#define SCC_RES_LAST    19

struct scc_symbol_st {
  scc_symbol_t* next;

  int type; // vars , room, script, cost, sound, charset
  int subtype; // for var type, etc
  char* sym;
  
  int addr; // 0 for auto
  int rid; // ressource id for auto addr

  scc_symbol_t* childs; // for room ressources
  scc_symbol_t* parent;

  char status; // used by the linker
};

struct scc_sym_fix_st {
  scc_sym_fix_t* next;

  uint32_t off;
  scc_symbol_t* sym;
};

#define SCC_SCR_LOCAL  0
#define SCC_SCR_GLOBAL 1

struct scc_scr_arg_st {
  scc_scr_arg_t* next;
  int type;
  char* sym;
};
  

// we'll probably still need such kind of struct
struct scc_script_st {
  scc_script_t* next;

  // the script ressource discribing this script
  scc_symbol_t* sym;

  uint8_t* code;
  uint32_t code_len;  
  scc_sym_fix_t* sym_fix;
};

struct scc_verb_script_st {
  scc_verb_script_t* next;
  scc_symbol_t* sym;
  scc_instruct_t* inst;
};

struct scc_operator_st {
  int scc_op;
  int op;
  int assign_op;
};

#define SCC_OP_PUSH_B            0x00
#define SCC_OP_PUSH              0x01
#define SCC_OP_VAR_READ_B        0x02
#define SCC_OP_VAR_READ          0x03

#define SCC_OP_ARRAY_READ_B      0x06
#define SCC_OP_ARRAY_READ        0x07

#define SCC_OP_ARRAY2_READ_B     0x0A
#define SCC_OP_ARRAY2_READ       0x0B
#define SCC_OP_DUP               0x0C
#define SCC_OP_NOT               0x0D
#define SCC_OP_EQ                0x0E
#define SCC_OP_NEQ               0x0F
#define SCC_OP_G                 0x10
#define SCC_OP_L                 0x11
#define SCC_OP_LE                0x12
#define SCC_OP_GE                0x13
#define SCC_OP_ADD               0x14
#define SCC_OP_SUB               0x15
#define SCC_OP_MUL               0x16
#define SCC_OP_DIV               0x17
#define SCC_OP_LAND              0x18
#define SCC_OP_LOR               0x19

#define SCC_OP_POP               0x1A

#define SCC_OP_VAR_WRITE_B       0x42
#define SCC_OP_VAR_WRITE         0x43

#define SCC_OP_ARRAY_WRITE_B     0x46
#define SCC_OP_ARRAY_WRITE       0x47

#define SCC_OP_ARRAY2_WRITE_B    0x4A
#define SCC_OP_ARRAY2_WRITE      0x4B

#define SCC_OP_INC_VAR_B         0x4E
#define SCC_OP_INC_VAR           0x4F

#define SCC_OP_INC_ARRAY_B       0x52
#define SCC_OP_INC_ARRAY         0x53

#define SCC_OP_DEC_VAR_B         0x56
#define SCC_OP_DEC_VAR           0x57

#define SCC_OP_DEC_ARRAY_B       0x5A
#define SCC_OP_DEC_ARRAY         0x5B

#define SCC_OP_JNZ               0x5C
#define SCC_OP_JZ                0x5D

#define SCC_OP_VERB_RET          0x65
#define SCC_OP_SCR_RET           0x66

#define SCC_OP_CUTSCENE_END      0x67
#define SCC_OP_CUTSCENE_BEGIN    0x68

#define SCC_OP_JMP               0x73

#define SCC_OP_OVERRIDE_BEGIN    0x95
#define SCC_OP_OVERRIDE_END      0x96

#define SCC_OP_ARRAY_WRITE_STR   0xA4CD

#define SCC_OP_ARRAY_WRITE_LIST  0xA4D0
#define SCC_OP_ARRAY2_WRITE_LIST 0xA4D4

#define SCC_OP_BAND              0xD6
#define SCC_OP_BOR               0xD7

