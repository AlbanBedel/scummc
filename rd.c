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

/**
 * @file rd.c
 * @brief Experiment playground, currently dump SCUMM files.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"
#include "scc_codec.h"
#include "scc.h"

int scc_image_2_rgb(uint8_t* dst,int dst_stride,
		    uint8_t* src, int src_stride,
		    int width,int height,
		    scc_pal_t* pal) {
  int r,c;

  for(r = 0 ; r < height ; r++) {
    for(c = 0 ; c < width ; c++) {
      dst[3*c+0] = pal->r[src[c]];
      dst[3*c+1] = pal->g[src[c]];
      dst[3*c+2] = pal->b[src[c]];
    }
    src += src_stride;
    dst += dst_stride;
  }
  return 1;
}


int main(int argc,char** argv) {
  scc_fd_t* fd2,*fd = new_scc_fd("TENTACLE.000",O_RDONLY,0x69);
  scc_res_idx_t* idx;
  scc_res_t* res;
  int i,len;
  scc_lfl_t* lfl;
  scc_res_list_t *d;

  if(!fd) {
    printf("Failed to open index :((\n");
    return -1;
  }

  idx = scc_read_res_idx(fd);
  d = idx->scr_list;

  //for(i = 0 ; i < d->size ; i++)
  //  printf("%d ==> ROOM %d: %d\n",i,d->room_no[i],d->room_off[i]);

  //return 0;
  //  fd2 = new_scc_fd("dump.000",O_WRONLY|O_CREAT,0x69);
  //  scc_write_res_idx(fd2,idx);
  //  return 0;


  fd2 = new_scc_fd("TENTACLE.001",O_RDONLY,0x69); 
  res = scc_parse_res(fd2,idx);

  scc_dump_res(res);
  return 0;

    scc_fd_close(fd2);
  fd2 = new_scc_fd("copy.001",O_WRONLY|O_CREAT,0x69);
  scc_write_lecf(fd2,res);

  return 0;

  for(lfl = res->lfl ; lfl ; lfl = lfl->next) {
    scc_room_t* r = &lfl->room;
    uint8_t* img8 = calloc(r->width,r->height+5);
    uint8_t* img24 = malloc(r->width*r->height*3);
    int w,i,c = 0;
    char name[1024];
    scc_obim_t* ob;
    scc_imnn_t* im;
    uint8_t* smap;
    uint32_t smap_len;

    if(r->rmim->num_z_buf < 1) continue;

    printf("ROOM %d has %d zbuf\n",r->idx,r->rmim->num_z_buf);

    for(i = 1 ; i <= r->rmim->num_z_buf ; i++) {
      scc_decode_zbuf(img8,r->width/8,
		      r->width,r->height,
		      r->rmim->z_buf[i],
		      r->rmim->z_buf_size[i],0);
      smap_len = scc_code_zbuf(img8,r->width/8,
			       r->width,r->height,
			       &smap);
      if(smap_len != r->rmim->z_buf_size[i]) printf("ZMAP size mismatch %d != %d !!!\n",smap_len,r->rmim->z_buf_size[i]);
      if(!smap) { printf("Failed to code ???\n"); continue; }

      scc_decode_zbuf(img24,r->width/8,
		      r->width,r->height,
		      smap,
		      smap_len,0);
      free(smap);

      for(w = 0 ; w < r->width/8*r->height ; w++) {
	if(img24[w] != img8[w]) { 
	  printf("Mismatch at 0x%x/0x%x : 0x%x 0x%x\n",
		 i,r->width/8*r->height,img24[w],img8[w]);
	  printf("Ref data: %0x %0x %0x %0x\n",img8[w-1],img8[w],img8[w+1],img8[w+230]);
	  printf("Our data: %0x %0x %0x %0x\n",img24[w-1],img24[w],img24[w+1],img24[w+2]);
	  break;
	}
      }	   

    }
#if 0
    scc_code_image(img8,r->width,
		   r->width,r->height,r->transp,
		   &smap,&smap_len);
    scc_decode_image(img24,r->width,
		     r->width,r->height,
		     smap,
		     smap_len,-1);

    for(i = 0 ; i < r->width*r->height ; i++) {
      if(img24[i] != img8[i]) { 
	printf("Mismatch at 0x%x/0x%x : 0x%x 0x%x\n",
	       i,r->width*r->height,img24[i],img8[i]);
	printf("Ref data: %0x %0x %0x %0x\n",img8[i-1],img8[i],img8[i+1],img8[i+230]);
	printf("Our data: %0x %0x %0x %0x\n",img24[i-1],img24[i],img24[i+1],img24[i+2]);
	break;
      }
    }	   
		   

    scc_image_2_rgb(img24,r->width*3,img8,r->width,
		    r->width,r->height,
		    r->pals);
    sprintf(name,"rmim.%3.3d-%dx%d.raw",r->idx,r->width,r->height);
    fd2 = new_scc_fd(name,O_WRONLY|O_CREAT,0);
    while(c < r->width*r->height*3) {
      w = scc_fd_write(fd2,img24+c,r->width*r->height*3-c);
      if(c < 0) {
	printf("Write error\n");
	return -1;
      }
      c += w;
    }

    for(ob = r->obim ; ob ; ob = ob->next) {
 
      for(im = ob->img ; im ; im = im->next) {
	printf("Parsing images of %d: %d\n",ob->idx,im->idx);
	img8 = realloc(img8,ob->width*ob->height);
	scc_decode_image(img8,ob->width,
		     ob->width,ob->height,
		     im->smap,
		     im->smap_size,-1);
      }
      printf("\n");
    }
#endif
    free(img8);
    free(img24);
  }

  return 0;

  res = scc_open_res_file(fd2,idx);

  scc_dump_res(res);
  return 0;

  len = scc_res_find(res,MKID('S','C','R','P'),55);
  if(len < 0) {
    fprintf(stderr,"Failed to find script 55 (0x%x)\n",MKID('D','S','C','R'));
  } else {
    fprintf(stderr,"Script 55: %d bytes\n",len);
  }

  return 0;
}

