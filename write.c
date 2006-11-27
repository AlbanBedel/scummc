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
 * @file write.c
 * @ingroup scumm
 * @brief Write SCUMM data. Mostly obsolete.
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
#include "scc.h"

int scc_res_list_size(scc_res_list_t* list) {
  return 2 + 5*list->size;
}

int scc_aary_size(scc_aary_t* aary) {
  int size = 0;

  while(aary) {
    size += 4*2;
    aary = aary->next;
  }
  size += 2; // Terminal code
  return size;
}


int scc_loff_size(scc_res_idx_t* idx) {
  int num,i;

  for(num = i = 0 ; i < idx->room_list->size ; i++) {
    if(idx->room_list->room_off[i]) num++;
  }
  return 1 + 5*num;
}

int scc_cycl_size(scc_cycl_t* c) {
  int size;
  scc_cycl_t* iter;
  
  for(size = 0,iter = c ; iter ; iter = iter->next)
    size += 9;
  size += 1;
  size = ((size+1)/2)*2; // Round up to next 2 multiple
  return size;
}

int scc_pals_size(scc_pal_t* p) {
  int size = 8 + 8 + 8; // WRAP OFFS APAL
  scc_pal_t* i;
    
  for(i = p ; i ; i = i->next) {
    size += 4; // Offset entry
    size += 256*3; // APAL data
    if(i != p) size += 8; // sub pal header
  }

  return size;
}

int scc_rmim_size(scc_rmim_t* rmim) {
  int size = 8 + 2 + 8 + 8 + rmim->smap_size;
  int i;
  for(i  = 0 ; i < SCC_MAX_IM_PLANES ; i++) {
    if(!rmim->z_buf[i]) continue;
    size += 8 + rmim->z_buf_size[i];
  }
  return size;
}

int scc_imnn_size(scc_imnn_t* i) {
  int n,size = 0;

  size += 8 + i->smap_size;
  for(n = 0 ; n < SCC_MAX_IM_PLANES ; n++) {
    if(!i->z_buf[n]) continue;
    size += 8 + i->z_buf_size[n];
  }
  return size;
}

int scc_obim_size(scc_obim_t* obim) {
  int size = 8 + 18 + 4*obim->num_hotspots;
  scc_imnn_t* i;

  for(i = obim->img ; i ; i = i->next) {
    size += 8 + scc_imnn_size(i);
  }
  return size;
}

int scc_verb_size(scc_verb_t* verb) {
  int size = 1;

  //if(!verb) return 4;

  while(verb) {
    size += 3 + verb->len;
    //if(verb->idx == 0xFF) break;
    //if(!verb->next)
    //  size += 3; // closing 0
    verb = verb->next;
  }

  return size;
}

int scc_obcd_size(scc_obcd_t* obcd) {
  int size = 8 + 17; // CDHD

  size += 8 + scc_verb_size(obcd->verb);

  if(obcd->name)
    size += 8 + strlen(obcd->name) + 1;

  return size;
}

int scc_boxd_size(scc_boxd_t* boxd) {
  int size = 2;

  while(boxd) {
    size += 20;
    boxd = boxd->next;
  }

  return size;
}

int scc_boxm_size(uint8_t ** m) {
  int num,i,j;
  int len = 0;

  for(num = 0 ; m[num] ; num++);

  for(i = 0 ; i < num ; i++) {
    for(j = 0 ; j < num ; j++) {
      int d = m[i][j];
      if(d == 0xFF) continue;
      while(j+1 < num && m[i][j+1] == d) j++;
      len += 3;
    }
    len++;
  }

  return len;
}


int scc_room_size(scc_room_t* r) {
  int size = 8 + 6 + // RMHD
    8 + scc_cycl_size(r->cycl) + // CYCL
    8 + 2 + // TRNS
    8 + scc_pals_size(r->pals) +
    8 + scc_rmim_size(r->rmim) +
    (r->excd ? 8 + r->excd->size : 0) +
    (r->encd ? 8 + r->encd->size : 0) +
    8 + 2 + // NLSC
    (r->boxd ? 8 + scc_boxd_size(r->boxd) : 0) +
    (r->boxm ? 8 + scc_boxm_size(r->boxm) : 0) +
    (r->scal ? 8 + r->scal->size : 0);
  scc_obim_t* obim;
  scc_obcd_t* obcd;
  scc_data_list_t* lscr;

  for(obim = r->obim ; obim ; obim = obim->next)
    size += 8 + scc_obim_size(obim);
  
  for(obcd = r->obcd ; obcd ; obcd = obcd->next)
    size += 8 + scc_obcd_size(obcd);


  for(lscr = r->lscr ; lscr ; lscr = lscr->next)
    size += 8 + 1 + lscr->size;



  return size;
}

int scc_cost_anim_size(scc_cost_anim_t* a) {
  int i;
  int size = 2; // mask

  if(a->redir) return 0;

  for(i = 15 ; i >= 0 ; i--) {
    if(!(a->mask & (1<<i))) continue;
    size += 2;
    if(a->limb[i].start != 0xFFFF) size += 1;
  }
  return size;
}

int scc_cost_pic_size(scc_cost_pic_t* p,uint8_t fmt) {
  scc_cost_pic_t* i;
  int s,n,size = 0;

  // dup limb table
  if(!p || p->id == 0xFF) return 0;

  for(n = 0, i = p ; i ; i = i->next) {
    // get the hightest id, need for the table
    if(i->id > n) n = i->id;
    s = size;
    size += 2 + 2 + // width,height
      2 + 2 + // rel_x, rel_y
      2 + 2; // move_x, move_y

    if((fmt & ~(0x81)) == 0x60) size += 1 + 1; // redir_limb, redir_pic
        
    size += i->data_size;

    //printf("Computed size pict %d -> %d\n",i->id,size-s);
  }

  // limb table
  size += 2*(n+1);

  return size;

}

int scc_cost_get_cmds_offset(scc_cost_t* c) {
  scc_cost_anim_t* a;
  int n,off =
    4 + // cost size (repeated apparently)
    2 + // CO header
    1 + // num anim
    1 + // format
    c->pal_size +
    2 + // anim cmds offset
    16*2; // limbs offset

  // anims
  for(n = 0,a = c->anims ; a ; a = a->next) {
    off += scc_cost_anim_size(a); // anim def size
    if(a->id > n) n = a->id;
  }
  off += 2*(n+1);

  return off;
}

int scc_cost_size_intern(scc_cost_t* c) {
  int n,size = scc_cost_get_cmds_offset(c);
  
  size += c->cmds_size; // cmds array

  //printf("Computed cmds end: %d\n",size); 
  
  for(n = 0 ; n < 16 ; n++) {
    //int s = scc_cost_pic_size(c->limb_pic[n],c->format);
    size += scc_cost_pic_size(c->limb_pic[n],c->format);
  }
  //printf("Pict size: %d %d\n",size -(cs + c->cmds_size),c->cmds_size);
  //size = ((size+1)/2)*2;
  return size;
}

int scc_cost_size(scc_cost_t* c) {
  int size = scc_cost_size_intern(c);
  size = ((size+1)/2)*2;
  return size;
}

int scc_lfl_size(scc_lfl_t* l) {
  int size = 8 + scc_room_size(&l->room);
  scc_data_list_t* i;
  scc_cost_t* c;

  for(i = l->scr ; i ; i = i->next)
    size += 8 + i->size;

  for(i = l->snd ; i ; i = i->next)
    size += 8 + i->size;
  
  for(c = l->cost ; c ; c = c->next)
    size += 8 + scc_cost_size(c);

  for(i = l->chr ; i ; i = i->next)
    size += 8 + i->size;

  return size;
}

int scc_lecf_size(scc_res_t* r) {
  int size = 8 + scc_loff_size(r->idx);
  scc_lfl_t* i;

  for(i = r->lfl ; i ; i = i->next)
    size += 8 + scc_lfl_size(i);

  return size;
}

int scc_write_data_list(scc_fd_t* fd,uint32_t type,scc_data_list_t* l) {
  int r,w;

  while(l) {
    scc_fd_w32(fd,type);
    scc_fd_w32be(fd,8+l->size);
  
    w=0;
    while(w < l->size) {
      r = scc_fd_write(fd,l->data+w,l->size-w);
      if(r < 0) {
	printf("Write error???\n");
	return 0;
      }
      w += r;
    }
    l = l->next;
  }
  return 1;
}

int scc_write_data(scc_fd_t* fd,uint32_t type,scc_data_t* d) {
  int r,w=0;
  scc_fd_w32(fd,type);
  scc_fd_w32be(fd,8+d->size);

  while(w < d->size) {
    r = scc_fd_write(fd,d->data+w,d->size-w);
    if(r < 0) {
      printf("Write error???\n");
      return 0;
    }
    w += r;
  }
    
  return 1;
}

int scc_write_cycl(scc_fd_t* fd,scc_cycl_t* c) {
  int s = 0;
  while(c) {
    scc_fd_w8(fd,c->idx);
    scc_fd_w16be(fd,c->unk);
    scc_fd_w16be(fd,c->freq);
    scc_fd_w16be(fd,c->flags);
    scc_fd_w8(fd,c->start);
    scc_fd_w8(fd,c->end);
    s += 9;
    c = c->next;
  }

  scc_fd_w8(fd,0);
  s++;
  if(s % 2) scc_fd_w8(fd,0);

  return 1;
}

int scc_write_pals(scc_fd_t* fd,scc_pal_t* p) {
  scc_pal_t* i;
  int n,ofs,pos;

  for(n = 0,i = p ; i ; i = i->next,n++);
  ofs = 8 +n*4;

  scc_fd_w32(fd,MKID('W','R','A','P'));
  scc_fd_w32be(fd,scc_pals_size(p));

  scc_fd_w32(fd,MKID('O','F','F','S'));
  scc_fd_w32be(fd,8+ 4*n);
  scc_fd_w32le(fd,ofs);
  ofs += 3*256 + 8;
  for(i = p->next, pos = ofs ; i ; pos += 3*256 + 8,i = i->next)
    scc_fd_w32le(fd,pos);
  
  scc_fd_w32(fd,MKID('A','P','A','L'));
  scc_fd_w32be(fd,8+3*256);
  for(pos = 0 ; pos < 256 ; pos++) {
    scc_fd_w8(fd,p->r[pos]);
    scc_fd_w8(fd,p->g[pos]);
    scc_fd_w8(fd,p->b[pos]);
  }
  for(i = p->next ; i ; i = i->next) {
    scc_fd_w32(fd,MKID('A','P','A','L'));
    scc_fd_w32be(fd,8+3*256);
    for(pos = 0 ; pos < 256 ; pos++) {
      scc_fd_w8(fd,i->r[pos]);
      scc_fd_w8(fd,i->g[pos]);
      scc_fd_w8(fd,i->b[pos]);
    }
  }
  return 1;
}

int scc_write_rmim(scc_fd_t* fd,scc_rmim_t* rmim) {
  int r,w,i;
  // Room image header
  scc_fd_w32(fd,MKID('R','M','I','H'));
  scc_fd_w32be(fd,8 + 2);
  scc_fd_w16le(fd,rmim->num_z_buf);

  // IM00
  scc_fd_w32(fd,MKID('I','M','0','0'));
  scc_fd_w32be(fd,scc_rmim_size(rmim)-8-2);

  // SMAP
  scc_fd_w32(fd,MKID('S','M','A','P'));
  scc_fd_w32be(fd,8 + rmim->smap_size);

  w = 0;
  while(w < rmim->smap_size) {
    r = scc_fd_write(fd,rmim->smap+w,rmim->smap_size-w);
    if(r < 0) {
      printf("Write error???\n");
      return 0;
    }
    w += r;
  }
  
  for(i = 0 ; i < SCC_MAX_IM_PLANES ; i++) {
    if(!rmim->z_buf[i]) continue;
    scc_fd_w32(fd,MKID('Z','P','0',('0'+i)));
    scc_fd_w32be(fd,8 + rmim->z_buf_size[i]);

    w = 0;
    while(w < rmim->z_buf_size[i]) {
      r = scc_fd_write(fd,rmim->z_buf[i]+w,rmim->z_buf_size[i]-w);
      if(r < 0) {
	printf("Write error???\n");
	return 0;
      }
      w += r;
    }
  }

  return 1;
}

int scc_write_imnn(scc_fd_t* fd,scc_imnn_t* img) {
  int r,w,nz;

  // SMAP
  scc_fd_w32(fd,MKID('S','M','A','P'));
  scc_fd_w32be(fd,8 + img->smap_size);
  w = 0;
  while(w < img->smap_size) {
    r = scc_fd_write(fd,img->smap+w,img->smap_size-w);
    if(r < 0) {
      printf("Write error???\n");
      return 0;
    }
    w += r;
  }

  for(nz = 0 ; nz < SCC_MAX_IM_PLANES ; nz++) {
    if(!img->z_buf[nz]) continue;
      
    scc_fd_w32(fd,MKID('Z','P','0',('0'+nz)));
    scc_fd_w32be(fd,8 + img->z_buf_size[nz]);
    
    w = 0;
    while(w < img->z_buf_size[nz]) {
      r = scc_fd_write(fd,img->z_buf[nz]+w,img->z_buf_size[nz]-w);
      if(r < 0) {
	printf("Write error???\n");
	return 0;
      }
      w += r;
    }
  }
  return 1;
}

int scc_write_obim(scc_fd_t* fd,scc_obim_t* im) {
  int i;
  scc_imnn_t* img;
  // Object image header
  scc_fd_w32(fd,MKID('I','M','H','D'));
  scc_fd_w32be(fd,8 + 18 + im->num_hotspots*4);
  
  scc_fd_w16le(fd,im->idx);
  scc_fd_w16le(fd,im->num_imnn);
  scc_fd_w16le(fd,im->num_zpnn);
  scc_fd_w16le(fd,im->unk);
  scc_fd_w16le(fd,im->x);
  scc_fd_w16le(fd,im->y);
  scc_fd_w16le(fd,im->width);
  scc_fd_w16le(fd,im->height);
  scc_fd_w16le(fd,im->num_hotspots);

  for(i= 0 ; i < im->num_hotspots ; i++) {
    scc_fd_w16le(fd,im->hotspots[2*i]);
    scc_fd_w16le(fd,im->hotspots[2*i+1]);
  }

  for(img = im->img ; img ; img = img->next) {
    scc_fd_w32(fd,MKID('I','M','0','0'+img->idx));
    scc_fd_w32be(fd,8 + scc_imnn_size(img));
    scc_write_imnn(fd,img);
  }
  return 1;
}

int scc_write_obcd(scc_fd_t* fd,scc_obcd_t* o) {
  
  scc_fd_w32(fd,MKID('C','D','H','D'));
  scc_fd_w32be(fd,8 + 17);

  scc_fd_w16le(fd,o->idx);
  scc_fd_w16le(fd,o->x);
  scc_fd_w16le(fd,o->y);
  scc_fd_w16le(fd,o->width);
  scc_fd_w16le(fd,o->height);
  scc_fd_w8(fd,o->flags);
  scc_fd_w8(fd,o->parent);
  scc_fd_w16le(fd,o->unk1);
  scc_fd_w16le(fd,o->unk2);
  scc_fd_w8(fd,o->actor_dir);

  if(o->verb)
    scc_write_data(fd,MKID('V','E','R','B'),o->verb);

  if(o->name) {
    int r,w=0,len = strlen(o->name) + 1;
    scc_fd_w32(fd,MKID('O','B','N','A'));
    scc_fd_w32be(fd,8 + len);
    while(w < len) {
      r = scc_fd_write(fd,o->name+w,len-w);
      if(r < 0) {
	printf("Write error???\n");
	return 0;
      }
      w += r;
    }
  }
  return 1;
}

int scc_write_boxd(scc_fd_t* fd,scc_boxd_t* b) {
  uint16_t num;
  scc_boxd_t* i;

  for(num = 0, i = b ; i ; i = i->next) num++;
  
  scc_fd_w16le(fd,num);
  while(b) {
    scc_fd_w16le(fd,b->ulx);
    scc_fd_w16le(fd,b->uly);
    scc_fd_w16le(fd,b->urx);
    scc_fd_w16le(fd,b->ury);
    scc_fd_w16le(fd,b->lrx);
    scc_fd_w16le(fd,b->lry);
    scc_fd_w16le(fd,b->llx);
    scc_fd_w16le(fd,b->lly);
    scc_fd_w8(fd,b->mask);
    scc_fd_w8(fd,b->flags);
    scc_fd_w16le(fd,b->scale);
    b = b->next;
  }

  return 1;
}

int scc_write_boxm(scc_fd_t* fd,uint8_t ** m) {
  int num,i,j;
  
  for(num = 0 ; m[num] ; num++);

  for(i = 0 ; i < num ; i++) {
    for(j = 0 ; j < num ; j++) {
      int d = m[i][j];
      if(d == 0xFF) continue;
      scc_fd_w8(fd,j);
      while(j+1 < num && m[i][j+1] == d) j++;
      scc_fd_w8(fd,j);
      scc_fd_w8(fd,d);
    }
    scc_fd_w8(fd,0xFF);
  }

  return 1;
}

int scc_write_room(scc_fd_t* fd,scc_room_t* room) {
  
  // Room header
  scc_fd_w32(fd,MKID('R','M','H','D'));
  scc_fd_w32be(fd,8 + 6);

  scc_fd_w16le(fd,room->width);
  scc_fd_w16le(fd,room->height);
  scc_fd_w16le(fd,room->num_objs);

  // Cycle
  scc_fd_w32(fd,MKID('C','Y','C','L'));
  scc_fd_w32be(fd,8 + scc_cycl_size(room->cycl));;
  scc_write_cycl(fd,room->cycl);

  // Trns
  scc_fd_w32(fd,MKID('T','R','N','S'));
  scc_fd_w32be(fd,8 + 2);
  scc_fd_w16le(fd,room->transp);

  // Pals
  scc_fd_w32(fd,MKID('P','A','L','S'));
  scc_fd_w32be(fd,8 + scc_pals_size(room->pals));
  scc_write_pals(fd,room->pals);

  // RMIM
  scc_fd_w32(fd,MKID('R','M','I','M'));
  scc_fd_w32be(fd,8 + scc_rmim_size(room->rmim));
  scc_write_rmim(fd,room->rmim);

  // OBIM
  if(room->obim) {
    scc_obim_t* i = room->obim;
    while(i) {
      scc_fd_w32(fd,MKID('O','B','I','M'));
      scc_fd_w32be(fd,8 + scc_obim_size(i));
      scc_write_obim(fd,i);
      i = i->next;
    }
  }

  // OBCD
  if(room->obcd) {
    scc_obcd_t* i = room->obcd;
    while(i) {
      scc_fd_w32(fd,MKID('O','B','C','D'));
      scc_fd_w32be(fd,8 + scc_obcd_size(i));
      scc_write_obcd(fd,i);
      i = i->next;
    }
  }

  // EXCD
  scc_write_data(fd,MKID('E','X','C','D'),room->excd);
  
  // ENCD
  scc_write_data(fd,MKID('E','N','C','D'),room->encd);

  // NLSC
  scc_fd_w32(fd,MKID('N','L','S','C'));
  scc_fd_w32be(fd,8+2);
  scc_fd_w16le(fd,room->nlsc);

  if(room->lscr) {
    scc_data_list_t* l = room->lscr;
    int r,w;

    while(l) {
      scc_fd_w32(fd,MKID('L','S','C','R'));
      scc_fd_w32be(fd,8+1+l->size);
      scc_fd_w8(fd,l->idx);
      w = 0;
      while(w < l->size) {
	r = scc_fd_write(fd,l->data+w,l->size-w);
	if(r < 0) {
	  printf("Write error\n");
	  break;
	}
	w += r;
      }
      l = l->next;
    }
  }
  
  if(room->boxd) { 
    scc_fd_w32(fd,MKID('B','O','X','D'));
    scc_fd_w32be(fd,8 + scc_boxd_size(room->boxd));
    scc_write_boxd(fd,room->boxd);
  }

  if(room->boxm) {
    scc_fd_w32(fd,MKID('B','O','X','M'));
    scc_fd_w32be(fd,8 + scc_boxm_size(room->boxm));
    scc_write_boxm(fd,room->boxm);
  }

  if(room->scal)
    scc_write_data(fd,MKID('S','C','A','L'),room->scal);

  return 1;
}

int scc_write_cost(scc_fd_t* fd,scc_cost_t* c) {
  int off,n,len;
  scc_cost_anim_t* a;
  uint16_t limbs[16],*anims;
  int num_anim;
  off_t start_pos;

  scc_fd_w32(fd,MKID('C','O','S','T'));
  scc_fd_w32be(fd,8+scc_cost_size(c));

  start_pos = scc_fd_pos(fd);

  // strange size
  if(c->flags & SCC_COST_HAS_SIZE)
    scc_fd_w32le(fd,scc_cost_size_intern(c));
  else
    scc_fd_w32le(fd,0);

  // CO header
  scc_fd_w8(fd,'C');
  scc_fd_w8(fd,'O');

  // Num anims
  for(num_anim = 0, a = c->anims ; a ; a = a->next) {
    if(a->id > num_anim) num_anim = a->id;
  }
  num_anim++;

  scc_fd_w8(fd, (c->flags & SCC_COST_HAS_SIZE) ? num_anim-1 : num_anim );

  // format
  scc_fd_w8(fd,c->format);
  // palette
  scc_fd_write(fd,c->pal,c->pal_size);
  // cmds off
  off = scc_cost_get_cmds_offset(c);
  scc_fd_w16le(fd,off);

  // limbs offset
  off += c->cmds_size;
  len = 0;
  // we may need a 2 pass approch to support all redirect
  for(n = 15 ; n >= 0 ; n--) {
    scc_cost_pic_t* p = c->limb_pic[n];
    int i;    

    if(p && p->id == 0xFF) {
      if(p->redir_limb >= n) {
	limbs[n] = limbs[p->redir_limb];
	// this doesn't seems to be needed
	//off += len;
	//len = 0;
      } else {
	  printf("Limb %d has an invalid redirect\n",n);
      }
     
    } else {

      // if we have some length left look what's comming next
      // we should add this length now only if there is no
      // other limb after or the next limb is not a redirect
      if(len) {
	// find the next element
	for(i = n-1 ; i >= 0 ; i--) {
	  if(c->limb_pic[i]) break;
	}
	// look if we need to add the length now
	if(c->limb_pic[i]->id != 0xFF || i < 0) {
	  off += len;
	  len = 0;
	}
      }

      if(p) {
	//limbs[n] = off;
	off += len;
	// count the pictures
	for(i = -1 ; p ; p = p->next) {
	  if(i < p->id) i = p->id;
	}
	// add them
	if(i >= 0) {
	  len = (i+1)*2;
	} else
	  len = 0;
      }
      limbs[n] = off;
    }
  }

  for(n = 15 ; n >= 0 ; n--)
    scc_fd_w16le(fd,limbs[n]);

  anims = calloc(num_anim,2);
  off = 4 + // cost size (repeated apparently)
    2 + // CO header
    1 + // num anim
    1 + // format
    c->pal_size +
    2 + // anim cmds offset
    16*2 + // limbs offset
    num_anim*2;

  for(a = c->anims ; a ; a = a->next) {
    if(a->redir) {
      if(a->redir < a->id)
	anims[a->id] = anims[a->redir];
      else
	printf("Anim %d has an invalid redirect\n",a->id);
      
    } else {
      anims[a->id] = off;
      off += scc_cost_anim_size(a);
    }
  }
    
  for(n = 0 ; n < num_anim ; n++)
    scc_fd_w16le(fd,anims[n]);
    
  free(anims);

  // anim definitions
  for(a = c->anims ; a ; a = a->next) {
    if(a->redir) continue;

    scc_fd_w16le(fd,a->mask);
    for(n = 15 ; n >= 0 ; n--) {
      if(!(a->mask & (1<<n))) continue;
      scc_fd_w16le(fd,a->limb[n].start);
      if(a->limb[n].start != 0xFFFF)
	scc_fd_w8(fd,(a->limb[n].end - a->limb[n].start) | 
		  (a->limb[n].flags << 7));
    }
  }

  // cmds
#ifdef TESTING
  if(scc_cost_get_cmds_offset(c) != scc_fd_pos(fd) - start_pos)
    printf("Cmds start at wrong offset: %d / %d\n",scc_fd_pos(fd) - start_pos,
  	   (int)scc_cost_get_cmds_offset(c));
#endif

  scc_fd_write(fd,c->cmds,c->cmds_size);

  off = scc_cost_get_cmds_offset(c) + c->cmds_size;

  // limb tables, first we need to find the start of the first pic
  for(n = 15 ; n >= 0 ; n--) { 
    scc_cost_pic_t* pic = c->limb_pic[n];
    int num_pic = -1;
    if(!pic || pic->id == 0xFF) continue;

    for( ; pic ; pic = pic->next) {
      if(pic->id > num_pic) num_pic = pic->id;
    }
    if(num_pic >= 0) off += (num_pic+1)*2;
  }
    
  // now we can write the tables
  for(n = 15 ; n >= 0 ; n--) { 
    scc_cost_pic_t* pic = c->limb_pic[n];
    int last_id = -1;

    if(!pic || pic->id == 0xFF) continue;

#ifdef TESTING
    if(limbs[n] != scc_fd_pos(fd) - start_pos)
      printf("Limb table starts at the wrong place: %d / %d\n",
	     (int)scc_fd_pos(fd) - start_pos,limbs[n]);
#endif

    for( ; pic ; pic = pic->next) {
      if(pic->id > last_id+1) {
	for( ; last_id+1 < pic->id ; last_id++)
	  scc_fd_w16le(fd,0);
      }
      last_id = pic->id;

      scc_fd_w16le(fd,off);
      off += 2 + 2 + // width,height
	2 + 2 + // rel_x, rel_y
	2 + 2; // move_x, move_y

      if((c->format & ~(0x81)) == 0x60) 
	off += 1 + 1; // redir_limb, redir_pic
    
      off += pic->data_size;
    }
  }

  // and finnaly write the pics
  for(n = 15 ; n >= 0 ; n--) { 
    scc_cost_pic_t* pic = c->limb_pic[n];

    if(!pic || pic->id == 0xFF) continue;

    for( ; pic ; pic = pic->next) {
      // the header
      scc_fd_w16le(fd,pic->width);
      scc_fd_w16le(fd,pic->height);
      scc_fd_w16le(fd,pic->rel_x);
      scc_fd_w16le(fd,pic->rel_y);
      scc_fd_w16le(fd,pic->move_x);
      scc_fd_w16le(fd,pic->move_y);
      if((c->format & ~(0x81)) == 0x60) {
	scc_fd_w8(fd,pic->redir_limb);
	scc_fd_w8(fd,pic->redir_pic);
      }
      scc_fd_write(fd,pic->data,pic->data_size);
    }
  }

  n = scc_fd_pos(fd) - start_pos;
  if(n % 2) scc_fd_w8(fd,0);

#ifdef TESTING
  n = scc_fd_pos(fd) - start_pos;
  off = scc_cost_size(c);
  if(n != off)
    printf("Failed to write costume (%d / %d => %d )????\n",n,off,n-off);
#endif

  return 1;
}

int scc_write_lfl(scc_fd_t* fd,scc_lfl_t* lfl) {
  scc_cost_t* c;

  printf("Writing LFLF %d\n",lfl->room.idx);

  scc_fd_w32(fd,MKID('R','O','O','M'));
  scc_fd_w32be(fd,8+scc_room_size(&lfl->room));

  scc_write_room(fd,&lfl->room);

  if(lfl->scr) scc_write_data_list(fd,MKID('S','C','R','P'),lfl->scr);
  if(lfl->snd) scc_write_data_list(fd,MKID('S','O','U','N'),lfl->snd);
  //  FIXME
  for(c = lfl->cost ; c ; c = c->next)
    scc_write_cost(fd,c);

  //  if(lfl->cost) scc_write_data_list(fd,MKID('C','O','S','T'),lfl->cost);
  if(lfl->chr) scc_write_data_list(fd,MKID('C','H','A','R'),lfl->chr);

  return 1;
}


int scc_write_lecf(scc_fd_t* fd,scc_res_t* r) {
  scc_lfl_t* i;
  int s,j;
  // LECF block
  scc_fd_w32(fd,MKID('L','E','C','F'));
  scc_fd_w32be(fd,8+scc_lecf_size(r));

  // LOFF block
  s = scc_loff_size(r->idx);
  scc_fd_w32(fd,MKID('L','O','F','F'));
  scc_fd_w32be(fd,8+s);
  // num of block :)
  s -= 1;
  s /= 5;
  scc_fd_w8(fd,s);
  for(j = 0 ; j < r->idx->room_list->size ; j++) {
    if(!r->idx->room_list->room_off[j]) continue;
    scc_fd_w8(fd,j);
    scc_fd_w32le(fd,r->idx->room_list->room_off[j]);
  }

  for(i = r->lfl ; i ; i = i->next) {
    scc_fd_w32(fd,MKID('L','F','L','F'));
    scc_fd_w32be(fd,8+scc_lfl_size(i));
    scc_write_lfl(fd,i);
  }

  return 1;
}
  


int scc_write_res_list(scc_fd_t* fd,uint32_t type,scc_res_list_t* list) {
  int i,size = 8 + scc_res_list_size(list);

  scc_fd_w32(fd,type);
  scc_fd_w32be(fd,size);

  scc_fd_w16le(fd,list->size);
  for(i = 0 ; i < list->size ; i++)
    scc_fd_w8(fd,list->room_no[i]);
  for(i = 0 ; i < list->size ; i++)
    scc_fd_w32le(fd,list->room_off[i]);

  return size;
}

int scc_write_res_idx(scc_fd_t* fd,scc_res_idx_t* idx) {

  scc_fd_w32(fd,MKID('R','N','A','M'));
  scc_fd_w32be(fd,8+1);
  scc_fd_w8(fd,idx->rnam);

  scc_fd_w32(fd,MKID('M','A','X','S'));
  scc_fd_w32be(fd,8+30);
  scc_fd_w16le(fd,idx->num_vars);
  scc_fd_w16le(fd,idx->unk1);
  scc_fd_w16le(fd,idx->num_bit_vars);
  scc_fd_w16le(fd,idx->num_local_objs);
  scc_fd_w16le(fd,idx->num_array);
  scc_fd_w16le(fd,idx->unk2);
  scc_fd_w16le(fd,idx->num_verbs);
  scc_fd_w16le(fd,idx->num_fl_objs);
  scc_fd_w16le(fd,idx->num_inv);
  scc_fd_w16le(fd,idx->num_rooms);
  scc_fd_w16le(fd,idx->num_scr);
  scc_fd_w16le(fd,idx->num_snds);
  scc_fd_w16le(fd,idx->num_chst);
  scc_fd_w16le(fd,idx->num_cost);
  scc_fd_w16le(fd,idx->num_glob_objs);

  // Check me room are different
  // i think offset here indicate the disk
  if(idx->room_list)
    scc_write_res_list(fd,MKID('D','R','O','O'),idx->room_list);

  if(idx->scr_list)
    scc_write_res_list(fd,MKID('D','S','C','R'),idx->scr_list);

  if(idx->snd_list)
    scc_write_res_list(fd,MKID('D','S','O','U'),idx->snd_list);

  if(idx->cost_list)
    scc_write_res_list(fd,MKID('D','C','O','S'),idx->cost_list);
  
  if(idx->chst_list)
    scc_write_res_list(fd,MKID('D','C','H','R'),idx->chst_list);

  if(idx->num_obj_owners > 0) {
    scc_fd_w32(fd,MKID('D','O','B','J'));
    scc_fd_w32be(fd,8+2+5*idx->num_obj_owners);
    scc_fd_w16le(fd,idx->num_obj_owners);
    if(scc_fd_write(fd,idx->obj_owners,idx->num_obj_owners) !=
       idx->num_obj_owners) {
      printf("Write error???\n");
      return 0;
    }
    if(scc_fd_write(fd,idx->class_data,idx->num_obj_owners*4) !=
       idx->num_obj_owners*4) {
      printf("Write error???\n");
      return 0;
    }
  }

  if(idx->aary) {
    scc_aary_t* iter;
    scc_fd_w32(fd,MKID('A','A','R','Y'));
    scc_fd_w32be(fd,8 + scc_aary_size(idx->aary));
    for(iter = idx->aary ; iter ; iter = iter->next) {
      scc_fd_w16le(fd,iter->idx);
      scc_fd_w16le(fd,iter->a);
      scc_fd_w16le(fd,iter->b);
      scc_fd_w16le(fd,iter->c);
    }
    scc_fd_w16le(fd,0);
  }

  return 1;
}


