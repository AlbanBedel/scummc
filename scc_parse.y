
%{

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"
#include "scc_util.h"

#include "scc_parse.h"
#include "scc_ns.h"
#include "scc_img.h"
#include "scc.h"
#include "scc_roobj.h"
#include "scc_code.h"
#include "scc_param.h"

#define YYERROR_VERBOSE 1

  int yylex(void);
  int yyerror (const char *s);

#include "scc_func.h"

#define SCC_LIST_ADD(list,last,c) if(c){                  \
  if(last) last->next = c;                                \
  else list = c;                                          \
  for(last = c ; last && last->next ; last = last->next); \
}

#define SCC_BOP(d,bop,a,cop,b) {                              \
  if(a->type == SCC_ST_VAL &&                                 \
     b->type == SCC_ST_VAL) {                                 \
    a->val.i = ((int16_t)a->val.i) bop ((int16_t)b->val.i);   \
    free(b);                                                  \
    d = a;                                                    \
  } else {                                                    \
    d = calloc(1,sizeof(scc_statement_t));                    \
    d->type = SCC_ST_OP;                                      \
    d->val.o.type = SCC_OT_BINARY;                            \
    d->val.o.op = cop;                                        \
    d->val.o.argc = 2;                                        \
    d->val.o.argv = a;                                        \
    a->next = b;                                              \
  }                                                           \
}

#define SCC_ABORT(at,msg, args...)  { \
  printf("Line %d, column %d: ",at.first_line,at.first_column); \
  printf(msg, ## args); \
  YYABORT; \
}

  // output filename
  static char* scc_output = NULL;
  // output fd
  static scc_fd_t* out_fd = NULL;

  static scc_roobj_t* scc_roobj_list = NULL;

  // the global namespace
  // not static bcs of code gen, we'll see how to fix it
  static scc_ns_t* scc_ns = NULL;
  // current room
  static scc_roobj_t* scc_roobj = NULL;
  // current object
  static scc_roobj_obj_t* scc_obj = NULL;

  static int local_vars = 0;
  static int local_scr = 0;

  scc_func_t* scc_get_func(char* sym) {
    int i;

    if(!sym) return NULL;

    for(i = 0 ; scc_func[i].sym ; i++) {
      if(strcmp(sym,scc_func[i].sym)) continue;
      return &scc_func[i];
    }

    return NULL;
  }

  char* scc_statement_check_func(scc_call_t* c) {
    int n;
    scc_statement_t* a;
    // should be big enouth
    static char func_err[2048];

    if(c->argc != c->func->argc) {
      sprintf(func_err,"Function %s need %d args, %d found.\n",
	      c->func->sym,c->func->argc,c->argc);
      return func_err;
    }
      
    for(n = 0, a = c->argv ; a ; n++, a = a->next) {
      if(c->func->argt[n] == SCC_FA_VAL) {
	if(a->type == SCC_ST_STR ||
	   a->type == SCC_ST_LIST) {
	  sprintf(func_err,"Argument %d of call to %s is of a wrong type.\n",
		  n+1,c->func->sym);
	  return func_err;
	}
      } else if(c->func->argt[n] == SCC_FA_PTR ||
		c->func->argt[n] == SCC_FA_SPTR) {
	if(a->type != SCC_ST_RES) {
	  sprintf(func_err,"Argument %d of call to %s must be a variable.\n",
		  n+1,c->func->sym);
	  return func_err;
	}
      } else if(c->func->argt[n] == SCC_FA_APTR ||
		c->func->argt[n] == SCC_FA_SAPTR) {
	if(a->type != SCC_ST_RES ||
	   a->val.r->type != SCC_VAR_ARRAY) {
	  sprintf(func_err,"Argument %d of call to %s must be an array.\n",
		  n+1,c->func->sym);
	  return func_err;
	}

      } else if(c->func->argt[n] == SCC_FA_LIST &&
		a->type != SCC_ST_LIST) {
	sprintf(func_err,"Argument %d of %s must be a list.\n",
		n+1,c->func->sym);
	return func_err;
      } else if(c->func->argt[n] == SCC_FA_STR &&
		a->type != SCC_ST_STR) {
	sprintf(func_err,"Argument %d of %s must be a string.\n",
		n+1,c->func->sym);
	return func_err;
      }
	
    }

    return NULL;
  }

%}

%union {
  int integer;
  char* str;
  char** strlist;
  scc_symbol_t* sym;
  scc_statement_t* st;
  scc_instruct_t* inst;
  scc_scr_arg_t* arg;
  scc_script_t* scr;
  scc_str_t* strvar;
}

// Operators, the order is defining the priority.

%token AADD
%token ASUB
%token AMUL
%token ADIV
%token AAND
%token AOR
%token INC
%token DEC
%token PREINC
%token POSTINC
%token PREDEC
%token POSTDEC

%right <integer> ASSIGN
%right <integer> '?' ':'
%left <integer>  LOR
%left <integer>  LAND
%left <integer>  '|'
%left <integer>  '&'
%left <integer>  NEQ EQ
%left <integer>  '>' GE
%left <integer>  '<' LE

%left <integer>  '-' '+'
%left <integer> '*' '/'
%right <integer> NEG
%right <integer> '!'
%nonassoc <integer> SUFFIX
%right PREFIX
%left POSTFIX

%token <integer> INTEGER
%token <integer> RESTYPE
%token <str> STRING
%token <strvar> STRVAR
%token <str> SYM
%token <integer> TYPE
%token <integer> NUL

%token <integer> BRANCH

%token ROOM
%token OBJECT
%token NS
%token SCRIPT
%token VERB
%token ACTOR

%nonassoc <integer> IF
%nonassoc ELSE
%nonassoc FOR
%nonassoc <integer> WHILE
%nonassoc DO
%nonassoc SWITCH
%nonassoc CASE
%nonassoc DEFAULT
%nonassoc CUTSCENE
%nonassoc CLASS
%nonassoc TRY
%nonassoc OVERRIDE

%token <integer> SCRTYPE

%token ERROR

%type <strvar> str
%type <strvar> string
%type <integer> location
%type <st> dval

%type <st> cargs
%type <st> var
%type <st> call

%type <sym> vdecl

%type <st> statement
%type <st> statements
%type <inst> instruct
%type <inst> ifblock
%type <st> caseval
%type <st> caseblock
%type <inst> switchblock
%type <inst> loopblock
%type <inst> block
%type <inst> instructions
%type <inst> body
%type <scr> scriptbody

%type <integer> gvardecl
%type <integer> gresdecl
%type <integer> resdecl
%type <integer> scripttype

%type <arg> scriptargs
%type <sym> roomscrdecl
%type <sym> verbentrydecl
%type <strlist> zbufs

%defines
%locations

%%

srcfile: /* empty */
| rooms
| globdecl rooms
;

globdecl: gdecl ';'
| globdecl gdecl ';'
;

gdecl: gvardecl
{
}
| groomdecl
| actordecl
| verbdecl
| classdecl
| objdecl
| scrdecl
| gresdecl
{
}
;

gvardecl: TYPE SYM location
{
  if($1 == SCC_VAR_BIT)
    scc_ns_decl(scc_ns,NULL,$2,SCC_RES_BVAR,0,$3);
  else
    scc_ns_decl(scc_ns,NULL,$2,SCC_RES_VAR,$1,$3);

  $$ = $1;
}
| gvardecl ',' SYM location
{
  if($1 == SCC_VAR_BIT)
    scc_ns_decl(scc_ns,NULL,$3,SCC_RES_BVAR,0,$4);
  else
    scc_ns_decl(scc_ns,NULL,$3,SCC_RES_VAR,$1,$4);

  $$ = $1;
}
;

groomdecl: ROOM SYM location
{
  scc_ns_decl(scc_ns,NULL,$2,SCC_RES_ROOM,0,$3);
}
| groomdecl ',' SYM location
{
  scc_ns_decl(scc_ns,NULL,$3,SCC_RES_ROOM,0,$4);
}
;

actordecl: ACTOR SYM location
{
  scc_ns_decl(scc_ns,NULL,$2,SCC_RES_ACTOR,0,$3);
}
| actordecl ',' SYM location
{
  scc_ns_decl(scc_ns,NULL,$3,SCC_RES_ACTOR,0,$4);
}
;

verbdecl: VERB SYM location
{
  scc_ns_decl(scc_ns,NULL,$2,SCC_RES_VERB,0,$3);
}
| verbdecl ',' SYM location
{
  scc_ns_decl(scc_ns,NULL,$3,SCC_RES_VERB,0,$4);
}
;

classdecl: CLASS SYM location
{
  scc_ns_decl(scc_ns,NULL,$2,SCC_RES_CLASS,0,$3);
}
| classdecl ',' SYM location
{
  scc_ns_decl(scc_ns,NULL,$3,SCC_RES_CLASS,0,$4);
}
; 

objdecl: OBJECT SYM NS SYM location
{
  scc_ns_decl(scc_ns,$2,$4,SCC_RES_OBJ,0,$5);
}
| objdecl ',' SYM NS SYM location
{
  scc_ns_decl(scc_ns,$3,$5,SCC_RES_OBJ,0,$6);
}
;

scrdecl: SCRIPT SYM NS SYM location
{
  scc_ns_decl(scc_ns,$2,$4,SCC_RES_SCR,0,$5);
}
| scrdecl ',' SYM NS SYM location
{
  scc_ns_decl(scc_ns,$3,$5,SCC_RES_SCR,0,$6);
}
;

// cost, sound, chset
gresdecl: RESTYPE SYM NS SYM location
{
  scc_ns_decl(scc_ns,$2,$4,$1,0,$5);
  $$ = $1; // hack to propagte the type
}
| gresdecl ',' SYM NS SYM location
{
  scc_ns_decl(scc_ns,$3,$5,$1,0,$6);
}
;

// This allow us to have the room declared before we parse the body.
// At this point the roobj object is created.
roombdecl: ROOM SYM location
{
  scc_symbol_t* sym = scc_ns_decl(scc_ns,NULL,$2,SCC_RES_ROOM,0,$3);
  scc_ns_get_rid(scc_ns,sym);
  scc_ns_push(scc_ns,sym);
  scc_roobj = scc_roobj_new(sym);
}
;

rooms: room
| rooms room
;

// Here we should have the whole room.
room: roombdecl roombody
{
  printf("Room done :)\n");
  scc_ns_pop(scc_ns);

  scc_roobj->next = scc_roobj_list;
  scc_roobj_list = scc_roobj;
}
;

roombody: '{' '}'
| '{' roombodyentries '}'
//| '{' roomdecls '}'
| '{' roomdecls roombodyentries '}'
;

roombodyentries: roombodyentry
| roombodyentries roombodyentry
;

// scripts, both local and global
roombodyentry: roomscrdecl '{' scriptbody '}'
{
  if($3) {
    $3->sym = $1;
    scc_roobj_add_scr(scc_roobj,$3);
  }

  scc_ns_pop(scc_ns);
}
// forward declaration
| roomscrdecl ';'
{
  // well :)
  scc_ns_pop(scc_ns);
}
// objects
| roomobjdecl '{' objectparams objectverbs '}'
{
  // add the obj to the room
  scc_roobj_add_obj(scc_roobj,scc_obj);
  scc_obj = NULL;
}
| roomobjdecl '{' objectparams '}'
{
  // add the obj to the room
  scc_roobj_add_obj(scc_roobj,scc_obj);
  scc_obj = NULL;
}
// forward declaration
| roomobjdecl ';'
{
  // TODO
  scc_obj = NULL;
}
;

roomscrdecl: scripttype SCRIPT SYM  '(' scriptargs ')' location
{
  scc_scr_arg_t* a = $5;
  scc_symbol_t* s;
  int took_local = 0;

  if($1 == SCC_RES_LSCR) {
    if($7 >= 0) printf("Ignoring location for local scripts.\n");
    if(strcmp($3,"entry") &&
       strcmp($3,"exit") ) {
      s = scc_ns_get_sym(scc_ns,NULL,$3);
      if(!s) {
	$7 = local_scr;
	took_local = 1;
      } else
	$7 = -1;
    } else
      $7 = -1;
  }

  s = scc_ns_decl(scc_ns,NULL,$3,$1,0,$7);
  if(!s) SCC_ABORT(@3,"Failed to declare script %s.\n",$3);

  if(took_local)
    local_scr++;

  if(!s->rid) scc_ns_get_rid(scc_ns,s);

  scc_ns_push(scc_ns,s);

  // declare the arguments
  local_vars = 0;
  while(a) {
    scc_ns_decl(scc_ns,NULL,a->sym,SCC_RES_LVAR,a->type,local_vars);
    local_vars++;
    a = a->next;
  }
  $$ = s;
}
;

roomobjdecl: OBJECT SYM location
{
  scc_symbol_t* sym;

  if(scc_obj)
    SCC_ABORT(@1,"Something went wrong with object declarations.\n");


  sym = scc_ns_decl(scc_ns,NULL,$2,SCC_RES_OBJ,0,$3);
  if(!sym) SCC_ABORT(@1,"Failed to declare object %s.\n",$2);
  
  scc_obj = scc_roobj_obj_new(sym);
  //  scc_ns_push(scc_ns,sym);
  if(!sym->rid) scc_ns_get_rid(scc_ns,sym);

}
;

// costume, sounds, and other external ressources
// can be defined along with the room parameters
// such as image, etc
roomdecls: roomdecl
| roomdecls roomdecl
;

roomdecl: resdecl ';'
{
}
| SYM ASSIGN STRING ';'
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_set_param(scc_roobj,scc_ns,$1,-1,$3))
    SCC_ABORT(@1,"Failed to set room parameter.\n");

}
| SYM '[' INTEGER ']' ASSIGN STRING ';'
{
  if($5 != '=')
    SCC_ABORT(@5,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_set_param(scc_roobj,scc_ns,$1,$3,$6))
    SCC_ABORT(@1,"Failed to set room parameter.\n");
}
;

// ressource declaration, possibly with harcoded
// location
resdecl: RESTYPE SYM location ASSIGN STRING
{
  scc_symbol_t* r;

  if($4 != '=')
    SCC_ABORT(@4,"Invalid operator for ressource declaration.\n");

  r = scc_ns_decl(scc_ns,NULL,$2,$1,0,$3);
  // then we need to add that to the roobj
  scc_roobj_add_res(scc_roobj,r,$5);
  
  if(!r->rid) scc_ns_get_rid(scc_ns,r);
  $$ = $1; // propagate type
}
| resdecl ',' SYM location ASSIGN STRING
{
  scc_symbol_t* r;

  if($5 != '=')
    SCC_ABORT(@5,"Invalid operator for ressource declaration.\n");

  r = scc_ns_decl(scc_ns,NULL,$3,$1,0,$4);
  // then we need to add that to the roobj
  scc_roobj_add_res(scc_roobj,r,$6);

  if(!r->rid) scc_ns_get_rid(scc_ns,r);
  $$ = $1; // propagate type
}
;

// params chain, we can't use the same as the room
// bcs we don't want ressource declaration here
objectparams: objectparam ';'
| objectparams objectparam ';'
;

// used by objects
objectparam: SYM ASSIGN STRING
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_obj_set_param(scc_obj,$1,$3))
    SCC_ABORT(@1,"Failed to set object parameter.\n");
}
| SYM ASSIGN INTEGER
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_obj_set_int_param(scc_obj,$1,$3))
    SCC_ABORT(@1,"Failed to set object parameter.\n");

}
| SYM ASSIGN '-' INTEGER
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_obj_set_int_param(scc_obj,$1,-$4))
    SCC_ABORT(@1,"Failed to set object parameter.\n");

}
| SYM ASSIGN '{' imgdecls '}'
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");
  // bitch on the keyword
  if(strcmp($1,"states"))
    SCC_ABORT(@1,"Expexted \"images\".\n");
}
| SYM ASSIGN SYM
{
  scc_symbol_t* sym;

  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");
  // bitch on the keyword
  if(strcmp($1,"owner"))
    SCC_ABORT(@1,"Expexted \"owner\".\n");

  sym = scc_ns_get_sym(scc_ns,NULL,$3);
  if(!sym)
    SCC_ABORT(@3,"%s is not a declared actor.\n",$3);
  if(sym->type != SCC_RES_ACTOR)
    SCC_ABORT(@3,"%s is not an actor.\n",sym->sym);
  
  scc_obj->owner = sym;
}
| CLASS ASSIGN '{' classlist '}'
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");
}
;

classlist: SYM
{
  scc_symbol_t* sym = scc_ns_get_sym(scc_ns,NULL,$1);

  if(!sym)
    SCC_ABORT(@1,"%s is not a declared class.\n",$1);

  if(sym->type != SCC_RES_CLASS)
    SCC_ABORT(@1,"%s is not a class in the current context.\n",$1);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(scc_ns,sym);

  if(!scc_roobj_obj_set_class(scc_obj,sym))
    SCC_ABORT(@1,"Failed to set object class.\n");

}
| classlist ',' SYM
{
  scc_symbol_t* sym = scc_ns_get_sym(scc_ns,NULL,$3);

  if(!sym)
    SCC_ABORT(@3,"%s is not a declared class.\n",$3);

  if(sym->type != SCC_RES_CLASS)
    SCC_ABORT(@3,"%s is not a class in the current context.\n",$3);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(scc_ns,sym);

  if(!scc_roobj_obj_set_class(scc_obj,sym))
    SCC_ABORT(@3,"Failed to set object class.\n");
}
;

imgdecls: imgdecl
| imgdecls ',' imgdecl
;

imgdecl: '{' INTEGER ',' INTEGER ',' STRING '}'
{
  if(!scc_roobj_obj_add_state(scc_obj,$2,$4,$6,NULL))
    SCC_ABORT(@1,"Failed to add room state\n");
}
| '{' INTEGER ',' INTEGER ',' STRING ',' '{' zbufs '}' '}'
{
  if(!scc_roobj_obj_add_state(scc_obj,$2,$4,$6,$9))
    SCC_ABORT(@2,"Failed to add room state.\n");
}
;

zbufs: STRING
{
  $$ = malloc(2*sizeof(char*));
  $$[0] = $1;
  $$[1] = NULL;
}
| zbufs ',' STRING
{
  int l;
  for(l = 0 ; $1[l] ; l++);
  $$ = realloc($1,(l+2)*sizeof(char*));
  $$[l] = $3;
  $$[l+1] = NULL;
}
;


objectverbs: verbentry
| objectverbs verbentry;

// a verb entry. SYM should be a declared verb name
verbentry: verbentrydecl '{' scriptbody '}'
{
  if($3) {
    $3->sym = $1;
    if(!scc_roobj_obj_add_verb(scc_obj,$3))
      SCC_ABORT(@1,"Failed to add verb script.\n");
  }

  scc_ns_pop(scc_ns);
}
;

verbentrydecl: SYM '(' scriptargs ')'
{
  scc_scr_arg_t* a = $3;
  scc_symbol_t* sym = scc_ns_get_sym(scc_ns,NULL,$1);
  if(!sym)
    SCC_ABORT(@1,"%s is not a declared verb.\n",$1);
  
  if(sym->type != SCC_RES_VERB)
    SCC_ABORT(@1,"%s is not a verb in the current context.\n",$1);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(scc_ns,sym);
  // push the obj for the local vars
  scc_ns_push(scc_ns,scc_obj->sym);

  // declare the arguments
  local_vars = 0;
  while(a) {
    scc_ns_decl(scc_ns,NULL,a->sym,SCC_RES_LVAR,a->type,local_vars);
    local_vars++;
    a = a->next;
  }
  
  $$ = sym;
}
| DEFAULT '(' scriptargs ')'
{
  scc_symbol_t* sym = scc_ns_get_sym(scc_ns,NULL,"default");
  scc_scr_arg_t* a = $3;

  if(!sym) {
    // we need this little hack to declare it at the toplevel of the
    // namespace.
    scc_symbol_t* tmp = scc_ns->cur;
    scc_ns->cur = NULL;
    sym = scc_ns_decl(scc_ns,NULL,"default",SCC_RES_VERB,0,0xFF);
    scc_ns->cur = tmp;
    scc_ns_get_rid(scc_ns,sym);
  } else if(sym->type != SCC_RES_VERB) // shouldn't happend
    SCC_ABORT(@1,"default is already defined as something else than a verb.\n");

  // push the obj for the local vars
  scc_ns_push(scc_ns,scc_obj->sym);

  // declare the arguments
  local_vars = 0;
  while(a) {
    scc_ns_decl(scc_ns,NULL,a->sym,SCC_RES_LVAR,a->type,local_vars);
    local_vars++;
    a = a->next;
  }
  
  $$ = sym;
}
;

// script can be local or global. Dunno yet what will be default,
// probably global.
scripttype: /* empty */
{
  $$ = SCC_RES_SCR;
}
| SCRTYPE
{
  $$ = $1;
}
;

scriptargs: /* */
{
  $$ = NULL;
}
| TYPE SYM
{
  if($1 == SCC_VAR_BIT)
    SCC_ABORT(@1,"Script argument can't be of bit type.\n");

  $$ = malloc(sizeof(scc_scr_arg_t));
  $$->next = NULL;
  $$->type = $1;
  $$->sym = $2;
}
| scriptargs ',' TYPE SYM
{
  scc_scr_arg_t *i,*a = malloc(sizeof(scc_scr_arg_t));
  a->next = NULL;
  a->type = $3;
  a->sym = $4;

  for(i = $1 ; i->next ; i = i->next);
  i->next = a;
  $$ = $1;
}
;

location: /* empty */
{
  $$ = -1;
}
| '@' INTEGER
{
  $$ = $2;
}
;

scriptbody:  /* empty */
{
  $$ = NULL;
}
| vardecl
{
  $$ = NULL;
}
| instructions
{
  $$ = scc_script_new(scc_ns,$1);
}

| vardecl instructions
{
  $$ = scc_script_new(scc_ns,$2);
}
;

// bit/word/array/local
vardecl: vdecl ';'
{}
| vardecl vdecl ';'
;

/// this will only decl local vars
vdecl: TYPE SYM
{
  if($1 == SCC_VAR_BIT)
    SCC_ABORT(@1,"Local bit variable are not possible.\n");

  $$ = scc_ns_decl(scc_ns,NULL,$2,SCC_RES_LVAR,$1,local_vars);
  if(!$$) SCC_ABORT(@1,"Declaration failed.\n");
  local_vars++;
}

| vdecl ',' SYM
{
  $$ = scc_ns_decl(scc_ns,NULL,$3,SCC_RES_LVAR,$1->subtype,local_vars);
  if(!$$) SCC_ABORT(@3,"Declaration failed.\n");
  local_vars++;
  $$ = $1;
};
 
body: instruct
{
  $$ = $1;
}

| '{' instructions '}'
{
  $$ = $2;
}
;

instructions: instruct
{
  $$ = $1;
}

| instructions instruct
{
  scc_instruct_t* i;
  for(i = $1 ; i->next ; i = i->next);
  i->next = $2;
}
;

instruct: statements ';'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_ST;
  $$->pre = $1;
}

| BRANCH ';'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_BRANCH;
  $$->subtype = $1;
}

| BRANCH SYM ';'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_BRANCH;
  $$->subtype = $1;
  $$->sym = $2;
}

| block
{
  $$ = $1;
}
;

block: ifblock
{
  $$ = $1;
}

| loopblock
{
  $$ = $1;
}

| SYM ':' loopblock
{
  $$ = $3;
  $$->sym = $1;
}
;

loopblock: FOR '(' statements ';' statements ';' statements ')' body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_FOR;
  $$->pre = $3;
  $$->cond = $5;
  $$->post = $7;
  $$->body = $9;
}


| WHILE '(' statements ')' body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_WHILE;
  $$->subtype = $1;
  $$->cond = $3;
  $$->body = $5; 
}

| DO body WHILE '(' statements ')' ';'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_DO;
  $$->subtype = $3;
  $$->cond = $5;
  $$->body = $2; 
}

| SWITCH '(' statements ')' '{' switchblock  '}'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_SWITCH;
  $$->cond = $3;
  $$->body = $6;
}

| CUTSCENE '(' cargs ')' body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_CUTSCENE;
  $$->cond = $3;
  $$->body = $5;
}

| TRY body OVERRIDE body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_OVERRIDE;
  $$->body = $2;
  $$->body2 = $4;
}
;

switchblock: caseblock instructions
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_CASE;
  $$->cond = $1;
  $$->body = $2; 
}

| switchblock caseblock instructions
{
  scc_instruct_t *n,*i;

  for(i = $1 ; i->next ; i = i->next);
  if(!i->cond)
    SCC_ABORT(@2,"Case statements can't be added after a default.\n");

  n = calloc(1,sizeof(scc_instruct_t));
  n->type = SCC_INST_CASE;
  n->cond = $2;
  n->body = $3;

  i->next = n;
  
  $$ = $1;
}
;

caseblock: caseval
{
  $$ = $1;
}

| DEFAULT ':'
{
  $$ = NULL;
};

caseval: CASE statement ':'
{
  $$ = $2;
}

| caseval CASE statement ':'
{
  scc_statement_t* a;
  for(a = $1 ; a->next ; a = a->next);
  a->next = $3;
  $$ = $1;
}
;

ifblock: IF '(' statements ')' body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_IF;
  $$->subtype = $1;
  $$->cond = $3;
  $$->body = $5;
}

| IF '(' statements ')' body ELSE body
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_IF;
  $$->subtype = $1;
  $$->cond = $3;
  $$->body = $5;
  $$->body2 = $7;
};


// statement chain
statements: statement
{
  // single statement stay as-is
  $$ = $1;
}
| statements ',' statement
{
  scc_statement_t* i;

  // no chain yet, create it
  if($1->type != SCC_ST_CHAIN) {
    $$ = calloc(1,sizeof(scc_statement_t));
    $$->type = SCC_ST_CHAIN;
    // init the chain
    $$->val.l = $1;
  } else
    $$ = $1;

  // append the new element
  for(i = $$->val.l ; i->next ; i = i->next);
  i->next = $3;
}
;


statement: dval
{
  $$ = $1;
}

| var
{
  $$ = $1;
}

| call
{
  $$ = $1;
}

| '[' cargs ']'
{
  scc_statement_t* a;
  
  for(a = $2 ; a ; a = a->next) {
    if(a->type == SCC_ST_STR ||
       a->type == SCC_ST_LIST)
      SCC_ABORT(@2,"String and list can't be used inside a list.\n");
  }

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_LIST;
  $$->val.l = $2;
}

| statement ASSIGN statement
{
  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_OP;
  $$->val.o.type = SCC_OT_ASSIGN;
  $$->val.o.op = $2;
  $$->val.o.argc = 2;
  $$->val.o.argv = $1;
  $1->next = $3;
}

| statement '?' statement ':' statement
{
  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_OP;
  $$->val.o.type = SCC_OT_TERNARY;
  $$->val.o.op = $2;
  $$->val.o.argc = 3;
  $$->val.o.argv = $1;
  $1->next = $3;
  $3->next = $5;
}

| statement LOR statement
{
  SCC_BOP($$,||,$1,$2,$3);
}

| statement LAND statement
{
  SCC_BOP($$,&&,$1,$2,$3);
}

| statement '|' statement
{
  SCC_BOP($$,|,$1,$2,$3);
}

| statement '&' statement
{
  SCC_BOP($$,&,$1,$2,$3);
}

| statement NEQ statement
{
  SCC_BOP($$,!=,$1,$2,$3);
}

| statement EQ statement
{
  SCC_BOP($$,==,$1,$2,$3);
}

| statement GE statement
{
  SCC_BOP($$,>=,$1,$2,$3);
}

| statement '>' statement
{
  SCC_BOP($$,>,$1,$2,$3);
}

| statement LE statement
{
  SCC_BOP($$,<=,$1,$2,$3);
}

| statement '<' statement
{
  SCC_BOP($$,<,$1,$2,$3);
}

| statement '-' statement
{
  SCC_BOP($$,-,$1,$2,$3);
}

| statement '+' statement
{
  SCC_BOP($$,+,$1,$2,$3);
}

| statement '/' statement
{
  SCC_BOP($$,/,$1,$2,$3);
}

| statement '*' statement
{
  SCC_BOP($$,*,$1,$2,$3);
}

| '-' statement %prec NEG
{
   if($2->type == SCC_ST_VAL) {
    $2->val.i = -$2->val.i;
    $$ = $2;
  } else { 
    $$ = calloc(1,sizeof(scc_statement_t));
    $$->type = SCC_ST_OP;
    $$->val.o.type = SCC_OT_UNARY;
    $$->val.o.op = $1;
    $$->val.o.argc = 1;
    $$->val.o.argv = $2;
  }
}

| '!' statement
{
  if($2->type == SCC_ST_VAL) {
    $2->val.i = ! $2->val.i;
    $$ = $2;
  } else { // we call not
    $$ = calloc(1,sizeof(scc_statement_t));
    $$->type = SCC_ST_OP;
    $$->val.o.type = SCC_OT_UNARY;
    $$->val.o.op = $1;
    $$->val.o.argc = 1;
    $$->val.o.argv = $2;
  }
}

| SUFFIX statement %prec PREFIX
{
  if(($2->type == SCC_ST_RES &&
      ($2->val.r->type == SCC_RES_VAR ||
       $2->val.r->type == SCC_RES_LVAR ||
       $2->val.r->type == SCC_RES_BVAR)) ||
     $2->type == SCC_ST_AVAR) {
    if($2->type == SCC_ST_AVAR && $2->val.av.x)
      SCC_ABORT(@1,"Suffix operators can't be applied to 2 dimmensional arrays.\n");
    $$ = calloc(1,sizeof(scc_statement_t));
    $$->type = SCC_ST_OP;
    $$->val.o.type = SCC_OT_UNARY;
    $$->val.o.op = ($1 == INC ? PREINC : PREDEC);
    $$->val.o.argc = 1;
    $$->val.o.argv = $2;
  } else
    SCC_ABORT(@1,"Suffix operators can only be used on variables.\n");
}
| statement SUFFIX %prec POSTFIX
{
  if(($1->type == SCC_ST_RES &&
      ($1->val.r->type == SCC_RES_VAR ||
       $1->val.r->type == SCC_RES_LVAR ||
       $1->val.r->type == SCC_RES_BVAR)) ||
     $1->type == SCC_ST_AVAR) {
    if($1->type == SCC_ST_AVAR && $1->val.av.x)
      SCC_ABORT(@1,"Suffix operators can't be applied to 2 dimmensional arrays.\n");
    $$ = calloc(1,sizeof(scc_statement_t));
    $$->type = SCC_ST_OP;
    $$->val.o.type = SCC_OT_UNARY;
    $$->val.o.op = ($2 == INC) ? POSTINC : POSTDEC;
    $$->val.o.argc = 1;
    $$->val.o.argv = $1;
  } else
    SCC_ABORT(@1,"Suffix operators can only be used on variables.\n");
}
| '(' statements ')'
{
  $$ = $2;
}
;

var: SYM
{
  scc_symbol_t* v = scc_ns_get_sym(scc_ns,NULL,$1);

  if(!v)
    SCC_ABORT(@1,"%s is not a declared ressource.\n",$1);

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_RES;
  $$->val.r = v;
  // allocate rid
  if(!v->rid) scc_ns_get_rid(scc_ns,v);
}
| SYM NS SYM
{
  scc_symbol_t* v = scc_ns_get_sym(scc_ns,$1,$3);

  if(!v)
    SCC_ABORT(@1,"%s::%s is not a declared ressource.\n",$1,$3);

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_RES;
  $$->val.r = v;
  // allocate rid
  if(!v->rid) scc_ns_get_rid(scc_ns,v);

}

| SYM '[' statement ']'
{
  scc_symbol_t* v = scc_ns_get_sym(scc_ns,NULL,$1);

  if(!v || (v->type != SCC_RES_VAR &&
	    v->type != SCC_RES_BVAR &&
	    v->type != SCC_RES_LVAR))
    SCC_ABORT(@1,"%s is not a declared variable.\n",$1);

  if(v->subtype != SCC_VAR_ARRAY)
    SCC_ABORT(@1,"%s is not an array variable, so it can't be subscribed.\n",
	      $1);

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_AVAR;
  $$->val.av.r = v;
  $$->val.av.y = $3;

  // allocate rid
  if(!v->rid) scc_ns_get_rid(scc_ns,v);
}

| SYM '[' statement ',' statement ']'
{
  scc_symbol_t* v = scc_ns_get_sym(scc_ns,NULL,$1);

  if(!v || (v->type != SCC_RES_VAR &&
	    v->type != SCC_RES_BVAR &&
	    v->type != SCC_RES_LVAR))
    SCC_ABORT(@1,"%s is not a declared variable.\n",$1);

  if(v->subtype != SCC_VAR_ARRAY)
    SCC_ABORT(@1,"%s is not an array variable, so it can't be subscribed.\n",
	      $1);

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_AVAR;
  $$->val.av.r = v;
  $$->val.av.x = $3;
  $$->val.av.y = $5;

  // allocate rid
  if(!v->rid) scc_ns_get_rid(scc_ns,v);
};

call: SYM '(' cargs ')'
{
  scc_statement_t* a;
  scc_func_t* f;
  char* err;

  f = scc_get_func($1);
  if(!f)
    SCC_ABORT(@1,"%s is not a know function.\n",$1);

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_CALL;

  $$->val.c.func = f;
  $$->val.c.argv = $3;

  for(a = $3 ; a ; a = a->next)
    $$->val.c.argc++;

  err = scc_statement_check_func(&$$->val.c);
  if(err)
    SCC_ABORT(@1,"%s",err);
};

cargs: 
/* empty */
{
  $$ = NULL;
}

| statement
{
  $$ = $1;
}

| cargs ',' statement
{
  scc_statement_t* i;

  $$ = $1;

  for(i = $$ ; i->next ; i = i->next);

  i->next = $3;
};

dval: INTEGER
{
  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_VAL;
  $$->val.i = $1;
}

| string
{
  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_STR;
  $$->val.s = $1;
  //printf("Got string: %s\n",$1);
};

string: str
{
  $$ = $1;
}
| string str
{
  scc_str_t* s;
  for(s = $1 ; s->next ; s = s->next);
  s->next = $2;
  $$ = $1;
}
;

str: STRING
{
  $$ = calloc(1,sizeof(scc_str_t));
  $$->type = SCC_STR_CHAR;
  $$->str = $1;
}
| STRVAR
{
  $$ = $1;
  // for color we have a raw value in str
  if($$->type != SCC_STR_COLOR) {
    $$->sym = scc_ns_get_sym(scc_ns,NULL,$$->str);
    if(!$$->sym) // fix me we leak str      
      SCC_ABORT(@1,"%s is not a declared variable.\n",$$->str);

    free($$->str);
    $$->str = NULL;
  }
}
;

%%



#if 0

int scc_code_write(scc_fd_t* fd) {
  scc_code_t *c,*next;

  c = scc_instruct_gen_code(scc_inst);

  if(!c) {
    printf("Warning instructions generated no code\n");
    return 0;
  }
   
  while(c) {
    if(!c->len) {
      printf("Warning statement generated has a 0 length.\n");
      scc_code_free(c);
      continue;
    }

    scc_fd_write(fd,c->data,c->len);
    
    next = c->next;
    scc_code_free(c);
    c = next;
  }
  
  return 1;
}

#endif

extern char* scc_lex_get_file(void);
extern int scc_lex_init(char* file);

int yyerror (const char *s)  /* Called by yyparse on error */
{
  printf ("In %s:\nline %d, column %d: %s\n",scc_lex_get_file(),
	  yylloc.first_line,yylloc.first_column,s);
  return 0;
}

static void usage(char* prog) {
  printf("Usage: %s [-o output] input.scumm [input2.scumm ...]\n",prog);
  exit(-1);
}

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &scc_output },
  { NULL, 0, 0, 0, NULL }
};

typedef struct scc_source_st scc_source_t;
struct scc_source_st {
  scc_source_t* next;
  scc_ns_t* ns;
  scc_roobj_t* roobj_list;
};


int main (int argc, char** argv) {
  scc_cl_arg_t* files,*f;
  char* out;
  scc_source_t *src,*srcs = NULL;

  if(argc < 2) usage(argv[0]);

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files) usage(argv[0]);

  for(f = files ; f ; f = f->next) {
    src = malloc(sizeof(scc_source_t));
    src->ns = scc_ns = scc_ns_new();
    local_scr = 0;
    scc_roobj = NULL;
    scc_roobj_list = NULL;
    if(!scc_lex_init(f->val)) return -1;

    if(yyparse()) return -1;

    src->roobj_list = scc_roobj_list;
    src->next = srcs;
    srcs = src;
  }

  out = scc_output ? scc_output : "output.roobj";
  out_fd = new_scc_fd(out,O_WRONLY|O_CREAT|O_TRUNC,0);
  if(!out_fd) {
    printf("Failed to open output file %s.\n",out);
    return -1;
  }

  for(src = srcs ; src ; src = src->next) {
    for(scc_roobj = src->roobj_list ; scc_roobj ; 
        scc_roobj = scc_roobj->next) {
      if(!scc_roobj_write(scc_roobj,src->ns,out_fd)) {
        printf("Failed to write ROOM ????\n");
        return 1;
      }
    }
  }

  scc_fd_close(out_fd);

  return 0;
}
