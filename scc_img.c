

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
#include "scc_img.h"

// create a new empty image of the given size
scc_img_t* scc_img_new(int w,int h,int ncol) {
  scc_img_t* img = calloc(1,sizeof(scc_img_t));
  img->w = w;
  img->h = h;
  img->ncol = ncol;

  img->pal = calloc(3,ncol);
  img->data = calloc(w,h);

  return img;
}

void scc_img_free(scc_img_t* img) {
  if(img->pal) free(img->pal);
  if(img->data) free(img->data);
  free(img);
}

int scc_img_write_bmp(scc_img_t* img,scc_fd_t* fd) {
  int doff = 14 + 40 + 4 * img->ncol;
  int stride = ((img->w+3)/4)*4;
  int isize = stride*img->h;
  int fsize = doff + isize;
  int i;

  scc_fd_w8(fd,'B');
  scc_fd_w8(fd,'M');
  // compute fsize
  scc_fd_w32le(fd,fsize);
  // reserved
  scc_fd_w32le(fd,0);
  // compute doff
  scc_fd_w32le(fd,doff);
  // hsize
  scc_fd_w32le(fd,40);

  // image size
  scc_fd_w32le(fd,img->w);
  scc_fd_w32le(fd,img->h);

  // n planes
  scc_fd_w16le(fd,1);
  // bpp
  scc_fd_w16le(fd,8);
  // comp
  scc_fd_w32le(fd,0);
  // isize
  scc_fd_w32le(fd,isize);

  // dpi, put some dummy values
  scc_fd_w32le(fd,1000);
  scc_fd_w32le(fd,1000);

  // ncol
  scc_fd_w32le(fd,img->ncol);
  // num of important color
  scc_fd_w32le(fd,16);

  // palette
  for(i = 0 ; i < img->ncol ; i++) {
    scc_fd_w8(fd,img->pal[i*3+2]); // r
    scc_fd_w8(fd,img->pal[i*3+1]); // g
    scc_fd_w8(fd,img->pal[i*3]);   // b
    scc_fd_w8(fd,0);               // junk
  }

  // data
  for(i = img->h-1 ; i >= 0 ; i--) {
    if(scc_fd_write(fd,&img->data[i*img->w],img->w) != img->w) {
      printf("Error while writing BMP data.\n");
      return 0;
    }
    if(stride > img->w) {
      int i;
      for(i = img->w ; i < stride ; i++)
	scc_fd_w8(fd,0);
    }       
  }
  
  return 1;
}
  


static scc_img_t* scc_img_parse_bmp(scc_fd_t* fd) {
  char hdr[2];
  int i,l,bpp;
  uint32_t fsize,doff,hsize,w,stride,h,isize,ncol;
  scc_img_t* img;
  uint8_t* dst;

  // Header
  if(scc_fd_read(fd,hdr,2) != 2) return NULL;
  if(hdr[0] != 'B' || hdr[1] != 'M') return NULL;

  // file size
  fsize = scc_fd_r32le(fd);
  if(fsize < 14) return NULL;

  // reserved
  scc_fd_r32(fd);

  // data offset
  doff = scc_fd_r32le(fd);
  if(doff < 44) return NULL;

  // header size
  hsize = scc_fd_r32le(fd);
  if(hsize < 40) return NULL;
  
  // image size
  w = scc_fd_r32le(fd);
  h = scc_fd_r32le(fd);

  if(scc_fd_r16le(fd) != 1) {
    printf("BMP file %s has an invalid number of planes.\n",fd->filename);
    return NULL;
  }

  bpp = scc_fd_r16le(fd);
  switch(bpp) {
  case 8:
    stride = w;
    break;
  case 4:
    stride = (w+1)/2;
    break;
  case 1:
    stride = (w+7)/8;
    break;
  default:
    printf("%s is not an indexed BMP: %d bpp.\n",fd->filename,bpp);
    return NULL;
  }
  // align the stride on dword boundary
  stride = ((stride+3)/4)*4;

  if(scc_fd_r32le(fd) != 0) {
    printf("BMP file %s use an unsupported compression format.\n",fd->filename);
    return NULL;
  }

  isize = scc_fd_r32le(fd);
 
  if(isize && isize != stride*h) {
    printf("BMP file %s has an invalid image size: %d.\n",fd->filename,isize);
    return NULL;
  }
  
  // dpi in pix/meter
  scc_fd_r32(fd);
  scc_fd_r32(fd);

  // num of color
  ncol = scc_fd_r32le(fd);
  // num of important color
  scc_fd_r32(fd);

  if(doff != 14 + 40 + 4 * ncol) {
    printf("BMP file %s has an invalid data offset.\n",fd->filename);
    return NULL;
  }


  img = calloc(1,sizeof(scc_img_t));
  img->w = w;
  img->h = h;

  img->ncol = ncol;
  img->pal = malloc(3*ncol);
  for(img->ncol = 0 ; img->ncol < ncol ; img->ncol++) {
    img->pal[img->ncol*3+2] = scc_fd_r8(fd); // r
    img->pal[img->ncol*3+1] = scc_fd_r8(fd); // g
    img->pal[img->ncol*3] = scc_fd_r8(fd);   // b
    scc_fd_r8(fd); // skip junk
  }
  // gimp use the last color for transparent areas
  img->trans = img->ncol-1;
  
  img->data = malloc(w*h);
  for(l = 0, dst = &img->data[w*(h-1)] ; l < h ; dst -= w, l++) {
    uint8_t data[stride];

    if(scc_fd_read(fd,data,stride) != stride) {
      printf("Error while reading BMP file %s\n",fd->filename);
      scc_img_free(img);
      return NULL;
    }

    switch(bpp) {
    case 1:
      for(i = 0 ; i < w ; i++) {
	if(data[i/8] & (1<<(7-(i%8))))
	  dst[i] = 1;
	else
	  dst[i] = 0;
      }
      break;
    case 2:
      for(i = 0 ; i < w ; i += 2) {
	dst[i] = data[i/2]>>4;
	dst[i+1] = data[i/2] & 0xF;
      }
      break;
    case 8:
      memcpy(dst,data,w);
      break;
    } 
  }

  return img;
}

// later this func should probably go through the various
// decoder to find the right one.
scc_img_t* scc_img_open(char* path) {
  scc_fd_t* fd;
  scc_img_t* img;

  fd = new_scc_fd(path,O_RDONLY,0);
  if(!fd) {
    printf("Failed to open %s.\n",path);
    return NULL;
  }

  img = scc_img_parse_bmp(fd);

  scc_fd_close(fd);

  return img;
}

//#define SCC_IMG_TEST 1
#ifdef SCC_IMG_TEST

int main(int argc,char** argv) {
  scc_img_t* img;
  

  if(argc < 2) {
    printf("We need an argument\n");
    return -1;
  }

  img = scc_img_open(argv[1]);
  if(!img) return -1;

  printf("Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\n",
	 img->w,img->h,img->ncol,img->trans);

  return 0;

}

#endif
