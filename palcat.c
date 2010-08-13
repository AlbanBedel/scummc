/* ScummC
 * Copyright (C) 2005-2006  Alban Bedel
 * Portions Copyright (C) 2010 James Urquhart
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
 * @file palcat.c
 * @brief Tool to concatenate image palette
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
#include "scc_img.h"

#include "palcat_help.h"

static char* outname = NULL;
static int combine_colors = 0;
static int do_dump = 0;
static int make_single_pal = 0;
static int clip_colors = 0;

static scc_param_t scc_parse_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &outname },
  { "d", SCC_PARAM_FLAG, 0, 1, &do_dump },
  { "col", SCC_PARAM_INT, 0, 256, &combine_colors },
  { "pal", SCC_PARAM_FLAG, 0, 1, &make_single_pal },
  { "clip", SCC_PARAM_INT, 0, 256, &clip_colors },
  { "v", SCC_PARAM_FLAG, LOG_MSG, LOG_V, &scc_log_level },
  { NULL, 0, 0, 0, NULL }
};

typedef struct remap_img_s {
  char* path;
  char* name;
  char* basepath;
  scc_img_t* img;
  
  int mask_x;
  int mask_y;
} remap_img_t;

static char name_buffer[4096];
char* calcbasepath(char* fname) {
  char* ptr = fname;
  char* last = ptr;
  while (*ptr != '\0') {
    if (*ptr == '/' || *ptr == '\\')
      last = ptr+1;
    ptr++;
  }
  
  strncpy(name_buffer, fname, last-fname);
  name_buffer[last-fname] = '\0';
  return name_buffer;
}

char* calcbasename(char* fname) {
  char* ptr = fname;
  char* last = ptr;
  while (*ptr != '\0') {
    if (*ptr == '/')
      last = ptr+1;
    ptr++;
  }
  
  strcpy(name_buffer, last);
  return name_buffer;
}

void clear_remaps(remap_img_t* list, int count) {
  int i;
  for (i=0; i<count; i++) {
    if (list[i].path) {
      free(list[i].path);
      free(list[i].basepath);
      free(list[i].name);
    }
    if (list[i].img)
      scc_img_free(list[i].img);
  }
  free(list);
}

void dump_remaps(remap_img_t* remaps, int count) {
  int i;
  char buffer[4096];
  for (i=0; i<count; i++) {
    remap_img_t *remap = remaps+i;
    sprintf(buffer, "%s%s%s", remap->basepath, outname, remap->name);
    scc_img_save_bmp(remap->img, buffer);
  }
}

void dump_pal(uint8_t* pal, int size) {
  uint8_t* data;
  int i;
  char buffer[4096];
  sprintf(buffer, "%spal.bmp", outname);
  
  scc_log(LOG_V, "Palette: %s\t[%i colors]\n", buffer, size);
  scc_img_t* img = scc_img_new(size, 1, size, 8);
  data = img->data;
  memcpy(img->pal, pal, size*3);
  
  for (i=0; i<size; i++) {
    *data++ = i;
  }
  
  scc_img_save_bmp(img, buffer);
  scc_img_free(img);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files,*f;
  remap_img_t* remaps;
  uint8_t new_pal[256];
  uint8_t *np;
  int remap_count = 0;
  int i = 0, j = 0;
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);
  
  for (f = files; f; f = f->next)
    remap_count++;
  
  if(!files || !outname || remap_count == 0) scc_print_help(&palcat_help,1);
  
  remaps = calloc(remap_count, sizeof(remap_img_t));

  for(f = files ; f ; f = f->next) {
    remap_img_t *remap = remaps+i;
    remap->path = f->val;
    remap->name = strdup(calcbasename(f->val));
    remap->basepath = strdup(calcbasepath(f->val));
    remap->img = scc_img_open(f->val);
    if (!remap->img) {
      scc_log(LOG_ERR,"Failed to open image '%s'.\n",f->val);
      clear_remaps(remaps, remap_count);
      return 1;
    }
    if (combine_colors <= 0 && remap->img->pal == NULL) {
      scc_log(LOG_ERR,"'%s' does not have a palette. Use -col to combine images with no palette.\n",f->val);
      clear_remaps(remaps, remap_count);
      return 1;
    }
    
    scc_log(LOG_V, "Read: %s\t[%i colors, trans=%i]\n", remap->name, remap->img->ncol, remap->img->trans);
    remap->img->trans = 0;
    i++;
  }
  
  if (combine_colors > 0)
  {
    scc_log(LOG_V, "Combining...\n");
    // Combine images to single image (transparent color is at the end)
    scc_img_t** maps = malloc(sizeof(scc_img_t*) * remap_count);
    for (i=0; i<remap_count; i++) {
      maps[i] = remaps[i].img;
    }
    scc_images_quantize(maps, remap_count, combine_colors, do_dump);
    
    // Write output
    if (!make_single_pal) {
      dump_remaps(remaps, remap_count);
    } else {
      dump_pal(remaps[0].img->pal, remaps[0].img->ncol);
    }
    
    free(maps);
  }
  else
  {  
    // Append and remap colors in each image
    scc_log(LOG_V, "Appending...\n");
    int count = 0;
    np = new_pal;
    memset(np, '\0', sizeof(new_pal));
    
    for (i=0; i<remap_count; i++) {
        remap_img_t *remap = remaps+i;
        scc_img_t *in = remap->img;
        int real_inc = (clip_colors > 0) ? clip_colors : in->ncol;
        if (in->pal == NULL || count+real_inc > 256) {
          scc_log(LOG_ERR,"'%s' has too many colors to fit into current palette\n",f->val);
          clear_remaps(remaps, remap_count);
          return 1;
        }
        
        scc_log(LOG_V, "Offset: %s\t%i\n", remap->name, count);
        // Offset bitmap data in image by input colors (assuming data is 8bit)
        for (j = 0 ; j < in->w*in->h ; j++)
            in->data[j] += count;
        
        if (real_inc > in->ncol)
          memcpy(np,in->pal,in->ncol*3);
        else
          memcpy(np,in->pal,real_inc*3);
        np += real_inc*3;
        count += real_inc;
    }
    
    if (!make_single_pal) {
      // Copy new palette to all images and dump!
      for (i=0; i<remap_count; i++) {
        remap_img_t *remap = remaps+i;
        scc_img_t *in = remap->img;
      
        free(in->pal);
        in->pal = malloc(count*3);
        in->ncol = count;
        memcpy(in->pal, new_pal, count*3);
      }
      
      dump_remaps(remaps, remap_count);
    } else {
      dump_pal(new_pal, count);
    }
  }
  scc_log(LOG_V, "Processed %i images.\n", remap_count);
  clear_remaps(remaps, remap_count);
  
  return 0;
}
