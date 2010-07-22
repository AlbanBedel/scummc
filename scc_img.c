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
 * @file scc_img.c
 * @ingroup utils
 * @brief Read/write image files
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
#include "scc_img.h"

#include "quantize.h"

// create a new empty image of the given size
scc_img_t* scc_img_new(int w,int h,int ncol,int bpp) {
  scc_img_t* img = calloc(1,sizeof(scc_img_t));
  img->w = w;
  img->h = h;
  img->ncol = ncol;
  img->bpp = bpp;

  img->pal = img->ncol >= 0 ? calloc(3,ncol) : NULL;
  img->data = calloc(w,h);

  return img;
}

void scc_img_free(scc_img_t* img) {
  if(img->pal) free(img->pal);
  if(img->data) free(img->data);
  free(img);
}

int scc_img_write_bmp(scc_img_t* img,scc_fd_t* fd) {
  int doff = 14 + 40 + (4 * img->ncol);
  int stride = ((img->w*img->bpp)>>3);
      stride = ((stride+3)/4)*4;
  int isize = stride*img->h;
  int fsize = doff + isize;
  int i;
  
  // Switch to BGR for saving...
  if (img->pal == NULL)
    scc_img_swapchannels(img);

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
  scc_fd_w16le(fd,img->bpp);
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
  scc_fd_w32le(fd,img->pal ? 16 : 0);

  // palette
  for(i = 0 ; i < img->ncol ; i++) {
    scc_fd_w8(fd,img->pal[i*3+2]); // b
    scc_fd_w8(fd,img->pal[i*3+1]); // g
    scc_fd_w8(fd,img->pal[i*3]);   // r
    scc_fd_w8(fd,0);               // junk
  }
  
  // data
  isize = (img->w*img->bpp)>>3;
  for(i = img->h-1 ; i >= 0 ; i--) {
    if(scc_fd_write(fd,&img->data[(i*img->w*img->bpp)>>3],isize) != isize) {
      printf("Error while writing BMP data.\n");
      scc_img_swapchannels(img);
      return 0;
    }
    
    if(stride > isize) {
      int s;
      for(s = isize ; s < stride ; s++)
        scc_fd_w8(fd,0);
    }
  }
  
  // Switch back from BGR for saving...
  if (img->pal == NULL)
    scc_img_swapchannels(img);
  
  return 1;
}

// Swap R and B channels in an image
void scc_img_swapchannels(scc_img_t* img) {
  int i;
  int r,g,b;
  int sz = (img->w*img->h*img->bpp)>>3;
  int calc;
  char* data = img->data;
  switch (img->bpp) {
    case 16:
      for(i = 0 ; i < sz ; i ++) {
        calc = ((short*)data)[i];
        r = (calc>>11) & 0x1F; // r
        g = (calc>>5)  & 0x3F; // g
        b = (calc)     & 0x1F; // b
        ((short*)data)[i] = (b) | (g>>5) | (r>>11);
      }
      break;
    case 24:
      for(i = 0 ; i < sz ; i += 3) {
        r = data[i]; // r
        g = data[i+1]; // g
        b = data[i+2];   // b

        data[i]   = b;
        data[i+1] = g;
        data[i+2] = r;
      }
      break;
    case 32:
      for(i = 0 ; i < sz ; i += 4) {
        r = data[i]; // r
        g = data[i+1]; // g
        b = data[i+2];   // b

        data[i]   = b;
        data[i+1] = g;
        data[i+2] = r;
      }
      break;
  }
}
  
int scc_img_save_bmp(scc_img_t* img,char* path) {
  scc_fd_t* fd = new_scc_fd(path,O_WRONLY|O_CREAT|O_TRUNC,0);
  
  if(!fd) {
    printf("Failed to open %s for writing.\n",path);
    return 0;
  }
  
  if(!scc_img_write_bmp(img,fd)) {
    printf("BMP writing failed.\n");
    scc_fd_close(fd);
    return 0;
  }
  
  scc_fd_close(fd);
  return 1;
}


static scc_img_t* scc_img_parse_bmp(scc_fd_t* fd) {
  char hdr[2];
  int i,l,w,h,bpp,scan_stride;
  int flip;
  uint32_t fsize,doff,hsize,stride,isize,ncol,fmt;
  scc_img_t* img;
  uint8_t* dst;
  int n,c,x;
  int calc,r,g,b;

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
  flip = h < 0 ? 1 : 0;
  h = flip ? -h : h;

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
    stride = (bpp*w)>>3;
  }
  // align the stride on dword boundary
  stride = ((stride+3)/4)*4;

  fmt = scc_fd_r32le(fd);
  if(fmt != 0 && fmt != 1) {
    printf("BMP file %s uses an unsupported compression format: 0x%x.\n",fd->filename,fmt);
    return NULL;
  }

  isize = scc_fd_r32le(fd);
 
  if(fmt == 0 && isize && isize != stride*h) {
    printf("BMP file %s has an invalid image size: %d (should be %d).\n",fd->filename,isize, stride*h);
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
  img->bpp = bpp;

  img->ncol = ncol;
  
  if (img->ncol > 0) {
    img->pal = malloc(3*ncol);
    for(img->ncol = 0 ; img->ncol < ncol ; img->ncol++) {
      img->pal[img->ncol*3+2] = scc_fd_r8(fd); // b
      img->pal[img->ncol*3+1] = scc_fd_r8(fd); // g
      img->pal[img->ncol*3] = scc_fd_r8(fd);   // r
      scc_fd_r8(fd); // skip junk
    }
    // gimp use the last color for transparent areas
    img->trans = img->ncol-1;
  } else {
    img->pal = NULL;
  }
  
  img->data = malloc((w*h*bpp)>>3);

  switch(fmt) {
  case 0: // raw data
      dst = flip ? &img->data[0] : &img->data[(w*(h-1)*bpp)>>3];
      scan_stride = flip ? stride : -stride;
      for(l = 0; l < h ; dst += scan_stride, l++) {
          uint8_t data[stride];
          //printf("Strip %d OF %d [%i, %i]\n", l, h, stride, scan_stride);

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
              img->bpp = 8;
              break;
          case 4:
              for(i = 0 ; i < w ; i += 2) {
                  dst[i] = data[i/2]>>4;
                  dst[i+1] = data[i/2] & 0xF;
              }
              img->bpp = 8;
              break;
          case 8:
              memcpy(dst,data,w);
              break;
          case 16:
            for(i = 0 ; i < w ; i ++) {
                ((short*)dst)[i] = ((short*)data)[i];
            }
            break;
          case 24:
            for(i = 0 ; i < w ; i += 3) {
                dst[i]   = data[i]; // r
                dst[i+1] = data[i+1]; // g
                dst[i+2] = data[i+2];   // b
            }
            break;
          case 32:
            for(i = 0 ; i < w ; i ++) {
                ((int*)dst)[i] = ((int*)data)[i];
            }
            break;
          }
      }
      break;
  case 1: // RLE 8 bit
      dst = &img->data[w*(h-1)];
      x = 0;
      while(1) {
          n = scc_fd_r8(fd);
          c = scc_fd_r8(fd);

          // repeated color
          if(n > 0) {
              if(x+n > w) {
                  printf("Warning: repetition should continue on the next line???\n");
                  n = w-x;
              }
              if(n > 0) {
                  memset(&dst[x],c,n);
                  x += n;
              }
              continue;
          }
           
          if(c == 0) { // end of line
              if(x < w)
                  memset(&dst[x],0,w-x);
              x = 0;
              if(dst == img->data) break;
              dst -= w;
              continue;
          } 

          if(c == 1) { // end of bitmap
              if(x < w)
                  memset(&dst[x],0,w-x);
              if(dst > img->data)
                  memset(img->data,0,dst-img->data);
              break;
          }

          if(c == 2) { // delta
              // x delta
              n = scc_fd_r8(fd);
              // check we are still in the img
              if(x+n > w) {
                  printf("Warning: bitmap contains invalid x delta: %d+%d > %d\n",
                         x,n,w);
                  n = w-x;
              }
              // fill the line
              if(n > 0) {
                  memset(&dst[x],0,n);
                  x += n;
              }
              // y delta
              n = scc_fd_r8(fd);
              if(dst - n*w < img->data) {
                  printf("Warning: bitmap contains invalid y delta: %p -%d*%d < %p\n",
                         dst,n,w,img->data);
                  n = (dst - img->data)/w;
              }
              if(n > 0) {
                  // finish the line
                  if(x < w)
                      memset(&dst[x],0,w-x);
                  // fill the full lines
                  while(n > 1) {
                      dst -= w;
                      memset(dst,0,w);
                      n--;
                  }
                  // fill the last line up to x
                  dst -= w;
                  memset(dst,0,x);
              }
              continue;
          }

          // c >= 3 raw data
          if(x+c > w) {
              printf("Warning: raw data should continue on the next line???\n");
              c = w-x;
          }
          scc_fd_read(fd,&dst[x],c);
          if(c & 1) scc_fd_r8(fd); // padding
          x += c;
      } 
      break;
  default:
      printf("Got unsupported image format.\n");
      
  }
  
  // BMP will be BGR if we don't have a palette
  if (img->pal == NULL)
    scc_img_swapchannels(img);

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

// Remove palette from an image, converts to 24bpp
void scc_img_unpal(scc_img_t* img) {
  uint8_t* out = malloc(img->w*img->h*3);
  uint8_t* optr = out;
  uint8_t* iptr = img->data;
  uint8_t* pal = img->pal;

  if (pal == NULL)
    return;

  int i;
  int sz=img->w*img->h;
  for (i=0; i<sz; i++) {
    int pos = *iptr++;
    *optr++ = pal[pos++];
    *optr++ = pal[pos++];
    *optr++ = pal[pos];
  }

  free(img->data);
  free(img->pal);
  img->data = out;
  img->pal = NULL;
  img->ncol = 0;
  img->bpp = 24;
}

/// Reduce colors in an image
int scc_img_quantize(scc_img_t* img, int colors) {
  int i;
  int sz = colors,
    stride = (img->bpp*img->w)>>3,
    isize=img->w*img->h,
    w = img->w,
    h = img->h;
  GifByteType* RedInput = malloc(w*h);
  GifByteType* GreenInput = malloc(w*h);
  GifByteType* BlueInput = malloc(w*h);
  GifByteType* OutImg = malloc(w*h);
  GifColorType* OutPal = malloc(w*h*colors);
  uint8_t* data;

  // Ditch the palette
  if (img->pal)
    scc_img_unpal(img);

  data = img->data;

  switch (img->bpp)
  {
    case 16:
      for (i=0; i<isize; i++) {
        uint16_t val = ((short*)data)[i];
        RedInput[i]   = val & 0x1F; val >>= 5;
        GreenInput[i] = val & 0x3F; val >>= 6;
        BlueInput[i]  = val & 0x1F;
      }
      break;
    case 24:
      for (i=0; i<isize; i++) {
        uint8_t* ptr = data + (i*3);
        RedInput[i]   = *ptr++;
        GreenInput[i] = *ptr++;
        BlueInput[i]  = *ptr++;
      }
      break;
    case 32:
      for (i=0; i<isize; i++) {
        uint32_t val = ((int*)data)[i];
        RedInput[i]   = val & 0xFF; val >>= 8;
        GreenInput[i] = val & 0xFF; val >>= 8;
        BlueInput[i]  = val & 0xFF;
      }
      break;
  }

  // Go ahead!
  if (QuantizeBuffer(w, h, &sz, RedInput, GreenInput, BlueInput, OutImg, OutPal) != GIF_OK) {
    free(RedInput);
    free(GreenInput);
    free(BlueInput);
    free(OutImg);
    free(OutPal);
    return 0;
  }

  // Copy OutPal into final palette
  if (img->pal)
    free(img->pal);
  img->pal = malloc(sz*3);
  data = img->pal;
  memcpy(img->pal, OutPal, sz*3);

  free(RedInput);
  free(GreenInput);
  free(BlueInput);
  free(OutPal);
  free(img->data);

  img->data = OutImg;
  img->ncol = sz;
  img->bpp = 8;
  return 1;
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

  printf("Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n",
   img->w,img->h,img->ncol,img->trans,img->bpp);
  
  // Test Quantize
  scc_img_quantize(img, 255);
  printf("Quantized Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n",
       img->w,img->h,img->ncol,img->trans,img->bpp);
  scc_img_save_bmp(img, "dump.bmp");
  scc_img_free(img);
  
  scc_img_open("dump.bmp");
  printf("Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n",
     img->w,img->h,img->ncol,img->trans,img->bpp);

  return 0;

}

#endif
