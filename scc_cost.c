#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc.h"


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
			int trans) {
  int shr,mask,x = 0,y = 0,end = 0,pos = 0;
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
    printf("Costume picture have an unknow palette size: %d\n",cost->pal_size);
    return 0;
  }

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
      if(!(x < x_min || x > x_max || 
	   y < y_min || y > y_max || 
	   trans == color))
	dst[dst_stride*y+x] = color;
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
    printf("Costume picture have an unknow palette size: %d\n",cost->pal_size);
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
