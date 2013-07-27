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
static char* inname = NULL;


static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &outname },
  { "i", SCC_PARAM_STR, 0, 0, &inname },
  { NULL, 0, 0, 0, NULL }
};

static void usage(char* prog) {
  printf("Usage: %s -o merged.bmp [-i in.bmp] inputA.bmp in.bmp inputB.bmp\n",prog);
  exit(-1);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files,*f;
  scc_img_t *out = NULL,*in;
  unsigned pre_off = 0;
  
  if(argc < 5) usage(argv[0]);
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files || !outname) usage(argv[0]);

  if (inname) {
    out = scc_img_open(inname);
    if (!out)
      inname = NULL;
  }

  for(f = files ; f ; f = f->next) {
    if (!inname || strcmp(f->val, inname)) {
      in = scc_img_open(f->val);
    } else {
      // Found input filename, now switch to append mode
      inname = NULL;
      continue;
    }

    if(!in) {
      scc_log(LOG_ERR,"Failed to open image '%s'.\n",f->val);
      return 1;
    }

    if(!out) {
      // Assign output to first file specified
      out = in;
      continue;
    }

    // Realloc the palette
    out->pal = realloc(out->pal,(out->ncol+in->ncol)*3);

    // Pre-append the palette
    if (inname) {
      int i;
      memmove(out->pal+pre_off+3*in->ncol,out->pal+pre_off,out->ncol*3-pre_off);
      memcpy(out->pal+pre_off,in->pal,in->ncol*3);
      pre_off += in->ncol*3;
      // Offset bitmap data in image by input colors (assuming data is 8bit)
      for (i = 0 ; i < out->w*out->h ; i++)
          out->data[i] += in->ncol;
    } else  // Append the palette
      memcpy(out->pal+3*out->ncol,in->pal,in->ncol*3);
    
    out->ncol += in->ncol;
    
    scc_img_free(in);
  }
  if(!scc_img_save_bmp(out,outname)) return 1;
  
  return 0;
}
