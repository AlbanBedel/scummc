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
 * @file imgremap.c
 * @brief Tool to swap colors in an image
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
//#include "scc.h"
#include "scc_img.h"


static int from = -1, to = -1;


static scc_param_t scc_parse_params[] = {
  { "from", SCC_PARAM_INT, 0, 255, &from },
  { "to", SCC_PARAM_INT, 0, 255, &to },
  { NULL, 0, 0, 0, NULL }
};

static void usage(char* prog) {
  printf("Usage: %s -from colorA -to colorB input.bmp\n",prog);
  exit(-1);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files,*f;
  int x,y;
  char outname[1024];
  scc_img_t *in;
  uint8_t* src;
  
  if(argc < 6) usage(argv[0]);
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files || from < 0 || to < 0) usage(argv[0]);


  for(f = files ; f ; f = f->next) {
    in = scc_img_open(f->val);
    if(!in) {
      printf("Failed to open %s.\n",f->val);
      return 1;
    }
    if(in->ncol <= from || in->ncol <= to) {
      printf("Input image %s have only %d colors.\n",f->val,in->ncol);
      return 1;
    }
    
    for(x = 0 ; x < 3 ; x++) {
      y = in->pal[to*3+x];
      in->pal[to*3+x] = in->pal[from*3+x];
      in->pal[from*3+x] = y;
    }

    src = in->data;
    for(y = 0 ; y < in->h ; y++) {
      for(x = 0 ; x < in->w ; x++) {
        if(src[x] == from) src[x] = to;
        else if(src[x] == to) src[x] = from;
      }
      src += in->w;
    }
    
    snprintf(outname,1023,"%s.map",f->val);
    outname[1023] = '\0';

    if(!scc_img_save_bmp(in,outname)) return 1;
  }
  return 0;
}
