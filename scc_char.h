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
 * @file scc_char.h
 * @ingroup scumm
 * @brief SCUMM charset parser
 */

typedef struct scc_char_st {
  uint8_t w,h;
  int8_t  x,y;
  uint8_t* data;
} scc_char_t;


#define SCC_MAX_CHAR 8192

typedef struct scc_charmap_st {
  uint8_t    pal[15];             // palette used
  uint8_t    bpp;                 // bpp used for coding
  uint8_t    height;              // font height
  uint8_t    width;               // max width
  uint16_t   max_char;            // index of the last good char
  scc_char_t chars[SCC_MAX_CHAR];
} scc_charmap_t;

scc_charmap_t* scc_parse_charmap(scc_fd_t* fd, unsigned size);
