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


static int srate = 22050;
static char* output = NULL;

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &output },
  { "r", SCC_PARAM_INT, 0, 0xFFFF, &srate },
  { NULL, 0, 0, 0, NULL }
};

void usage(char* name) {
  printf("Usage: %s [ -r rate ] [ -o out.voc ] input.raw\n",name);
  exit(1);
}

int main(int argc, char** argv) {
  scc_fd_t* ifd,*ofd;
  scc_cl_arg_t* files;
  char* ofl;
  unsigned dlen = 26 + 1 + 3 + 2, dsize = 0;
  char* data;
  int r,blen;
  
  if(argc < 2) usage(argv[0]);

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  if(!files) usage(argv[0]);

  ifd = new_scc_fd(files->val,O_RDONLY,0);
  if(!ifd) {
    printf("Failed to open input %s\n",files->val);
    return 1;
  }
  
  dsize = 1024;
  data = malloc(1024);

  while(1) {
    if(dsize <= dlen) {
      dsize += 1024;
      data = realloc(data,dsize);
    }
    r = scc_fd_read(ifd,data+dlen,dsize-dlen);
    if(r < 0) {
      printf("Error while reading input data.\n");
      return 2;
    } else if(r == 0) break;
    dlen += r;
  }

  scc_fd_close(ifd);

  if(dlen == 32) {
    printf("No input data found.\n");
    return 1;
  }

  ofl = output ? output : "output.voc";
  ofd = new_scc_fd(ofl,O_WRONLY|O_CREAT|O_TRUNC,0);
  if(!ofd) {
    printf("Failed to open output %s\n",ofl);
    return 1;
  }
  
  // write the voc header
  memcpy(data,"Creative Voice File",19);
  data[19] = 0x1A; // eof
  data[20] = 0x1A; // header size
  data[21] = 0;
  data[22] = 0x0A; // version
  data[23] = 0x01;
  data[24] = 0x29; // magic number
  data[25] = 0x11;

  // write the header of the first block
  blen = dlen - 30;
  data[26] = 0x01;                 // block type
  data[27] = blen & 0xFF;          // block size
  data[28] = (blen >> 8) & 0xFF;
  data[29] = (blen >> 16) & 0xFF;
  
  data[30] = 256-(1000000/srate);  // sample rate
  data[31] = 0;                    // packing

  if(scc_fd_write(ofd,data,dlen) != dlen) {
    printf("Failed to write %s\n",ofl);
    return 1;
  }

  // write the terminator byte
  scc_fd_w8(ofd,0);
  
  scc_fd_close(ofd);

  return 0;
}
