      
%{
  /* need this for the call to atof() below */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef YYSTYPE
typedef union {
  int integer;
  char* str;
} yystype;
# define YYSTYPE yystype
#endif

#include "cost_parse.tab.h"

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

 static int line=1,col=0;
 static int str_first_line=0,str_first_column=0;

#define SET_POS(len) yylloc.first_line = line;       \
                     yylloc.first_column = col;      \
                     yylloc.last_line = line;        \
                     yylloc.last_column = col+len-1; \
                     col += len

%}

%option stack

%x STR
%x INCLUDE

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


";"|","|"@"|"("|")"|"["|"]"|"{"|"}"|"+"|"-"|"=" {
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

"palette" {
  SET_POS(7);
  return PALETTE;
}

"limb" {
  SET_POS(4);
  return LIMB;
}

"picture" {
  SET_POS(7);
  return PICTURE;
}

"anim" {
  SET_POS(4);
  return ANIM;
}

"path" {
  SET_POS(4);
  return PATH;
}

"glob" {
  SET_POS(4);
  return GLOB;
} 

"position" {
  SET_POS(8);
  return POSITION;
}

"move" {
  SET_POS(4);
  return MOVE;
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

  \\\" {
    str_buf[str_buf_pos] = '"';
    str_buf_pos++;
    if(str_buf_pos >= str_buf_len) {
      str_buf_len += 1024;
      str_buf = realloc(str_buf,str_buf_len);
    }
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
  }

}
      
%%

char* cost_lex_get_file(void) {
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

int cost_lex_init(char* file) {
  
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
  
  return yylex();
}
#endif
