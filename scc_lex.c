/* ScummC
 * Copyright (C) 2005-2006  Alban Bedel
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
 *  @file scc_lex.c
 *  @ingroup lex
 *  @brief Base class to implement lexers.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "scc_util.h"
#include "scc_fd.h"
#include "scc_lex.h"

/// Size of the blocks read by the lexbuf.
#define SCC_LEX_BLOCK_SIZE 1024

/** @brief Lexer buffer, the lexer input.
 *
 * The lexbuf take care of reading the data from the input
 * files and tracking the current position (in term of lines
 * and columns) in the input file.
 */
struct scc_lexbuf {
    /// Next buffer.
    scc_lexbuf_t* next;
    /// Input fd
    scc_fd_t* fd;
    /// Did we read all the data out of the fd
    char eof;
    /// Buffer
    char* data;
    /// Current position in the buffer
    unsigned data_pos;
    /// Length of the data contained in the buffer
    unsigned data_len;
    /// Size allocated for the buffer
    unsigned data_size;
    /// Current line
    int line;
    /// Current column
    int column;
    /// Current filename
    char* filename;
};

/// Lexer function stack.
struct scc_lexer {
    scc_lexer_t* next;
    scc_lexer_f lex;
};

/// Define definition
struct scc_define {
    /// Defined name
    char* name;
    /// Defined value
    char* value;
    /// File where it was defined
    char* filename;
    /// Line where it was defined
    int line;
    /// Column where it was defined
    int column;
};

scc_lex_t* scc_lex_new(scc_lexer_f lexer,scc_lexer_pos_f set_start_pos,
                       scc_lexer_pos_f set_end_pos, char** include) {
    scc_lex_t* lex = calloc(1,sizeof(scc_lex_t));
    scc_lex_push_lexer(lex,lexer);
    lex->set_start_pos = set_start_pos;
    lex->set_end_pos = set_end_pos;
    lex->include = include;
    return lex;
}

// Front end
int scc_lex_lex(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex) {
    scc_lexer_t* lexer;
    int r;
    char set_start = 1;

    if(lex->error) return 0;

    // check that we have both a buffer and a lexer
    if(!lex->buffer) {
        scc_lex_error(lex,"No input have been setup");
        return 0;
    }

    do {
        // save the current lexer
        if(!(lexer = lex->lexer)) {
            scc_lex_error(lex,"Lexer error, no lexer function");
            return 0;
        }
        // set token start
        if(lex->set_start_pos && set_start)
            lex->set_start_pos(llocp,lex->buffer->line,lex->buffer->column);
        // call it
        r = lexer->lex(lvalp,llocp,lex);
        // we got an error or a token give it back
        if(lex->error) return 0;
        // we got an "empty" token, continue
        if(r < 0) {
            set_start = 1;
            continue;
        }
        // we got a token, return it
        if(r) {
            if(lex->set_end_pos)
                lex->set_end_pos(llocp,lex->buffer->line,lex->buffer->column);
            return r;
        }
        // no token back, if we switched the lexer continue
        // with the next one (or detect a missing one)
        if(lex->lexer != lexer) {
            set_start = 0;
            continue;
        }
        // otherwise it's an eof pop the buffer
        scc_lex_pop_buffer(lex);
        set_start = 1;
    } while(lex->buffer);

    return 0;
}

char* scc_lex_get_file(scc_lex_t* lex) {
    char *str,*tmp;
    scc_lexbuf_t* buf;

    if(!(buf = lex->buffer)) return NULL;

    // not included, just return name:line
    if(!buf->next) {
        asprintf(&str,"%s:%d",buf->filename,buf->line);
        return str;
    }

    // It was included, print from where it was included
    buf = buf->next;
    asprintf(&tmp,"In file included from %s:%d:\n",
             buf->filename,buf->line);
    // add the included includes
    while((buf = buf->next)) {
        str = tmp;
        asprintf(&tmp,"%s                 from %s:%d:\n",
                 str,buf->filename,buf->line);
        free(str);
    }
    // and finnaly the current file:line
    buf = lex->buffer;
    asprintf(&str,"%s%s:%d",tmp,buf->filename,buf->line);
    free(tmp);

    return str;
}


// Internal stuff for the lexers

// put an error
void scc_lex_error(scc_lex_t* lex,char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if(lex->error) {
        printf("Warning the lexer already have a pending error. Can't add:\n");
        vprintf(fmt,ap);
    } else {
        if(vasprintf(&lex->error,fmt,ap) < 0) {
            lex->error = NULL;
            printf("Warning the lexer failed to allocate mem for error message:\n");
            vprintf(fmt,ap);
        }
    }
    va_end(ap);
}

void scc_lex_clear_error(scc_lex_t* lex) {
    if(!lex->error) return;
    free(lex->error);
    lex->error = NULL;
}

// switch to another input file
int scc_lex_push_buffer(scc_lex_t* lex,char* file) {
    scc_fd_t* fd = NULL;
    scc_lexbuf_t* buf;
    // try in the include paths if any
    if(lex->include) {
        int i;
        for(i = 0 ; lex->include[i] ; i++) {
            int l = strlen(lex->include[i]) + 1 + strlen(file) + 1;
            char name[l];
            sprintf(name,"%s/%s",lex->include[i],file);
            fd = new_scc_fd(name,O_RDONLY,0);
            if(fd) break;
        }
    }
    // try the filename alone
    if(!fd && !(fd = new_scc_fd(file,O_RDONLY,0))) {
        scc_lex_error(lex,"Failed to open %s: %s",file,strerror(errno));
        return 0;
    }
    buf = calloc(1,sizeof(scc_lexbuf_t));
    buf->fd = fd;
    buf->filename = strdup(fd->filename);
    buf->next = lex->buffer;
    lex->buffer = buf;

    if(lex->opened) lex->opened(lex->userdata,fd->filename);
    return 1;
}

// return to the last file
int scc_lex_pop_buffer(scc_lex_t* lex) {
    scc_lexbuf_t* buf;
    if(!(buf = lex->buffer)) return 0;
    if(buf->fd) scc_fd_close(buf->fd);
    if(buf->filename) free(buf->filename);
    if(buf->data) free(buf->data);
    if(buf->next)
        lex->buffer = buf->next;
    else
        lex->buffer = NULL;
    free(buf);
    return 1;
}

// Fill the buffer so that min_len bytes can be read
int scc_lex_fill_buffer(scc_lex_t* lex,unsigned min_len) {
    int len,r;
    scc_lexbuf_t* buf = lex->buffer;

    if(!(buf = lex->buffer)) return 0;

    while(1) {
        // safe guard
        if(buf->data_pos > buf->data_len) {
            printf("Warning bug in the lexer buffer: data_pos > data_len\n");
            buf->data_pos = buf->data_len;
        }
        // count how much data we have ahead
        len = buf->data_len - buf->data_pos;
        // already enouth data
        if(len >= min_len) return 1;
        // already at eof
        if(buf->eof) return 0;
        // we must read at least r
        r = min_len-len;
        // alloc the buffer
        if(!buf->data_size) {
            // round up to the next block size
            buf->data_size = r =((r+SCC_LEX_BLOCK_SIZE-1)/SCC_LEX_BLOCK_SIZE)*
                SCC_LEX_BLOCK_SIZE;
            buf->data = malloc(buf->data_size);
        } else {
            // move the data back to make some space
            // but keep at least SCC_LEX_MAX_UNPUT space for ungetc
            if(buf->data_pos > 0) {
                if(len)
                    memmove(buf->data,buf->data+buf->data_pos,len);
                buf->data_len = len;
                buf->data_pos = 0;
            }
            // we read at least a block
            if(r < SCC_LEX_BLOCK_SIZE) r = SCC_LEX_BLOCK_SIZE;
            // look if we need to resize
            if(buf->data_size < buf->data_len + r ||    // expand
               buf->data_size > buf->data_len + r + SCC_LEX_BLOCK_SIZE) { // shrink
                buf->data_size = buf->data_len + r;
                buf->data = realloc(buf->data,buf->data_size);
            }
        }
        // read
        r = scc_fd_read(buf->fd,buf->data+buf->data_len,r);
        // read error
        if(r < 0) {
            scc_lex_error(lex,"Error while reading %s: %s",
                          buf->fd->filename,strerror(errno));
            return 0;
        }
        // eof
        if(!r) {
            // mark the buffer as read
            buf->eof = 1;
            return 0;
        } else
            buf->data_len += r;
    }
}

int scc_lex_get_line_column(scc_lex_t* lex,int* line,int* column) {
    if(!lex->buffer) return 0;
    if(line) line[0] = lex->buffer->line+1;
    if(column) column[0] = lex->buffer->column-1;
    return 1;
}

// read a char but doesn't move the input stream
char scc_lex_at(scc_lex_t* lex,unsigned pos) {

    // check that we can read up to pos
    if(!scc_lex_fill_buffer(lex,pos+1)) return 0;

    return lex->buffer->data[lex->buffer->data_pos+pos];
}

// pump a char from the input stream
char scc_lex_getc(scc_lex_t* lex) {
    scc_lexbuf_t* buf;
    char c;

    // check that we can read
    if(!scc_lex_fill_buffer(lex,1)) return 0;

    buf = lex->buffer;
    c = buf->data[buf->data_pos];
    buf->data_pos++;
    if(c == '\n') {
        buf->column = 0;
        buf->line++;
    } else
        buf->column++;
    return c;
}

// get a string of length len from the input
char* scc_lex_gets(scc_lex_t* lex,unsigned len) {
    scc_lexbuf_t* buf = lex->buffer;
    char *str,*ptr,*nl;

    // check that we can read
    if(!scc_lex_fill_buffer(lex,len)) return 0;

    str = malloc(len+1);
    // copy from the buffer
    if(len > 0) {
        memcpy(str,buf->data+buf->data_pos,len);
        buf->data_pos += len;
    }
    str[len] = '\0';
    ptr = str;
    while((nl = strchr(ptr,'\n'))) {
        buf->column = 0;
        buf->line++;
        ptr = nl+1;
    }
    buf->column += strlen(ptr);
    return str;
}

// get a string of length len from the input
char* scc_lex_strcat(scc_lex_t* lex,char* str,unsigned len) {
    scc_lexbuf_t* buf = lex->buffer;
    char *ptr,*nl;
    int slen;

    // check that we can read
    if(!scc_lex_fill_buffer(lex,len)) return 0;

    if(str) {
        slen = strlen(str);
        str = realloc(str,slen+len+1);
    } else {
        slen = 0;
        str = malloc(len+1);
    }
    if(len > 0) {
        memcpy(str+slen,buf->data+buf->data_pos,len);
        buf->data_pos += len;
    }
    str[slen+len] = '\0';
    ptr = str + slen;
    while((nl = strchr(ptr,'\n'))) {
        buf->column = 0;
        buf->line++;
        ptr = nl+1;
    }
    buf->column += strlen(ptr);
    return str;
}

void scc_lex_drop(scc_lex_t* lex,unsigned n) {
    scc_lexbuf_t* buf = lex->buffer;
    int i;

    if(!n) return;

    // The data is in the buffer
    if(buf->data_pos+n > buf->data_len)
        scc_lex_fill_buffer(lex,n);

    if(buf->data_pos+n > buf->data_len)
        n = buf->data_len-buf->data_pos;

    for(i = 0 ; i < n ; i++) {
        if(buf->data[buf->data_pos+i] == '\n') {
            buf->column = 0;
            buf->line++;
        } else
            buf->column++;
    }
    buf->data_pos += n;
}

int scc_lex_strchr(scc_lex_t* lex,unsigned start,char c) {
    int r = start;
    scc_lexbuf_t* buf = lex->buffer;
    char *cptr,*ptr;

    do {
        // fill the buffer if needed
        if(buf->data_pos+r >= buf->data_len &&
           !scc_lex_fill_buffer(lex,r+1))
            return -1;
        ptr = buf->data+buf->data_pos;
        cptr = memchr(ptr+r,c,buf->data_len-buf->data_pos-r);
        r = buf->data_len-buf->data_pos;
    } while(!cptr);
    return cptr-ptr;
}

// Push a new lexer
int scc_lex_push_lexer(scc_lex_t* lex, scc_lexer_f lexf) {
    scc_lexer_t* lexer = malloc(sizeof(scc_lexer_t));

    lexer->next = lex->lexer;
    lexer->lex = lexf;
    lex->lexer = lexer;

    return 1;
}

// Pop a lexer
int scc_lex_pop_lexer(scc_lex_t* lex) {
    scc_lexer_t* lexer = lex->lexer;

    if(!lexer) return 0;

    lex->lexer = lexer->next;
    free(lexer);

    return 1;
}

void scc_lex_define(scc_lex_t* lex, char* name, char* val, int line, int col) {
    int i;

    for(i = 0 ; i < lex->num_define ; i++)
        if(!strcmp(name,lex->define[i].name)) break;

    if(i == lex->num_define) {
        lex->num_define++;
        lex->define = realloc(lex->define,lex->num_define*sizeof(scc_define_t));
    } else if(lex->define[i].value)
        free(lex->define[i].value);
    lex->define[i].name = strdup(name);
    lex->define[i].value = val ? strdup(val) : NULL;
    lex->define[i].filename = strdup(lex->buffer->filename);
    lex->define[i].line = line;
    lex->define[i].column = col;
}

int scc_lex_is_define(scc_lex_t* lex, char* name) {
    int i;
    for(i = 0 ; i < lex->num_define ; i++)
        if(!strcmp(name,lex->define[i].name)) return 1;
    return 0;
}

int scc_lex_expand_define(scc_lex_t* lex, char* name) {
    scc_lexbuf_t* buf;
    int i;
    for(i = 0 ; i < lex->num_define ; i++)
        if(!strcmp(name,lex->define[i].name)) break;
    if(i == lex->num_define)
        return 0;

    if(!lex->define[i].value) return 1;

    buf = calloc(1,sizeof(scc_lexbuf_t));
    buf->filename = strdup(lex->define[i].filename);
    buf->line = lex->define[i].line;
    buf->column = lex->define[i].column;
    buf->eof = 1;

    buf->data = strdup(lex->define[i].value);
    buf->data_len = strlen(buf->data);

    buf->next = lex->buffer;
    lex->buffer = buf;

    return 1;
}

// Do a binary search of the given (sorted) keyword array
scc_keyword_t* scc_is_keyword(char* s,scc_keyword_t* kw,unsigned num_kw) {
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
