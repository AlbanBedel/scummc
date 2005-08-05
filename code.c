/* ScummC
 * Copyright (C) 2004-2005  Alban Bedel
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

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc.h"

static int compute_shr(uint8_t* src,int src_stride,
		       int width,int height) {
  int l,c,shr = 4,mask = 0xFF >> (8 -shr);

  for(l = 0 ; l < height ; l++) {
    for(c = (l>0?0:1) ; c < width ; c++) {
      while((src[c] & mask) != src[c]) {
	shr++;
	mask = 0xFF >> (8 -shr);
	if(shr == 8) return 8;
      }
    }
    src += src_stride;
  }
  return shr;    
}

#define WRITE_BITS while(cl >= 8) {  \
	*dst=bits & 0xFF;            \
 	bits >>= 8;                  \
	cl -= 8;                     \
	len++;                       \
	dst++;                       \
}

#define FLUSH_BITS WRITE_BITS \
   if(cl > 0) {       \
	*dst=bits & (0xFF >> (8-cl)); \
	cl = bits = 0;                \
	len++;                        \
        dst++;                        \
}

#define WRITE_BIT(b) {       \
	bits |= (b & 1)<<cl; \
	cl++;                \
	WRITE_BITS           \
}

#define WRITE_8(b) {            \
	bits |= (b & 0xFF)<<cl; \
	cl+=8;                  \
	WRITE_BITS              \
}

#define WRITE_NBIT(b,n) {                 \
	bits |= ((b) & (0xff >> (8-n)))<<cl;  \
	cl += n;                          \
	WRITE_BITS                        \
}

int unkCodeA(uint8_t* dst,uint8_t* src,int src_stride,
		     int width,int height,int shr) {
  uint32_t bits = 0;
  uint32_t cl = 0;
  uint32_t len = 0;
  int l,c,inc,rep = 0,color = -1;

  for(l = 0 ; l < height ; l++) {
    for(c = 0 ; c < width ; c++) {
      // Write the first color as is
      if(color < 0) {
	//	printf("Start color: %x %x %x\n",src[c],src[c+1],src[c+2]);
	color = src[c];
	WRITE_8(color);
	continue;
      }
      if(src[c] == color) {
	rep++;
	if(rep < 255) continue;
      }

      // Repeating
      //      if(rep >= 5+8) {
      if(rep >= 13) {
	WRITE_BIT(1);
	WRITE_BIT(1);

	WRITE_NBIT(4,3);
	WRITE_8(rep);
      } else if(rep > 0) {
	while(rep) {
	  WRITE_BIT(0);
	  rep--;
	}
      }
      rep = 0;
      if(src[c] == color) continue;

      // If we are near enouh use an inc
      inc = src[c]-color;
      if(inc >= -4 && inc < 4) {
	WRITE_BIT(1);
	WRITE_BIT(1);

	WRITE_NBIT(inc+4,3);
      } else { // Otherwise write the color
	WRITE_BIT(1);
	WRITE_BIT(0);
	
	WRITE_NBIT(src[c],shr);
      }
      color = src[c];
    }
    src += src_stride;
  }

  // Write the last pixel
  // Repeating
  if(rep >= 5+8) {
    WRITE_BIT(1);
    WRITE_BIT(1);
    
    WRITE_NBIT(4,3);
    WRITE_8(rep);
  } else if(rep > 0) {
    while(rep) {
      WRITE_BIT(0);
      rep--;
    }
  }

  FLUSH_BITS;
  

  return len;
}


int unkCodeA6(uint8_t* dst,uint8_t* src,int src_stride,
		     int width,int height,int shr) {
  uint32_t bits = 0;
  uint32_t cl = 0;
  uint32_t len = 0;
  int l,c,inc,color = -1;

  for(l = 0 ; l < height ; l++) {
    for(c = 0 ; c < width ; c++) {
      // Write the first color as is
      if(color < 0) {
	//	printf("Start color: %x %x %x\n",src[c],src[c+1],src[c+2]);
	color = src[c];
	WRITE_8(color);
	continue;
      }

      if(src[c] == color) {
	WRITE_BIT(0);
	continue;
      }

      // If we are near enouh use an inc
      inc = src[c]-color;
      if(inc >= -4 && inc < 4) {
	WRITE_BIT(1);
	WRITE_BIT(1);

	WRITE_NBIT(inc+4,3);
      } else { // Otherwise write the color
	WRITE_BIT(1);
	WRITE_BIT(0);
	
	WRITE_NBIT(src[c],shr);
      }
      color = src[c];
    }
    src += src_stride;
  }

  FLUSH_BITS;
  

  return len;
}

int unkCodeB(uint8_t* dst,uint8_t* src,int src_stride,
		    int width,int height,int shr) {
  uint32_t bits = 0;
  uint32_t cl = 0;
  uint32_t len = 0;
  int l,c,inc = -1,color = -1;

  for(l = 0 ; l < height ; l++) {
    for(c = 0 ; c < width ; c++) {
      if(color < 0) {
	color = src[c];
	WRITE_8(color);
	continue;
      }
      if(src[c] == color) {
	WRITE_BIT(0);
	continue;
      }
      WRITE_BIT(1);
     
      if(src[c] == color + inc) {
	WRITE_BIT(1);
	WRITE_BIT(0);
	color += inc;
      } else if(src[c] == color -inc) {
	WRITE_BIT(1);
	WRITE_BIT(1);
	inc = -inc;
	color += inc;
      } else {
	WRITE_BIT(0);
	WRITE_NBIT(src[c],shr);
	color = src[c];
	inc = -1;
      }
    }
    src += src_stride;
  }
  FLUSH_BITS;
  return len;
}

int unkCodeC(uint8_t* dst,uint8_t* src,int src_stride,
		    int width,int height,int shr) {
  uint32_t bits = 0;
  uint32_t cl = 0;
  uint32_t len = 0;
  int l,c,inc = -1,color = -1;

  for(c = 0 ; c < width ; c++) {
    for(l = 0 ; l < height ; l++) {
      if(color < 0) {
	color = src[l];
	WRITE_8(color);
	continue;
      }
      if(src[l*src_stride] == color) {
	WRITE_BIT(0);
	continue;
      }
      WRITE_BIT(1);
     
      if(src[l*src_stride] == color + inc) {
	WRITE_BIT(1);
	WRITE_BIT(0);
	color += inc;
      } else if(src[l*src_stride] == color -inc) {
	WRITE_BIT(1);
	WRITE_BIT(1);
	inc = -inc;
	color += inc;
      } else {
	color = src[l*src_stride];
	inc = -1;
	WRITE_BIT(0);
	WRITE_NBIT(color,shr);
      }
    }
    src++;
  }
  FLUSH_BITS;

  return len;
}

typedef int (*unkCode_f)(uint8_t* dst,uint8_t* src,int src_stride,
			 int width,int height,int shr);
struct scc_img_coder_st {
  unkCode_f code;
  int opaque,trans;
} coders[] = {
  { unkCodeA,100,120 },
  { unkCodeA6,60,80 },
  { unkCodeB,20,40 },
  { unkCodeC,10,30 },
  { NULL, 0, 0, },
};

int scc_code_image(uint8_t* src, int src_stride,
		   int width,int height,int transparentColor,
		   uint8_t** smap_p) {
  int stripes = width/8;
  int codecs[stripes];
  int len = 0,slen,c,pos,i,j;
  int shr;
  uint8_t *tbuf,*smap;

  if(stripes*8 != width) {
    printf("Can't encode image with width %% 8 != 0 !!!!\n");
    return 0;
  }

  // should be enouth
  tbuf = malloc(height*8*2);

  // find the best codec for each stripe
  for(i = 0 ; i < stripes ; i++) {
    // compute the shr
    shr = compute_shr(&src[8*i],src_stride,8,height);
    // and try
    slen = coders[0].code(tbuf,&src[8*i],src_stride,8,height,shr);
    for(c = 0, j = 1 ; coders[j].code ; j++) {
      int l = coders[j].code(tbuf,&src[8*i],src_stride,8,height,shr);
      if(l < slen) {
	c = j;
	slen = l;
      }
    }
    codecs[i] = shr+c*10;
    len += slen+1;
  }
  free(tbuf);

  pos = stripes*4;
  len += pos;
  len = ((len+1)/2)*2;
  smap = malloc(len);
  
  for(i = 0 ; i < stripes ; i++) {
    // get the shr back
    shr = codecs[i] % 10;
    // put the offset table entry
    SCC_SET_32LE(smap,i*4,pos+8);
    // code
    smap[pos] = ((transparentColor >= 0) ? coders[codecs[i]/10].trans :
		 coders[codecs[i]/10].opaque) + shr;
    pos++;
    pos += coders[codecs[i]/10].code(&smap[pos],&src[8*i],src_stride,8,height,shr);
    if(pos > len) {
      printf("Big problem %d > %d !!!!\n",pos,len+stripes*4);
      free(smap);
      return 0;
    }
  }

  smap_p[0] = smap;

  return len;
}


int check_coder(uint8_t* dec, int dec_stride,
		uint8_t* ref,int ref_size,int height,
		int (*unkCode)(uint8_t* dst,uint8_t* src,int src_stride,
			       int width,int height,int shr),
		void (*unkDecode)(uint8_t *dst, int dst_stride,
				  uint8_t *src, int height,
				  uint32_t palette_mod,
				  uint32_t decomp_mask,uint32_t decomp_shr)
		) {
  uint8_t* test = calloc(1,8*height*3/2);
  uint8_t* final = calloc(1,18*height);
  int i,j,len,shr = compute_shr(dec,dec_stride,8,height);
  int c = 0;
  //printf("Computed shr: %d\n",shr);

  for(j = 0,i = 0 ; coders[i].code ; i++) {
    len = coders[i].code(test,dec,dec_stride,8,height,shr);
    if(!j) j = len;
    else if(len < j) j = len, c = i;
    //    printf("%soder %c: %d\n",(coders[i] == unkCode ? "Selectec c" : "C"),
    //			      i+'A',len);
    if(coders[i].code == unkCode) printf("Testing codec: %c\n",i+'A');
  }

  len = unkCode(test,dec,dec_stride,8,height,shr);

  if(coders[c].code != unkCode) printf("It wasn't the best codec (%d > %d (%d: %d %%)) ??\n",len,j,c,len*100/j);

  unkDecode(final,18,test,height,0,0xFF >> (8-shr),shr);

  if(len != ref_size) printf("Stripe size mismatch: %d instead of %d\n",len,ref_size);
  else printf("Stripe size match\n");
  for(i = 0 ; i < len ; i++) {
    if(test[i] != ref[i]) { 
      printf("Mismatch at 0x%x/0x%x : 0x%x 0x%x\n",
	     i,len,test[i],ref[i]);
      printf("Ref data: %0x %0x %0x %0x\n",ref[i-1],ref[i],ref[i+1],ref[i+230]);
      printf("Our data: %0x %0x %0x %0x\n",test[i-1],test[i],test[i+1],test[i+2]);
      break;
    }
  }

 
  for(j = 0 ; j < height ; j++) {
    for(i = 0 ; i < 8 ; i++) {
      if(final[i+j*18] != dec[i+j*dec_stride]) {
	printf("First mismatch at 0x%x/0x%x : %x %x %x <=> %x %x %x %x\n",i,8*height,
	       final[i+j*18-1],final[i+j*18],final[i+j*18+1],
	       dec[i+j*dec_stride-1],dec[i+j*dec_stride],dec[i+j*dec_stride+1],dec[i+j*dec_stride+2]);
	free(test);
	return 0;
      }
    }
  }
    
  free(test);
  free(final);
  return 1;
}



static int compMask(uint8_t *dst, 
		    uint8_t *src, int src_stride,
		    int height) {
  uint8_t buf[height];
  int ds = -1,rs = -1,r = 0;
  int l;
  int len = 0;

  
  for(l = 0 ; l < height ; l++)
    buf[l] = src[l*src_stride];

  for(l = 0 ; l < height ; l++) {

    if(l+1 < height && buf[l+1] == buf[l]) {
      if(rs < 0) rs = l, r = 1;
      r++;
      continue;
    }

    if(r > 2) {
      if(ds >= 0) {
	*dst = rs-ds;
	memcpy(dst+1,&buf[ds],dst[0]);
	len += dst[0]+1;
	dst += dst[0]+1;
      } 
      //else if(r >= height && buf[l] == 0) return 0;
      // scummvm have no pb with this, but not the original engine.
      while(r > 0) {
	uint8_t v = (r > 0x7f) ? 0x7f : r;
	*dst = v | 0x80; dst++;
	*dst = buf[l]; dst++;
	len += 2;
	r -= v;
      }
      ds = rs = -1;
      continue;
    }

    if(ds < 0) {
      if(rs >= 0) ds = rs;
      else ds = l;
    }

    r = 0;
    rs = -1;



    if(l+1 < height && l+1-ds < 0x7f) continue;
    
    *dst = l+1-ds;
    memcpy(dst+1,&buf[ds],dst[0]);
    len += dst[0]+1;
    dst += dst[0]+1;
    ds = -1;
  }

  return len;  
}


int scc_code_zbuf(uint8_t* src, int src_stride,
		  int width,int height,
		  uint8_t** smap_p) {
  int strides = width/8;
  int slen,len,i,pos = strides*2;
  uint8_t tmp[2*height];
  uint8_t* smap;

  *smap_p = NULL;

  for(len = i = 0 ; i < strides ; i++)
    len += compMask(tmp,&src[i],src_stride,height);

  len += pos;
  smap = malloc(len);

  for(i = 0 ; i < strides ; i++) {
    slen = compMask(&smap[pos],&src[i],src_stride,height);
    SCC_SET_16LE(smap,i*2,(slen) ? pos+8 : 0);
    pos += slen;
    if(pos > len) {
      printf("Big problem while coding zplane\n");
      break;
    }
  }

  *smap_p = smap;
  return len;
}
