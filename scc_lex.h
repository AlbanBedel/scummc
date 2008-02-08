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

/// @defgroup lex Lexical parsing
/** 
 *  @file scc_lex.h
 *  @ingroup lex
 *  @brief Base class to implement lexers.
 */

// Get a default definition for YYSYPE and YYLTYPE.
#ifndef DOXYGEN_SKIP
#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#define YYSTYPE void
#endif

#if ! defined (YYLTYPE) && ! defined (YYLTYPE_IS_DECLARED)
#define YYLTYPE void
#endif
#endif

typedef struct scc_lexbuf scc_lexbuf_t;
typedef struct scc_lexer scc_lexer_t;
typedef struct scc_lex scc_lex_t;
typedef struct scc_define scc_define_t;
typedef struct scc_keyword scc_keyword_t;

/// Lexer function.
typedef int (*scc_lexer_f)(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex);
/// Callback to set YYLTYPE
typedef void (*scc_lexer_pos_f)(YYLTYPE *llocp,int line,int column);
/// Callback to track which files are opened.
typedef void (*scc_lexer_opened_f)(void* userdata,char* file);


/// The lexer class.
struct scc_lex {
    /// buffer stack
    scc_lexbuf_t* buffer;
    /// pending error
    char* error;
    /// lexer stack
    scc_lexer_t* lexer;
    /// position tracking
    scc_lexer_pos_f set_start_pos;
    scc_lexer_pos_f set_end_pos;
    /// include paths
    char** include;
    /// If set just continue when an include is missing
    int ignore_missing_include;
    /// callback to track deps
    scc_lexer_opened_f opened;
    void* userdata;
    /// Defines list
    scc_define_t* define;
    unsigned num_define;
};

/// Common struct to store compiler keywords.
struct scc_keyword {
    char* name;
    int type, val;
};


/// @name Public Functions
//@{

/// Create a lexer instance.
scc_lex_t* scc_lex_new(scc_lexer_f lexer,scc_lexer_pos_f set_start_pos,
                       scc_lexer_pos_f set_end_pos, char** include);

/// Return the next token setting lvalp and llocp
int scc_lex_lex(YYSTYPE *lvalp, YYLTYPE *llocp,scc_lex_t* lex);

/// Set a buffer to be read, buffers are automatically poped when eof is reached.
int scc_lex_push_buffer(scc_lex_t* lex,char* file);

/// Clear an error to resume parsing
void scc_lex_clear_error(scc_lex_t* lex);

/// Get the current file.
char* scc_lex_get_file(scc_lex_t* lex);

/// Get the current position.
int scc_lex_get_line_column(scc_lex_t* lex,int* line,int* column);

//@}


/// @name Lexer Functions
/// These functions are for the lexer implementations.
//@{

/// Set an error.
void scc_lex_error(scc_lex_t* lex,char* fmt, ...)  PRINTF_ATTRIB(2,3);

/// Read a char but doesn't move the input stream.
char scc_lex_at(scc_lex_t* lex,unsigned pos);

/// Pump a char from the input stream.
char scc_lex_getc(scc_lex_t* lex);

/// Get a string of length len from the input.
char* scc_lex_gets(scc_lex_t* lex,unsigned len);

/// Get a string of length len from the input and append it to str.
char* scc_lex_strcat(scc_lex_t* lex,char* str,unsigned len);

/// Put a char back in the input stream.
int scc_lex_ungetc(scc_lex_t* lex,char c);

/// Move the input stream forward of n chars.
void scc_lex_drop(scc_lex_t* lex,unsigned n);

/// Search for the char c starting from start.
int scc_lex_strchr(scc_lex_t* lex,unsigned start,char c);

/// Push a new lexer.
int scc_lex_push_lexer(scc_lex_t* lex, scc_lexer_f lexf);

/// Pop the current lexer.
int scc_lex_pop_lexer(scc_lex_t* lex);

/// Set a define
void scc_lex_define(scc_lex_t* lex, char* name, char* val, int line, int col);

/// Check if a symbol is a define
int scc_lex_is_define(scc_lex_t* lex, char* name);

/// Expand a define
int scc_lex_expand_define(scc_lex_t* lex, char* name);

/// Find a keyword, the array MUST be sorted.
scc_keyword_t* scc_is_keyword(char* s,scc_keyword_t* kw,unsigned num_kw);

//@}


/// @name Private Functions
//@{

/// Get back to the last file.
int scc_lex_pop_buffer(scc_lex_t* lex);

/// Fill the buffer so that min_len bytes can be read
int scc_lex_fill_buffer(scc_lex_t* lex,unsigned min_len);

//@}
