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
 * @file scc_cost.c
 * @ingroup scumm
 * @brief SCUMM costume parser and decoder
 */

#include "config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"


scc_cost_anim_t* scc_cost_new_anim(scc_cost_t* cost,uint8_t id) {
  scc_cost_anim_t* ani = calloc(1,sizeof(scc_cost_anim_t));
  scc_cost_anim_t* iter;

  ani->id = id;

  if(!cost->anims) {
    cost->anims = ani;
    return cost->anims;
  }

  if(cost->anims->id > id) {
    ani->next = cost->anims;
    cost->anims = ani;
    return cost->anims;
  }

  for(iter = cost->anims ; iter->next && iter->next->id <= id ;
      iter = iter->next);

  if(iter->id == id) return NULL;

  ani = calloc(1,sizeof(scc_cost_anim_t));
  ani->id = id;

  ani->next = iter->next;
  iter->next = ani;
  
  return ani;
}

scc_cost_anim_t* scc_cost_get_anim(scc_cost_t* cost,uint8_t id) {
  scc_cost_anim_t* ani = cost->anims;
  int redir = 8;
  while(ani) {
    // Found
    if(ani->id == id) {
      // No redirect (or slightly invalid)
      if(ani->redir == 0xFF ||
         ani->redir == id)
        break;
      // Too deep redirect
      if(redir <= 0)
        return NULL;
      // Redirect
      redir--;
      id = ani->redir;
      // Restart
      if(id < ani->id) {
        ani = cost->anims;
        continue;
      }
    }
    ani = ani->next;
  }
  return ani;
}

int scc_cost_add_pic(scc_cost_t* cost,uint8_t limb,scc_cost_pic_t* pic) {
  scc_cost_pic_t* iter;

  if(limb > 15) return 0;

  if(!cost->limb_pic[limb]) {
    cost->limb_pic[limb] = pic;
    return 1;
  }
  
  if(pic->id < cost->limb_pic[limb]->id) {
    pic->next = cost->limb_pic[limb];
    cost->limb_pic[limb] = pic;
    return 1;
  }

  for(iter = cost->limb_pic[limb] ; iter->next && iter->next->id <= pic->id ;
      iter = iter->next);

  if(iter->id == pic->id) return 0;
   
  pic->next = iter->next;
  iter->next = pic;
 
  return 1;
}

int scc_cost_decode_pic(scc_cost_t* cost,scc_cost_pic_t* pic,
			uint8_t* dst,int dst_stride, 
			int x_min,int x_max,int y_min,int y_max,
			int trans, int x_scale, int y_scale,
			int y_flip) {
  int shr,mask,x = 0,y = 0,end = 0,pos = 0, x_step;
  int yerr = 0, xerr = 0, xskip = 0, yskip = 0, dx = 0, dy = 0;
  uint8_t color,rep;

  switch(cost->pal_size) {
  case 16:
    shr = 4;
    mask = 0x0F;
    break;
  case 32:
    shr = 3;
    mask = 0x07;
    break;
  default:
    printf("Costume picture has an unknown palette size: %d\n",cost->pal_size);
    return 0;
  }

  if(y_flip) {
    dx = pic->width-1;
    x_step = -1;
  } else
    x_step = 1;

  while(!end) {
    if(pos >= pic->data_size) {
      printf("Error while decoding costume picture.\n");
      return 0;
    }
    rep = pic->data[pos]; pos++;
    color = cost->pal[rep >> shr];
    rep &= mask;
    if(!rep) {
      if(pos >= pic->data_size) {
	printf("Error while decoding costume picture.\n");
	return 0;
      }
      rep = pic->data[pos]; pos++;
    }
    while(rep) {
      if(dx >= x_min && dx < x_max &&
         dy >= y_min && dy < y_max &&
         xskip == 0 && yskip == 0 &&
         trans != color)
	dst[dst_stride*dy+dx] = color;
      y++;
      yerr += y_scale;
      if(yerr<<1 >= 255) {
        yerr -= 255;
        dy++;
        yskip = 0;
      } else
        yskip = 1;
      if(y >= pic->height) {
	y = 0, dy = 0, x++;
	if(x >= pic->width) {
	  end = 1;
	  break;
	}
        xerr += x_scale;
        if(xerr<<1 >= 255) {
          xerr -= 255;
          dx += x_step;
          xskip = 0;
        } else
          xskip = 1;
      }
      rep--;
    }
  }
  
  return 1;
}

int scc_read_cost_pic(scc_fd_t* fd,scc_cost_t* cost,scc_cost_pic_t* pic,int len,int* posp) {
  int pos = *posp,off = *posp;
  int shr,mask,x = 0,y = 0,end = 0;
  uint8_t rep;

  switch(cost->pal_size) {
  case 16:
    shr = 4;
    mask = 0x0F;
    break;
  case 32:
    shr = 3;
    mask = 0x07;
    break;
  default:
    printf("Costume picture has an unknown palette size: %d\n",cost->pal_size);
    return 0;
  }

  pic->data = malloc(pic->width*pic->height);
  
  while(!end) {
    if(pos >= len) {
      free(pic->data); pic->data = NULL;
      *posp = pos;
      return 0;
    }
    pic->data[pos-off] = rep = scc_fd_r8(fd); pos++;
    rep &= mask;
    if(!rep) {
      if(pos >= len) {
	free(pic->data); pic->data = NULL;
	*posp = pos;
	return 0;
      }
      pic->data[pos-off] = rep = scc_fd_r8(fd); pos++;
    }
#if 1
    x += rep/pic->height;
    y += rep%pic->height;
    if(y >= pic->height) { x++; y -= pic->height; }
    if(x >= pic->width) end = 1;
    rep = 0;
#else
    while(rep) {
      y++;
      if(y >= pic->height) {
	y = 0; x++;
	if(x >= pic->width) {
	  end = 1;
	  break;
	}
      }
      rep--;
    }
#endif
  }

  *posp = pos;
  pic->data_size = pos - off;
  //printf("Pict size: %d\n",pic->data_size);

  return 1;
}


scc_cost_pic_t* scc_cost_get_limb_pic(scc_cost_t* cost,uint8_t limb, 
				      uint8_t pid,int max_depth) {
  scc_cost_pic_t* pic;

  if(limb >= 16 || max_depth == 0 || !cost->limb_pic[limb]) return NULL;

  max_depth--;

  if(cost->limb_pic[limb]->id == 0xFF) {
    pic = scc_cost_get_limb_pic(cost,cost->limb_pic[limb]->redir_limb,pid,max_depth);
    if(pic) return pic;
    printf("Limb redirect from %u to %u failed\n",limb,cost->limb_pic[limb]->redir_limb);
    return NULL;
  }
    

  for(pic = cost->limb_pic[limb] ; pic ; pic = pic->next) {
    if(pic->id != pid) continue;

    if(pic->redir_limb == 0xFF && pic->redir_pic == 0xFF) return pic;

    //printf("Redirecting from %d-%d to %d-%d\n",limb,pid,
    //   pic->redir_limb,pic->redir_pic);
    pic = scc_cost_get_limb_pic(cost,pic->redir_limb,pic->redir_pic,max_depth);
    if(pic) return pic;
    
    printf("Costume picture redirect from %u-%u failed\n",limb,pid);
    break;
  }

  return NULL;
}

scc_cost_t* scc_parse_cost(scc_fd_t* fd,int len) {
  uint8_t num_anim;
  uint16_t cmds_off,mask,cmask = 0;
  uint16_t limbs[16],pict_off = 0;
  int i,j,pos = 0,cmd_end;
  uint32_t unk;
  scc_cost_t* cost = calloc(1,sizeof(scc_cost_t));
  scc_cost_anim_t* ani;

  unk = scc_fd_r32le(fd);
  if(scc_fd_r16le(fd) != 0x4f43) printf("Bad CO header???\n");

  pos += 3*2;
  // number of anims
  num_anim = scc_fd_r8(fd); pos += 1;
  if(unk) { 
    num_anim += 1;
  }
  
  // format, tell palette size
  if(unk)
    cost->flags |= SCC_COST_HAS_SIZE;
  //  cost->size = unk;
  cost->format = scc_fd_r8(fd); pos += 1;
  cost->pal_size = (cost->format & 1) ? 32 : 16;

  // skip palette
  cost->pal = scc_fd_load(fd,cost->pal_size); pos += cost->pal_size;

  cmds_off = scc_fd_r16le(fd); pos += 2;

  // read limbs table
  for(i = 15 ; i >= 0 ; i--)
    limbs[i] = scc_fd_r16le(fd);
  pos += 16*2;

  if(num_anim > 0) {
    uint16_t aoff[num_anim];
    uint16_t alen;

    for(i = 0 ; i < num_anim ; i++) {
      aoff[i] = scc_fd_r16le(fd); pos += 2;
    }

    // compute the global anim mask (ie which limbs are used)
    for(i = 0 ; i < num_anim ; i++) {
      if(!aoff[i]) continue;
      if(pos != aoff[i]) {
	// we migth need to seek back as sometimes several entries in the
	// offset table give the same value.
	scc_fd_seek(fd,aoff[i]-pos,SEEK_CUR); pos = aoff[i];
      }

      mask = scc_fd_r16le(fd); pos += 2;
      cmask |= mask;

    }
    
    if(!cmask) {
      printf("Costume with no limbs????\n");
      scc_fd_seek(fd,len-pos,SEEK_CUR);
      return cost;
    }

    // Read the commands
    // find the first limb (yep it go backward)
    for(i = 15 ; ! ( (cmask & (1<<i)) && limbs[i] ) ; i--);
    cmd_end = limbs[i];
    scc_fd_seek(fd,cmds_off-pos,SEEK_CUR);
    pos = cmds_off;

    cost->cmds_size = cmd_end-cmds_off;
    cost->cmds = scc_fd_load(fd,cost->cmds_size); pos += cost->cmds_size;
    
    // Read the anim definitions
    for(i = 0 ; i < num_anim ; i++) {
      if(!aoff[i]) continue;

      if(pos != aoff[i]) {
	// we migth need to seek back as sometimes several entries in the
	// offset table give the same value.
	scc_fd_seek(fd,aoff[i]-pos,SEEK_CUR); pos = aoff[i];
      }

      ani = scc_cost_new_anim(cost,i);
      if(!ani) continue;

      for(j = 0 ; j < i ; j++) {
	if(aoff[j] == aoff[i]) break;
      }

      if(j < i) {
	ani->redir = j;
     } else {

	ani->mask = mask = scc_fd_r16le(fd); pos += 2;
	ani->redir = 0xFF;

	for(j = 15 ; j >= 0 ; j--) {
	  
	  if(mask & 0x8000) {
	    ani->limb[j].start = scc_fd_r16le(fd); pos += 2;
	    if(ani->limb[j].start != 0xFFFF) {
	      alen = scc_fd_r8(fd); pos += 1;
	      ani->limb[j].end = ani->limb[j].start + (alen & 0x7F);
	      ani->limb[j].flags = !(alen>>7);
	    }
	  } else
	    ani->limb[j].start = 0xFFFF;

	  mask <<= 1;
	}
      }
    }
  }    

        
  // read the limbs
  for(i = 15 ; i >= 0 ; i--) {
    int limb_size;
    if(!(cmask & (1<<i))) continue;

    // look if it's a duplicate limb
    for(j = 15 ; j > i ; j--) {
      if((cmask & (1<<j)) && limbs[j] == limbs[i]) break;
    }

    // it's a dup, so add a special pic
    if(j > i) {
      scc_cost_pic_t* pic = calloc(1,sizeof(scc_cost_pic_t));
      pic->id = 0xFF;
      pic->redir_limb = j;
      cost->limb_pic[i] = pic;
      continue;
    }

    // Got to the limb table
    scc_fd_seek(fd,limbs[i]-pos,SEEK_CUR);
    pos = limbs[i];

    // Try to find out the table entry size
    for(j = i - 1 ; j >= 0 ; j--) {
      if(cmask & (1<<j) && limbs[j] != limbs[i]) break;
    }

    if(j >= 0)  limb_size = limbs[j]-limbs[i];
    else {
      while(!pict_off) {

	  for(j = 15 ; j >= 0 ; j--) {
	    if((cmask & (1<<j)) && limbs[j]) break;
	  }
	 
	  scc_fd_seek(fd,limbs[j]-pos,SEEK_CUR); pos = limbs[j];
	  while(!pict_off) {
	    pict_off = scc_fd_r16le(fd); pos += 2;
	  }
      }
      limb_size = pict_off-limbs[i];
    }

    if(!(limb_size && limbs[i])) continue;

    // parse the pictures    
    for(j = 0 ; j < limb_size/2 ; j++) {
      uint16_t poff;
      scc_cost_pic_t* pic;
      
      scc_fd_seek(fd,limbs[i]+j*2-pos,SEEK_CUR); pos = limbs[i]+j*2;
      poff = scc_fd_r16le(fd); pos += 2;


      if(poff > 0) {
	//printf("  pict %d %d: 0x%x \n",i,j,poff);      
	scc_fd_seek(fd,poff-pos,SEEK_CUR); pos = poff;
	
	pic = calloc(1,sizeof(scc_cost_pic_t));
	pic->id = j;
	pic->width = scc_fd_r16le(fd); pos += 2;
	pic->height = scc_fd_r16le(fd); pos += 2;
	pic->rel_x = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->rel_y = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->move_x = (int16_t)scc_fd_r16le(fd); pos += 2;
	pic->move_y = (int16_t)scc_fd_r16le(fd); pos += 2;
	
	if((cost->format & ~(0x81)) == 0x60) {	  
	  pic->redir_limb = scc_fd_r8(fd); pos++;
	  pic->redir_pic = scc_fd_r8(fd); pos++;	
	} else {
	  pic->redir_limb = pic->redir_pic = 0xFF;
	}

	if(pic->redir_limb == 0xFF && pic->redir_pic == 0xFF) {
	  if(!scc_read_cost_pic(fd,cost,pic,len,&pos))
	    printf("Failed to decode picture %d of limb %d\n",j,i);    
	}
	scc_cost_add_pic(cost,i,pic);
      }
    }
  }
    
  scc_fd_seek(fd,len-pos,SEEK_CUR);

#if 0
  i = scc_cost_size(cost);
  if(i != len)
    printf("COST size failed (%d/%d)\n",i,len);
#endif
  
  return cost;
}



void scc_cost_dec_init(scc_cost_dec_t* dec) {
  memset(dec,0,sizeof(scc_cost_dec_t));
  memset(dec->pc,0xFF,16*2);
}

int scc_cost_dec_load_anim(scc_cost_dec_t* dec,uint16_t aid) {
  scc_cost_anim_t* anim;
  int i;
  uint8_t cmd;

  if(!(anim = scc_cost_get_anim(dec->cost,aid)))
    return 0;

  dec->anim = anim;

  for(i = 0 ; i < 16 ; i++) {
    dec->pc[i] = anim->limb[i].start;
    if(dec->pc[i] == 0xFFFF) continue;
    if(dec->pc[i] >= dec->cost->cmds_size) {
      printf("Warning: limb %d got out of the cmd array\n",i);
      continue;
    }
    cmd = dec->cost->cmds[dec->pc[i]];
    if(cmd == 0x79)
      dec->stopped |= (1 << i);
    else if(cmd == 0x7A)
      dec->stopped &= ~(1 << i);
  }

  return 1;
}

int scc_cost_dec_step(scc_cost_dec_t* dec) {
  uint8_t cmd;
  int i;

  for(i = 0 ; i < 16 ; i++) {
    if(dec->anim->limb[i].start == 0xFFFF) continue;
    
    while(1) {
      if(dec->pc[i] >= dec->anim->limb[i].end) {
	if(dec->anim->limb[i].flags & SCC_COST_ANIM_LOOP)
	  dec->pc[i] = dec->anim->limb[i].start;
      } else
	dec->pc[i]++;
	 
      if(dec->pc[i] >= dec->cost->cmds_size) return 0;
      cmd = dec->cost->cmds[dec->pc[i]];
    
      if(cmd == 0x7C) {
	// anim counter ++ ?
	if(dec->anim->limb[i].start != dec->anim->limb[i].end &&
	   dec->pc[i] != dec->anim->limb[i].end) continue;
      } else if(cmd > 0x70 && cmd < 0x79) {
	// queue soound ?
	if(dec->anim->limb[i].start != dec->anim->limb[i].end &&
	   dec->pc[i] != dec->anim->limb[i].end) continue;
      }
      break;
    }
  }

  return 1;
}

int scc_cost_dec_bbox(scc_cost_dec_t* dec,int* x1p,int* y1p,
		      int* x2p,int* y2p) {
  scc_cost_pic_t* pic;
  int x1 = 34000, y1 = 34000, x2 = -34000, y2 = -34000;
  int i,n = 0;
  uint8_t cmd;

  if(!dec->anim) return 0;

  for(i = 0 ; i < 16 ; i++) {
    if(dec->pc[i] == 0xFFFF || dec->stopped & (1<<i)) continue;
    
    if(dec->pc[i] >= dec->cost->cmds_size) {
      printf("Warning: limb %d got out of the cmd array\n",i);
      continue;
    }
    cmd = dec->cost->cmds[dec->pc[i]];
    
    if(cmd > 0x70) continue;
    
    pic = scc_cost_get_limb_pic(dec->cost,i,cmd,4);
    if(!pic) continue;
    
    n++;
    
    if(pic->rel_x < x1) x1 = pic->rel_x;
    if(pic->rel_y < y1) y1 = pic->rel_y;
    if(pic->rel_x+pic->width > x2) x2 = pic->rel_x+pic->width;
    if(pic->rel_y+pic->height > y2) y2 = pic->rel_y+pic->height;
  }
  
   
  if(!n) return 0;

  *x1p = x1;
  *x2p = x2;
  *y1p = y1;
  *y2p = y2;

  return 1;
}

int scc_cost_dec_frame(scc_cost_dec_t* dec,uint8_t* dst,
		       int x, int y,
		       int dst_width, int dst_height,
		       int dst_stride,
                       int x_scale, int y_scale) {
  int i,l,c,l_max,c_max,rel_x,rel_y,width,height,flip;
  scc_cost_pic_t* pic;
  uint8_t cmd,trans = dec->cost->pal[0];

  if(!dec->anim)
    return 0;

  flip = (!(dec->cost->format & 0x80)) && (!(dec->anim->id&3));

  for(i = 15 ; i >= 0 ; i--) {
    if(dec->pc[i] == 0xFFFF || dec->stopped & (1<<i)) continue;
    
    if(dec->pc[i] >= dec->cost->cmds_size) {
      printf("Warning: limb %d got out of the cmd array\n",i);
      continue;
    }
    cmd = dec->cost->cmds[dec->pc[i]];

    if(cmd > 0x70) continue;
      
    pic = scc_cost_get_limb_pic(dec->cost,i,cmd,4);
    if(!pic) {
      printf("Failed to find pic %d of limb %d\n",cmd,i);
      continue;
    }

    if(!pic->data) continue;
  
    rel_x = pic->rel_x*x_scale/255;
    rel_y = pic->rel_y*y_scale/255;
    width = pic->width*x_scale/255;
    height = pic->width*y_scale/255;
    if(flip) rel_x = -width-rel_x;
    l = c = 0;
    if(x+rel_x < 0) c = -x-rel_x;
    if(y+rel_y < 0) l = -y-rel_y;
    c_max = width;
    l_max = height;
    if(x+rel_x + width > dst_width)
      c_max = dst_width-x-rel_x;
    if(y+rel_y + height > dst_height)
      l_max = dst_height-y-rel_y;

    if(c_max < 0 || l_max < 0) continue;

    
    scc_cost_decode_pic(dec->cost,pic,
			&dst[dst_stride*(y+rel_y)+x+rel_x],
			dst_stride,
			c,c_max,l,l_max,trans,x_scale,y_scale,flip);
  }

  return 1;
}

