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

// code.c
   
// create an smap out from a bitmap. It simply try every codec for each
// stripe and take the best one. Note that in the doot data at least some
// room aren't using the best encoding.

int scc_code_image(uint8_t* src, int src_stride,
                   int width,int height,int transparentColor,
                   uint8_t** smap_p);

int scc_code_zbuf(uint8_t* src, int src_stride,
                  int width,int height,
                  uint8_t** smap_p);

// decode.c

int scc_decode_image(uint8_t* dst, int dst_stride,
                     int width,int height,
                     uint8_t* smap,uint32_t smap_size,
                     int transparentColor);

int scc_decode_zbuf(uint8_t* dst, int dst_stride,
                    int width,int height,
                    uint8_t* zmap,uint32_t zmap_size,int or);

