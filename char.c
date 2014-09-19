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
 * @file char.c
 * @ingroup scumm
 * @brief SCUMM charset generator
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
#include <ctype.h>

#ifdef HAVE_FT
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include "scc_util.h"
#include "scc_fd.h"
#include "scc_img.h"
#include "scc_param.h"
#include "scc_char.h"

#include "char_help.h"

uint8_t default_pal[] = {
    0x00, 0x00, 0x00, // 0
    0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0xAD,
    0x00, 0xAA, 0x00,
    0x00, 0xAA, 0xAD, // 4
    0xAD, 0x00, 0x00,
    0xAD, 0x00, 0xAD,
    0xAD, 0x55, 0x00,
    0xAD, 0xAA, 0xAD, // 8
    0x52, 0x55, 0x52,
    0x52, 0x55, 0xFF,
    0x52, 0xFF, 0x52,
    0x52, 0xFF, 0xFF, // 12
    0xFF, 0x55, 0x52,
    0xFF, 0x55, 0x55,
    0xFF, 0xFF, 0x52,
};

#ifdef HAVE_FT

static FT_GlyphSlot ft_render_char(FT_Face face,int ch) {
  FT_ULong gidx;
  FT_Error err;
    
  gidx = FT_Get_Char_Index(face,ch);
  err = FT_Load_Glyph(face, gidx, FT_LOAD_DEFAULT);
  if(err) {
    printf("Failed to load character %d (%c)\n",
           ch,isprint(ch) ? ch : ' ');
    return NULL;
  }
  err = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
  if(err) {
    printf("Failed to render character %d (%c)\n",
           ch,isprint(ch) ? ch : ' ');
    return NULL;
  }
  return face->glyph;
}

scc_charmap_t* new_charmap_from_ft(int* chars,unsigned num_char,
                                   char* font, int face_id,
                                   int char_w, int char_h,
                                   int hdpi, int vdpi,
                                   int linespace) {
  static int inited_ft = 0;
  static FT_Library ft;
  FT_Face face;
  FT_GlyphSlot slot;
  FT_Error err;
  int i,r,c,top = 0,bottom = 0,left = 0,right = 0;
  scc_charmap_t* chmap;
  scc_char_t* ch;
  uint8_t *src,*dst;

  // init freetype once
  if(!inited_ft) {
    if(FT_Init_FreeType(&ft)) {
      printf("Failed to init freetype.\n");
      return NULL;
    }
    inited_ft = 1;
  }

  // load the font
  err = FT_New_Face(ft,font,face_id,&face);
  if(err == FT_Err_Unknown_File_Format) {
    printf("%s: Unknown file format.\n",font);
    return NULL;
  } else if(err) {
    printf("%s: Failed to open font file.\n",font);
    return NULL;
  }

  // set the char size
  if(FT_Set_Char_Size(face,char_w,char_h,hdpi,vdpi)) {
    printf("Failed to set character size to %dx%d with dpi %dx%d.\n",
           char_w,char_h,hdpi,vdpi);
    FT_Done_Face(face);
    return NULL;
  }
    
  // first we check all chars to make
    
  for(i = 0 ; i < num_char ; i++) {
    if(!chars[i]) continue;
    if(!(slot = ft_render_char(face,chars[i]))) {
      chars[i] = 0;
      continue;
    }

    // look for bigger glyphs
    if(slot->bitmap_top > top)
      top = slot->bitmap_top;
    if(slot->bitmap_top - slot->bitmap.rows < bottom)
      bottom = slot->bitmap_top - slot->bitmap.rows;
        
    if(slot->bitmap_left < left)
      left = slot->bitmap_left;

    if(slot->bitmap_left+slot->bitmap.width > 255) {
        printf("Warning, character too wide: %c (%d)\n",
               chars[i],chars[i]);
        chars[i] = 0;
        continue;
    }

    if(slot->bitmap_left+slot->bitmap.width > right)
      right = slot->bitmap_left+slot->bitmap.width;
    // in scumm the advance is done with width+offset
    // so we must include it if it's bigger
    if((slot->advance.x >> 6) > right)
      right = slot->advance.x >> 6;
        
  }

  chmap = calloc(1,sizeof(scc_charmap_t));
  chmap->bpp = 1;
  chmap->height = top - bottom + linespace;
  chmap->width = right - left;
    
  for(i = 0 ; i < num_char ; i++) {
    if(!chars[i]) continue;
    if(!(slot = ft_render_char(face,chars[i]))) {
      chars[i] = 0;
      continue;
    }
    ch = &chmap->chars[i];

    ch->x = slot->bitmap_left;
    ch->y = top - slot->bitmap_top;
    if((slot->advance.x>>6) > slot->bitmap_left+slot->bitmap.width)
      ch->w = (slot->advance.x>>6) - ch->x;
    else
      ch->w = slot->bitmap.width;
    ch->h = slot->bitmap.rows;

    ch->data = calloc(ch->w,ch->h);
    src = slot->bitmap.buffer;
    dst = ch->data;
    for(r = 0 ; r < ch->h ; r++) {
      for(c = 0 ; c < slot->bitmap.width ; c++)
        dst[c] = (src[c/8] & (0x80 >> (c%8))) ? 1 : 0;
      dst += ch->w;
      src += slot->bitmap.pitch;
    }
    chmap->max_char = i;
  }
    
  return chmap;
}

#endif // HAVE_FT

void import_char(scc_char_t* ch,char* src, int stride,int w, int h) {
    int x,y,s;
    
    if(w <= 0 || h <= 0) {
        ch->w = 0;
        ch->h = 0;
        return;
    }

    for(s = 1, y = 0 ; y < h ; y++) {
        for(x = 0 ; x < w ; x++)
            if(src[y*stride+x]) {
                s = 0; 
                break;
            }
        if(!s) break;
    }
    if(y >= h) {
        ch->data = calloc(1,1);
        ch->x = w-1;
        ch->y = h-1;
        ch->w = ch->h = 1;
        return;
    }
    ch->y = y;
    ch->h = h - y;
    for(s = 1, x = 0 ; x < w ; x++) {
        for(y = ch->y ; y < h ; y++)
            if(src[y*stride+x]) {
                s = 0;
                break;
            }
        if(!s) break;
    }

    ch->x = x;
    ch->w = w - x;

    ch->data = malloc(ch->w*ch->h);

    for(y = 0 ; y < ch->h ; y++)
        memcpy(&ch->data[y*ch->w],&src[(y+ch->y)*stride+ch->x],ch->w);
}

scc_charmap_t* new_charmap_from_bitmap(char* path, int base_idx) {
    scc_img_t* img = scc_img_open(path);
    char bcol = img->ncol-1;
    int x,y,x2,y2,w,h,idx = base_idx;
    char* src;
    scc_charmap_t* chmap;

    if(!img) {
        printf("Failed to open %s\n",path);
        return NULL;
    }
    // check the palette
    // keep in mind we need an extra color for our "borders"
    if (img->pal == NULL) {
      scc_log(LOG_ERR,"Image must have a palette.\n");
      scc_img_free(img);
      return NULL;
    }
    if(img->ncol > 17) {
        printf("Image has too many colors.\n");
        scc_img_free(img);
        return NULL;
    }
    if(img->ncol < 3) {
        printf("Image has too few colors.\n");
        scc_img_free(img);
        return NULL;
    }

    chmap = calloc(1,sizeof(scc_charmap_t));

    if(img->ncol > 9)
        chmap->bpp = 4;
    else if(img->ncol > 5)
        chmap->bpp = 3;
    else if(img->ncol >= 3)
        chmap->bpp = 2;
    else
        chmap->bpp = 1;
    

    y = 1;
    src = img->data+img->w;
    while(y < img->h) {
        x = 0;
        while(x < img->w) {
            // look for the start of a border
            if(src[x] != bcol || src[x-(int)img->w] == bcol) {
                x++;
                continue;
            }
            // look for some width
            for( x2 = x+1 ; x2 < img->w ; x2++)
                if(src[x2] != bcol) break;
            w = x2-x-2;
            if(w < 0) {
                x++;
                continue;
            }
            // and some height
            for( y2 = y+1 ; y2 < img->h ; y2++)
                if(img->data[img->w*y2+x] != bcol) break;
            h = y2-y-2;
            if(h < 0) {
                x++;
                continue;
            }
            //printf("Found char %d at %dx%d (%dx%d)\n",idx,x+1,y+1,w,h);

            if(w > chmap->width) chmap->width = w;
            if(h > chmap->height) chmap->height = h;

            // read the char
            import_char(&chmap->chars[idx],&src[img->w+x+1],
                        img->w,w,h);
            
            idx++;
            x = x2;
        }
        src += img->w;
        y++;
    }
    chmap->max_char = idx-1;

    return chmap;
}

scc_charmap_t* charmap_from_char(char* path) {
    scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
    uint32_t size,fmt;
    scc_charmap_t* chmap;

    if(!fd) {
        printf("Failed to open %s\n",path);
        return NULL;
    }

    // read the scumm block header
    fmt = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);

    if(fmt != MKID('C','H','A','R') || size < 8+25 ) {
        printf("%s doesn't seem to be a valid CHAR file.\n",path);
        scc_fd_close(fd);
        return NULL;
    }
    chmap = scc_parse_charmap(fd,size-8);
    scc_fd_close(fd);
    return chmap;
}

scc_charmap_t* charmap_from_nut(char* path) {
    scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
    uint32_t size,fmt;
    scc_charmap_t* chmap;

    if(!fd) {
        printf("Failed to open %s\n",path);
        return NULL;
    }

    // read the scumm block header
    fmt = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);

    if(fmt != MKID('A','N','I','M') || size < 16) {
        printf("%s doesn't seem to be a valid NUT file.\n",path);
        scc_fd_close(fd);
        return NULL;
    }
    chmap = scc_parse_nutmap(fd,size);
    scc_fd_close(fd);
    return chmap;
}

void charmap_render_char(uint8_t* dst,unsigned dst_stride,
                         uint8_t* src,unsigned src_stride,
                         unsigned h) {
  while(h > 0) {
    memcpy(dst,src,src_stride);
    dst += dst_stride;
    src += src_stride;
    h--;
  }

}

scc_img_t* charmap_to_bitmap(scc_charmap_t* chmap, unsigned width,unsigned space,
                             int pal_size) {
  scc_img_t* img;
  int cpl,rows,ncol,total_color,border;
  int i,j,x,y;
  scc_char_t* ch;

  if(width < chmap->width+2*space) {
    printf("Bitmap width is too small for exporting charmap.\n"
           "%d is needed for this charmap.\n",chmap->width+2*space);
    return NULL;
  }

  cpl = width / (chmap->width+2*space);
  rows = (chmap->max_char+cpl-1)/cpl;

  ncol = 1<<chmap->bpp;
  if(chmap->bpp < 8) {
    total_color = ncol+1;
    border = ncol;
  } else {
    total_color = ncol;
    border = 0xFF;
  }

  if(pal_size) {
      if(total_color > pal_size) {
          printf("A bigger palette is needed for this charset (at least %d colors).\n",
                 total_color);
          return NULL;
      }
      total_color = pal_size;
  }

  img = scc_img_new(width,rows*(chmap->height+ 2*space),total_color, 8);
  if(chmap->rgb_pal) {
    memcpy(img->pal,chmap->rgb_pal,3*ncol);
    if(ncol < total_color)
      memset(img->pal+3*ncol,0,(total_color-ncol)*3);
  } else {
    int n = sizeof(default_pal)/3;
    if(n > total_color) n = total_color;
    memcpy(img->pal,default_pal,3*n);
    if(n < total_color)
      memset(img->pal+3*n,0,(total_color-n)*3);
  }
  img->pal[3*border+0] = 0xFF;
  img->pal[3*border+1] = 0;
  img->pal[3*border+2] = 0;


  y = x = space;
  for(i = 0 ; i <= chmap->max_char ; i++) {
    ch = &chmap->chars[i];
      
    memset(&img->data[width*(y-1)+x],border,ch->x+ch->w);
    memset(&img->data[width*(y+chmap->height)+x],border,ch->x+ch->w);
    for(j = -1 ; j <= chmap->height ; j++) {
        img->data[width*(y+j)+x-1] = border;
        img->data[width*(y+j)+x+ch->x+ch->w] = border;
    }

    if(ch->data) charmap_render_char(&img->data[width*(y+ch->y)+x+ch->x],
                                     width,ch->data,ch->w,ch->h);

    x += chmap->width + 2*space;
    if(x + chmap->width + space > width) {
      x = space;
      y += chmap->height + 2*space;
    }
  }
    
    
  return img;

}


int charmap_to_char(scc_charmap_t* chmap, char* path) {
  unsigned i,size;
  uint32_t off;
  scc_char_t *ch;
  scc_fd_t *fd = new_scc_fd(path,O_WRONLY|O_CREAT|O_TRUNC,0);
  uint8_t byte,color,*src;
  unsigned x,bpos,mask;

  switch(chmap->bpp) {
  case 1:
    mask = 1;
    break;
  case 2:
    mask = 3;
    break;
  case 4:
    mask = 0xF;
    break;
  default:
    printf("Unsupported bpp for charset: %d\n",chmap->bpp);
    return 0;
  }

  if(!fd) {
    printf("Failed to open %s for writing.\n",path);
    return 0;
  }

  size =
    4 +    // size-15
    2 +    // version ?
    15 +   // palette
    1 +    // bpp - offset base
    1 +    // font height
    2 +    // num char
    (chmap->max_char+1)*4; // offset table

  for(i = 0 ; i <= chmap->max_char ; i++) {
    if(!chmap->chars[i].data) continue;
    size += 4 + ((chmap->chars[i].w*chmap->chars[i].h)*chmap->bpp + 7)/8;
  }

  // scumm block header
  scc_fd_w32(fd,MKID('C','H','A','R'));
  scc_fd_w32be(fd,8 + size);

  // char header
  scc_fd_w32le(fd,size - 15);
  scc_fd_w16le(fd,0x0363);

  // palette
  for(i = 0 ; i < 15 ; i++)
    scc_fd_w8(fd,chmap->pal[i]);
  // bpp
  scc_fd_w8(fd,chmap->bpp);
  // font height
  scc_fd_w8(fd,chmap->height);

  // num char
  scc_fd_w16le(fd,chmap->max_char+1);
  // offset table
  off = 4 + (chmap->max_char+1)*4;
  for(i = 0 ; i <= chmap->max_char ; i++) {
    if(!chmap->chars[i].data) {
      scc_fd_w32le(fd,0);
      continue;
    }
    scc_fd_w32le(fd,off);
    off += 4 + ((chmap->chars[i].w*chmap->chars[i].h)*chmap->bpp + 7)/8;
  }
  
  // write the chars
  for(i = 0 ; i <= chmap->max_char ; i++) {
    if(!chmap->chars[i].data) continue;
    ch = &chmap->chars[i];
    scc_fd_w8(fd,ch->w);
    scc_fd_w8(fd,ch->h);
    scc_fd_w8(fd,ch->x);
    scc_fd_w8(fd,ch->y);
    // write the data
    byte = 0;
    bpos = 8;
    src = ch->data;
    for(x = 0 ; x < ch->w*ch->h ; x++) {
      color = src[0];
      src++;
      bpos -= chmap->bpp;
      byte |= (color & mask) << bpos;
      if(!bpos) {
        scc_fd_w8(fd,byte);
        byte = 0;
        bpos = 8;
      }
    }
    // write the final byte if it didn't ended on a byte bondary
    if(bpos < 8) scc_fd_w8(fd,byte);
  }

  scc_fd_close(fd);

  return 1;
}


static char* font_file = NULL;
static char* obmp_file = NULL;
static char* ibmp_file = NULL;
static char* ochar_file = NULL;
static char* ichar_file = NULL;
static char* inut_file = NULL;
static int   ibmp_base = 0;
#ifdef HAVE_FT
static int char_width = 0;
static int char_height = 24*64;
static int hdpi = 30;
static int vdpi = 30;
static int vspace = 0;
#endif
static int palsize = 0;
static int bmp_width = 800;
static int bmp_space = 8;

static scc_param_t scc_parse_params[] = {
  { "obmp", SCC_PARAM_STR, 0, 0, &obmp_file },
  { "ibmp", SCC_PARAM_STR, 0, 0, &ibmp_file },
  { "ochar", SCC_PARAM_STR, 0, 0, &ochar_file },
  { "ichar", SCC_PARAM_STR, 0, 0, &ichar_file },
  { "inut",  SCC_PARAM_STR, 0, 0, &inut_file },
  { "ibmp-base", SCC_PARAM_INT, 0, 255, &ibmp_base },
#ifdef HAVE_FT
  { "font", SCC_PARAM_STR, 0, 0, &font_file },
  { "cw", SCC_PARAM_INT, 1, 1000*64, &char_width },
  { "ch", SCC_PARAM_INT, 1, 1000*64, &char_height },
  { "vdpi", SCC_PARAM_INT, 1, 1000, &vdpi },
  { "hdpi", SCC_PARAM_INT, 1, 1000, &hdpi },
  { "vspace", SCC_PARAM_INT, -1000, 1000, &vspace },
#endif
  { "palsize", SCC_PARAM_INT, 2, 256, &palsize },
  { "bmp-width", SCC_PARAM_INT, 1, 10000, &bmp_width },
  { "bmp-space", SCC_PARAM_INT, 3, 10000, &bmp_space },
  { "help", SCC_PARAM_HELP, 0, 0, &char_help },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_cl_arg_t* files;
  scc_charmap_t* chmap;
  scc_img_t* oimg;
  int chars[SCC_MAX_CHAR];
  int i;

  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  
  if(files || !(font_file || ibmp_file || ichar_file || inut_file) ||
     !(obmp_file || ochar_file)) scc_print_help(&char_help,1);

  // init some default chars
  memset(chars,0,SCC_MAX_CHAR*sizeof(int));
#if 0
  for(i = 'a' ; i <= 'z' ; i++)
    chars[i] = i;
  for(i = 'A' ; i <= 'Z' ; i++)
    chars[i] = i;
  for(i = '0' ; i <= '9' ; i++)
    chars[i] = i;
#else
  for(i = 0 ; i < 255 ; i++)
    chars[i] = i;
#endif

#ifdef HAVE_FT
  // size less than two dot don't make much sense
  if(char_width < 128) char_width *= 64;
  if(char_height < 128) char_height *= 64;

  if(font_file)
      chmap = new_charmap_from_ft(chars,SCC_MAX_CHAR,
                                  font_file,0,
                                  char_width,char_height,
                                  hdpi,vdpi,vspace);
  else 
#endif
  if(ichar_file)
      chmap = charmap_from_char(ichar_file);
  else if(inut_file)
      chmap = charmap_from_nut(inut_file);
  else
      chmap = new_charmap_from_bitmap(ibmp_file,ibmp_base);

  if(!chmap) return 1;

  if(obmp_file) {
    oimg = charmap_to_bitmap(chmap,bmp_width,bmp_space,palsize);
    if(!oimg) return 1;

    if(!scc_img_save_bmp(oimg,obmp_file)) return 1;
  }

  if(ochar_file && !charmap_to_char(chmap,ochar_file))
    return 1;

  return 0;
}
