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
 * @file decode.c
 * @ingroup scumm
 * @brief SCUMM image decoders
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

#define READ_BIT (cl--, bit = bits&1, bits>>=1, bit)
#define FILL_BITS if(cl < 8) { \
  bits |= (*src++) << cl; \
  cl += 8; \
} 
 

static void unkDecodeA(uint8_t *dst, int dst_stride,
		       uint8_t *src, int height,
		       uint32_t palette_mod,
		       uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  int16_t incm, reps;

  //  printf("Start data: %x %x %x %x\n",color,bits,src[0],src[1]);
  
  do {
    int x = 8;
    do {
      FILL_BITS;
      *dst++ = color + palette_mod;
      
    againPos:
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
      } else {
	incm = (bits & 7) - 4;
	cl -= 3;
	bits >>= 3;
	if (incm) {
	  color += incm;
	} else {
	  FILL_BITS;
	  reps = bits & 0xFF;
	  do {
	    if (!--x) {
	      x = 8;
	      dst += dst_stride - 8;
	      if (!--height)
		return;
	    }
	    *dst++ = color + palette_mod;
	  } while (--reps);
	  bits >>= 8;
	  bits |= (*src++) << (cl - 8);
	  goto againPos;
	}
      }
      x--;
    } while (x);
    dst += dst_stride - 8;
    height--;
  } while (height);
}


static void unkDecodeA_trans(uint8_t *dst, int dst_stride,
			     uint8_t *src, int height,
			     uint32_t palette_mod,uint8_t transparentColor,
			     uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  uint8_t incm, reps;

  do {
    int x = 8;
    do {
      FILL_BITS;
      if (color != transparentColor)
	*dst = color + palette_mod;
      dst++;
      
    againPos:
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
      } else {
	incm = (bits & 7) - 4;
	cl -= 3;
	bits >>= 3;
	if (incm) {
	  color += incm;
	} else {
	  FILL_BITS;
	  reps = bits & 0xFF;
	  do {
	    if (!--x) {
	      x = 8;
	      dst += dst_stride - 8;
	      if (!--height)
		return;
	    }
	    if (color != transparentColor)
	      *dst = color + palette_mod;
	    dst++;
	  } while (--reps);
	  bits >>= 8;
	  bits |= (*src++) << (cl - 8);
	  goto againPos;
	}
      }
    } while (--x);
    dst += dst_stride - 8;
  } while (--height);
}

void unkDecodeB(uint8_t *dst, int dst_stride,
		uint8_t *src, int height,
		uint32_t palette_mod,
		uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  int8_t inc = -1;
  
  do {
    int x = 8;
    do {
      FILL_BITS;
      *dst++ = color + palette_mod;
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
	inc = -1;
      } else if (!READ_BIT) {
	color += inc;
      } else {
	inc = -inc;
	color += inc;
      }
    } while (--x);
    dst += dst_stride - 8;
  } while (--height);
}

void unkDecodeB_trans(uint8_t *dst, int dst_stride,
		      uint8_t *src, int height,
		      uint32_t palette_mod,uint8_t transparentColor,
		      uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  int8_t inc = -1;

  do {
    int x = 8;
    do {
      FILL_BITS;
      if (color != transparentColor)
	*dst = color + palette_mod;
      dst++;
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
	inc = -1;
      } else if (!READ_BIT) {
	color += inc;
      } else {
	inc = -inc;
	color += inc;
      }
    } while (--x);
    dst += dst_stride - 8;
  } while (--height);
}

void unkDecodeC(uint8_t *dst, int dst_stride,
		uint8_t *src, int height,
		uint32_t palette_mod,
		uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  int8_t inc = -1;
  
  int x = 8;
  do {
    int h = height;
    do {
      FILL_BITS;
      *dst = color + palette_mod;
      dst += dst_stride;
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
	inc = -1;
      } else if (!READ_BIT) {
	color += inc;
      } else {
	inc = -inc;
	color += inc;
      }
    } while (--h);
    dst -= height*dst_stride - 1;
  } while (--x);
}

void unkDecodeC_trans(uint8_t *dst, int dst_stride,
			     uint8_t *src, int height,
			     uint32_t palette_mod,uint8_t transparentColor,
			     uint32_t decomp_mask,uint32_t decomp_shr) {
  uint8_t color = *src++;
  uint32_t bits = *src++;
  uint8_t cl = 8;
  uint8_t bit;
  int8_t inc = -1;

  int x = 8;
  do {
    int h = height;
    do {
      FILL_BITS;
      if (color != transparentColor)
	*dst = color + palette_mod;
      dst += dst_stride;
      if (!READ_BIT) {
      } else if (!READ_BIT) {
	FILL_BITS;
	color = bits & decomp_mask;
	bits >>= decomp_shr;
	cl -= decomp_shr;
	inc = -1;
      } else if (!READ_BIT) {
	color += inc;
      } else {
	inc = -inc;
	color += inc;
      }
    } while (--h);
    dst -= height*dst_stride - 1;
  } while (--x);
}


int scc_decode_image(uint8_t* dst, int dst_stride,
		     int width,int height,
		     uint8_t* smap,uint32_t smap_size,
		     int transparentColor) {
  int i,offs = (width/8);
  uint8_t type;
  uint32_t decomp_shr,decomp_mask;
  int stripe_size;
  int have_trans = 0;

  scc_log(LOG_V,"\nDecode image: %dx%d smap: %d\n",width,height,smap_size);

  for(i = 0 ; i < offs ; i++) {
    int o = ((uint32_t*)smap)[i]-8;
    if(i+1 < offs)
      stripe_size = ((uint32_t*)smap)[i+1] - ((uint32_t*)smap)[i];
    else
      stripe_size = smap_size-o;
    stripe_size -= 1;
    //scc_log(LOG_V,"Stripe %d: 0x%x\n",i,o);
    type = smap[o];
    scc_log(LOG_V,"SMAP type: %d (0x%x)\n",type,type);
    decomp_shr = type % 10;
    decomp_mask = 0xFF >> (8 - decomp_shr);

    scc_log(LOG_V,"Decomp_shr: %d (0x%x)\n",decomp_shr,decomp_mask);

    switch(type) {
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
      unkDecodeC(dst + i*8,dst_stride,&smap[o+1],
		 height,0,
		 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeC,unkDecodeC);
      break;

    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
      unkDecodeB(dst + i*8,dst_stride,&smap[o+1],
		 height,0,
		 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeB,unkDecodeB);
      break;

    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
      have_trans = 1;
      if(transparentColor < 0)
	unkDecodeC(dst + i*8,dst_stride,&smap[o+1],
		   height,0,
		   decomp_mask,decomp_shr);
      else
	unkDecodeC_trans(dst + i*8,dst_stride,&smap[o+1],
			 height,0,transparentColor,
			 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeC,unkDecodeC);
      break;

    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
      have_trans = 1;
      if(transparentColor < 0)
	unkDecodeB(dst + i*8,dst_stride,&smap[o+1],
		   height,0,
		   decomp_mask,decomp_shr);
      else
	unkDecodeB_trans(dst + i*8,dst_stride,&smap[o+1],
			 height,0,transparentColor,
			 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeB,unkDecodeB);
      break;
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
      unkDecodeA(dst + i*8,dst_stride,&smap[o+1],
		 height,0,
		 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeA6,unkDecodeA);
      break;
    case 104:
    case 105:
    case 106:
    case 107:
    case 108:
      unkDecodeA(dst + i*8,dst_stride,&smap[o+1],
		 height,0,
		 decomp_mask,decomp_shr);
      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeA,unkDecodeA);
      break;
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 124:
    case 125:
    case 126:
    case 127:
    case 128:
      have_trans = 1;
      if(transparentColor < 0)
	unkDecodeA(dst + i*8,dst_stride,&smap[o+1],
		   height,0,
		   decomp_mask,decomp_shr);
      else
	unkDecodeA_trans(dst + i*8,dst_stride,&smap[o+1],
			 height,0,transparentColor,
			 decomp_mask,decomp_shr);

      //check_coder(dst + i*8,dst_stride,&smap[o+1],stripe_size,height,unkCodeA,unkDecodeA);
      break;
    default:
      scc_log(LOG_ERR,"Unknown image coding: %d\n",type);
      break;
    }
  }

  return 1+have_trans;
}

static void decompMask(uint8_t *dst, int dst_stride,
		       uint8_t *src, int height) {

  uint8_t b, c;

  while (height) {
    b = *src++;
    
    if (b & 0x80) {
      b &= 0x7F;
      c = *src++;

      do {
	*dst = c;
	dst += dst_stride;
	--height;
      } while (--b && height);
    } else {
      do {
	*dst = *src++;
	dst += dst_stride;
	--height;
      } while (--b && height);
    }
  }
}

static void decompMaskOr(uint8_t *dst, int dst_stride,
			 uint8_t *src, int height) {

  uint8_t b, c;

  while (height) {
    b = *src++;
    
    if (b & 0x80) {
      b &= 0x7F;
      c = *src++;

      do {
	*dst |= c;
	dst += dst_stride;
	--height;
      } while (--b && height);
    } else {
      do {
	*dst |= *src++;
	dst += dst_stride;
	--height;
      } while (--b && height);
    }
  }
}

int scc_decode_zbuf(uint8_t* dst, int dst_stride,
		    int width,int height,
		    uint8_t* zmap,uint32_t zmap_size,int or) {
  int stripes = width/8;
  int i;

  if(width != stripes*8) {
    scc_log(LOG_V,"Can't decode zbuf if width %% 8 != 0 !!\n");
    return 0;
  }

  for(i = 0 ; i < stripes ; i++) {
    int o = ((uint16_t*)zmap)[i];
    
    if(o) {
      o -= 8;
      if(or)
	decompMaskOr(&dst[i],dst_stride,
		     &zmap[o],height);
      else
	decompMask(&dst[i],dst_stride,
		   &zmap[o],height);
    } else {
      int j;
      for(j = 0 ; j < height ; j++)
	dst[i+j*dst_stride] = 0;
    }
  }
  return 1;
}
