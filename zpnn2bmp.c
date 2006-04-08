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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"
#include "scc_util.h"

#include "scc_param.h"
#include "scc_codec.h"
#include "scc_img.h"

static int w = -1, h = -1;
static char* out_name = NULL;

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &out_name },
  { "w", SCC_PARAM_INT, 0, 0xFFFF, &w },
  { "h", SCC_PARAM_INT, 0, 0xFFFF, &h },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_fd_t* in_fd;
  scc_cl_arg_t* files;
  char* out;
  uint32_t type,len;
  uint8_t* data,*zdata;
  scc_img_t* img;
  int i;

  if(argc < 2) {
    printf("Usage: %s -w nn -h nn [-o out] file.zpnn\n",argv[0]);
    return -1;
  }

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  if(!files) return -1;

  if(w < 0 || h < 0) {
    printf("You must specify the width, height of the zplanes.");
    return -1;
  }

  if(w % 8 != 0) {
    printf("Z planes must have w%%8==0 !!!!!\n");
    return -1;
  }

  out = out_name ? out_name : "zplane.bmp";

  in_fd = new_scc_fd(files->val,O_RDONLY,0);
  if(!in_fd) {
    printf("Failed to open input file %s.\n",files->val);
    return -1;
  }

  type = scc_fd_r32(in_fd);
  len = scc_fd_r32be(in_fd) - 8;

  if(((type>>24)&0xFF) != 'Z' ||
     ((type>>16)&0xFF) != 'P' ||
     ((type>> 8)&0xFF) != '0') {
    printf("The input file is not a zplane: %c%c%c%c\n",UNMKID(type));
    return -1;
  }

  if(len <= 0) {
    printf("The input file have an invalid length.\n");
    return -1;
  }

  data = scc_fd_load(in_fd,len);
  if(!data) {
    printf("Failed to load the data.\n");
    return -1;
  }
  scc_fd_close(in_fd);

  zdata = malloc(w/8*h);
  if(!scc_decode_zbuf(zdata,w/8,w,h,
		      data,len,0)) {
    printf("Failed to decode zplane.\n");
    return -1;
  }

  img = scc_img_new(w,h,2);
  // set the ones to black
  img->pal[3] = 0xFF;
  img->pal[4] = 0xFF;
  img->pal[5] = 0xFF;

  // expand the zplane
  for(i = 0 ; i < w*h ; i++)
    img->data[i] = (zdata[i/8] & (1<<(7-(i%8))) ? 1 : 0);
  
  // save the bmp
  if(!scc_img_save_bmp(img,out)) return -1;

  return 0;
}
