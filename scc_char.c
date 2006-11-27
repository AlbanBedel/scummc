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
