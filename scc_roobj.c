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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "scc_util.h"
#include "scc_parse.h"
#include "scc_ns.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"

#include "scc_img.h"

#include "scc.h"
#include "scc_roobj.h"
#include "scc_code.h"


static int scc_roobj_set_image(scc_roobj_t* ro,scc_ns_t* ns,
			       int idx,char* val);
static int scc_roobj_set_zplane(scc_roobj_t* ro,scc_ns_t* ns,
				int idx,char* val);
static int scc_roobj_set_boxd(scc_roobj_t* ro,scc_ns_t* ns,
			      int idx,char* val);
static int scc_roobj_set_boxm(scc_roobj_t* ro,scc_ns_t* ns,
			      int idx,char* val);
static int scc_roobj_set_scal(scc_roobj_t* ro,scc_ns_t* ns,
			      int idx,char* val);
static int scc_roobj_set_trans(scc_roobj_t* ro,scc_ns_t* ns,
			       int idx,char* val);

struct {
  char* name;
  int (*set)(scc_roobj_t* ro,scc_ns_t* ns,int idx,char* val);
} roobj_params[] = {
  { "image", scc_roobj_set_image },
  { "zplane", scc_roobj_set_zplane },
  { "boxd", scc_roobj_set_boxd },
  { "boxm", scc_roobj_set_boxm },
  { "scal", scc_roobj_set_scal },
  { "trans", scc_roobj_set_trans },
  { NULL, NULL }
};

static struct scc_res_types {
  int type;
  uint32_t id;
  uint32_t fixid;
} scc_res_types[] = {
  { SCC_RES_SOUND, MKID('S','O','U','N'), MKID('s','o','u','n') },
  { SCC_RES_COST, MKID('C','O','S','T'), MKID('c','o','s','t') },
  { SCC_RES_CHSET, MKID('C','H','A','R'), MKID('c','h','a','r') },
  { SCC_RES_VOICE, MKID('v','o','i','c'), MKID('v','o','i','c') },
  { -1, 0, 0 }
};

scc_roobj_t* scc_roobj_new(scc_symbol_t* sym) {
  scc_roobj_t* ro = calloc(1,sizeof(scc_roobj_t));

  ro->sym = sym;

  return ro;
}

static void scc_roobj_res_free(scc_roobj_res_t* r) {
  if(r->data) free(r->data);
  free(r);
}

void scc_roobj_free(scc_roobj_t* ro) {
  scc_roobj_cycl_t* cycl;
  scc_script_t* scr;
  scc_roobj_obj_t* obj;
  scc_roobj_res_t* res;
  scc_boxd_t* box;
  int i;

  SCC_LIST_FREE_CB(ro->scr,scr,scc_script_free);
  SCC_LIST_FREE_CB(ro->lscr,scr,scc_script_free);

  SCC_LIST_FREE(ro->cycl,cycl);
  SCC_LIST_FREE_CB(ro->obj,obj,scc_roobj_obj_free);
  SCC_LIST_FREE_CB(ro->res,res,scc_roobj_res_free);

  if(ro->image) scc_img_free(ro->image);
  for(i = 0 ; i < SCC_MAX_IM_PLANES ; i++)
    if(ro->zplane[i]) scc_img_free(ro->zplane[i]);
  SCC_LIST_FREE(ro->boxd,box);
  if(ro->boxm) free(ro->boxm);
  if(ro->scal) free(ro->scal);
  free(ro);
}

scc_script_t* scc_roobj_get_scr(scc_roobj_t* ro, scc_symbol_t* sym) {
  scc_script_t* s;

  for(s = (sym->type == SCC_RES_SCR ? ro->scr : ro->lscr) ; s ; s = s->next) {
    if(!strcmp(sym->sym,s->sym->sym)) return s;
  }

  return NULL;
}

int scc_roobj_add_scr(scc_roobj_t* ro,scc_script_t* scr) {

  if(scc_roobj_get_scr(ro,scr->sym)) {
    printf("Why are we trying to add a script we alredy have ????\n");
    return 0;
  }
  
  if(scr->sym->type == SCC_RES_SCR) {
    scr->next = ro->scr;
    ro->scr = scr;
  } else {
    scr->next = ro->lscr;
    ro->lscr = scr;
  }
  return 1;
}

scc_roobj_res_t* scc_roobj_get_res(scc_roobj_t* ro,scc_symbol_t* sym) {
  scc_roobj_res_t* r;

  for(r = ro->res ; r ; r = r->next) {
    if(sym == r->sym) return r;
  }

  return NULL;
}

scc_roobj_obj_t* scc_roobj_get_obj(scc_roobj_t* ro,scc_symbol_t* sym) {
  scc_roobj_obj_t* obj;

  for(obj = ro->obj ; obj ; obj = obj->next) {
    if(obj->sym == sym) return obj;
  }

  return NULL;
}

scc_roobj_cycl_t* scc_roobj_get_cycl(scc_roobj_t* ro,scc_symbol_t* sym) {
  scc_roobj_cycl_t* c;

  for(c = ro->cycl ; c ; c = c->next)
    if(c->sym == sym) return c;
  return NULL;
}

int scc_roobj_add_obj(scc_roobj_t* ro,scc_roobj_obj_t* obj) {
  scc_roobj_obj_t* o = scc_roobj_get_obj(ro,obj->sym);

  if(o) {
    printf("%s is alredy defined.\n",obj->sym->sym);
    return 0;
  }

  SCC_LIST_ADD(ro->obj,ro->last_obj,obj);

  return 1;
}

// add costume, sound, etc
scc_roobj_res_t* scc_roobj_add_res(scc_roobj_t* ro,scc_symbol_t* sym, 
				   char* val) {
  scc_roobj_res_t* r = scc_roobj_get_res(ro,sym);
  scc_fd_t* fd;
  int len,flen,rt;
  uint32_t ft;
  
  for(rt = 0 ; scc_res_types[rt].type >= 0 ; rt++) {
    if(scc_res_types[rt].type == sym->type) break;
  }

  if(scc_res_types[rt].type < 0) {
    printf("Unknow ressource type !!!!\n");
    return NULL;
  }    

  if(r) {
    printf("Room symbol %s is alredy defined.\n",sym->sym);
    return NULL;
  }

  fd = new_scc_fd(val,O_RDONLY,0);
  if(!fd) {
    printf("Failed to open %s.\n",val);
    return NULL;
  }
  // get file size
  scc_fd_seek(fd,0,SEEK_END);
  flen = scc_fd_pos(fd);
  scc_fd_seek(fd,0,SEEK_SET);

  // get the header
  ft = scc_fd_r32(fd);
  if(ft != scc_res_types[rt].id) {
    printf("The file %s doesn't seems to contain what we want.\n",val);
    scc_fd_close(fd);
    return NULL;
  }
  len = scc_fd_r32be(fd);
  if(len <= 8 || len < flen) {
    printf("The file %s seems to be invalid.\n",val);
    scc_fd_close(fd);
    return NULL;
  }

  r = calloc(1,sizeof(scc_roobj_res_t));
  r->sym = sym;
  r->data = scc_fd_load(fd,len-8);
  r->data_len = len-8;

  r->next = ro->res;
  ro->res = r;

  scc_fd_close(fd);
  
  return r;
}

static int scc_check_voc(char* file,unsigned char* data,unsigned size) {
  int hsize,ver,magic,pos,type,len,pack;

  if(strncmp(data,"Creative Voice File",19)) {
    printf("%s is not a creative voice file.\n",file);
    return 0;
  }

  hsize = SCC_AT_16LE(data,20);
  ver = SCC_AT_16LE(data,22);
  magic = SCC_AT_16LE(data,24);
  if(hsize < 0x1A) {
    printf("%s: Header is too small.\n",file);
    return 0;
  }

  if(~ver + 0x1234 != magic) {
    printf("%s: Invalid voc header.\n",file);
    return 0;
  }

  pos = hsize;
  while(pos < size) {
    type = data[pos]; pos++;

    // terminator
    if(type == 0) {
      if(pos != size)
        printf("%s: Warning garbage after terminator ???\n",file);
      return 1;
    }

    len = data[pos]; pos++;
    len |= data[pos] << 8; pos++;
    len |= data[pos] << 16; pos++;

    switch(type) {
    case 1:
      pos++; // srate
      pack = data[pos]; pos++;
      len -= 2;

      if(pack != 0) {
        printf("%s: Unssuported packing format: %x\n",file,pack);
        return 0;
      }
    case 6:
    case 7:
      break;
    default:
      printf("%s: Unsupported block type: %x\n",file,type);
      return 0;
    }

    pos += len;
  }

  printf("%s: Truncated file, or the terminator is missing.\n",file);
  return 0;
}

int scc_roobj_add_voice(scc_roobj_t* ro, scc_symbol_t* sym, char* file,
                        int nsync, int* sync) {
  scc_fd_t* fd = new_scc_fd(file,O_RDONLY,0);
  off_t vsize;
  scc_roobj_res_t* r;
  int i;
  
  if(!fd) {
    printf("Failed to open %s.\n",file);
    return 0;
  }
  // get the voc file size
  vsize = scc_fd_seek(fd,0,SEEK_END);
  if(vsize < 0x1A) {
    printf("%s is too small to be voc file.\n",file);
    scc_fd_close(fd);
    return 0;
  }
  scc_fd_seek(fd,0,SEEK_SET);

  // alloc the res
  r = malloc(sizeof(scc_roobj_res_t));
  r->sym = sym;
  r->data_len = 8 + 2*nsync + vsize;
  r->data = malloc(r->data_len);

  // write the sync point table
  SCC_SET_32BE(r->data,0,MKID('V','C','T','L'));
  SCC_SET_32BE(r->data,4,8 + 2*nsync);
  for(i = 0 ; i < nsync ; i++) {
    SCC_SET_16BE(r->data,8+2*i,sync[i]);
  }
  // load the voc data
  if(scc_fd_read(fd,r->data+8+2*nsync,vsize) != vsize) {
    printf("Error while reading voc file.\n");
    free(r->data);
    free(r);
    scc_fd_close(fd);
    return 0;
  }

  scc_fd_close(fd);

  if(!scc_check_voc(file,r->data+8+2*nsync,vsize)) {
    free(r->data);
    free(r);
    return 0;
  }

  // add the res to the list
  r->next = ro->res;
  ro->res = r;

  return 1;
}

int scc_roobj_add_cycl(scc_roobj_t* ro, scc_symbol_t* sym,
                       int freq, int flags, int start, int end) {
  scc_roobj_cycl_t* c = scc_roobj_get_cycl(ro,sym);

  if(c) {
    printf("Cycle %s is alredy defined.\n",sym->sym);
    return 0;
  }

  if(freq < 1 || freq > 0xffff) {
    printf("Error invalid cycle frequency.\n");
    return 0;
  }
  if(start < 0 || start > 0xFF) {
    printf("Error invalid cycle start point.\n");
    return 0;
  }
  if(end < 0 || end > 0xFF) {
    printf("Error invalid cycle end point.\n");
    return 0;
  }
  if(start > end) {
    printf("Error cycle start point is after the end point.\n");
    return 0;
  }

  c = calloc(1,sizeof(scc_roobj_cycl_t));
  c->sym = sym;
  c->freq = freq;
  c->flags = flags;
  c->start = start;
  c->end = end;

  c->next = ro->cycl;
  ro->cycl = c;

  return 1;
}

int scc_roobj_set_param(scc_roobj_t* ro,scc_ns_t* ns,
			char* p,int idx, char* val) {
  int i;
  
  for(i = 0 ; roobj_params[i].name ; i++) {
    if(strcmp(roobj_params[i].name,p)) continue;

    return roobj_params[i].set(ro,ns,idx,val);
  }

  printf("Rooms have no parameter named %s.\n",p);
  return 0;
}


static int scc_roobj_set_image(scc_roobj_t* ro,scc_ns_t* ns,
			       int idx,char* val) {

  if(ro->image) {
    printf("Room image has alredy been set.\n");
    return 0;
  }

  ro->image = scc_img_open(val);
  if(!ro->image) {
    printf("Failed to parse image %s.\n",val);
    return 0;
  }

  if(ro->image->w%8 != 0) {
    printf("Images must have w%%8==0 !!!\n");
    scc_img_free(ro->image);
    ro->image = NULL;
    return 0;
  }
  
  return 1;
}

static int scc_roobj_set_zplane(scc_roobj_t* ro,scc_ns_t* ns,
				int idx,char* val) {

  if(idx < 0) {
    printf("Z-planes need a subscript.\n");
    return 0;
  }
  if(idx < 1 || idx >= SCC_MAX_IM_PLANES) {
    printf("Z-plane index is out of range.\n");
    return 0;
  }

  if(ro->zplane[idx]) {
    printf("Z-plane %d has alredy been set.\n",idx);
    return 0;
  }

  ro->zplane[idx] = scc_img_open(val);
  if(!ro->zplane[idx]) {
    printf("Failed to open zplane image %s.\n",val);
    return 0;
  }

  if(ro->zplane[idx]->w%8 != 0) {
    printf("Images must have w%%8==0 !!!\n");
    scc_img_free(ro->zplane[idx]);
    ro->zplane[idx] = NULL;
    return 0;
  }

  if(ro->zplane[idx]->ncol != 2)
    printf("Warning zplanes should have only 2 colors.\n");
  
  return 1;
}


static int scc_roobj_set_data_param(scc_data_t** ptr,char* name,char* val) {
  
  if(ptr[0]) {
    printf("Room %s have alredy been set.\n",name);
    return 0;
  }

  ptr[0] = scc_data_load(val);

  return (ptr[0] ? 1 : 0);
}

static int scc_roobj_set_boxd(scc_roobj_t* ro,scc_ns_t* ns,
			      int idx,char* path) {
  scc_fd_t* fd;
  uint32_t type,len,named=1;
  int pos = 8, boxn = 0,nlen = 0;
  scc_boxd_t* boxes=NULL,*last=NULL,*b;

  fd = new_scc_fd(path,O_RDONLY,0);
  if(!fd) {
    printf("Failed to open %s.\n",path);
    return 0;
  }

  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);

  if(type == MKID('B','O','X','D')) {
    named = 0;
    scc_fd_r16(fd); pos += 2; // skip num box
  } else if(type != MKID('b','o','x','d')) {
    printf("%s is not a boxd file.\n",path);
    scc_fd_close(fd);
    return 0;
  }

  while(pos < len) {
    if(named) nlen = scc_fd_r8(fd), pos++;
    if(pos + nlen + 20 > len) {
      printf("Invalid box entry.\n");
      break;
    }
    boxn++;
    if(nlen > 0) {
      char name[nlen+1];
      scc_fd_read(fd,name,nlen); pos += nlen;
      name[nlen] = '\0';
      if(!scc_ns_decl(ns,NULL,name,SCC_RES_BOX,0,boxn)) {
	printf("Failed to declare room box %s.\n",name);
	break;
      }
    }
    b = malloc(sizeof(scc_boxd_t));
    b->next = NULL;
    b->ulx = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->uly = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->urx = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->ury = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->lrx = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->lry = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->llx = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->lly = (int16_t)scc_fd_r16le(fd); pos += 2;
    b->mask = scc_fd_r8(fd); pos++;
    b->flags = scc_fd_r8(fd); pos++;
    b->scale = scc_fd_r16le(fd); pos += 2;

    SCC_LIST_ADD(boxes,last,b);

    // we are done
    if(pos == len) {
      // prepend the strange box 0
      if(named) {
	b = malloc(sizeof(scc_boxd_t));
	b->next = boxes;
	b->ulx = -32000;
	b->uly = -32000;
	b->urx = -32000;
	b->ury = -32000;
	b->lrx = -32000;
	b->lry = -32000;
	b->llx = -32000;
	b->lly = -32000;
	b->mask = 0;
	b->flags = 0;
	b->scale = 255;
	ro->boxd = b;
      
	// now the matrix should follow with the scal

	type = scc_fd_r32(fd);
	len = scc_fd_r32be(fd);
	if(type != MKID('B','O','X','M') || len <= 8) {
	  printf("The box file is missing the matrix ????\n");
	  break;
	}
	ro->boxm = malloc(sizeof(scc_data_t)+len);
	ro->boxm->size = len;
	SCC_AT_32(ro->boxm->data,0) = SCC_TO_32BE(MKID('B','O','X','M'));
	SCC_AT_32(ro->boxm->data,4) = SCC_TO_32BE(len);
	if(scc_fd_read(fd,&ro->boxm->data[8],len - 8) != len - 8) {
	  printf("Error while reading the box matrix.\n");
	  break;
	}
        
	type = scc_fd_r32(fd);
	len = scc_fd_r32be(fd);
	if(type != MKID('S','C','A','L') || len != 40) {
	  printf("The box file is missing the scal block ????\n");
	  break;
	}
	ro->scal = malloc(sizeof(scc_data_t)+len);
	ro->scal->size = len;
	SCC_AT_32(ro->scal->data,0) = SCC_TO_32BE(MKID('S','C','A','L'));
	SCC_AT_32(ro->scal->data,4) = SCC_TO_32BE(len);
	if(scc_fd_read(fd,&ro->scal->data[8],len - 8) != len - 8) {
	  printf("Error while reading the scal block.\n");
	  break;
	}
      } else
	ro->boxd = boxes;
      return 1;
    }
  }
  
  SCC_LIST_FREE(boxes,last);
  scc_fd_close(fd);
  return 0;
}

static int scc_roobj_set_boxm(scc_roobj_t* ro,scc_ns_t* ns,int idx,char* val) {
  return scc_roobj_set_data_param(&ro->boxm,"boxm",val);
}

static int scc_roobj_set_scal(scc_roobj_t* ro,scc_ns_t* ns,int idx,char* val) {
  return scc_roobj_set_data_param(&ro->scal,"scal",val);
}

static int scc_roobj_set_trans(scc_roobj_t* ro,scc_ns_t* ns,int idx,char* val) {
  char* end = NULL;
  int i = strtol(val,&end,0);

  if(end[0] != '\0' || i < 0 || i > 255) {
    printf("Invalid transparent color index: %s\n",val);
    return 0;
  }

  ro->trans = i;

  return 1;
}

////////////////////////// objects ////////////////////////////


scc_roobj_obj_t* scc_roobj_obj_new(scc_symbol_t* sym) {
  scc_roobj_obj_t* obj = calloc(1,sizeof(scc_roobj_obj_t));
  obj->sym = sym;
  return obj;
}

static void scc_roobj_obj_state_free(scc_roobj_state_t* st) {
  int l;

  if(st->img) scc_img_free(st->img);
  for(l = 0 ; l < SCC_MAX_IM_PLANES ; l++)
    if(st->zp[l]) scc_img_free(st->zp[l]);

  free(st);
}

void scc_roobj_obj_free(scc_roobj_obj_t* obj) {
  scc_roobj_state_t *st;
  scc_script_t *scr;

  if(obj->name) free(obj->name);
  SCC_LIST_FREE_CB(obj->states,st,scc_roobj_obj_state_free);
  SCC_LIST_FREE_CB(obj->verb,scr,scc_script_free);
  //if(obj->im) TODO
  free(obj);
}

int scc_roobj_obj_add_state(scc_roobj_obj_t* obj,int x, int y,
			    char *img_path,char** zp_paths) {
  scc_img_t* img;
  scc_roobj_state_t* st,*s;
  int i;

  img = scc_img_open(img_path);
  if(!img) return 0;

  if(img->w%8 || img->h%8) {
    printf("Image width and height must be multiple of 8.\n");
    scc_img_free(img);
    return 0;
  }

  if(!obj->w) obj->w = img->w;
  if(!obj->h) obj->h = img->h;
  if(obj->w != img->w || obj->h != img->h) {
    printf("Image size is not matching the alredy defined size.\n");
    scc_img_free(img);
    return 0;
  }

  st = calloc(1,sizeof(scc_roobj_state_t));
  st->hs_x = x;
  st->hs_y = y;
  st->img = img;

  if(zp_paths) {
    for(i = 0 ; zp_paths[i] ; i++) {
      if(zp_paths[i][0] == '\0')
        st->zp[i] = scc_img_new(img->w,img->h,2);
      else {
        st->zp[i] = scc_img_open(zp_paths[i]);
        if(!st->zp[i]) {
          scc_roobj_obj_state_free(st);
          return 0;
        }
        if(st->zp[i]->w != img->w ||
           st->zp[i]->h != img->h) {
          printf("ZPlane %d have a wrong size.\n",i+1);
          scc_roobj_obj_state_free(st);
          return 0;
        }
      }
    }
  }
   
  if(obj->states) {
    for(s = obj->states ; s->next ; s = s->next);
    s->next = st;
  } else
    obj->states = st;    
  
  return 1;
}


int scc_roobj_obj_add_verb(scc_roobj_obj_t* obj,scc_script_t* scr) {
  scc_script_t* s;

  for(s = obj->verb ; s ; s = s->next) {
    if(scr->sym == s->sym) {
      printf("Why are we trying to add a verb we alredy have ????\n");
      return 0;
    }
    if(!s->next) break;
  }

  if(s) s->next = scr;
  else obj->verb = scr;

  return 1;
}

int scc_roobj_obj_set_param(scc_roobj_obj_t* obj,char* sym, char* val) {
  if(!strcmp(sym,"name")) {
    if(obj->name) {
      printf("Object name is alredy defined.\n");
      return 0;
    }
    obj->name = strdup(val);
    return 1;
  } 
  
  printf("Unknow object parameter: %s\n",sym);
  return 0;
}

int scc_roobj_obj_set_int_param(scc_roobj_obj_t* obj,char* sym,int val) {

  if(!strcmp(sym,"x"))
    obj->x = val;
  else if(!strcmp(sym,"y"))
    obj->y = val;
  else if(!strcmp(sym,"w"))
    obj->w = val;
  else if(!strcmp(sym,"h"))
    obj->h = val;
  else if(!strcmp(sym,"hs_x"))
    obj->hs_x = val;
  else if(!strcmp(sym,"hs_y"))
    obj->hs_y = val;
  else if(!strcmp(sym,"dir"))
    obj->dir = val;
  else if(!strcmp(sym,"state")) {
    scc_roobj_state_t* s;
    int i;
    if(val < 0) {
      printf("Invalid object state: %d\n",val);
      return 0;
    }
    if(!val) {
      obj->state = 0;
      return 1;
    }
    for(i = 1, s = obj->states ; s && i != val ; i++, s = s->next);
    if(!s) {
      printf("Invalid object state: %d\n",val);
      return 0;
    }
    obj->state = val;
  } else if(!strcmp(sym,"parent_state")) {
    if(val < 0) {
      printf("Invalid parent state: %d.\n",val);
      return 0;
    }
    obj->parent_state = val;
  } else {
    printf("Unknow integer object parameter: %s\n",sym);
    return 0;
  }
  
  return 1;
}

int scc_roobj_obj_set_class(scc_roobj_obj_t* obj, scc_symbol_t* sym) {
  int i;

  // look if that class is alredy set
  for(i = 0 ; i < SCC_MAX_CLASS ; i++) {
    if(!obj->class[i]) continue;
    if(obj->class[i] == sym) return 1;
  }
  // add it
  for(i = 0 ; i < SCC_MAX_CLASS ; i++) {
    if(obj->class[i]) continue;
    obj->class[i] = sym;
    return 1;
  }
  // no space left ???
  return 0;
}

//////////////////////////// Writing ///////////////////////////////

scc_pal_t* scc_roobj_gen_pals(scc_roobj_t* ro) {
  scc_pal_t* pal;
  int i;

  if(!ro->image) {
    printf("Room have no image, using dummy one !!!!\n");
    ro->image = scc_img_new(8,8,256);
  }

  pal = calloc(1,sizeof(scc_pal_t));

  for(i = 0 ; i < ro->image->ncol ; i++) {
    pal->r[i] = ro->image->pal[3*i];
    pal->g[i] = ro->image->pal[3*i+1];
    pal->b[i] = ro->image->pal[3*i+2];
  }

  return pal;
}

scc_rmim_t* scc_roobj_gen_rmim(scc_roobj_t* ro) {
  scc_rmim_t* rmim;
  int i,j,d,nz = 0;
  uint8_t* zd;

  if(!ro->image) {
    printf("Room have no image, using dummy one  !!!!\n");
    ro->image = scc_img_new(8,8,256);
  }

  for(i = 1 ; i < SCC_MAX_IM_PLANES ; i++) {
    if(ro->zplane[i]) nz++;
    else break;
  }

  rmim = calloc(1,sizeof(scc_rmim_t));
  rmim->num_z_buf = nz;

  rmim->smap_size = scc_code_image(ro->image->data,ro->image->w,
				   ro->image->w,ro->image->h,
				   -1,&rmim->smap);

  for(i = 1 ; i < nz+1 ; i++) {
    // pack the zplane data
    zd = malloc(ro->zplane[i]->w/8*ro->zplane[i]->h);
    d = 0;
    for(j = 0 ; j < ro->zplane[i]->w*ro->zplane[i]->h ; j++) {
      if(ro->zplane[i]->data[j])
	d |= (1<<(7-(j%8)));

      if(j % 8 == 7) {
	zd[j/8] = d;
	d = 0;
      }
    }
    // code it
    rmim->z_buf_size[i] = scc_code_zbuf(zd,ro->zplane[i]->w/8,
					ro->zplane[i]->w,ro->zplane[i]->h,
					&rmim->z_buf[i]);
    free(zd);
  }

  return rmim;

}

static scc_imnn_t* scc_roobj_state_gen_imnn(scc_roobj_state_t* st,int idx) {
  scc_imnn_t* imnn;
  scc_img_t* i = st->img;
  scc_img_t** z = st->zp;
  int l;

  imnn = calloc(1,sizeof(scc_imnn_t));
  imnn->idx = idx;
  imnn->smap_size = scc_code_image(i->data,i->w,
				  i->w,i->h,
				  -1,&imnn->smap);
    
  for(l = 0 ; l < SCC_MAX_IM_PLANES && z[l] ; l++) {
    // pack the zplane data
    uint8_t* zd = malloc(z[l]->w/8*z[l]->h);
    int j,d = 0;
    for(j = 0 ; j < z[l]->w*z[l]->h ; j++) {
      if(z[l]->data[j])
	d |= (1<<(7-(j%8)));
      
      if(j % 8 == 7) {
	zd[j/8] = d;
	d = 0;
      }
    }
    // code it
    imnn->z_buf_size[l+1] = scc_code_zbuf(zd,z[l]->w/8,
					 z[l]->w,z[l]->h,
					 &imnn->z_buf[l+1]);
    free(zd);
  }

  return imnn;
}

static scc_imnn_t* scc_roobj_obj_gen_imnn(scc_roobj_obj_t* obj) {
  scc_imnn_t *imnn = NULL,*last = NULL,*new;
  scc_roobj_state_t* st;
  int idx = 1;

  for(st = obj->states ; st ; idx++, st = st->next) {
    new = scc_roobj_state_gen_imnn(st,idx);
    SCC_LIST_ADD(imnn,last,new);
  }

  return imnn;
}

static int scc_roobj_make_obj_parent(scc_roobj_t* ro) {
  scc_roobj_obj_t* obj,*iter;
  int idx;
  
  for(obj = ro->obj ; obj ; obj = obj->next) {
    if(!obj->parent) continue;
    // find the parent
    for(iter = ro->obj, idx = 1 ;
        iter && iter->sym != obj->parent ;
        iter = iter->next, idx++);
    if(!iter) {
      printf("Failed to find the parent of object %s\n",
             obj->sym->sym);
      return 0;
    }
    obj->parent_id = idx;
  }
  return 1;
}

static int scc_scob_size(scc_script_t* scr) {
  int size = 8 + scr->code_len;
  scc_sym_fix_t* f;

  if(scr->sym_fix) {
    size += 8;
    for(f = scr->sym_fix ; f ; f = f->next)
      size += 8;
  }

  return size;
}

static int scc_write_scob(scc_fd_t* fd, scc_script_t* scr) {
  int n;
  scc_sym_fix_t* f;

  if(scr->sym_fix) {
    for(n = 0, f = scr->sym_fix ; f ; n++, f = f->next);

    scc_fd_w32(fd,MKID('S','F','I','X'));
    scc_fd_w32be(fd,8 + n*8);

    for(f = scr->sym_fix ; f ; f = f->next) {
      scc_fd_w8(fd,f->sym->type);
      scc_fd_w8(fd,f->sym->subtype);      
      scc_fd_w16le(fd,f->sym->rid);
      scc_fd_w32le(fd,f->off);
    }
  }

  scc_fd_w32(fd,MKID('s','c','o','b'));
  scc_fd_w32be(fd,8 + scr->code_len);
  if(scr->code_len) scc_fd_write(fd,scr->code,scr->code_len);

  return 1;
}

static int scc_write_sym(scc_fd_t* fd, scc_symbol_t* s, char status) {
  // status: import/export
  scc_fd_w8(fd,status);
  // name
  scc_fd_w8(fd,strlen(s->sym));
  scc_fd_write(fd,s->sym,strlen(s->sym));
  // type
  scc_fd_w8(fd,s->type);
  // subtype
  scc_fd_w8(fd,s->subtype);
  // rid
  scc_fd_w16le(fd,s->rid);
  // addr
  scc_fd_w16le(fd,s->addr);

  return 1;
}

static int scc_sym_block_size(scc_symbol_t* s) {
  int size = 0;

  while(s) {
    if(s->rid && s->type != SCC_RES_LSCR)
      size += 1 + 1+strlen(s->sym) + 1 + 1 + 2 + 2;
    s = s->next;
  }
    
  return size;
}


static int scc_write_stab(scc_fd_t* fd, scc_roobj_t* ro, scc_ns_t* ns) {
  scc_symbol_t* s, *c;
  char status;
  int len = scc_sym_block_size(ns->glob_sym);

  // no sym at all
  if(!len) return 1;
  
  // first the global ns
  scc_fd_w32(fd,MKID('G','S','Y','M'));
  scc_fd_w32be(fd,8 + len);

  for(s = ns->glob_sym ; s ; s = s->next) {
    if(!s->rid) continue;

    // everything is imported except ourself
    scc_write_sym(fd,s,(s == ro->sym ? 'E' : 'I'));
  }

  // then the 
  for(s = ns->glob_sym ; s ; s = s->next) {
    if(!s->rid || s->type != SCC_RES_ROOM) continue;
    
    len = scc_sym_block_size(s->childs);
    if(!len) continue;

    // again stuff in our room is exported, the rest is imported
    status = (s == ro->sym ? 'E' : 'I');

    scc_fd_w32(fd,MKID('R','S','Y','M'));
    scc_fd_w32be(fd,8 + 2 + len);

    // rid
    scc_fd_w16le(fd,s->rid);

    for(c = s->childs ; c ; c = c->next) {
      if(!c->rid || c->type == SCC_RES_LSCR) continue;

      scc_write_sym(fd,c,status);
    }

  }
    
  return 1;
}

static int scc_stab_size(scc_ns_t* ns) {
  int size = 0;
  scc_symbol_t* s;
  int len = scc_sym_block_size(ns->glob_sym);

  if(!len) return 0;

  size += 8 + len;

  for(s = ns->glob_sym ; s ; s = s->next) {
    if(!s->rid || s->type != SCC_RES_ROOM) continue;

    len = scc_sym_block_size(s->childs);
    if(!len) continue;

    size += 8 + 2 + len;
  }

  return size;
}

int scc_lscr_block_size(scc_roobj_t* ro) {
  scc_script_t *scr;
  int en = 0,ex = 0;
  int size = 8 + 2; // NLSC

  for(scr = ro->lscr ; scr ; scr = scr->next) {
    if(!strcmp(scr->sym->sym,"entry")) en = 1;
    else if(!strcmp(scr->sym->sym,"exit")) ex = 1;
    else size += 1;
    size += 8 + scc_scob_size(scr);
  }

  if(!en) size += 8 + 1;
  if(!ex) size += 8 + 1;

  return size;
}

int scc_write_lscr_block(scc_roobj_t* ro, scc_fd_t* fd) {
  scc_script_t *scr,*en = NULL,*ex = NULL;
  int n = 0;

  for(scr = ro->lscr ; scr ; scr = scr->next) {
    if(!strcmp(scr->sym->sym,"entry")) en = scr;
    else if(!strcmp(scr->sym->sym,"exit")) ex = scr;
    else {
      n++;
      if(scr->sym->addr < 0)
	printf("Warning: a local script is missing his adress.\n");
    }
  }

  if(ex) {
    scc_fd_w32(fd,MKID('e','x','c','d'));
    scc_fd_w32be(fd,8 + scc_scob_size(ex));
    scc_write_scob(fd,ex);
  } else {
    scc_fd_w32(fd,MKID('E','X','C','D'));
    scc_fd_w32be(fd,8 + 1);
    scc_fd_w8(fd,0x65);
  }

  if(en) {
    scc_fd_w32(fd,MKID('e','n','c','d'));
    scc_fd_w32be(fd,8 + scc_scob_size(en));
    scc_write_scob(fd,en);
  } else {
    scc_fd_w32(fd,MKID('E','N','C','D'));
    scc_fd_w32be(fd,8 + 1);
    scc_fd_w8(fd,0x65);
  }

  scc_fd_w32(fd,MKID('N','L','S','C'));
  scc_fd_w32be(fd,8 + 2);
  scc_fd_w16le(fd,n);

  for(scr = ro->lscr ; scr ; scr = scr->next) {
    if(scr == en || scr == ex) continue;
    scc_fd_w32(fd,MKID('l','s','c','r'));
    scc_fd_w32be(fd,8 + 1 + scc_scob_size(scr));
    scc_fd_w8(fd,scr->sym->addr);
    scc_write_scob(fd,scr);
  }

  return 1;
}

static int scc_imob_size(scc_roobj_obj_t* obj) {
  scc_imnn_t* i;
  int size = 8 + 2 + 5*2 + 2+2 +2;
  int ni = 0;

  for(i = obj->im ; i ; i = i->next) {
    size += 8 + scc_imnn_size(i);
    ni++;
  }

  if(!ni) ni++;
  size += 4*ni;

  return size;
}

static int scc_write_imob(scc_roobj_obj_t* obj,scc_fd_t* fd) {
  int j,ni=0,nz=0;
  scc_imnn_t* i;
  scc_roobj_state_t* st;

  for(i = obj->im ; i ; i = i->next) {
    int z = 0;
    ni++;
    for(j = 0 ; j < SCC_MAX_IM_PLANES ; j++) {
      if(i->z_buf[j]) z++;
    }
    if(z > nz) nz = z;
  }

  scc_fd_w32(fd,MKID('i','m','h','d'));
  scc_fd_w32be(fd,8 + 2 + 5*2 + 2+2 + 2 + (ni ? 4*ni : 4));

  // obj id
  scc_fd_w16le(fd,obj->sym->rid);

  // write number of image and zplanes
  scc_fd_w16le(fd,ni);
  scc_fd_w16le(fd,nz);  
  // unk
  scc_fd_w16le(fd,0);
  // x,y
  scc_fd_w16le(fd,obj->x);
  scc_fd_w16le(fd,obj->y);

  // w,h
  scc_fd_w16le(fd,obj->w);
  scc_fd_w16le(fd,obj->h);

  // num hotspot
  if(ni) {
    scc_fd_w16le(fd,ni);
    for(st = obj->states ; st ; st = st->next) {
      scc_fd_w16le(fd,st->hs_x);
      scc_fd_w16le(fd,st->hs_y);
    }
  } else {
    scc_fd_w16le(fd,1);
    scc_fd_w16le(fd,obj->hs_x);
    scc_fd_w16le(fd,obj->hs_y);
  }

  for(i = obj->im ; i ; i = i->next) {  
    scc_fd_w32(fd,MKID('I','M','0','0'+i->idx));
    scc_fd_w32be(fd,8 + scc_imnn_size(i));
    scc_write_imnn(fd,i);
  }

  return 1;
}

int scc_verb_block_size(scc_script_t* scr) {
  int size = 0;

  while(scr) {
    size += 8 + 2 + scc_scob_size(scr);
    scr = scr->next;
  }
  
  return size;
}

int scc_write_verb_block(scc_script_t* scr,scc_fd_t* fd) {

  while(scr) {
    scc_fd_w32(fd,MKID('v','e','r','b'));
    scc_fd_w32be(fd,8 + 2 +scc_scob_size(scr));
    if(scr->sym) scc_fd_w16le(fd,scr->sym->rid);
    else scc_fd_w16le(fd,0);
    scc_write_scob(fd,scr);
    scr = scr->next;
  }
  return 1;
}

int scc_obob_size(scc_roobj_obj_t* obj) {
  int size = 8 + 2 +1+2+2*SCC_MAX_CLASS +2+2 +2+2 +1+1 +2*2 +1;

  size += scc_verb_block_size(obj->verb);

  size += 8 + (obj->name ? strlen(obj->name) : 0) + 1;

  return size;
}


int scc_write_obob(scc_roobj_obj_t* obj,scc_fd_t* fd) {
  int i;

  scc_fd_w32(fd,MKID('c','d','h','d'));
  scc_fd_w32be(fd,8 + 2 +1+2+2*SCC_MAX_CLASS +2+2 +2+2 +1+1 +2*2 +1);

  // rid
  scc_fd_w16le(fd,obj->sym->rid);

  // initial state
  scc_fd_w8(fd,obj->state);
  // owner
  if(obj->owner)
    scc_fd_w16le(fd,obj->owner->rid);
  else
    scc_fd_w16le(fd,0);
  // classes
  for(i = 0 ; i < SCC_MAX_CLASS ; i++) {
    if(obj->class[i])
      scc_fd_w16le(fd,obj->class[i]->rid);
    else
      scc_fd_w16le(fd,0);
  }

  scc_fd_w16le(fd,obj->x);
  scc_fd_w16le(fd,obj->y);

  scc_fd_w16le(fd,obj->w);
  scc_fd_w16le(fd,obj->h);

  // parent state
  scc_fd_w8(fd,obj->parent_state);
  // parent
  scc_fd_w8(fd,obj->parent_id);
  // unk
  scc_fd_w32(fd,0);
  // actor dir
  scc_fd_w8(fd,obj->dir);

  // VERBS
  scc_write_verb_block(obj->verb,fd);

  
  scc_fd_w32(fd,MKID('O','B','N','A'));
  scc_fd_w32be(fd,8 + (obj->name ? strlen(obj->name) : 0) + 1);
  if(obj->name)
    scc_fd_write(fd,obj->name,strlen(obj->name));
  scc_fd_w8(fd,0);
    
  return 1;
}

int scc_roobj_write_res(scc_roobj_res_t* res, scc_fd_t* fd) {
  int rt;

  for(rt = 0 ; scc_res_types[rt].type >= 0 ; rt++) {
    if(scc_res_types[rt].type == res->sym->type) break;
  }

  if(scc_res_types[rt].type < 0) {
    printf("Unknow ressource type !!!!\n");
    return 0;
  }
  
  scc_fd_w32(fd,scc_res_types[rt].fixid);
  scc_fd_w32be(fd,8 + 2 + res->data_len);
  scc_fd_w16le(fd,res->sym->rid);
  scc_fd_write(fd,res->data,res->data_len);

  return 1;
}


int scc_roobj_cycl_size(scc_roobj_cycl_t* c) {
  int size = 1;
  while(c) {
    size += 9;
    c = c->next;
  }
  return size;
}
  

int scc_roobj_write_cycl(scc_roobj_cycl_t* c, scc_fd_t* fd) {
  
  while(c) {
    scc_fd_w8(fd,c->sym->addr);
    scc_fd_w16(fd,0);
    scc_fd_w16be(fd,c->freq);
    scc_fd_w16be(fd,c->flags);
    scc_fd_w8(fd,c->start);
    scc_fd_w8(fd,c->end);
    c = c->next;
  }
  scc_fd_w8(fd,0);
  return 1;
}

int scc_roobj_write(scc_roobj_t* ro, scc_ns_t* ns, scc_fd_t* fd) {
  scc_pal_t* pals;
  scc_rmim_t* rmim;
  scc_script_t* scr;
  scc_roobj_obj_t* obj;
  scc_roobj_res_t* res;
  int i;
  int num_obj = 0;
  int stab_len;
  int size = 8 + 6; // RMHD

  stab_len = scc_stab_size(ns);
  if(stab_len) size += 8 + stab_len;
  
  // CYCL
  size += 8 + scc_roobj_cycl_size(ro->cycl);
  // TRNS
  size += 8 + 2;
  // PALS
  pals = scc_roobj_gen_pals(ro);
  if(!pals) return 0;
  size += 8 + scc_pals_size(pals);
  // RMIM
  rmim = scc_roobj_gen_rmim(ro);
  if(!rmim) return 0;
  size += 8 + scc_rmim_size(rmim);
  // OBIM/OBCD
  if(!scc_roobj_make_obj_parent(ro)) return 0;
  for(obj = ro->obj ; obj ; obj = obj->next) {
    obj->im = scc_roobj_obj_gen_imnn(obj);
    size += 8 + scc_imob_size(obj);
    size += 8 + scc_obob_size(obj);
    num_obj++;
  }
  // local scripts
  size += scc_lscr_block_size(ro);
  // BOXD
  if(!ro->boxd)
    size += 8 + 42;
  else
    size += 8 + scc_boxd_size(ro->boxd);
  // BOXM
  if(!ro->boxm)
    size += 8 + 8;
  else
    size += ro->boxm->size;
  // SCAL
  if(!ro->scal)
    size += 8 + 20;
  else
    size += ro->scal->size;

  // global scripts
  for(scr = ro->scr ; scr ; scr = scr->next)
    size += 8 + 2 + scc_scob_size(scr);

  // ressources
  for(res = ro->res ; res ; res = res->next)
    size += 8 + 2 + res->data_len;

  // Now write all that  
  // the block header
  scc_fd_w32(fd,MKID('r','o','o','m'));
  scc_fd_w32be(fd,size + 8);

  scc_fd_w32(fd,MKID('R','M','H','D'));
  scc_fd_w32be(fd,8 + 6);
  scc_fd_w16le(fd,ro->image->w);
  scc_fd_w16le(fd,ro->image->h);
  scc_fd_w16le(fd,num_obj);

  if(stab_len) {
    scc_fd_w32(fd,MKID('S','T','A','B'));
    scc_fd_w32be(fd,stab_len + 8);
    scc_write_stab(fd,ro,ns);
  }

  scc_fd_w32(fd,MKID('C','Y','C','L'));
  scc_fd_w32be(fd,8 + scc_roobj_cycl_size(ro->cycl));
  scc_roobj_write_cycl(ro->cycl,fd);

  scc_fd_w32(fd,MKID('T','R','N','S'));
  scc_fd_w32be(fd,8 + 2);
  scc_fd_w16le(fd,ro->trans);

  scc_fd_w32(fd,MKID('P','A','L','S'));
  scc_fd_w32be(fd,8 + scc_pals_size(pals));
  scc_write_pals(fd,pals);

  scc_fd_w32(fd,MKID('R','M','I','M'));
  scc_fd_w32be(fd,8 + scc_rmim_size(rmim));
  scc_write_rmim(fd,rmim);

  // OBIM
  for(obj = ro->obj ; obj ; obj = obj->next) {
    scc_fd_w32(fd,MKID('o','b','i','m'));
    scc_fd_w32be(fd,8 + scc_imob_size(obj));
    scc_write_imob(obj,fd);
  }
  // OBCD
  for(obj = ro->obj ; obj ; obj = obj->next) {
    scc_fd_w32(fd,MKID('o','b','c','d'));
    scc_fd_w32be(fd,8 + scc_obob_size(obj));
    scc_write_obob(obj,fd);
  }

  // All the stuff needed for local scripts
  scc_write_lscr_block(ro,fd);



  // BOXD
  scc_fd_w32(fd,MKID('B','O','X','D'));
  if(ro->boxd) {
    scc_fd_w32be(fd,8 + scc_boxd_size(ro->boxd));
    scc_write_boxd(fd,ro->boxd);
  } else {
    scc_fd_w32be(fd,8 + 42);
    scc_fd_w16le(fd,2);      // num box
    // box 0
    for(i = 0 ; i < 8 ; i++) // coords
      scc_fd_w16le(fd,-32000);
    scc_fd_w16(fd,0); // mask, flags
    scc_fd_w16le(fd,255); // scale
    // dummy box
    scc_fd_w16le(fd,2); // ulx
    scc_fd_w16le(fd,2); // uly
    scc_fd_w16le(fd,4); // urx
    scc_fd_w16le(fd,2); // ury
    scc_fd_w16le(fd,4); // lrx
    scc_fd_w16le(fd,4); // lry
    scc_fd_w16le(fd,2); // llx
    scc_fd_w16le(fd,4); // lly
    scc_fd_w16(fd,0); // mask, flags
    scc_fd_w16le(fd,255); // scale
  }
  // BOXM
  if(ro->boxm)
    scc_fd_write(fd,ro->boxm->data,ro->boxm->size);
  else {
    scc_fd_w32(fd,MKID('B','O','X','M'));
    scc_fd_w32be(fd,8 + 8);
    scc_fd_w32be(fd,0x000000FF);
    scc_fd_w32be(fd,0x010101FF);
  }

  // SCAL
  if(ro->scal)
    scc_fd_write(fd,ro->scal->data,ro->scal->size);
  else {
    scc_fd_w32(fd,MKID('S','C','A','L'));
    scc_fd_w32be(fd,8 + 20);
    for(i = 0 ; i < 5 ; i++)
      scc_fd_w32(fd,0);
  }

  // SCOB
  for(scr = ro->scr ; scr ; scr = scr->next) {
    scc_fd_w32(fd,MKID('s','c','r','p'));
    scc_fd_w32be(fd,8 + 2 + scc_scob_size(scr));
    scc_fd_w16le(fd,scr->sym->rid);
    scc_write_scob(fd,scr);
  }

  // write ressources
  for(res = ro->res ; res ; res = res->next)
    scc_roobj_write_res(res,fd);

  return 1;
}
