/* scanner for a toy Pascal-like language */
      
%{
  /* need this for the call to atof() below */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "scc_util.h"
#include "scc_parse.h"


#ifndef YYSTYPE
typedef union {
  int integer;
  char* str;
  scc_str_t* strvar;
} yystype;
# define YYSTYPE yystype
#endif

#include "scc_parse.tab.h"

 extern YYLTYPE yylloc;


#define MAX_INCLUDE_DEPTH 10

 static struct scc_lex_inc_st {
   YY_BUFFER_STATE buf;
   char* name;
   int line,col;
 } inc_stack[MAX_INCLUDE_DEPTH];

 static int inc_ptr = 0;

 static char* str_buf = NULL;
 static int str_buf_len = 0, str_buf_pos = 0;

 static scc_str_t* strvar = NULL;
 static int line=1,col=0;
 static int str_first_line=0,str_first_column=0;
 static int strvar_first_line=0,strvar_first_column=0;
 static int strvar_last_line=0,strvar_last_column=0;

#define SET_POS(len) yylloc.first_line = line;       \
                     yylloc.first_column = col;      \
                     yylloc.last_line = line;        \
                     yylloc.last_column = col+len-1; \
                     col += len

 static void str_buf_add_code(int v) {
   if(str_buf_pos + 2 >= str_buf_len) {
     str_buf_len += 1024;
     str_buf = realloc(str_buf,str_buf_len);
   }
   str_buf[str_buf_pos] = 0xFF;
   str_buf_pos++;
   str_buf[str_buf_pos] = v;
   str_buf_pos++;
 }

%}

%option stack

%x STR
%x INCLUDE
%x COMMENT
      
DIGIT    [0-9]
XDIGIT   [0-9a-fA-F]

ID       [a-zA-Z_][a-zA-Z0-9_]*
      
%%

^[ \t]*"#include"[ \t]+\" { 
  col += strlen(yytext);
  BEGIN(INCLUDE);
}

<INCLUDE>{
  \n {
    return ERROR;
  }

  [^\"\n]+\" {
    // count
    col += strlen(yytext);
    // end of file name
    yytext[strlen(yytext)-1] = '\0';

    if(inc_ptr+1 >= MAX_INCLUDE_DEPTH) {
      printf("Too deep includes.\n");
      return ERROR;
    }

    // setup the buffer
    inc_stack[inc_ptr].buf = YY_CURRENT_BUFFER;
    inc_stack[inc_ptr].line = line;
    inc_stack[inc_ptr].col = col;
    inc_ptr++;

    yyin = fopen(yytext,"r");
    if(!yyin) {
      printf("Failed to open %s.\n",yytext);
      return ERROR;
    }

    inc_stack[inc_ptr].name = strdup(yytext);
    
    yy_switch_to_buffer(yy_create_buffer( yyin, YY_BUF_SIZE ));

    line = 1;
    col = 0;

    BEGIN(INITIAL);
  }
}

<<EOF>> {
  if(!inc_ptr) {
    yyterminate();
  } else {
    // destroy the buffer name
    if(inc_stack[inc_ptr].name) {
      free(inc_stack[inc_ptr].name);
      inc_stack[inc_ptr].name = 0;
    }
    // restore the old buffer
    inc_ptr--;
    yy_delete_buffer(YY_CURRENT_BUFFER);
    yy_switch_to_buffer(inc_stack[inc_ptr].buf);
    line = inc_stack[inc_ptr].line;
    col = inc_stack[inc_ptr].col;
  }
}


"&&" { SET_POS(2); yylval.integer = LAND; return yylval.integer; }

"||" { SET_POS(2); yylval.integer = LOR; return yylval.integer; }

"==" { SET_POS(2); yylval.integer = EQ; return yylval.integer; }

"!=" { SET_POS(2); yylval.integer = NEQ; return yylval.integer; }

">=" { SET_POS(2); yylval.integer = GE; return yylval.integer; }

"<=" { SET_POS(2); yylval.integer = LE; return yylval.integer; }

"+=" { SET_POS(2); yylval.integer = AADD; return ASSIGN; }
"-=" { SET_POS(2); yylval.integer = ASUB; return ASSIGN; }
"*=" { SET_POS(2); yylval.integer = AMUL; return ASSIGN; }
"/=" { SET_POS(2); yylval.integer = ADIV; return ASSIGN; }
"&=" { SET_POS(2); yylval.integer = AAND; return ASSIGN; }
"|=" { SET_POS(2); yylval.integer = AOR;  return ASSIGN; }
"="  { SET_POS(1); yylval.integer = '=';  return ASSIGN; }
"++" { SET_POS(2); yylval.integer = INC;  return SUFFIX; }
"--" { SET_POS(2); yylval.integer = DEC;  return SUFFIX; }

"::" {
  SET_POS(2);
  return NS;
}
  

";"|","|"@"|"("|")"|"["|"]"|"{"|"}"|"?"|":"|"+"|"-"|"*"|"/"|"&"|"|"|"<"|">"|"!" {
  // all special char, ponctuation and operators
  SET_POS(1);
  yylval.integer = yytext[0];
  return yytext[0];
}

\" {
  BEGIN(STR);
  // init the string
  str_buf_len = 1024;
  str_buf = malloc(str_buf_len);
  str_buf_pos = 0;
  // count the quote
  col++;
  // set the starter
  str_first_line = line;
  str_first_column = col;
}
      
{DIGIT}+|"0x"{XDIGIT}+ {
  int l = strlen(yytext);
  yylval.integer = (int)strtol(yytext,NULL,0);

  SET_POS(l);
  return INTEGER;
}
         
"bit" {
  SET_POS(3);
  yylval.integer = SCC_VAR_BIT;
  return TYPE;
}

"int" {
  SET_POS(3);
  yylval.integer = SCC_VAR_WORD;
  return TYPE;
}

"array" {
  SET_POS(5);
  yylval.integer = SCC_VAR_ARRAY;
  return TYPE;
}

"if" {
  SET_POS(2);
  yylval.integer = 0;
  return IF;
}

"else" {
  SET_POS(4);
  return ELSE;
}

"unless" {
  SET_POS(6);
  yylval.integer = 1;
  return IF;
}

"for" {
  SET_POS(3);
  return FOR;
}

"while" {
  SET_POS(5);
  yylval.integer = 0;
  return WHILE;
}

"do" {
  SET_POS(2);
  return DO;
}

"until" {
  SET_POS(5);
  yylval.integer = 1;
  return WHILE;
}

"switch" {
  SET_POS(6);
  return SWITCH;
}

"case" {
  SET_POS(4);
  return CASE;
}

"default" {
  SET_POS(7);
  return DEFAULT;
}

"break" {
  SET_POS(5);
  yylval.integer = SCC_BRANCH_BREAK;
  return BRANCH;
}

"continue" {
  SET_POS(8);
  yylval.integer = SCC_BRANCH_CONTINUE;
  return BRANCH;
}

"cutscene" {
  SET_POS(8);
  return CUTSCENE;
}

"script" {
  SET_POS(6);
  return SCRIPT;
}

"room" {
  SET_POS(4);
  return ROOM;
}

"object" {
  SET_POS(6);
  return OBJECT;
}

"cost" {
  SET_POS(4);
  yylval.integer = SCC_RES_COST;
  return RESTYPE;
}

"sound" {
  SET_POS(5);
  yylval.integer = SCC_RES_SOUND;
  return RESTYPE;
}

"chset" {
  SET_POS(5);
  yylval.integer = SCC_RES_CHSET;
  return RESTYPE;
}

"actor" {
  SET_POS(5);
  return ACTOR;
}

"verb" {
  SET_POS(4);
  return VERB;
}

"local" {
  SET_POS(5);
  yylval.integer = SCC_RES_LSCR;
  return SCRTYPE;
}

"global" {
  SET_POS(6);
  yylval.integer = SCC_RES_SCR;
  return SCRTYPE;
}

"NORTH" {
  SET_POS(5);
  yylval.integer = 3;
  return INTEGER;
}

"EAST" {
  SET_POS(4);
  yylval.integer = 1;
  return INTEGER;
}

"SOUTH" {
  SET_POS(5);
  yylval.integer = 2;
  return INTEGER;
}

"WEST" {
  SET_POS(4);
  yylval.integer = 0;
  return INTEGER;
}


{ID}    {
  int l = strlen(yytext);
  //printf( "An identifier: %s\n", yytext );
  yylval.str = (char*)strdup(yytext);
  SET_POS(l);
  return SYM;
}

"//"[^\n]*"\n"  {   /* eat up one-line comments */
  col = 0;
  line++;
}

"/*" {
  BEGIN(COMMENT);
  col += 2;
}
      
[ \t]+  {         /* eat up whitespace */
  // we must perhaps do better with the \t
  col += strlen(yytext);
}

[\n]+ {
  col = 0;
  line += strlen(yytext);
}
      
.         {
  printf( "Unrecognized character: %s\n", yytext );
  return ERROR;
}

<STR>{

  \" {
    BEGIN(INITIAL);
    // If that is true an empty string follow, we can drop it
    if(strvar) {
      yylval.strvar = strvar;
      yylloc.first_line = strvar_first_line;
      yylloc.first_column = strvar_first_column;
      yylloc.last_line = strvar_last_line;
      yylloc.last_column = strvar_last_column;
      strvar = NULL;

      // count the quote
      col++;

      free(str_buf);
      str_buf = NULL;
      str_buf_pos = str_buf_len = 0;
      return STRVAR;
    }

    // return the string
    str_buf[str_buf_pos] = '\0';
    yylval.str = str_buf;
    yylloc.first_line = str_first_line;
    yylloc.first_column = str_first_column;
    yylloc.last_line = line;
    yylloc.last_column = col;

    str_buf = NULL;
    str_buf_pos = str_buf_len = 0;

    // count the quote
    col++;
    
    return STRING;
  }

  "%"[i|v|n|s|f|c]"{"({ID}|[0-2]?[0-9]?[0-9]|"0x"{XDIGIT}?{XDIGIT})"}" {
    int len;
    scc_str_t* s;

    len = strlen(yytext) - 4;
    s = calloc(1,sizeof(scc_str_t));
    s->str = malloc(len+1);
    memcpy(s->str,yytext+3,len);
    s->str[len] = '\0';

    switch(yytext[1]) {
    case 'i':
      s->type = SCC_STR_INT;
      break;
    case 'v':
      s->type = SCC_STR_VERB;
      break;
    case 'n':
      s->type = SCC_STR_NAME;
      break;
    case 's':
      s->type = SCC_STR_STR;
      break;
    case 'f':
      s->type = SCC_STR_FONT;
      break;
    case 'c': {
      int val  = strtol(s->str,NULL,0);
      if(val < 0 || val > 255) {
	printf("We got an unvalid color number.\n");
	return ERROR;
      }
      s->type = SCC_STR_COLOR;
      s->str = realloc(s->str,2);
      SCC_AT_16(s->str,0) = val;
    } break;
    }

    /* last token was an strvar too */
    if(strvar) {
      // return the last strvar
      yylval.strvar = strvar;
      yylloc.first_line = strvar_first_line;
      yylloc.first_column = strvar_first_column;
      yylloc.last_line = strvar_last_line;
      yylloc.last_column = strvar_last_column;

      // store the new one
      strvar = s;
      strvar_first_line = line;
      strvar_first_column = col;
      col += strlen(yytext);
      strvar_last_line = line;
      strvar_last_column = col;
      // put the string starter
      str_first_line = line;
      str_first_column = col;
      
      return STRVAR;
    }

    // last token was a plain normal string

    // empty string, so return the strvar directly
    if(str_buf_pos == 0) {
      yylval.strvar = s;
      yylloc.first_line = line;
      yylloc.first_column = col;
      col += strlen(yytext);
      yylloc.last_line = line;
      yylloc.last_column = col;

      // set the string starter
      str_first_line = line;
      str_first_column = col;

      return STRVAR;
    }

    // store the new strvar
    strvar_first_line = line;
    strvar_first_column = col;
    strvar_last_line = line;
    strvar_last_column = col+strlen(yytext);
    strvar = s;

    // finish the string
    str_buf[str_buf_pos] = '\0';
    yylval.str = str_buf;
    yylloc.first_line = str_first_line;
    yylloc.first_column = str_first_column;
    yylloc.last_line = line;
    yylloc.last_column = col;

    // count the strvar
    col += strlen(yytext);

    str_buf = malloc(1024);
    str_buf_pos = 0;
    str_buf_len = 1024;
    // set the string starter
    str_first_line = line;
    str_first_column = col;

    return STRING;
  }

  \\\" {
    str_buf[str_buf_pos] = '"';
    str_buf_pos++;
    if(str_buf_pos >= str_buf_len) {
      str_buf_len += 1024;
      str_buf = realloc(str_buf,str_buf_len);
    }
    col += 2;
  }

  "\\n" {
    // new line
    str_buf_add_code(1);
    col += 2;
  }

  "\\k" {
    // keep text
    str_buf_add_code(2);
    col += 2;
  }

  "\\w" {
    // wait
    str_buf_add_code(3);
    col += 2;
  }

  "\n" {
    str_buf[str_buf_pos] = '\0';
    printf("Unterminated string: %s\n",str_buf);
    free(str_buf);
    str_buf_pos = str_buf_len = 0;

    return ERROR;
  }

  [^\\\n\\\"] {
    int len = strlen(yytext);

    if(str_buf_pos+len >= str_buf_len) {
      while(str_buf_pos+len >= str_buf_len)
	str_buf_len += 1024;
      str_buf = realloc(str_buf,str_buf_len);
    }

    memcpy(&str_buf[str_buf_pos],yytext,len);
    str_buf_pos += len;
    
    col += len;

    if(strvar) {
      yylval.strvar = strvar;
      yylloc.first_line = strvar_first_line;
      yylloc.first_column = strvar_first_column;
      yylloc.last_line = strvar_last_line;
      yylloc.last_column = strvar_last_column;
      strvar = NULL;
      return STRVAR;
    }
  }

}

<COMMENT>{

    "*/" {
        BEGIN(INITIAL);
        col += 2;
    }

    [\n]+ {
        col = 0;
        line += strlen(yytext);
    }

    . {
        col++;
    }

}
      
%%

char* scc_lex_get_file(void) {
  static char name[2048];

  if(inc_ptr > 0) {
    sprintf(name,"%s included in %s at line %d",
	    inc_stack[inc_ptr].name,
	    inc_stack[inc_ptr-1].name,
	    inc_stack[inc_ptr-1].line);
    return name;
  }
  return inc_stack[inc_ptr].name;
}

int scc_lex_init(char* file) {
  
  if(inc_ptr != 0) {
    printf("This should never happend :)\n");
    return 0;
  }

  yyin = fopen(file,"r");

  if(!yyin) {
    printf("Failed to open %s.\n",file);
    return 0;
  }

  inc_stack[0].name = strdup(file);
  
  return 1;
}

int yywrap(void) {
  return 1;
}

#if 0
int main( int argc, char** argv ) {
  ++argv, --argc;  /* skip over program name */
  if ( argc > 0 ) yyin = fopen( argv[0], "r" );
  else yyin = stdin;
  
  yylex();
}
#endif
