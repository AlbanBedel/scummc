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
 * @file imgsplit.c
 * @brief Tool to split an image
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

#include "imgsplit_help.h"

static int dw = 0, dh = 0;
static char* outbase = NULL;
static char* outfmt = NULL;

static scc_param_t scc_parse_params[] = {
  { "outbase", SCC_PARAM_STR, 0, 0, &outbase },
  { "outfmt", SCC_PARAM_STR, 0, 0, &outfmt },
  { "dw", SCC_PARAM_INT, 1, 100000, &dw },
  { "dh", SCC_PARAM_INT, 1, 100000, &dh },
  { "help", SCC_PARAM_HELP, 0, 0, &imgsplit_help },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_cl_arg_t* files,*f;
  int num = 0,x,y,l;
  char outname[1024];
  scc_img_t *in,*out;
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files || !dw || !dh) scc_print_help(&imgsplit_help,1);

  if(!outbase) outbase = "frame";
  if(!outfmt) outfmt = "%s-%02d.bmp";

  for(f = files ; f ; f = f->next) {
    in = scc_img_open(f->val);
    if(!in) {
      printf("Failed to open %s.\n",f->val);
      return 1;
    }
    if(in->w < dw || in->h < dh) {
      printf("Input image %s is too small.\n",f->val);
      return 1;
    }
    if(in->w % dw)
      printf("Warning: input image %s width is not a multiple of destination height.\n",
             f->val);
    if(in->h % dh)
      printf("Warning: input image %s height is not a multiple of destination height.\n",
             f->val);
    for(y = 0 ; y < in->h ; y += dh) {
      for(x = 0 ; x < in->w ; x += dw) {
        out = scc_img_new(dw,dh,in->ncol);
        memcpy(out->pal,in->pal,3*in->ncol);
        for(l = 0 ; l < dh ; l++)
          memcpy(&out->data[l*dw],&in->data[(y+l)*in->w+x],dw);

        snprintf(outname,1023,outfmt,outbase,num);
        outname[1023] = '\0';

        if(!scc_img_save_bmp(out,outname)) return 1;
        num++;
      }
    }
  }
  return 0;
}
