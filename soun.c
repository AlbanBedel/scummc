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
 * @file soun.c
 * @ingroup scumm
 * @brief SCUMM soun block generator
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_util.h"
#include "scc_fd.h"
#include "scc_param.h"

#include "soun_help.h"

static char* out_file = NULL;
static char* midi_file = NULL;
static char* adl_file = NULL;
static char* rol_file = NULL;
static char* gmd_file = NULL;
static char* voc_file = NULL;
static char* spk_file = NULL;


static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &out_file },
  { "midi", SCC_PARAM_STR, 0, 0, &midi_file },
  { "adl", SCC_PARAM_STR, 0, 0, &adl_file },
  { "rol", SCC_PARAM_STR, 0, 0, &rol_file },
  { "gmd", SCC_PARAM_STR, 0, 0, &gmd_file },
  { "voc", SCC_PARAM_STR, 0, 0, &voc_file },
  { "spk", SCC_PARAM_STR, 0, 0, &spk_file },
  { "help", SCC_PARAM_HELP, 0, 0, &soun_help },
  { NULL, 0, 0, 0, NULL }
};

static int read_file(scc_fd_t* fd, char** rdata, unsigned *pos, unsigned *size) {
  char* data = rdata[0];
  int r;
  int bs = 0;
  unsigned s = size[0],p = pos[0];
  while(1) {
    if(p == s) {
      s += 2048;
      data = realloc(data,s);
    }
    r = scc_fd_read(fd,data+p,s-p);
    if(r < 0) {
      printf("Error while reading file.\n");
      bs = 0;
      break;
    } else if(r == 0) break;
    p += r;
    bs += r;
  }
  scc_fd_close(fd);
  rdata[0] = data;
  size[0] = s;
  pos[0] = p;
  return bs;
}

static int load_voc(char* path,char** rdata, unsigned *pos, unsigned *size) {
  scc_fd_t *fd = new_scc_fd(path,O_RDONLY,0);
  int r;
  char data[32];

  if(!fd) {
    printf("Failed to open %s.\n",path);
    return 0;
  }
  
  data[22] = 0x0A; // version
  data[23] = 0x01;
  r = scc_fd_read(fd,data,26);
  if (r < 0 || data[0] != 'C' || ((data[23]<<8) | data[22]) != 266) {
    printf("Invalid voc file %s  %c [%i].\n", path, data[0], (data[23]<<8) | data[23]);
    return 0;
  }
  
  // Read the rest of the file to data
  return read_file(fd, rdata, pos, size);
}

static int load_file(char* path,char** rdata, unsigned *pos, unsigned *size) {
  scc_fd_t *fd = new_scc_fd(path,O_RDONLY,0);
  
  if(!fd) {
    printf("Failed to open %s.\n",path);
    return 0;
  }
  
  return read_file(fd, rdata, pos, size);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files;
  char* out;
  scc_fd_t *fd;
  unsigned size = 2048,pos = 0;
  char *data;
  int s,ssize = 0,spos,apos,ahpos;

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  
  if(files || !(midi_file || adl_file || rol_file || gmd_file || voc_file))
    scc_print_help(&soun_help,1);

  data = malloc(size);
  SCC_SET_32(data,0,MKID('S','O','U','N'));

  if(midi_file) {
    SCC_SET_32(data,8,MKID('M','I','D','I'));
    pos = 16;
    if(!(s = load_file(midi_file,&data,&pos,&size))) return 1;
    SCC_SET_32BE(data,12,s + 8);
  } else {

    SCC_SET_32(data,8,MKID('S','O','U',' '));
    pos = 16;

    if(adl_file) {
      SCC_SET_32(data,pos,MKID('A','D','L',' '));
      spos = pos + 4;
      pos += 8;
      if(!(s = load_file(adl_file,&data,&pos,&size))) return 1;
      SCC_SET_32BE(data,spos,s + 8);
      ssize += s + 8;
    }

    if(rol_file) {
      SCC_SET_32(data,pos,MKID('R','O','L',' '));
      spos = pos + 4;
      pos += 8;
      if(!(s = load_file(rol_file,&data,&pos,&size))) return 1;
      SCC_SET_32BE(data,spos,s + 8);
      ssize += s + 8;
    }

    if(gmd_file) {
      SCC_SET_32(data,pos,MKID('G','M','D',' '));
      spos = pos + 4;
      pos += 8;
      if(!(s = load_file(gmd_file,&data,&pos,&size))) return 1;
      SCC_SET_32BE(data,spos,s + 8);
      ssize += s + 8;
    }
    
    if (voc_file) {
      SCC_SET_32(data,pos,MKID('S','B','L',' '));
      spos = pos + 4;
      pos += 8; // hdr+block size
      SCC_SET_32(data,pos,MKID('A','U','h','d'));
      apos = pos + 4;
      pos += 8; // ahdr+block size
      data[pos] = 0x00;
      data[pos+1] = 0x00;
      data[pos+2] = 0x80; 
      pos += 3;
      SCC_SET_32(data,pos,MKID('A','U','d','t'));
      ahpos = pos + 4;
      pos += 8; // ahdr+block size
      
      if(!(s = load_voc(voc_file,&data,&pos,&size))) return 1;
      
      SCC_SET_32BE(data,spos,s + 8 + 3 + 8 + 8);
      SCC_SET_32BE(data,apos,s + 8 + 3 + 8);
      SCC_SET_32BE(data,ahpos,s + 8);
      
      ssize += s + 8 + 3 + 8 + 8;
    }
    
    if (spk_file) {
      SCC_SET_32(data,pos,MKID('S','P','K',' '));
      spos = pos + 4;
      pos += 8;
      if(!(s = load_file(voc_file,&data,&pos,&size))) return 1;
      SCC_SET_32BE(data,spos,s + 8);
      ssize += s + 8;
    }

    SCC_SET_32BE(data,12,ssize + 8);
  }

  SCC_SET_32BE(data,4,pos);

  out = out_file ? out_file : "output.soun";
  fd = new_scc_fd(out,O_WRONLY|O_CREAT|O_TRUNC,0);
  if(!fd) {
    printf("Failed to open %s for writing.\n",out);
    return 1;
  }

  if(scc_fd_write(fd,data,pos) != pos) {
    printf("Failed to write to %s.\n",out);
    return 1;
  }

  scc_fd_close(fd);

  free(data);

  return 0;
}
    
  
