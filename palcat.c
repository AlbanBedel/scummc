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
 * @file palcat.c
 * @brief Tool to concatenate image palette
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
#include "scc_img.h"


static char* outname = NULL;


static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &outname },
  { NULL, 0, 0, 0, NULL }
};

static void usage(char* prog) {
  printf("Usage: %s -o merged.bmp inputA.bmp inputB.bmp\n",prog);
  exit(-1);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files,*f;
  scc_img_t *out = NULL,*in;
  
  if(argc < 5) usage(argv[0]);
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files || !outname) usage(argv[0]);


  for(f = files ; f ; f = f->next) {
    in = scc_img_open(f->val);
    if(!in) {
      printf("Failed to open %s.\n",f->val);
      return 1;
    }

    if(!out) {
      out = in;
      continue;
    }
    out->pal = realloc(out->pal,(out->ncol+in->ncol)*3);
    memcpy(out->pal+3*out->ncol,in->pal,in->ncol*3);
    out->ncol += in->ncol;
    
    scc_img_free(in);
  }
  if(!scc_img_save_bmp(out,outname)) return 1;
  
  return 0;
}
