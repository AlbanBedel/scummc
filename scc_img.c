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
#include "scc_util.h"

#include "quantize.h"

// create a new empty image of the given size
scc_img_t* scc_img_new(int w,int h,int ncol,int bpp) {
  scc_img_t* img = calloc(1,sizeof(scc_img_t));
  img->w = w;
  img->h = h;
  img->ncol = ncol;
  img->bpp = bpp;

  img->pal = bpp > 8 ? NULL : calloc(3,ncol);
  img->data = bpp > 8 ? calloc(1,(bpp*w*h)>>3) : calloc(w,h);
  img->trans = -1;

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
      if (img->pal == NULL)
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
  uint8_t* data = img->data;
  switch (img->bpp) {
    case 8:
      data = img->pal;
      sz = img->ncol*3;
      for(i = 0 ; i < sz ; i += 3) {
        r = data[i]; // r
        g = data[i+1]; // g
        b = data[i+2];   // b

        data[i]   = b;
        data[i+1] = g;
        data[i+2] = r;
      }
      break;
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
  int i,l,w,h,bpp,scan_bpp,scan_stride,dir_stride;
  int flip;
  uint32_t fsize,doff,hsize,stride,isize,ncol,nimp,fmt;
  scc_img_t* img;
  uint8_t* dst;
  int n,c,x;

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
    scan_bpp = 8;
    scan_stride = w;
    break;
  case 4:
    scan_bpp = 8;
    scan_stride = (w+1)/2;
    break;
  case 1:
    scan_bpp = 8;
    scan_stride = (w+7)/8;
    break;
  default:
    scan_bpp = bpp;
    scan_stride = (bpp*w)>>3;
    break;
  }
  // align the stride on dword boundary
  stride = ((scan_stride+3)/4)*4;
  scan_stride = (scan_bpp*w)>>3; // will at least be 8bpp
  dir_stride = flip ? scan_stride : -scan_stride;

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
  nimp = scc_fd_r32le(fd);

  if(doff != 14 + hsize + 4 * ncol) {
    printf("BMP file %s has an invalid data offset (%d should be 14 + %d + 4 * %d  [%d]).\n",fd->filename, doff, hsize, ncol, isize);
    //return NULL;
  }

  img = calloc(1,sizeof(scc_img_t));
  img->w = w;
  img->h = h;
  img->bpp = scan_bpp;

  img->ncol = ncol;
  
  if (img->ncol > 0) {
    img->pal = malloc(3*ncol);
    for(img->ncol = 0 ; img->ncol < ncol ; img->ncol++) {
      img->pal[img->ncol*3+2] = scc_fd_r8(fd); // b
      img->pal[img->ncol*3+1] = scc_fd_r8(fd); // g
      img->pal[img->ncol*3] = scc_fd_r8(fd);   // r
      scc_fd_r8(fd); // skip junk
    }
  } else {
    img->pal = NULL;
    img->trans = -1;
  }
  
  //scc_fd_seek(fd, doff, SEEK_SET);
  img->data = calloc(1,(w*h*scan_bpp)>>3);

  switch(fmt) {
  case 0: // raw data
      dst = flip ? &img->data[0] : &img->data[scan_stride*(h-1)];
      for(l = 0; l < h ; dst += dir_stride, l++) {
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
              memcpy(dst,data,scan_stride);
              break;
          case 16:
            for(i = 0 ; i < w ; i ++) {
                ((short*)dst)[i] = ((short*)data)[i];
            }
            break;
          case 24:
            for(i = 0 ; i < scan_stride ; i += 3) {
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
  
  if (img->pal)
  {
    // Use the first pixel as the transparent color
    // (better than nothing!)
    img->trans = *img->data;
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

// Remove palette from an image, converts to 32bpp
void scc_img_unpal(scc_img_t* img) {
  uint8_t* out = malloc(img->w*img->h*4);
  uint8_t* optr = out;
  uint8_t* iptr = img->data;
  uint8_t* pal = img->pal;

  if (pal == NULL)
    return;

  int i;
  int sz=img->w*img->h;
  for (i=0; i<sz; i++) {
    uint8_t col = *iptr++;
    int pos = col*3;
    *optr++ = pal[pos++];
    *optr++ = pal[pos++];
    *optr++ = pal[pos];
    *optr++ = col == img->trans ? 0 : 255;
  }

  free(img->data);
  free(img->pal);
  img->data = out;
  img->pal = NULL;
  img->ncol = 0;
  img->bpp = 32;
  img->trans = -1;
}

/// Reduce colors in an image
int scc_img_quantize(scc_img_t* img, int colors) {
  int i;
  int sz = colors,
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
        BlueInput[i]  = *ptr;
      }
      break;
    case 32:
      for (i=0; i<isize; i++) {
        uint8_t* ptr = data + (i*4);
        RedInput[i]   = *ptr++;
        GreenInput[i] = *ptr++;
        BlueInput[i]  = *ptr;
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
  img->pal = malloc(colors*3);
  data = img->pal;
  memcpy(img->pal, OutPal, colors*3);

  free(RedInput);
  free(GreenInput);
  free(BlueInput);
  free(OutPal);
  free(img->data);

  img->data = OutImg;
  img->ncol = colors;
  img->bpp = 8;
  return 1;
}

typedef struct {
  int x;
  int y;
} imgpos_t;

typedef struct {
  int x;
  int y;
  int width;
  int height;
} imgrect_t;

// Clips src into dest, storing the result in out
void scc_clip(const imgrect_t src, const imgrect_t dest, imgrect_t* out) {
  imgrect_t out_rect = src;

  //printf("CLIP x=%i,y=%i,w=%i,h=%i IN %i,%i,%i,%i\n", src.x, src.y, src.width, src.height, dest.x,dest.y,dest.width,dest.height);

  // x...
  if (src.x < dest.x) {
    out_rect.x = dest.x;
    out_rect.width = src.width + (src.x - dest.x);
  } else if (src.x+src.width > dest.x+dest.width){
    out_rect.width -= (src.x+src.width)-(dest.x+dest.width);
  }

  // y...
  if (src.y < dest.y) {
    out_rect.y = dest.y;
    out_rect.height = src.height + (src.y - dest.y);
  } else if (src.y+src.height > dest.y+dest.height){
    out_rect.height -= (src.y+src.height)-(dest.y+dest.height);
  }

  //printf("CLIPPED x=%i,y=%i,w=%i,h=%i\n", out_rect.x, out_rect.y, out_rect.width, out_rect.height);

  *out = out_rect;
}

// Blips src into dest using specified coordinates
void scc_img_blit(scc_img_t* src, scc_img_t* dest, int sx, int sy, int dx, int dy, int width, int height) {
  imgrect_t src_extent;
  imgrect_t dest_extent;
  imgrect_t test_extent;

  int copy_y,dest_sz,src_stride,dest_stride;
  uint8_t * sp;
  uint8_t* dp;

  if (src->bpp != dest->bpp)
    return;

  // Clip source with sx,sy -> width,height
  test_extent.x = test_extent.y = 0;
  test_extent.width = src->w;
  test_extent.height = src->h;

  src_extent.x = sx;
  src_extent.y = sy;
  src_extent.width = width;
  src_extent.height = height;

  scc_clip(src_extent, test_extent, &src_extent);

  // Clip dest with dx,dy -> src_extent.width, src_extent.height
  test_extent.x = test_extent.y = 0;
  test_extent.width = dest->w;
  test_extent.height = dest->h;

  dest_extent.x = dx + (src_extent.x - sx); // add in any offset introduced by the src clip
  dest_extent.y = dy + (src_extent.y - sy);
  dest_extent.width = src_extent.width;
  dest_extent.height = src_extent.height;

  scc_clip(dest_extent, test_extent, &dest_extent);

  src_extent.x = sx + (dest_extent.x - dx); // add in any offset introduced by the dest clip
  src_extent.y = sy + (dest_extent.y - dy);
  src_extent.width = dest_extent.width;
  src_extent.height = dest_extent.height;

  // Exclude bad drawing rects
  if (src_extent.width <= 0 || src_extent.height <= 0)
    return;

  dest_sz     = (dest_extent.width*dest->bpp)>>3;
  src_stride  = (src->w*src->bpp)>>3;
  dest_stride = (dest->w*dest->bpp)>>3;
  sp = &src->data[(((src_extent.y*src->w)+src_extent.x)   * src->bpp)  >> 3];
  dp = &dest->data[(((dest_extent.y*dest->w)+dest_extent.x) * dest->bpp) >> 3];
  /*printf("SCC_BLIT: %i, %i, %i, %i [%i,%i] -> [%i,%i %i,%i], [%i,%i %i,%i]\n",
                    sx,sy,dx,dy,width,height, 
                    src_extent.x,src_extent.y,src_extent.width,src_extent.height,
                    dest_extent.x,dest_extent.y,dest_extent.width,dest_extent.height);*/
  
  // We now have everything we need, so copy!
  for (copy_y=dest_extent.y; copy_y < dest_extent.y+dest_extent.height; copy_y++) {
    memcpy(dp, sp, dest_sz);
    sp += src_stride;
    dp += dest_stride;
  }
}

/// Copies palette from src to dest. Image will be erased if bpp > 8 
void scc_img_copypal(scc_img_t* img, scc_img_t* src) {
  if (img->pal)
    free(img->pal);
  
  img->pal = malloc(src->ncol*3);
  memcpy(img->pal, src->pal, src->ncol*3);
  img->ncol = src->ncol;
  img->bpp = 8;
  
  if (img->bpp > 8) {
    free(img->data);
    img->data = calloc(1,(img->w*img->h*img->bpp)>>3);
  }
}

void scc_img_swapcol(scc_img_t* img, uint8_t from, uint8_t to)
{
  uint8_t* data = img->data;
  uint8_t t;
  int i,j;
  int w = img->w, h = img->h;
  if (img->pal == NULL)
    return;
  
  j = to*3;
  for (i=0; i<3; i++, j++) {
    t = img->pal[(from*3)+i];
    img->pal[(from*3)+i] = img->pal[j];
    img->pal[j] = t;
  }
  
  for (i=0; i<w*h; i++, data++) {
    uint8_t col = *data;
    if (col == from)
      *data = to;
    else if (col == to)
      *data = from;
  }
}

int scc_img_findpixel(scc_img_t* img, int color, int* x, int* y)
{
  int sx, sy;
  uint8_t* data = img->data;
  int w = img->w, h = img->h;
  *x = *y = -1;
  
  switch (img->bpp)
  {
    case 8:
    for (sy=0; sy<h; sy++) {
      for (sx=0; sx<w; sx++) {
        if (*data++ == color) {
          *x = sx;
          *y = sy;
          return 1;
        }
      }
    }
    break;
  }
  return 0;
}

void scc_img_copymask(scc_img_t* img, scc_img_t* dest)
{
  int i;
  int trans = img->trans;
  uint8_t* data = img->data+3;
  uint8_t* dest_data = dest->data;
  int w = img->w, h = img->h;
  
  switch (img->bpp)
  {
    case 8:
      for (i=0; i<w*h; i++) {
        *dest_data++ = (*data++ == trans) ? 0 : 255;
      }
      break;
    case 32:
        for (i=3; i<w*h*4; i+= 4, data += 4) {
          *dest_data++ = *data;
        }
      break;
  }
}

void scc_img_mask(scc_img_t* img)
{
  int i;
  int trans = -1;
  int isize=img->w*img->h;
  uint8_t* data;
  uint8_t* new_data, *new_ptr;
  
  if (img->bpp == 32)
    return;

  // Ditch the palette
  if (img->pal)
    scc_img_unpal(img);

  data = img->data;
  new_data = new_ptr = malloc(isize*4);

  switch (img->bpp)
  {
    case 16:
      for (i=0; i<isize; i++) {
        uint16_t val = ((short*)data)[i];
        new_ptr[0] = val & 0x1F; val >>= 5;
        new_ptr[1] = val & 0x3F; val >>= 6;
        new_ptr[2] = val & 0x1F;
        if (i == 0)
          trans = ((new_ptr[0]) | (new_ptr[1] << 8) | (new_ptr[2] << 16));
        new_ptr[3] = trans >= 0 && (((new_ptr[0]) | (new_ptr[1] << 8) | (new_ptr[2] << 16)) == trans) ? 0 : 255;
        new_ptr += 4;
      }
      break;
    case 24:
      for (i=0; i<isize; i++) {
        uint8_t* ptr = data + (i*3);
        new_ptr[0] = *ptr++;
        new_ptr[1] = *ptr++;
        new_ptr[2] = *ptr;
        if (i == 0)
          trans = ((new_ptr[0]) | (new_ptr[1] << 8) | (new_ptr[2] << 16));
        new_ptr[3] = trans >= 0 && (((new_ptr[0]) | (new_ptr[1] << 8) | (new_ptr[2] << 16)) == trans) ? 0 : 255;
        new_ptr += 4;
      }
      break;
  }
  
  free(data);
  img->data = new_data;
  img->bpp = 32;
}

/// Reduce colors across a set of images
int scc_images_quantize(scc_img_t** imgs, int num, int colors, int dump) {
  // 1) Combine images into single image
  // 2) Reduce result
  // 3) Copy palette and resultant image data back into the images
  int i;
  int dest_height = 0;
  int dest_width = 0;
  int trans_x, trans_y;
  scc_img_t* mask;
  imgpos_t* pos = malloc(sizeof(imgpos_t)*num);

  // 0. Images need to ditch their palette, and they need a mask
  for (i=0; i<num; i++) {
    if (imgs[i]->pal)
      scc_img_unpal(imgs[i]);
    else if (imgs[i]->bpp < 32)
      scc_img_mask(imgs[i]);
  }

  // 1. Determine destination x,y of images
  for (i=0; i<num; i++) {
    pos[i].x = dest_width;
    pos[i].y = 0;
    dest_width += imgs[i]->w;
    dest_height = (imgs[i]->h > dest_height) ? imgs[i]->h : dest_height;
  }

  // 2. Make a new image
  scc_img_t* img = scc_img_new(dest_width, dest_height, 0, 32);

  // ... combine previous images
  for (i=0; i<num; i++) {
    imgpos_t ip = pos[i];
    scc_img_blit(imgs[i], img, 0, 0, ip.x, ip.y, imgs[i]->w, imgs[i]->h);
  }
  
  // copy alpha mask and find first instance of transparent color
  mask = scc_img_new(dest_width, dest_height, 256, 8);
  scc_img_copymask(img, mask);
  scc_img_findpixel(mask, 0, &trans_x, &trans_y);

  // 3. Quantize new image
  scc_img_quantize(img, colors);

  // make sure the transparent color (if used) is at the end
  if (trans_x >= 0) {
    uint8_t idx = *(img->data + ((img->w*trans_y)+trans_x));
    scc_log(LOG_V, "Transparent color @ %i,%i = %i\n", trans_x, trans_y, idx, mask->bpp);
    img->trans = img->ncol-1;
    scc_img_swapcol(img, idx, img->ncol-1);
    /* // Paint all transparent areas again
    if (trans_x >= 0) {
      uint8_t* data = img->data;
      uint8_t* mask_data = mask->data;
      int ext = img->w*img->h;
      for (i=0; i<ext; i++) {
        if (*mask_data < 255)
          *data = img->ncol-1;
        data++;mask_data++;
      }
    }*/
  }
  
  if (dump)
    scc_img_save_bmp(img, "combined.bmp"); // DEBUG
  
  if (mask)
    scc_img_free(mask);
  
  // 4. Extract images out of new image
  for (i=0; i<num; i++) {
    imgpos_t ip = pos[i];
    scc_img_copypal(imgs[i], img); // clone palette
    scc_img_blit(img, imgs[i], ip.x, ip.y, 0, 0, imgs[i]->w, imgs[i]->h);
  }

  free(img);
  free(pos);
  return 1;
}

//#define SCC_IMG_TEST 1
#ifdef SCC_IMG_TEST

int main(int argc,char** argv) {
  scc_img_t* img;
  scc_img_t* img2;
  scc_img_t* imgs[2];
  

  if(argc < 2) {
    printf("We need an argument\n");
    return -1;
  }

  img = scc_img_open(argv[1]);
  if(!img) return -1;

  printf("Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n\n",
   img->w,img->h,img->ncol,img->trans,img->bpp);
  
  // Test Quantize
  scc_img_quantize(img, 255);
  printf("Quantized Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n\n",
       img->w,img->h,img->ncol,img->trans,img->bpp);
  scc_img_save_bmp(img, "dump_quant.bmp");
  img2 = scc_img_open("dump_quant.bmp");
  printf("Reloaded Quantized Width: %d\nHeight: %d\nNum colors: %d\nTransparent color: %d\nBPP: %d\n\n",
       img->w,img->h,img->ncol,img->trans,img->bpp);
  scc_img_save_bmp(img2, "dump_quant_reloaded.bmp");
  scc_img_swapchannels(img2);
  
  // Test combiner
  imgs[0] = img; imgs[1] = img2;
  scc_images_quantize(imgs, 2, 255, 1);
  scc_img_save_bmp(imgs[0], "dump_0.bmp");
  scc_img_save_bmp(imgs[1], "dump_1.bmp");
  
  scc_img_free(img);
  scc_img_free(img2);

  return 0;

}

#endif
