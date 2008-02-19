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
 * @file scc_char.c
 * @ingroup scumm
 * @brief SCUMM charset parser
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_util.h"
#include "scc_fd.h"
#include "scc_char.h"

scc_charmap_t* scc_parse_charmap(scc_fd_t* fd, unsigned size) {
  uint32_t size2,unk,bpp,height,num_char;
  uint8_t pal[15],byte,mask,bits,step;
  off_t font_ptr;
  uint32_t* char_off,x,i;
  scc_charmap_t* chmap;
  scc_char_t *ch;

  if(size < 25 ) {
    scc_log(LOG_ERR,"CHAR block is too small.\n");
    return NULL;
  }

  // read the charset header
  size2 = scc_fd_r32le(fd);
  if(size2 != size-15)
    scc_log(LOG_WARN,"Warning, size2 is invalid: %d != %d\n",size2,size-15);
    
  unk = scc_fd_r16le(fd);
  if(scc_fd_read(fd,pal,15) != 15) {
    scc_log(LOG_ERR,"Failed to read CHAR palette.\n");
    return NULL;
  }
  
  font_ptr = scc_fd_pos(fd);
  bpp = scc_fd_r8(fd);
  switch(bpp) {
  case 1:
    mask = 0x1;
    break;
  case 2:
    mask = 0x3;
    break;
  case 4:
    mask = 0xF;
    break;
  default:
    scc_log(LOG_ERR,"Invalid CHAR bpp: %d\n",bpp);
    return NULL;
  }
  height = scc_fd_r8(fd);
  if(!height) {
    scc_log(LOG_ERR,"Charset has 0 height??\n");
    return NULL;
  }
  num_char = scc_fd_r16le(fd);
  if(!num_char) {
    scc_log(LOG_ERR,"Charset has no chars???\n");
    return NULL;
  }
  if(size < 25 + 4*num_char) {
    scc_log(LOG_ERR,"Charset is too small.\n");
    return NULL;
  }
  // read the offset table
  char_off = malloc(num_char*4);
  for(i = 0 ; i < num_char ; i++)
    char_off[i] = scc_fd_r32le(fd);
  
  chmap = calloc(1,sizeof(scc_charmap_t));
  chmap->bpp = bpp;
  chmap->height = height;
  
  // read the chars
  scc_log(LOG_V,"Reading %d chars\n",num_char);
  for(i = 0 ; i < num_char ; i++) {
    if(!char_off[i]) continue;
    scc_fd_seek(fd,font_ptr + char_off[i],SEEK_SET);
    ch = &chmap->chars[i];
    ch->w = scc_fd_r8(fd);
    ch->h = scc_fd_r8(fd);
    ch->x = scc_fd_r8(fd);
    ch->y = scc_fd_r8(fd);
    if(ch->w + ch->x > chmap->width) chmap->width = ch->w + ch->x;
    if(!ch->w || !ch->h) continue;
    ch->data = malloc(ch->w*ch->h);
    bits = 0;
    step = 8/bpp;
    x = 0;
    while(x < ch->w*ch->h) {
      if(!bits) {
        byte = scc_fd_r8(fd);
        bits = 8;
      }
      ch->data[x] = (byte >> (8-bpp)) & mask;
      byte <<= bpp;
      bits -= bpp;
      x++;
    }
    chmap->max_char = i;
  }
  return chmap; 
}

static int decode_smush1(scc_fd_t* fd, unsigned size,
                         unsigned w, unsigned h,
                         uint8_t* dst, int dst_stride) {
  unsigned x, y, line_size;

  for(y = 0 ; y < h && size > 1 ; y++) {
    line_size = scc_fd_r16le(fd), size -= 2;
    if(line_size > size) {
        scc_log(LOG_WARN,"Too long line in smush1 image.\n");
        line_size = size;
    }
    size -= line_size;
    x = 0;
    while(x < w && line_size > 1) {
      uint8_t code = scc_fd_r8(fd);
      uint8_t length = (code>>1)+1;
      line_size--;
      if(x + length > w) {
        scc_log(LOG_WARN,"Too long block length in smush1 image.\n");
        length = w-x;
      }
      if(code & 1) {
        memset(dst+x,scc_fd_r8(fd),length);
        line_size--, x += length;
      } else {
        if(length > line_size) {
          scc_log(LOG_WARN,"Too long code length in smush1 image.\n");
          length = line_size;
        }
        line_size -= length;
        while(length > 0)
          dst[x] = scc_fd_r8(fd), x++, length--;
      }
    }
    if(x < w) memset(dst+x,0,w-x);
    dst += dst_stride;
  }

  while(y < h)
    memset(dst,0,w), y++, dst += dst_stride;

  if(size > 0)
    scc_fd_seek(fd,SEEK_CUR,size);

  return 1;
}

static int decode_nut21(scc_fd_t* fd, unsigned size,
                          unsigned w, unsigned h,
                          uint8_t* dst, int dst_stride) {
  unsigned x, y, line_size;

  for(y = 0 ; y < h && size > 1 ; y++) {
    line_size = scc_fd_r16le(fd), size -= 2;
    if(line_size > size) {
        scc_log(LOG_WARN,"Too long line in smush21 image.\n");
        line_size = size;
    }
    size -= line_size;
    x = 0;
    while(x < w && line_size >= 2) {
      // Skip
      unsigned len = scc_fd_r16le(fd);
      line_size -= 2;
      if(x + len > w) {
        //scc_log(LOG_WARN,"Too long skip length in smush21 image.\n");
        len = w-x;
      }
      if(len) {
        memset(dst+x,0,len);
        x += len;
        if(x >= w) break;
      }
      // Draw
      line_size -= 2;
      if((len = scc_fd_r16le(fd)+1) > line_size) {
        scc_log(LOG_WARN,"Too long draw code in smush21 image.\n");
        len = line_size;
      }
      if(x + len > w) {
        scc_log(LOG_WARN,"Too long draw length in smush21 image.\n");
        len = w-x;
      }
      line_size -= len;
      while(len > 0)
        dst[x] = scc_fd_r8(fd), x++, len--;
    }
    if(line_size > 0)
      scc_fd_seek(fd,line_size,SEEK_CUR);
    if(x < w) memset(dst+x,0,w-x);
    dst += dst_stride;
  }

  while(y < h)
    memset(dst,0,w), y++, dst += dst_stride;

  if(size > 0)
    scc_fd_seek(fd,size,SEEK_CUR);

  return 1;

}

scc_charmap_t* scc_parse_nutmap(scc_fd_t* fd, unsigned block_size) {
  uint32_t block, size;
  uint8_t pal[3*256];
  scc_charmap_t* chmap;
  scc_char_t *ch;
  unsigned num_char,i,codec;

  if(block_size < 8+3*2+3*256) {
    scc_log(LOG_ERR,"Invalid ANIM block.\n");
    return NULL;
  }

  block = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);

  if(block != MKID('A','H','D','R') || size != 3*2 + 3*256) {
    scc_log(LOG_ERR,"Invalid AHDR block.\n");
    return NULL;
  }

  scc_fd_r16(fd); // unk
  num_char = scc_fd_r16le(fd);
  scc_fd_r16(fd); // unk

  if(scc_fd_read(fd,pal,sizeof(pal)) != sizeof(pal)) {
    scc_log(LOG_ERR,"Failed to read palette.\n");
    return NULL;
  }

  block_size -= 8 + 3*2 + 3*256;

  chmap = calloc(1,sizeof(scc_charmap_t));
  chmap->bpp = 8;
  chmap->rgb_pal = malloc(sizeof(pal));
  memcpy(chmap->rgb_pal,pal,sizeof(pal));

  for(i = 0 ; i < num_char ; i++) {
    block = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    block_size -= 8;
    if(block != MKID('F','R','M','E') ||
       size > block_size || size < 4*2+7*2) {
      scc_log(LOG_ERR,"Invalid FRME block (%d).\n",i);
      return NULL;
    }
    block = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    if(block != MKID('F','O','B','J') || size < 7*2) {
      scc_log(LOG_ERR,"Invalid FOBJ block.\n");
      return NULL;
    }
    // Read the header
    ch = &chmap->chars[i];
    codec  = scc_fd_r16le(fd);
    ch->x  = scc_fd_r16le(fd);
    ch->y  = scc_fd_r16le(fd);
    ch->w  = scc_fd_r16le(fd);
    ch->h  = scc_fd_r16le(fd);
    /*unk*/  scc_fd_r32(fd);

    if(ch->w + ch->x > chmap->width) chmap->width = ch->w + ch->x;
    if(ch->h + ch->y > chmap->height) chmap->height = ch->h + ch->y;
    if(!ch->w || !ch->h) continue;

    // Decode the pic
    ch->data = malloc(ch->w*ch->h);
    switch(codec) {
    case 1:
      if(!decode_smush1(fd,size-7*2,ch->w,ch->h,ch->data,ch->w))
        return NULL;
      break;
    case 21:
    case 44:
      if(!decode_nut21(fd,size-7*2,ch->w,ch->h,ch->data,ch->w))
        return NULL;
      break;
    default:
      scc_log(LOG_ERR,"Unknown NUT codec (%d) for char %d.\n",codec,i);
      scc_fd_seek(fd,size-7*2,SEEK_CUR);
      ch->w = ch->h = ch->x = ch->y = 0;
      free(ch->data);
      ch->data = NULL;
      continue;
    }
    chmap->max_char = i;
  }

  return chmap;
}
