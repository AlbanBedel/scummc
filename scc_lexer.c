
#include "config.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "scc_util.h"
#include "scc_parse.h"
#include "scc_parse.tab.h"
#include "scc_lex.h"


typedef struct scc_keyword {
    char* name;
    int type, val;
} scc_keyword_t;

// List of all the keywords
// it must be kept sorted bcs a binary search is used on it
static scc_keyword_t scc_keywords[] = {
    { "EAST",       INTEGER,   1 },
    { "NORTH",      INTEGER,   3 },
    { "SOUTH",      INTEGER,   2 },
    { "WEST",       INTEGER,   0 },
    { "actor",      ACTOR,     -1 },
    { "bit",        TYPE,      SCC_VAR_BIT },
    { "break",      BRANCH,    SCC_BRANCH_BREAK },
    { "byte",       TYPE,      SCC_VAR_BYTE },
    { "case",       CASE,      -1 },
    { "char",       TYPE,      SCC_VAR_CHAR },
    { "chset",      RESTYPE,   SCC_RES_CHSET },
    { "class",      CLASS,     -1 },
    { "continue",   BRANCH,    SCC_BRANCH_CONTINUE },
    { "cost",       RESTYPE,   SCC_RES_COST },
    { "cutscene",   CUTSCENE,  -1 },
    { "cycle",      CYCL,      -1 },
    { "default",    DEFAULT,   -1 },
    { "do",         DO,        -1 },
    { "else",       ELSE,      -1 },
    { "for",        FOR,       -1 },
    { "global",     SCRTYPE,   SCC_RES_SCR },
    { "if",         IF,        0 },
    { "int",        TYPE,      SCC_VAR_WORD },
    { "local",      SCRTYPE,   SCC_RES_LSCR },
    { "nibble",     TYPE,      SCC_VAR_NIBBLE },
    { "object",     OBJECT,    -1 },
    { "override",   OVERRIDE,  -1 },
    { "return",     BRANCH,    SCC_BRANCH_RETURN },
    { "room",       ROOM,      -1 },
    { "script",     SCRIPT,    -1 },
    { "sound",      RESTYPE,   SCC_RES_SOUND },
    { "switch",     SWITCH,    -1 },
    { "try",        TRY,       -1 },
    { "unless",     IF,        1 },
    { "until",      WHILE,     1 },
    { "verb",       VERB,     -1 },
    { "voice",      VOICE,     -1 },
    { "while",      WHILE,     0 },
    { "word",       TYPE,      SCC_VAR_WORD },
    { NULL, -1, -1 },
};

// Do a binary search of the given (sorted) keyword array
static scc_keyword_t* scc_is_keyword(char* s,scc_keyword_t* kw,unsigned num_kw) {
    unsigned min = 0,max,m;
    int c;

    if(num_kw < 1) return NULL;
    max = num_kw-1;

    while(1) {
        // check the middle element
        m = (max + min) >> 1;
        c = strcmp(kw[m].name,s);
        // ok
        if(c == 0) return &kw[m];
        // we are at the last step, check max if it doesn't match
        if(m == min)
            return m != max ?
                (strcmp(kw[max].name,s) ?
                 NULL : &kw[max]) : 0;
        // move in the array
        if(c < 0) min = m;
        else max = m;
    }
}



#define CHECK_EOF(lx,ch)                               \
        if(ch == 0) {                                  \
            if(!lx->error)                             \
                scc_lex_error(lx,"Unexpected eof.");   \
            return 0;                                  \
        }


static int scc_string_var_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    char c,d;
    char* esc = "%ivVnsfc";
    int tpos = 0;
    scc_str_t* str;

    scc_lex_pop_lexer(lex);

    c = scc_lex_getc(lex); // eat the %
    CHECK_EOF(lex,c);

    c = scc_lex_at(lex,0); // get the command
    CHECK_EOF(lex,c);

    // check that it's a known escape
    if(!strchr(esc,c))
        return 0;
    else
        scc_lex_getc(lex); 

    if(c == '%') {
        lvalp->str = strdup("%");
        d = scc_lex_at(lex,0);
        // little hack to get rid of the usless empty string
        if(d == '"') {
            scc_lex_getc(lex);
            scc_lex_pop_lexer(lex);
        }
        return STRING;
    }

    d = scc_lex_at(lex,0);
    CHECK_EOF(lex,d);

    if(d != '{') return 0;
    // read the word
    do {
        tpos++;
        d = scc_lex_at(lex,tpos);
        if(!d) {
            if(lex->error) return 0;
            break;
        }
    } while(SCC_ISALNUM(d) || d == '_');
    
    if(d != '}') return 0;
    
    lvalp->strvar = str = calloc(1,sizeof(scc_str_t));
    // read out the string
    scc_lex_getc(lex); // {
    str->str = scc_lex_gets(lex,tpos-1);
    scc_lex_getc(lex); // }
    
    switch(c) {
    case 'i':
        str->type = SCC_STR_INT;
        break;
    case 'v':
        str->type = SCC_STR_VERB;
        break;
    case 'V':
        str->type = SCC_STR_VOICE;
        break;
    case 'n':
        str->type = SCC_STR_NAME;
        break;
    case 's':
        str->type = SCC_STR_STR;
        break;
    case 'f':
        str->type = SCC_STR_FONT;
        break;
    case 'c':  {
        char* end;
        int val  = strtol(str->str,&end,0);
        if(end[0] != '\0' || val < 0 || val > 255) {
            scc_lex_error(lex,"Invalid color number: %s\n",str->str);
            return 0;
        }
        str->type = SCC_STR_COLOR;
        str->str = realloc(str->str,2);
        SCC_SET_16LE(str->str,0,val);
    } break;
    }

    d = scc_lex_at(lex,0);
    if(d == '"') {
        scc_lex_getc(lex);
        scc_lex_pop_lexer(lex);
    }

    return STRVAR;
}

static void scc_unescape_string(char* str) {
    int esc = 0,i;
    
    for(i = 0 ; str[i] != '\0' ; i++) {
        if(esc) {
            switch(str[i]) {
            case 'n':
                str[i-1] = 0xFF;
                str[i] = 1;
                break;
            case 'k':
                str[i-1] = 0xFF;
                str[i] = 2;
                break;
            case 'w':
                str[i-1] = 0xFF;
                str[i] = 3;
                break;
            case '\\':
            case '\"':
                memmove(str+i-1,str+i,strlen(str+i)+1);
                i--;
                break;
            default:
                // TODO warn
                break;
            }
            esc = 0;
            continue;
        }
        if(str[i] == '\\') esc = 1;
    }

}

static int scc_string_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    unsigned pos;
    char c;

    for(pos = 0 ; 1 ; pos++) {
        c = scc_lex_at(lex,pos);
        CHECK_EOF(lex,c);
        if(c == '\\') { // skip escape
            pos++;
            continue;
        }
        if(c == '"' || c == '%') break;
    }
    
    // finished with a % escape
    if(c == '%') {
        scc_lex_push_lexer(lex,scc_string_var_lexer);
        // some string first ?
        if(pos > 0) {
            lvalp->str = scc_lex_gets(lex,pos);
            //scc_lex_getc(lex); // eat the %
            scc_unescape_string(lvalp->str);
            return STRING;
        }
        // directly switch to the string var lexer
        //scc_lex_getc(lex); // eat the %
        return 0;
    }

    
    lvalp->str = scc_lex_gets(lex,pos);
    scc_lex_getc(lex); // eat the closing "
    scc_unescape_string(lvalp->str);
    // switch back to the previous parser
    scc_lex_pop_lexer(lex);
    return STRING;    
}

static int scc_preproc_lexer(scc_lex_t* lex,char* ppd,char* arg) {
    int len;

    if(!strcmp(ppd,"include")) {
        if(!arg || (len = strlen(arg)) < 3 || (arg[0] != '"' && arg[0] != '<') ||
           arg[len-1] != (arg[0] == '"' ? '"' : '>')) {
            scc_lex_error(lex,"Expecting include <FILENAME> or \"FILENAME\"");
            return 0;
        }
        arg[len-1] = '\0';
        arg++;
        return scc_lex_push_buffer(lex,arg) ? -1 : 0;
    }

    //printf("Got preproc: '%s' -> '%s'\n",ppd,arg);
    scc_lex_error(lex,"Unknow preprocessor directive %s",ppd);
    return 0;
}


int scc_main_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    char c,d;
    char* str,*ppd;
    int tpos = 0,pos = 0;
    scc_keyword_t* keyword;

    c = scc_lex_at(lex,0);

    // EOF
    if(!c) return 0;

    //scc_lex_get_line_column(lex,&llocp->first_line,&llocp->first_column);

    // a sym, a keyword or an integer
    if(SCC_ISALNUM(c) || c == '_') {
        // read the whole word
        do {
            tpos++;
            c = scc_lex_at(lex,tpos);
            if(!c) {
                if(lex->error) return 0;
                break;
            }
        } while(SCC_ISALNUM(c) || c == '_');
        // Get the string
        str = scc_lex_gets(lex,tpos);

        // Is it an integer ?
        if(SCC_ISDIGIT(str[0])) {
            char* end;
            lvalp->integer = strtol(str,&end,0);
            if(end[0] != '\0') {
                scc_lex_error(lex,"Failed to parse integer: '%s'",str);
                free(str);
                return 0;
            }
            free(str);
            return INTEGER;
        }

        // Is it a keyword ?
        if((keyword = scc_is_keyword(str,scc_keywords,
                                     sizeof(scc_keywords)/sizeof(scc_keyword_t)-1))) {
            free(str);
            if(keyword->val >= 0)
                lvalp->integer = keyword->val;
            return keyword->type;
        }
        // then it's symbol
        lvalp->str = str;
        return SYM;
    }

    scc_lex_getc(lex);

// && &= & || |= | ++ += + -- -= -
#define OP_OPEQ_OQDBL(op,eq_val,dbl_type,dbl_val)  \
        d = scc_lex_at(lex,0);                     \
        CHECK_EOF(lex,d);                          \
        if(d == op) {                              \
            scc_lex_getc(lex);                     \
            lvalp->integer = dbl_val;              \
            return dbl_type;                       \
        } else if(d == '=') {                      \
            scc_lex_getc(lex);                     \
            lvalp->integer = eq_val;               \
            return ASSIGN;                         \
        }                                          \
        lvalp->integer = op;                       \
        return op


// == = != ! >= > <= < *= *
#define OP_OPEQ(op,eq_type,eq_val)                 \
        d = scc_lex_at(lex,0);                     \
        CHECK_EOF(lex,d);                          \
        if(d == '=') {                             \
            scc_lex_getc(lex);                     \
            lvalp->integer = eq_val;               \
            return eq_type;                        \
        }                                          \
        lvalp->integer = op;                       \
        return op                                  \


    switch(c) {
        // && &= &
    case '&':
        OP_OPEQ_OQDBL('&',AAND,LAND,LAND);

        // || |= |
    case '|':
        OP_OPEQ_OQDBL('|',AOR,LOR,LOR);

        // ++ += +
    case '+':
        OP_OPEQ_OQDBL('+',AADD,SUFFIX,INC);

        // -- -= -
    case '-':
        OP_OPEQ_OQDBL('-',ASUB,SUFFIX,DEC);
        break;

    case '=':
        d = scc_lex_at(lex,0);
        CHECK_EOF(lex,d);
        if(d == '=') {
            scc_lex_getc(lex);
            lvalp->integer = EQ;
            return EQ;
        }
        lvalp->integer = '=';
        return ASSIGN;

        // != !
    case '!':
        OP_OPEQ('!',NEQ,NEQ);

        // >= >
    case '>':
        OP_OPEQ('>',GE,GE);

        // <= <
    case '<':
        OP_OPEQ('<',LE,LE);

        // *= *
    case '*':
        OP_OPEQ('*',ASSIGN,AADD);

        // /= // /* // /
    case '/':
        d = scc_lex_at(lex,0);
        if(d == '=') {  // divide and assign
            scc_lex_getc(lex);
            lvalp->integer = ADIV;
            return ASSIGN;
        } else if(d == '/') { // single line comments
            tpos = scc_lex_strchr(lex,1,'\n');
            if(tpos > 0)
                scc_lex_drop(lex,tpos+1);
            return -1;
        } else if(d == '*') { // multi line comments
            do {
                tpos = scc_lex_strchr(lex,tpos+1,'/');
                if(tpos < 0) {
                    // TODO warn instead
                    if(!lex->error)
                        scc_lex_error(lex,"Unfinished multi line comment");
                    return 0;
                }
            } while(scc_lex_at(lex,tpos-1) != '*');
            scc_lex_drop(lex,tpos+1);
            return -1;
        }
        // divide
        lvalp->integer = '/';
        return '/';

        // :: :
    case ':':
        d = scc_lex_at(lex,0);
        if(d == ':') {
            scc_lex_getc(lex);
            return NS;
        }
        lvalp->integer = ':';
        return ':';

        // ; , @ ( ) [ ] { } ? 
    case ';':
    case ',':
    case '@':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '?':
        lvalp->integer = c;
        return c;

        // single chars
    case '\'':
        c = scc_lex_getc(lex);
        if(c == '\\')
            c = scc_lex_getc(lex);
        CHECK_EOF(lex,c);
        d = scc_lex_getc(lex);
        CHECK_EOF(lex,d);
        if(d != '\'') {
            scc_lex_error(lex,"Unterminated charcter constant.");
            return 0;
        }
        lvalp->integer = c;
        return INTEGER;

        // strings
    case '"':
        scc_lex_push_lexer(lex,scc_string_lexer);
        return 0;

        // pre precessor
    case '#':
        // read the directive name
        c = scc_lex_at(lex,tpos);
        CHECK_EOF(lex,c);
        // skip initial spaces
        while(c == ' ' || c == '\t') {
            tpos++;
            c = scc_lex_at(lex,tpos);
            CHECK_EOF(lex,c);
        }
        // check that the first char is an alpha
        if(!SCC_ISALPHA(c)) {
            scc_lex_error(lex,"Invalid preprocessor directive.");
            return 0;
        }
        // drop the spaces
        scc_lex_drop(lex,tpos);
        // read the name
        tpos = 0;
        do {
            tpos++;
            c = scc_lex_at(lex,tpos);
            CHECK_EOF(lex,c);
        } while(SCC_ISALPHA(c));
        // check we ended on a space
        if(c != ' ' && c != '\t' && c != '\n') {
            scc_lex_error(lex,"Invalid preprocessor directive.");
            return 0;
        }
        // read the directive
        ppd = scc_lex_gets(lex,tpos);

        str = NULL;
        while(1) {
            // skip spaces
            tpos = 0;
            while(c == ' ' || c == '\t') {
                tpos++;
                c = scc_lex_at(lex,tpos);
                CHECK_EOF(lex,c);
            }
            // only space, that's it for now
            if(c == '\n') {
                scc_lex_drop(lex,tpos+1);
                 break;
            } else
                scc_lex_drop(lex,tpos);

            // find the eol
            tpos = scc_lex_strchr(lex,0,'\n');
            if(tpos < 0) { // we should do better here
                scc_lex_error(lex,"Unxpected eof");
                return 0;
            }
            // count the leading spaces
            pos = tpos;
            do {
                pos--;
                c = scc_lex_at(lex,pos);
                CHECK_EOF(lex,c);
            } while(c == ' ' || c == '\t');
            // not finished on a \, we are done
            if(c != '\\') {
                str = scc_lex_strcat(lex,str,pos+1);
                break;
            }
            // read that line
            str = scc_lex_strcat(lex,str,pos);
            // and drop the \ and the spaces
            scc_lex_drop(lex,tpos+1-pos);
        }

        // call the preproc
        tpos = scc_preproc_lexer(lex,ppd,str);
        free(ppd);
        if(str) free(str);
        return tpos;

    case ' ':
    case '\n':
    case '\t':
        do {
            c = scc_lex_at(lex,tpos);
            tpos++;
        } while (c == ' ' || c == '\n' || c == '\t');
        scc_lex_drop(lex,tpos-1);
        return -1;
            

    default:
        scc_lex_error(lex,"Unrecognized character: '%c'",c);
        return 0;
    }
    
}

#ifdef LEX_TEST

#include <stdio.h>
#include <ctype.h>

static void set_start_pos(YYLTYPE *llocp,int line,int column) {
    llocp->first_line = line+1;
    llocp->first_column = column;
}

static void set_end_pos(YYLTYPE *llocp,int line,int column) {
    llocp->last_line = line+1;
    llocp->last_column = column;
}

int main(int argc,char** argv) {
    int i,tok;
    scc_lex_t* lex = scc_lex_new(scc_main_lexer,set_start_pos,set_end_pos);
    YYSTYPE val;
    YYLTYPE loc;

    for(i = 1 ; i < argc ; i++) {
        scc_lex_push_buffer(lex,argv[i]);
        while((tok = scc_lex_lex(&val,&loc,lex))) {
            switch(tok) {
            case SYM:
                printf("Sym: %s [%d:%d][%d:%d]\n",val.str,loc.first_line,
                       loc.first_column,loc.last_line,
                       loc.last_column);
                free(val.str);
                break;
            case STRING:
                printf("String: '%s' [%d:%d][%d:%d]\n",val.str,loc.first_line,
                       loc.first_column,loc.last_line,
                       loc.last_column);
                free(val.str);
                break;
            default:
                if(isgraph(tok))
                    printf("Token: %c (%d) [%d:%d][%d:%d]\n",tok,tok,loc.first_line,
                           loc.first_column,loc.last_line,
                           loc.last_column);
                else
                    printf("Token: %d [%d:%d][%d:%d]\n",tok,loc.first_line,
                           loc.first_column,loc.last_line,
                           loc.last_column);
            }
        }
        if(lex->error) {
            printf("Lex error: %s\n",lex->error);
            scc_lex_clear_error(lex);
        }
    }
    return 0;
}
        
#endif
