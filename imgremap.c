

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
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
  scc_fd_t* fd;
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

    fd = new_scc_fd(outname,O_WRONLY|O_CREAT|O_TRUNC,0);
    if(!fd) {
      printf("Failed to open %s.\n",outname);
      return 1;
    }
    scc_img_write_bmp(in,fd);
    scc_fd_close(fd);
  }
  return 0;
}
