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

/*
 * A little lib to open image files. We are only intersted in
 * paletted format atm. Currently only uncompressed bmp is supported.
 *
 */

typedef struct scc_img {
  unsigned int w,h;
  unsigned int ncol,trans;
  uint8_t* pal;
  uint8_t* data;
} scc_img_t;

// create a new empty image of the given size
scc_img_t* scc_img_new(int w,int h,int ncol);

void scc_img_free(scc_img_t* img);

int scc_img_write_bmp(scc_img_t* img,scc_fd_t* fd);

int scc_img_save_bmp(scc_img_t* img,char* path);

scc_img_t* scc_img_open(char* path);
