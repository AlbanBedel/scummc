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

/// @defgroup utils Common utilities
/**
 * @file scc_util.h
 * @ingroup utils
 * @brief Common stuff and portabilty helpers
 */


#define SCC_SWAP_16(x) ((((x)>>8)&0xFF)|(((x)<<8)&0xFF00))
#define SCC_SWAP_32(x) (  SCC_SWAP_16((uint16_t)(((x)>>16)&0xFFFF)) | (SCC_SWAP_16((uint16_t)(x))<<16) )

#ifdef IS_LITTLE_ENDIAN
#define SCC_TO_16BE(x) SCC_SWAP_16(x)
#define SCC_TO_32BE(x) SCC_SWAP_32(x)
#define SCC_TO_16LE(x) (x)
#define SCC_TO_32LE(x) (x)

#define MKID(a,b,c,d) ((uint32_t) \
			((a) & 0x000000FF) | \
			(((b) << 8) & 0x0000FF00) | \
			(((c) << 16) & 0x00FF0000) | \
			(((d) << 24) & 0xFF000000))

#define UNMKID(a) (a)&0xFF, ((a)>>8)&0xFF, ((a)>>16)&0xFF, ((a)>>24)&0xFF

#define SCC_GET_16(x,at) SCC_GET_16LE(x,at)
#define SCC_GET_32(x,at) SCC_GET_32LE(x,at)

#define SCC_SET_16(x,at,v) SCC_SET_16LE(x,at,v)
#define SCC_SET_32(x,at,v) SCC_SET_32LE(x,at,v)

#elif defined IS_BIG_ENDIAN
#define SCC_TO_16BE(x) (x)
#define SCC_TO_32BE(x) (x)
#define SCC_TO_16LE(x) SCC_SWAP_16(x)
#define SCC_TO_32LE(x) SCC_SWAP_32(x)

#define MKID(a,b,c,d) ((uint32_t) \
			((d) & 0x000000FF) | \
			(((c) << 8) & 0x0000FF00) | \
			(((b) << 16) & 0x00FF0000) | \
			(((a) << 24) & 0xFF000000))

#define UNMKID(a) ((a)>>24)&0xFF, ((a)>>16)&0xFF, ((a)>>8)&0xFF, (a)&0xFF

#define SCC_GET_16(x,at) SCC_GET_16BE(x,at)
#define SCC_GET_32(x,at) SCC_GET_32BE(x,at)

#define SCC_SET_16(x,at,v) SCC_SET_16BE(x,at,v)
#define SCC_SET_32(x,at,v) SCC_SET_32BE(x,at,v)

#else
#error "Endianness is not defined !!!"
#endif

#define SCC_SET_16LE(x,at,v)  { uint16_t tmp__ = (v); \
                                ((uint8_t*)((x)+(at)))[0] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>8) & 0xFF; }
#define SCC_SET_S16LE(x,at,v) { int16_t tmp__ = (v); \
                                ((uint8_t*)((x)+(at)))[0] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>8) & 0xFF; }

#define SCC_SET_16BE(x,at,v)  { uint16_t tmp__ = (v); \
                                ((uint8_t*)((x)+(at)))[1] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[0] = (tmp__>>8) & 0xFF; }
#define SCC_SET_S16BE(x,at,v) { int16_t tmp__ = (v); \
                                ((uint8_t*)((x)+(at)))[1] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[0] = (tmp__>>8) & 0xFF; }


#define SCC_SET_32LE(x,at,v)  { uint32_t tmp__ = (v); \
                                ((uint8_t*)((x)+(at)))[0] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>8) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[2] = (tmp__>>16) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[3] = (tmp__>>24) & 0xFF; }
#define SCC_SET_S32LE(x,at,v) { int32_t tmp__ = (v);  \
                                ((uint8_t*)((x)+(at)))[0] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>8) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[2] = (tmp__>>16) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[3] = (tmp__>>24) & 0xFF; }

#define SCC_SET_32BE(x,at,v)  { uint32_t tmp__ = (v);  \
                                ((uint8_t*)((x)+(at)))[3] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[2] = (tmp__>>8) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>16) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[0] = (tmp__>>24) & 0xFF; }
#define SCC_SET_S32BE(x,at,v) { int32_t tmp__ = (v);  \
                                ((uint8_t*)((x)+(at)))[3] = tmp__ & 0xFF; \
                                ((uint8_t*)((x)+(at)))[2] = (tmp__>>8) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[1] = (tmp__>>16) & 0xFF; \
                                ((uint8_t*)((x)+(at)))[0] = (tmp__>>24) & 0xFF; }


#define SCC_GET_16LE(x,at)  ((uint16_t) (  ((uint8_t*)((x)+(at)))[0] | \
                                          (((uint8_t*)((x)+(at)))[1] << 8) ) )
#define SCC_GET_S16LE(x,at)  ((int16_t) (  ((uint8_t*)((x)+(at)))[0] | \
                                          (((uint8_t*)((x)+(at)))[1] << 8) ) )

#define SCC_GET_16BE(x,at)  ((uint16_t) (  ((uint8_t*)((x)+(at)))[1] | \
                                          (((uint8_t*)((x)+(at)))[0] << 8) ) )
#define SCC_GET_S16BE(x,at)  ((int16_t) (  ((uint8_t*)((x)+(at)))[1] | \
                                          (((uint8_t*)((x)+(at)))[0] << 8) ) )

#define SCC_GET_32LE(x,at)  ((uint32_t) (  ((uint8_t*)((x)+(at)))[0] | \
                                          (((uint8_t*)((x)+(at)))[1] << 8) | \
                                          (((uint8_t*)((x)+(at)))[2] << 16) | \
                                          (((uint8_t*)((x)+(at)))[3] << 24) ) )
#define SCC_GET_S32LE(x,at)  ((int32_t) (  ((uint8_t*)((x)+(at)))[0] | \
                                          (((uint8_t*)((x)+(at)))[1] << 8) | \
                                          (((uint8_t*)((x)+(at)))[2] << 16) | \
                                          (((uint8_t*)((x)+(at)))[3] << 24) ) )

#define SCC_GET_32BE(x,at)  ((uint32_t) (  ((uint8_t*)((x)+(at)))[3] | \
                                          (((uint8_t*)((x)+(at)))[2] << 8) | \
                                          (((uint8_t*)((x)+(at)))[1] << 16) | \
                                          (((uint8_t*)((x)+(at)))[0] << 24) ) )
#define SCC_GET_S32BE(x,at)  ((int32_t) (  ((uint8_t*)((x)+(at)))[3] | \
                                          (((uint8_t*)((x)+(at)))[2] << 8) | \
                                          (((uint8_t*)((x)+(at)))[1] << 16) | \
                                          (((uint8_t*)((x)+(at)))[0] << 24) ) )

// Some lib c (OpenBSD at least) miss the PRI stuff.
// We might need a configure check for that.
#ifndef PRIu64
#define PRIu64 "llu"
#endif

// A coomon struct to hold some data
typedef struct scc_data scc_data_t;
struct scc_data {
  uint32_t size;
  uint8_t data[0];
};

// the bloody SCC_LIST :)
#define SCC_LIST_ADD(list,last,c) if(c){                  \
  if(last) last->next = c;                                \
  else list = c;                                          \
  for(last = c ; last && last->next ; last = last->next); \
}

#define SCC_LIST_FREE(list,last) while(list) {            \
  last = list->next;                                      \
  free(list);                                             \
  list = last;                                            \
}

#define SCC_LIST_FREE_CB(list,last,cb) while(list) {      \
  last = list->next;                                      \
  cb(list);                                               \
  list = last;                                            \
}

// Locale independant char test
#define SCC_ISALPHA(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define SCC_ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define SCC_ISALNUM(c) (SCC_ISALPHA(c) || SCC_ISDIGIT(c))

#define SCC_TOUPPER(c) (((c) >= 'a' && (c) <= 'z') ? (c)-'a'+'A' : (c))
#define SCC_TOLOWER(c) (((c) >= 'A' && (c) <= 'Z') ? (c)-'A'+'a' : (c))

#ifndef HAVE_ASPRINTF
#ifndef va_start
#include <stdarg.h>
#endif
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
#endif

#ifdef __GNUC__
#define PRINTF_ATTRIB(fmt,args) __attribute__ ((format (printf, fmt, args)))
#define PACKED_ATTRIB __attribute__ ((__packed__))
#else
#define PRINTF_ATTRIB(fmt,args)
#define PACKED_ATTRIB
#endif

scc_data_t* scc_data_load(char* path);


#ifdef IS_MINGW
typedef struct {
  size_t gl_pathc;
  char **gl_pathv;
  size_t gl_offs;
} glob_t;

void globfree(glob_t *pglob);

int  glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);

#endif

#define LOG_ERR     0
#define LOG_WARN    1
#define LOG_MSG     2
#define LOG_V       3
#define LOG_DBG     4
#define LOG_DBG1    5
#define LOG_DBG2    6

extern int scc_log_level;

void scc_log(int lvl,char* msg, ...) PRINTF_ATTRIB(2,3);
