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
 * @file cost_lexer.c
 * @ingroup lex
 * @brief Costume lexer.
 */

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include "scc_util.h"
#include "cost_parse.tab.h"
#include "scc_lex.h"

// List of all the keywords
// it must be kept sorted bcs a binary search is used on it
static scc_keyword_t cost_keywords[] = {
    { "EAST",       INTEGER,     1 },
    { "HIDE",       HIDE,       -1 },
    { "NORTH",      INTEGER,     3 },
    { "SKIP",       SKIP,       -1 },
    { "SOUND",      SOUND,      -1 },
    { "SOUTH",      INTEGER,     2 },
    { "START",      START,      -1 },
    { "STOP",       STOP,       -1 },
    { "WEST",       INTEGER,     0 },
    { "anim",       ANIM,       -1 },
    { "glob",       GLOB,       -1 },
    { "limb",       LIMB,       -1 },
    { "move",       MOVE,       -1 },
    { "palette",    PALETTE,    -1 },
    { "path",       PATH,       -1 },
    { "picture",    PICTURE,    -1 },
    { "position",   POSITION,   -1 },
    { NULL, -1, -1 }
};

#define CHECK_EOF(lx,ch)                               \
        if(ch == 0) {                                  \
            if(!lx->error)                             \
                scc_lex_error(lx,"Unexpected eof.");   \
            return 0;                                  \
        }


static void cost_unescape_string(char* str) {
    int esc = 0,i;
    
    for(i = 0 ; str[i] != '\0' ; i++) {
        if(esc) {
            switch(str[i]) {
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

static int cost_string_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    unsigned pos;
    char c;

    for(pos = 0 ; 1 ; pos++) {
        c = scc_lex_at(lex,pos);
        CHECK_EOF(lex,c);
        if(c == '\\') { // skip escape
            pos++;
            continue;
        }
        if(c == '"') break;
    }
    
    lvalp->str = scc_lex_gets(lex,pos);
    scc_lex_getc(lex); // eat the closing "
    cost_unescape_string(lvalp->str);
    // switch back to the previous parser
    scc_lex_pop_lexer(lex);
    return STRING;    
}


static int cost_preproc_lexer(scc_lex_t* lex,char* ppd,char* arg) {
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

    scc_lex_error(lex,"Unknow preprocessor directive %s",ppd);
    return 0;
}


int cost_main_lexer(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    char c,d;
    char* str,*ppd;
    int tpos = 0,pos = 0;
    scc_keyword_t* keyword;
    
    c = scc_lex_at(lex,0);

    // EOF
    if(!c) return 0;
    
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
        if((keyword = scc_is_keyword(str,cost_keywords,
                                     sizeof(cost_keywords)/sizeof(scc_keyword_t)-1))) {
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

    switch(c) {
        // Comments
    case '/':
        d = scc_lex_at(lex,0);
        if(d == '/') { // single line comments
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
        // Error
        break;
        
        // ; , @ ( ) [ ] { } + - =
    case ';':
    case ',':
    case '@':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '+':
    case '-':
    case '=':
        lvalp->integer = c;
        return c;
        
        // strings
    case '"':
        scc_lex_push_lexer(lex,cost_string_lexer);
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
                scc_lex_error(lex,"Unexpected eof");
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
        tpos = cost_preproc_lexer(lex,ppd,str);
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

    }
    
    scc_lex_error(lex,"Unrecognized character: '%c'",c);
    return 0;
}
