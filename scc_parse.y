/* ScummC
 * Copyright (C) 2004-2005  Alban Bedel
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

%{
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

#include "scc_parse.h"
#include "scc_ns.h"
#include "scc_img.h"
#include "scc.h"
#include "scc_roobj.h"
#include "scc_code.h"
#include "scc_param.h"

#include "scc_parse.tab.h"

#include "scc_lex.h"

#define YYERROR_VERBOSE 1

typedef struct scc_parser {
  scc_lex_t* lex;
  scc_ns_t* ns;
  scc_roobj_t* roobj_list;
  scc_roobj_t* roobj;
  scc_roobj_obj_t* obj;
  int local_vars;
  int local_scr;
  int cycl;
} scc_parser_t;

#define YYPARSE_PARAM v_sccp
#define sccp ((scc_parser_t*)v_sccp)
#define YYLEX_PARAM sccp->lex

// redefined the function names
#define yyparse scc_parser_parse_internal
#define yylex scc_lex_lex
#define yyerror(msg) scc_parser_error(sccp,&yyloc,msg)

int scc_parser_error (scc_parser_t* p,YYLTYPE *llocp, const char *s);

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
  printf("%s: ",scc_lex_get_file(sccp->lex)); \
  printf(msg, ## args); \
  YYABORT; \
}

  // output filename
  static char* scc_output = NULL;

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
      } else if(c->func->argt[n] == SCC_FA_ARRAY) {
	if(a->type != SCC_ST_VAR ||
	   !(a->val.v.r->subtype & SCC_VAR_ARRAY)) {
	  sprintf(func_err,"Argument %d of call to %s must be an array variable.\n",
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

%pure_parser

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
  int* intlist;
  scc_verb_script_t* vscr;
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
%token VOICE
%token CYCL

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
%type <inst> cutsceneblock
%type <inst> block
%type <inst> instructions
%type <inst> oneinstruct
%type <inst> dobody
%type <inst> body
%type <inst> loophead
%type <str> label
%type <inst> dohead
%type <inst> switchhead
%type <scr> scriptbody
%type <inst> verbcode
%type <sym> verbentry
%type <vscr> verbsblock

%type <integer> gvardecl
%type <integer> gresdecl
%type <integer> groomresdecl
%type <integer> globalres
%type <integer> roomres
%type <integer> resdecl
%type <integer> scripttype

%type <arg> scriptargs
%type <sym> roomscrdecl
%type <sym> roomobjdecl
%type <sym> cycledecl
%type <sym> voicedecl
%type <strlist> zbufs
%type <intlist> synclist
%type <integer> typemod

%%

srcfile: /* empty */
| srcs
;

srcs: src
| srcs src
;

src: room
| gdecl
;

gdecl: gvardecl ';'
{}
| gresdecl ';'
{}
| groomresdecl ';'
{}
| groomdecl ';'
;

gvardecl: TYPE typemod SYM location
{
  if($1 == SCC_VAR_BIT && !$2)
    scc_ns_decl(sccp->ns,NULL,$3,SCC_RES_BVAR,$1,$4);
  else
    scc_ns_decl(sccp->ns,NULL,$3,SCC_RES_VAR,$1 | $2,$4);

  $$ = $1;
}
| gvardecl ',' typemod SYM location
{
  if($1 == SCC_VAR_BIT && !$3)
    scc_ns_decl(sccp->ns,NULL,$4,SCC_RES_BVAR,$1,$5);
  else
    scc_ns_decl(sccp->ns,NULL,$4,SCC_RES_VAR,$1 | $3,$5);

  $$ = $1;
}
;

typemod: /* nothing */
{
  $$ = 0;
}
| '*'
{
  $$ = SCC_VAR_ARRAY;
}
;

// room must be put outside of globalres
// otherwise it conflict with roombdecl
groomdecl: ROOM SYM location
{
  scc_ns_decl(sccp->ns,NULL,$2,SCC_RES_ROOM,0,$3);
}
| groomdecl ',' SYM location
{
  scc_ns_decl(sccp->ns,NULL,$3,SCC_RES_ROOM,0,$4);
}
;

gresdecl: globalres SYM location
{
  scc_ns_decl(sccp->ns,NULL,$2,$1,0,$3);
}
| gresdecl ',' SYM location
{
  scc_ns_decl(sccp->ns,NULL,$3,$1,0,$4);
}
;

globalres: ACTOR
{ 
  $$ = SCC_RES_ACTOR;
}
| VERB
{ 
  $$ = SCC_RES_VERB;
}
| CLASS
{ 
  $$ = SCC_RES_CLASS;
}
;

// cost, sound, chset, object, script, voice, cycl
groomresdecl: roomres SYM NS SYM location
{
  scc_ns_decl(sccp->ns,$2,$4,$1,0,$5);
  $$ = $1; // hack to propagte the type
}
| groomresdecl ',' SYM NS SYM location
{
  scc_ns_decl(sccp->ns,$3,$5,$1,0,$6);
}
;

roomres: RESTYPE
| OBJECT
{ 
  $$ = SCC_RES_OBJ;
}
| SCRIPT
{ 
  $$ = SCC_RES_SCR;
}
| VOICE
{ 
  $$ = SCC_RES_VOICE;
}
| CYCL
{ 
  $$ = SCC_RES_CYCL;
}
;

// This allow us to have the room declared before we parse the body.
// At this point the roobj object is created.
roombdecl: ROOM SYM location
{
  scc_symbol_t* sym = scc_ns_decl(sccp->ns,NULL,$2,SCC_RES_ROOM,0,$3);
  scc_ns_get_rid(sccp->ns,sym);
  scc_ns_push(sccp->ns,sym);
  sccp->roobj = scc_roobj_new(sym);
}
;

// Here we should have the whole room.
room: roombdecl roombody
{
  printf("Room done :)\n");
  sccp->local_scr = SCC_MAX_GLOB_SCR;
  memset(&sccp->ns->as[SCC_RES_LSCR],0,0x10000/8);
  sccp->cycl = 1;
  scc_ns_clear(sccp->ns,SCC_RES_CYCL);
  scc_ns_pop(sccp->ns);

  if(!sccp->roobj->scr &&
     !sccp->roobj->lscr &&
     !sccp->roobj->obj &&
     !sccp->roobj->res &&
     !sccp->roobj->cycl &&
     !sccp->roobj->image) {
    printf("Room is empty, only declarations.\n");
    scc_roobj_free(sccp->roobj);
  } else {
    sccp->roobj->next = sccp->roobj_list;
    sccp->roobj_list = sccp->roobj;
  }
  sccp->roobj = NULL;
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
  if(!$1->rid) scc_ns_get_rid(sccp->ns,$1);
  if($3) {
    $3->sym = $1;
    scc_roobj_add_scr(sccp->roobj,$3);
  }

  scc_ns_clear(sccp->ns,SCC_RES_LVAR);
  scc_ns_pop(sccp->ns);
}
// forward declaration
| roomscrdecl ';'
{
  // well :)
  scc_ns_clear(sccp->ns,SCC_RES_LVAR);
  scc_ns_pop(sccp->ns);
}
// objects
| roomobjdecl '{' objectparams objectverbs '}'
{
  if(!$1->rid) scc_ns_get_rid(sccp->ns,$1);
  // add the obj to the room
  scc_roobj_add_obj(sccp->roobj,sccp->obj);
  sccp->obj = NULL;
}
// forward declaration
| roomobjdecl ';'
{
  scc_roobj_obj_free(sccp->obj);
  sccp->obj = NULL;
}
| voicedef ';'
{}
| voicedecl ';'
{}
| cycledef ';'
{}
| cycledecl ';'
{}
;

roomscrdecl: scripttype SCRIPT SYM  '(' scriptargs ')' location
{
  scc_scr_arg_t* a = $5;
  scc_symbol_t* s;

  s = scc_ns_decl(sccp->ns,NULL,$3,$1,0,$7);
  if(!s) SCC_ABORT(@3,"Failed to declare script %s.\n",$3);

  if($1 == SCC_RES_LSCR && s->addr < 0 &&
     strcmp($3,"entry") && strcmp($3,"exit"))
    if(!scc_ns_alloc_sym_addr(sccp->ns,s,&sccp->local_scr))
      SCC_ABORT(@3,"Failed to allocate local script address.\n");

  scc_ns_push(sccp->ns,s);

  // declare the arguments
  sccp->local_vars = 0;
  while(a) {
    scc_ns_decl(sccp->ns,NULL,a->sym,SCC_RES_LVAR,a->type,sccp->local_vars);
    sccp->local_vars++;
    a = a->next;
  }
  $$ = s;
}
;

roomobjdecl: OBJECT SYM location
{
  scc_symbol_t* sym;

  if(sccp->obj)
    SCC_ABORT(@1,"Something went wrong with object declarations.\n");


  sym = scc_ns_decl(sccp->ns,NULL,$2,SCC_RES_OBJ,0,$3);
  if(!sym) SCC_ABORT(@1,"Failed to declare object %s.\n",$2);
  
  sccp->obj = scc_roobj_obj_new(sym);
  $$ = sym;
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

  if(!scc_roobj_set_param(sccp->roobj,sccp->ns,$1,-1,$3))
    SCC_ABORT(@1,"Failed to set room parameter.\n");

}
| SYM '[' INTEGER ']' ASSIGN STRING ';'
{
  if($5 != '=')
    SCC_ABORT(@5,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_set_param(sccp->roobj,sccp->ns,$1,$3,$6))
    SCC_ABORT(@1,"Failed to set room parameter.\n");
}
;

cycledef: cycledecl ASSIGN '{' INTEGER ',' INTEGER ',' INTEGER ',' INTEGER '}'
{
  if($2 != '=')
    SCC_ABORT(@3,"Invalid operator for cycle declaration.\n");

  if(!scc_roobj_add_cycl(sccp->roobj,$1,$4,$6,$8,$10))
    SCC_ABORT(@1,"Failed to add cycl.\n");
}
;

cycledecl: CYCL SYM location
{
  $$ = scc_ns_decl(sccp->ns,NULL,$2,SCC_RES_CYCL,0,$3);
  if(!$$) SCC_ABORT(@1,"Cycl declaration failed.\n");
  if($$->addr < 0 && !scc_ns_alloc_sym_addr(sccp->ns,$$,&sccp->cycl))
      SCC_ABORT(@1,"Failed to allocate cycle address.\n");
}
;

voicedef: voicedecl ASSIGN '{' STRING ',' synclist '}'
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for voice declaration.\n");

  if(!$1->rid) scc_ns_get_rid(sccp->ns,$1);

  if(!scc_roobj_add_voice(sccp->roobj,$1,$4,$6[0],$6+1))
    SCC_ABORT(@1,"Failed to add voice.");

  free($6);
}
| voicedecl ASSIGN '{' STRING '}'
{
  if($2 != '=')
    SCC_ABORT(@3,"Invalid operator for voice declaration.\n");

  if(!$1->rid) scc_ns_get_rid(sccp->ns,$1);

  if(!scc_roobj_add_voice(sccp->roobj,$1,$4,0,NULL))
    SCC_ABORT(@1,"Failed to add voice.");
}
;

voicedecl: VOICE SYM
{
  $$ = scc_ns_decl(sccp->ns,NULL,$2,SCC_RES_VOICE,0,-1);
  if(!$$) SCC_ABORT(@1,"Declaration failed.\n");
}
;

synclist: INTEGER
{
  $$ = malloc(2*sizeof(int));
  $$[0] = 1;
  $$[1] = $1;
}
| synclist ',' INTEGER
{
  int l = $1[0]+1;
  $$ = realloc($1,(l+1)*sizeof(int));
  $$[l] = $3;
  $$[0] = l;
}
;

// ressource declaration, possibly with harcoded
// location
resdecl: RESTYPE SYM location ASSIGN STRING
{
  scc_symbol_t* r;

  if($4 != '=')
    SCC_ABORT(@4,"Invalid operator for ressource declaration.\n");

  r = scc_ns_decl(sccp->ns,NULL,$2,$1,0,$3);
  // then we need to add that to the roobj
  scc_roobj_add_res(sccp->roobj,r,$5);
  
  if(!r->rid) scc_ns_get_rid(sccp->ns,r);
  $$ = $1; // propagate type
}
| resdecl ',' SYM location ASSIGN STRING
{
  scc_symbol_t* r;

  if($5 != '=')
    SCC_ABORT(@5,"Invalid operator for ressource declaration.\n");

  r = scc_ns_decl(sccp->ns,NULL,$3,$1,0,$4);
  // then we need to add that to the roobj
  scc_roobj_add_res(sccp->roobj,r,$6);

  if(!r->rid) scc_ns_get_rid(sccp->ns,r);
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

  if(!scc_roobj_obj_set_param(sccp->obj,$1,$3))
    SCC_ABORT(@1,"Failed to set object parameter.\n");
}
| SYM ASSIGN INTEGER
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_obj_set_int_param(sccp->obj,$1,$3))
    SCC_ABORT(@1,"Failed to set object parameter.\n");

}
| SYM ASSIGN '-' INTEGER
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");

  if(!scc_roobj_obj_set_int_param(sccp->obj,$1,-$4))
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

  if(!strcmp($1,"owner")) {
    sym = scc_ns_get_sym(sccp->ns,NULL,$3);
    if(!sym)
      SCC_ABORT(@3,"%s is not a declared actor.\n",$3);
    if(sym->type != SCC_RES_ACTOR)
      SCC_ABORT(@3,"%s is not an actor.\n",sym->sym);
  
    sccp->obj->owner = sym;

  } else if(!strcmp($1,"parent")) {
    sym = scc_ns_get_sym(sccp->ns,NULL,$3);
    if(!sym)
      SCC_ABORT(@3,"%s is not a declared object.\n",$3);
    if(sym->type != SCC_RES_OBJ)
      SCC_ABORT(@3,"%s is not an object.\n",sym->sym);
    if(sym->parent != sccp->roobj->sym)
      SCC_ABORT(@3,"%s doesn't belong to room %s.\n",sym->sym,sccp->roobj->sym->sym);

    sccp->obj->parent = sym;
  } else
    SCC_ABORT(@1,"Expexted 'owner' or 'parent'.\n");
    
}
| CLASS ASSIGN '{' classlist '}'
{
  if($2 != '=')
    SCC_ABORT(@2,"Invalid operator for parameter setting.\n");
}
;

classlist: SYM
{
  scc_symbol_t* sym = scc_ns_get_sym(sccp->ns,NULL,$1);

  if(!sym)
    SCC_ABORT(@1,"%s is not a declared class.\n",$1);

  if(sym->type != SCC_RES_CLASS)
    SCC_ABORT(@1,"%s is not a class in the current context.\n",$1);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(sccp->ns,sym);

  if(!scc_roobj_obj_set_class(sccp->obj,sym))
    SCC_ABORT(@1,"Failed to set object class.\n");

}
| classlist ',' SYM
{
  scc_symbol_t* sym = scc_ns_get_sym(sccp->ns,NULL,$3);

  if(!sym)
    SCC_ABORT(@3,"%s is not a declared class.\n",$3);

  if(sym->type != SCC_RES_CLASS)
    SCC_ABORT(@3,"%s is not a class in the current context.\n",$3);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(sccp->ns,sym);

  if(!scc_roobj_obj_set_class(sccp->obj,sym))
    SCC_ABORT(@3,"Failed to set object class.\n");
}
;

imgdecls: imgdecl
| imgdecls ',' imgdecl
;

imgdecl: '{' INTEGER ',' INTEGER ',' STRING '}'
{
  if(!scc_roobj_obj_add_state(sccp->obj,$2,$4,$6,NULL))
    SCC_ABORT(@1,"Failed to add room state\n");
}
| '{' INTEGER ',' INTEGER ',' STRING ',' '{' zbufs '}' '}'
{
  if(!scc_roobj_obj_add_state(sccp->obj,$2,$4,$6,$9))
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

objectverbs: /* nothing */
{
}
| verbentrydecl '{' vardecl verbsblock '}'
{
  scc_verb_script_t* v,*l;
  scc_script_t* scr;

  for(v = $4 ; v ; l = v, v = v->next,free(l) ) {
    if(v->inst)
      scr = scc_script_new(sccp->ns,v->inst,SCC_OP_VERB_RET,v->next ? 0 : 1);
    else
      scr = calloc(1,sizeof(scc_script_t));
    scr->sym = v->sym;
    if(!scc_roobj_obj_add_verb(sccp->obj,scr))
      SCC_ABORT(@1,"Failed to add verb %s.\n",v->sym ? v->sym->sym : "default");
  }    
  scc_ns_clear(sccp->ns,SCC_RES_LVAR);
  scc_ns_pop(sccp->ns);
}
;

verbentrydecl: VERB '(' scriptargs ')'
{
  scc_scr_arg_t* a = $3;
  // push the obj for the local vars
  scc_ns_push(sccp->ns,sccp->obj->sym);

  // declare the arguments
  sccp->local_vars = 0;
  while(a) {
    scc_ns_decl(sccp->ns,NULL,a->sym,SCC_RES_LVAR,a->type,sccp->local_vars);
    sccp->local_vars++;
    a = a->next;
  }
}
;

verbsblock: verbentry verbcode
{
  $$ = calloc(1,sizeof(scc_verb_script_t));
  $$->sym = $1;
  $$->inst = $2;
}

| verbsblock verbentry verbcode
{
  $$ = $1;
  for(; $1 ; $1 = $1->next) {
    if($1->sym == $2)
      SCC_ABORT(@2,"Verb %s is already defined.\n",
                $2 ? $2->sym : "default");
    if(!$1->next) break;
  }
  $1->next = calloc(1,sizeof(scc_verb_script_t));
  $1 = $1->next;
  $1->sym = $2;
  $1->inst = $3;
}
;

verbcode: /* nothing */
{
  $$ = NULL;
}
| instructions
;

verbentry: CASE SYM ':'
{
  scc_symbol_t* sym = scc_ns_get_sym(sccp->ns,NULL,$2);
  if(!sym)
    SCC_ABORT(@1,"%s is not a declared verb.\n",$2);
  
  if(sym->type != SCC_RES_VERB)
    SCC_ABORT(@1,"%s is not a verb in the current context.\n",$2);

  // allocate an rid
  if(!sym->rid) scc_ns_get_rid(sccp->ns,sym);

  $$ = sym;
}
| DEFAULT ':'
{
  $$ = NULL;
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
| TYPE typemod SYM
{
  //if($1 == SCC_VAR_BIT)
  //  SCC_ABORT(@1,"Script argument can't be of bit type.\n");

  $$ = malloc(sizeof(scc_scr_arg_t));
  $$->next = NULL;
  $$->type = $1 | $2;
  $$->sym = $3;
}
| scriptargs ',' TYPE typemod SYM
{
  scc_scr_arg_t *i,*a = malloc(sizeof(scc_scr_arg_t));
  a->next = NULL;
  a->type = $3 | $4;
  a->sym = $5;

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

| vardecl instructions
{
  $$ = scc_script_new(sccp->ns,$2,SCC_OP_SCR_RET,1);
  if(!$$)
    SCC_ABORT(@1,"Code generation failed.\n");
}
;


// bit, nibble, char, byte, int
vardecl: /* empty */
| vardec
;

vardec: vdecl ';'
{
}
| vardec vdecl ';'
;

/// this will only decl local vars
vdecl: TYPE typemod SYM
{
  //if($1 == SCC_VAR_BIT)
  //  SCC_ABORT(@1,"Local bit variable are not possible.\n");

  $$ = scc_ns_decl(sccp->ns,NULL,$3,SCC_RES_LVAR,$1 | $2,sccp->local_vars);
  if(!$$) SCC_ABORT(@1,"Declaration failed.\n");
  sccp->local_vars++;
}

| vdecl ',' typemod SYM
{
  $$ = scc_ns_decl(sccp->ns,NULL,$4,SCC_RES_LVAR,$1->subtype | $3,sccp->local_vars);
  if(!$$) SCC_ABORT(@4,"Declaration failed.\n");
  sccp->local_vars++;
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

instruct: oneinstruct ';'
| block
;

oneinstruct: statements
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_ST;
  $$->pre = $1;
}

| BRANCH
{
  if($1 != SCC_BRANCH_RETURN) {
    scc_loop_t* l = scc_loop_get($1,NULL);
    if(!l)
      SCC_ABORT(@1,"Invalid branch instruction.\n");
  }
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_BRANCH;
  $$->subtype = $1;
}

| BRANCH SYM
{
  if($1 != SCC_BRANCH_RETURN) {
    scc_loop_t* l = scc_loop_get($1,$2);
    if(!l)
      SCC_ABORT(@1,"Invalid branch instruction.\n");
  }
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_BRANCH;
  $$->subtype = $1;
  $$->sym = $2;
}
;


dobody: oneinstruct
{
  $$ = $1;
}

| '{' instructions '}'
{
  $$ = $2;
}
;

block: ifblock
{
  $$ = $1;
}
| cutsceneblock

| loopblock
{
  $$ = $1;
}
;

loopblock: loophead body
{
  $$ = $1;
  $$->body = $2;
  free(scc_loop_pop());
}
| dohead dobody WHILE '(' statements ')' ';'
{
  $$ = $1;
  $$->subtype = $3;
  $$->cond = $5;
  $$->body = $2;
  free(scc_loop_pop());
}
| switchhead  '{' switchblock  '}'
{
  $$ = $1;
  $$->body = $3;
  free(scc_loop_pop());
}
;


loophead: label FOR '(' statements ';' statements ';' statements ')'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_FOR;
  $$->sym = $1;
  $$->pre = $4;
  $$->cond = $6;
  $$->post = $8;
  scc_loop_push($$->type,$$->sym);
}

| label WHILE '(' statements ')'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_WHILE;
  $$->sym = $1;
  $$->subtype = $2;
  $$->cond = $4;
  scc_loop_push($$->type,$$->sym);
}
;

dohead: label DO
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_DO;
  $$->sym = $1;
  scc_loop_push($$->type,$$->sym);
}
;

switchhead: label SWITCH '(' statements ')'
{
  $$ = calloc(1,sizeof(scc_instruct_t));
  $$->type = SCC_INST_SWITCH;
  $$->sym = $1;
  $$->cond = $4;
  scc_loop_push($$->type,$$->sym);
}
;

label: /* nothing */
{
  $$ = NULL;
}
| SYM ':'
{
  $$ = $1;
}
;

cutsceneblock: CUTSCENE '(' cargs ')' body
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

| var '[' statement ']'
{
  scc_symbol_t* v;

  if($1->type != SCC_ST_VAR)
    SCC_ABORT(@1,"%s is not a variable, so it can't be subscribed.\n",
	      $1->val.r->sym);
  v = $1->val.v.r;
  if(!(v->subtype & SCC_VAR_ARRAY))
    SCC_ABORT(@1,"%s is not an array variable, so it can't be subscribed.\n",v->sym);
  $$ = $1;
  $$->val.v.y = $3;
}

| var '[' statement ',' statement ']'
{
  scc_symbol_t* v;

  if($1->type != SCC_ST_VAR)
    SCC_ABORT(@1,"%s is not a variable, so it can't be subscribed.\n",
	      $1->val.r->sym);
  v = $1->val.v.r;
  if(!(v->subtype & SCC_VAR_ARRAY))
    SCC_ABORT(@1,"%s is not an array variable, so it can't be subscribed.\n",v->sym);
  $$ = $1;
  $$->val.v.x = $3;
  $$->val.v.y = $5;
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
  if($1->type != SCC_ST_VAR)
    SCC_ABORT(@1,"rvalue is not a variable, so it can't be assigned.\n");

  if($1->val.v.x && $3->type == SCC_ST_STR)
    SCC_ABORT(@1,"Strings can't be assigned to 2-dim arrays.\n");

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
  if($2->type != SCC_ST_VAR)
    SCC_ABORT(@1,"Suffix operators can only be used on variables.\n");

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_OP;
  $$->val.o.type = SCC_OT_UNARY;
  $$->val.o.op = ($1 == INC ? PREINC : PREDEC);
  $$->val.o.argc = 1;
  $$->val.o.argv = $2;
}

| statement SUFFIX %prec POSTFIX
{
  if($1->type != SCC_ST_VAR)
    SCC_ABORT(@1,"Suffix operators can only be used on variables.\n");

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_OP;
  $$->val.o.type = SCC_OT_UNARY;
  $$->val.o.op = ($2 == INC) ? POSTINC : POSTDEC;
  $$->val.o.argc = 1;
  $$->val.o.argv = $1;
}

| '(' statements ')'
{
  $$ = $2;
}
;

var: SYM
{
  scc_symbol_t* v = scc_ns_get_sym(sccp->ns,NULL,$1);

  if(!v)
    SCC_ABORT(@1,"%s is not a declared ressource.\n",$1);

  $$ = calloc(1,sizeof(scc_statement_t));
  if(scc_sym_is_var(v->type)) {
    $$->type = SCC_ST_VAR;
    $$->val.v.r = v;
  } else {
    $$->type = SCC_ST_RES;
    $$->val.r = v;
  }
  // allocate rid
  if(!v->rid) scc_ns_get_rid(sccp->ns,v);
}
| SYM NS SYM
{
  scc_symbol_t* v = scc_ns_get_sym(sccp->ns,$1,$3);

  if(!v)
    SCC_ABORT(@1,"%s::%s is not a declared ressource.\n",$1,$3);

  $$ = calloc(1,sizeof(scc_statement_t));
  if(scc_sym_is_var(v->type)) {
    $$->type = SCC_ST_VAR;
    $$->val.v.r = v;
  } else {
    $$->type = SCC_ST_RES;
    $$->val.r = v;
  }
  // allocate rid
  if(!v->rid) scc_ns_get_rid(sccp->ns,v);

}
;

call: SYM '(' cargs ')'
{
  scc_statement_t *a,*scr,*list;
  scc_func_t* f;
  scc_symbol_t* s;
  char* err;

  f = scc_get_func($1);
  if(!f) {
    s = scc_ns_get_sym(sccp->ns,NULL,$1);
    if(!s || (s->type != SCC_RES_SCR && s->type != SCC_RES_LSCR))
      SCC_ABORT(@1,"%s is not a know function or script.\n",$1);

    f = scc_get_func("startScript0");
    if(!f)
      SCC_ABORT(@1,"Internal error: startScriptQuick not found.\n");

    if(!s->rid) scc_ns_get_rid(sccp->ns,s);

    // create the arguments
    scr = calloc(1,sizeof(scc_statement_t));
    scr->type = SCC_ST_RES;
    scr->val.r = s;

    list = calloc(1,sizeof(scc_statement_t));
    list->type = SCC_ST_LIST;
    list->val.l = $3;

    scr->next = list;

    $3 = scr;
  }

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_CALL;

  $$->val.c.func = f;
  $$->val.c.argv = $3;

  for(a = $3 ; a ; a = a->next)
    $$->val.c.argc++;

  err = scc_statement_check_func(&$$->val.c);
  if(err)
    SCC_ABORT(@1,"%s",err);
}

| SYM NS SYM '(' cargs ')'
{
  scc_statement_t *scr,*list;
  scc_func_t* f;
  scc_symbol_t* s;

  s = scc_ns_get_sym(sccp->ns,$1,$3);
  if(!s || (s->type != SCC_RES_SCR && s->type != SCC_RES_LSCR))
    SCC_ABORT(@1,"%s::%s is not a know function or script.\n",$1,$3);

  if(s->type == SCC_RES_LSCR &&
     s->parent != sccp->roobj->sym)
    SCC_ABORT(@1,"%s is a local script and %s is not the current room.\n",
              $3,$1);

  f = scc_get_func("startScript0");
  if(!f)
    SCC_ABORT(@1,"Internal error: startScriptQuick not found.\n");

  if(!s->rid) scc_ns_get_rid(sccp->ns,s);

  // create the arguments
  scr = calloc(1,sizeof(scc_statement_t));
  scr->type = SCC_ST_RES;
  scr->val.r = s;
  
  list = calloc(1,sizeof(scc_statement_t));
  list->type = SCC_ST_LIST;
  list->val.l = $5;
  
  scr->next = list;

  $$ = calloc(1,sizeof(scc_statement_t));
  $$->type = SCC_ST_CALL;

  $$->val.c.func = f;
  $$->val.c.argv = scr;
  $$->val.c.argc = 2;
}
;

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
    $$->sym = scc_ns_get_sym(sccp->ns,NULL,$$->str);
    if(!$$->sym) // fix me we leak str      
      SCC_ABORT(@1,"%s is not a declared variable.\n",$$->str);

    free($$->str);
    $$->str = NULL;

    switch($$->type) {
    case SCC_STR_VERB:
    case SCC_STR_NAME:
      if($$->sym->type != SCC_RES_VAR &&
         $$->sym->type != SCC_RES_LVAR)
        SCC_ABORT(@1,"%s is not a variable",$$->sym->sym);
      break;
    case SCC_STR_VOICE:
      if($$->sym->type != SCC_RES_VOICE)
        SCC_ABORT(@1,"%s is not a voice",$$->sym->sym);
      break;
    case SCC_STR_FONT:
      if($$->sym->type != SCC_RES_CHSET)
        SCC_ABORT(@1,"%s is not a charset",$$->sym->sym);
      break;
    }
    // allocate rid
    if(!$$->sym->rid) scc_ns_get_rid(sccp->ns,$$->sym);
  }
}
;

%%

#undef yyparse
#undef yylex
#undef yyerror
#undef sccp


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

extern int scc_main_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex);

typedef struct scc_source_st scc_source_t;
struct scc_source_st {
  scc_source_t* next;
  scc_ns_t* ns;
  scc_roobj_t* roobj_list;
};


static void set_start_pos(YYLTYPE *llocp,int line,int column) {
  llocp->first_line = line+1;
  llocp->first_column = column;
}

static void set_end_pos(YYLTYPE *llocp,int line,int column) {
  llocp->last_line = line+1;
  llocp->last_column = column;
}


scc_source_t* scc_parser_parse(scc_parser_t* sccp,char* file) {
  scc_source_t* src;

  if(!scc_lex_push_buffer(sccp->lex,file)) return NULL;

  sccp->ns = scc_ns_new();
  sccp->roobj_list = NULL;
  sccp->roobj = NULL;
  sccp->obj = NULL;
  sccp->local_vars = 0;
  sccp->local_scr = SCC_MAX_GLOB_SCR;
  sccp->cycl = 1;

  if(scc_parser_parse_internal(sccp)) return NULL;

  if(sccp->lex->error) {
    printf("%s: %s\n",scc_lex_get_file(sccp->lex),sccp->lex->error);
    return NULL;
  }

  src = malloc(sizeof(scc_source_t));
  src->ns = sccp->ns;
  src->roobj_list = sccp->roobj_list;
  return src;
}

scc_parser_t* scc_parser_new(char** include) {
  scc_parser_t* p = calloc(1,sizeof(scc_parser_t));
  p->lex = scc_lex_new(scc_main_lexer,set_start_pos,set_end_pos,include);
  return p;
}


int scc_parser_error(scc_parser_t* sccp,YYLTYPE *loc, const char *s)  /* Called by yyparse on error */
{
  printf("%s: %s\n",scc_lex_get_file(sccp->lex),
         sccp->lex->error ? sccp->lex->error : s);
  return 0;
}

static void usage(char* prog) {
  printf("Usage: %s [-o output] input.scumm [input2.scumm ...]\n",prog);
  exit(-1);
}

char** scc_include = NULL;

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &scc_output },
  { "I", SCC_PARAM_STR_LIST, 0, 0, &scc_include },
  { NULL, 0, 0, 0, NULL }
};


int main (int argc, char** argv) {
  scc_cl_arg_t* files,*f;
  scc_parser_t* sccp;
  char* out;
  scc_source_t *src,*srcs = NULL;
  scc_roobj_t* scc_roobj;
  scc_fd_t* out_fd;

  if(argc < 2) usage(argv[0]);

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files) usage(argv[0]);

  sccp = scc_parser_new(scc_include);

  for(f = files ; f ; f = f->next) {
    src = scc_parser_parse(sccp,f->val);
    if(!src) return 1;
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
