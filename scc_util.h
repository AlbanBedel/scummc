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

// FIXME: Add those for be platform too
#define SCC_TO_16BE(x) ((((x)>>8)&0xFF)|(((x)<<8)&0xFF00))
#define SCC_TO_32BE(x) (  SCC_TO_16BE((uint16_t)(((x)>>16)&0xFFFF)) | (SCC_TO_16BE((uint16_t)(x))<<16) )
#define SCC_TO_16LE(x) (x)
#define SCC_TO_32LE(x) (x)

#define SCC_AT_32BE(x,at)  SCC_TO_32BE( (*((uint32_t*)&x[at])) )
#define SCC_AT_S32BE(x,at) SCC_TO_32BE( (*((int32_t*)&x[at])) )
#define SCC_AT_32LE(x,at)  (((uint32_t*)&x[at])[0])
#define SCC_AT_S32LE(x,at) (((int32_t*)&x[at])[0])
#define SCC_AT_32(x,at)    (((uint32_t*)&x[at])[0])
#define SCC_AT_S32(x,at)   (((int32_t*)&x[at])[0])
#define SCC_AT_16BE(x,at)  SCC_TO_16BE( (*((uint16_t*)&x[at])) )
#define SCC_AT_S16BE(x,at) SCC_TO_16BE( (*((int16_t*)&x[at])) )
#define SCC_AT_16LE(x,at)  (((uint16_t*)&x[at])[0])
#define SCC_AT_S16LE(x,at) (((int16_t*)&x[at])[0])
#define SCC_AT_16(x,at)    (((uint16_t*)&x[at])[0])
#define SCC_AT_S16(x,at)   (((int16_t*)&x[at])[0])

#define SCC_SET_16(x,at,v)    SCC_AT_16(x,at) = v
#define SCC_SET_16LE(x,at,v)  SCC_AT_16(x,at) = v
#define SCC_SET_S16LE(x,at,v) SCC_AT_S16(x,at) = v

#define SCC_SET_16BE(x,at,v)  SCC_AT_16(x,at) = v;  \
                              SCC_AT_16(x,at) =  SCC_TO_16BE(SCC_AT_16(x,at))
#define SCC_SET_S16BE(x,at,v) SCC_AT_S16(x,at) = v;  \
                              SCC_AT_S16(x,at) = SCC_TO_16BE(SCC_AT_S16(x,at))

#define SCC_SET_32(x,at,v)    SCC_AT_32(x,at) = v
#define SCC_SET_32LE(x,at,v)  SCC_AT_32(x,at) = v
#define SCC_SET_S32LE(x,at,v) SCC_AT_S32(x,at) = v

#define SCC_SET_32BE(x,at,v)  SCC_AT_32(x,at) = v;  \
                              SCC_AT_32(x,at) =  SCC_TO_32BE(SCC_AT_32(x,at))
#define SCC_SET_S32BE(x,at,v) SCC_AT_S32(x,at) = v;  \
                              SCC_AT_S32(x,at) = SCC_TO_32BE(SCC_AT_S32(x,at))

#define MKID(a,b,c,d) ((uint32_t) \
			((d) & 0x000000FF) | \
			((c << 8) & 0x0000FF00) | \
			((b << 16) & 0x00FF0000) | \
			((a << 24) & 0xFF000000))

#define UNMKID(a) (a>>24)&0xFF, (a>>16)&0xFF, (a>>8)&0xFF, a&0xFF

// needed by the room builder
#define SCC_MAX_IM_PLANES 10
#define SCC_MAX_GLOB_SCR  200
#define SCC_MAX_ACTOR     17

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

scc_data_t* scc_data_load(char* path);
