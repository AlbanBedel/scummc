
/* ScummC
 * Copyright (C) 2006  Alban Bedel
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
 * @file scvm_view.c
 * @ingroup scvm
 * @brief SCVM rendering
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
#include "scc_cost.h"
#include "scc_box.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"


static void scale_copy(uint8_t* dst, int dst_stride,
                       unsigned clip_width, unsigned clip_height,
                       int x, int y,
                       int dst_width, int dst_height,
                       uint8_t* src, int src_stride,
                       int src_width, int src_height,
                       int trans) {
  int sx = 0,sy = 0,dx = 0,dy = 0,xerr = 0,yerr = 0,skip = 0;

  dst += dst_stride*y;
  dst += x;

  while(dy < dst_height && dy+y < clip_height) {
    if(!skip && dy+y >= 0) {
      dx = sx = 0;
      while(dx < dst_width) {
        if(!skip && dx+x >= 0 && dx+x < clip_width &&
           (trans < 0 || trans != src[sx]))
          dst[dx] = src[sx];
        xerr += dst_width;
        if(xerr<<1 >= src_width) {
          xerr -= src_width;
          dx++;
          skip = 0;
          if(xerr<<1 >= src_width) {
            xerr -= dst_width;
            continue;
          }
        } else
          skip = 1;
        sx++;
      }
    }
    yerr += dst_height;
    if(yerr<<1 >= src_height) {
      yerr -= src_height;
      dy++;
      dst += dst_stride;
      skip = 0;
      if(yerr<<1 >= src_height) {
        yerr -= dst_height;
        continue;
      }
    } else
      skip = 1;
    sy++;
    src += src_stride;
  }
}

uint8_t* make_zplane(scvm_t* vm, scvm_view_t* view,
                     unsigned view_width, unsigned view_height,
                     unsigned dst_width, unsigned dst_height,
                     unsigned src_width, unsigned src_height,
                     unsigned src_x, unsigned zid) {
  uint8_t* zplane = malloc(dst_width*dst_height);
  unsigned o,obj_w,obj_h;

  if(vm->room->image.zplane[zid])
      scale_copy(zplane,dst_width,dst_width,dst_height,
                 0,0,dst_width,dst_height,
                 vm->room->image.zplane[zid] + src_x,vm->room->width,
                 src_width, src_height,-1);
  else
      memset(zplane,0,dst_width*dst_height);

  for(o = 0 ; o < vm->room->num_object ; o++) {
    scvm_object_t* obj = vm->room->object[o];
    scvm_image_t* img;
    if(!obj->pdata->state ||
       obj->pdata->state > obj->num_image ||
       zid > obj->num_zplane)
      continue;
    img = &obj->image[obj->pdata->state];
    if(!img->zplane || !img->zplane[zid]) continue;
    obj_w = obj->width;
    obj_h = obj->height;
    if(obj->x >= src_x + vm->room->width ||
       obj->x + obj->width < src_x ||
       obj->y >= src_height ||
       obj->y + obj_h < 0)
      continue;

    scale_copy(zplane,dst_width,dst_width,dst_height,
               (obj->x-src_x)*view_width/view->screen_width,
               obj->y*view_height/view->screen_height,
               obj_w*view_width/view->screen_width,
               obj_h*view_height/view->screen_height,
               img->zplane[zid], obj_w, obj_w, obj_h,
               -1);
  }
  return zplane;
}


int scvm_view_draw(scvm_t* vm, scvm_view_t* view,
                   uint8_t* buffer, int stride,
                   unsigned width, unsigned height) {
  int sx,dx,dy,w,h,dw,dh,a;
  int i,num_actor = 0;
  scvm_actor_t* actor[vm->num_actor];

  if(!vm->room) return 0;

  h = vm->room->height;
  if(view->room_start + h != view->room_end)
    scc_log(LOG_WARN,"View setup doesn't match the room height.\n");
  
  if(view->room_start + h > height)
    h = height - view->room_start;
  
  if(h <= 0) return 0;  
  w = vm->room->width;
  if(w > view->screen_width) w = view->screen_width;
  
  sx = vm->view->camera_x-w/2;
  if(sx + w/2 > vm->room->width) sx = vm->room->width-w;
  if(sx < 0) sx = 0;
  
  dw = w*width/view->screen_width;
  dh = h*height/view->screen_height;
  dx = (view->screen_width-w)*width/view->screen_width/2;
  dy = view->room_start*height/view->screen_height;

  scale_copy(buffer,stride,width,height,
             dx,dy,dw,dh,
             vm->room->image.data+sx, vm->room->width,
             w,h,-1);

  for(a = 0 ; a < vm->room->num_object ; a++) {
    scvm_object_t* obj = vm->room->object[a];
    scvm_image_t* img;
    int obj_w,obj_h;
    if(!obj->pdata->state ||
       obj->pdata->state > obj->num_image)
      continue;
    img = &obj->image[obj->pdata->state];
    obj_w = obj->width;
    obj_h = obj->height;
    if(obj->x >= sx + vm->room->width ||
       obj->x + obj->width < sx ||
       obj->y >= h ||
       obj->y + obj_h < 0)
      continue;

    scale_copy(buffer,stride,width,height,
               dx + (obj->x-sx)*width/view->screen_width,
               dy + obj->y*height/view->screen_height,
               obj_w*width/view->screen_width,
               obj_h*height/view->screen_height,
               img->data, obj_w, obj_w, obj_h,
               img->have_trans ? vm->room->trans : -1);
  }

  for(a = 0 ; a < vm->num_verb ; a++) {
    scvm_image_t* img;
    int vrb_x,vrb_y,vrb_w,vrb_h;
    scvm_verb_t* vrb = vm->verb + a;
    if(!vrb->mode || vrb->save_id ||
       !(img = scvm_get_verb_image(vm,vrb))) continue;
    vrb_w = vrb->width;
    vrb_h = vrb->height;
    vrb_x = vrb->x;
    vrb_y = vrb->y;
    if(vrb->flags & SCVM_VERB_CENTER)
        vrb_x -= vrb_w/2;
    if(vrb_x >=  view->screen_width ||
       vrb_x + vrb_w < 0 ||
       vrb_y >= view->screen_height ||
       vrb_y + vrb_h < 0)
      continue;

    scale_copy(buffer,stride,width,height,
               vrb_x*width/view->screen_width,
               vrb_y*height/view->screen_height,
               vrb_w*width/view->screen_width,
               vrb_h*height/view->screen_height,
               img->data, vrb_w, vrb_w, vrb_h,
               img->have_trans ? vm->room->trans : -1);
  }

  for(a = 0 ; a < vm->num_actor ; a++) {
    if(!vm->actor[a].room ||
       vm->actor[a].room != vm->room->id ||
       !vm->actor[a].costdec.cost) continue;
    for(i = 0 ; i < num_actor ; i++)
      if(vm->actor[a].y < actor[i]->y) break;
    if(i < num_actor) memmove(&actor[i+1],&actor[i],
                              (num_actor-i)*sizeof(scvm_actor_t*));
    actor[i] = &vm->actor[a];
    num_actor += 1;
  }

  for(a = 0 ; a < num_actor ; a++) {
    uint8_t* zplane = NULL;
    if(actor[a]->box) {
      int mask = vm->room->box[actor[a]->box].mask;
      if(mask && mask <= vm->room->num_zplane) {
        if(!vm->room->zplane[mask])
          vm->room->zplane[mask] = make_zplane(vm,view,
                                               width,height,
                                               dw,dh,
                                               w,h,sx,mask);
        zplane = vm->room->zplane[mask];
      }
    }

    scc_log(LOG_MSG,"Draw actor %d at %dx%d (zplane: %d)\n",a,
            actor[a]->x,actor[a]->y,
            zplane ? vm->room->box[actor[a]->box].mask : -1);

    scc_cost_dec_frame(&actor[a]->costdec,
                       buffer + dy*stride + dx,
                       (actor[a]->x-sx)*width/view->screen_width,
                       actor[a]->y*height/view->screen_height,
                       dw,dh,stride,
                       zplane,dw,
                       actor[a]->scale_x*width/view->screen_width,
                       actor[a]->scale_y*height/view->screen_height);
  }

  // TODO: Use some invalidation to avoid recomputing the
  //       whole thing when not needed
  for(a = 0 ; a <= vm->room->num_zplane ; a++)
    if(vm->room->zplane[a]) {
      free(vm->room->zplane[a]);
      vm->room->zplane[a] = NULL;
    }

  return 1;
}

void scvm_view_scale_palette(scvm_view_t* view, scvm_color_t* palette,
                             unsigned red, unsigned green, unsigned blue,
                             unsigned start, unsigned end) {
  if(start > 0xFF) return;
  if(start < 0) start = 0;
  if(end > 0xFF) end = 0xFF;
  
  while(start <= end) {
    int color = palette[start].r;
    color = color * red / 0xFF;
    if(color > 0xFF) color = 0xFF;
    view->palette[start].r = color;
    
    color = palette[start].g;
    color = color * green / 0xFF;
    if(color > 0xFF) color = 0xFF;
    view->palette[start].g = color;
    
    color = palette[start].b;
    color = color * blue / 0xFF;
    if(color > 0xFF) color = 0xFF;
    view->palette[start].b = color;
    
    view->flags |= SCVM_VIEW_PALETTE_CHANGED;
    start++;
  }
  
}

int scvm_abs_position_to_virtual(scvm_t* vm, int* dx, int* dy) {
    int x = *dx, y = *dy;
    if(y < vm->view->room_start || y >= vm->view->room_end) return -1;
    y -= vm->view->room_start;
    x += (vm->view->camera_x-vm->view->screen_width/2);
    if(x < 0 || x > vm->room->width) return -1;
    *dx = x;
    *dy = y;
    return 0;
}

void scvm_pan_camera_to(scvm_t* vm, int x) {
  if(x < vm->var->camera_min_x)
    x = vm->var->camera_min_x;
  if(x > vm->var->camera_max_x)
    x = vm->var->camera_max_x;
  vm->view->camera_dst_x = x & ~7;
  vm->view->flags |= SCVM_VIEW_PAN;
}


int scvm_set_camera_at(scvm_t* vm, int x) {
  int r;
  if(x < vm->var->camera_min_x)
    x = vm->var->camera_min_x;
  if(x > vm->var->camera_max_x)
    x = vm->var->camera_max_x;
  vm->view->camera_x = x & ~7;
  if(vm->var->camera_script) {
    vm->var->camera_pos_x = vm->view->camera_x;
    if((r = scvm_start_script(vm,0,vm->var->camera_script,NULL)) < 0)
      return r;
    return r+1;
  }
  return 0;
}

int scvm_move_camera(scvm_t* vm) {
  int fast = vm->var->camera_fast_x;
  int x = vm->view->camera_x;
  // Keep into range. Such adjustement are not
  // calling the camera script
  if(x < vm->var->camera_min_x) {
    if(fast)
      vm->view->camera_x = vm->var->camera_min_x & ~7;
    else
      vm->view->camera_x += 8;
    return 0;
  }

  if(x > vm->var->camera_max_x) {
    if(fast)
      vm->view->camera_x = vm->var->camera_max_x & ~7;
    else
      vm->view->camera_x -= 8;
    return 0;
  }

  if(vm->view->follow) {
    scvm_actor_t* a = &vm->actor[vm->view->follow];
    // In wich strip is the actor
    int stripe = (a->x-x)/8;
    if(abs(stripe) < vm->view->screen_width/8/2-10)
      return 0;

    vm->view->camera_dst_x = a->x & ~7;
  } else {
    if(!(vm->view->flags & SCVM_VIEW_PAN))
      return 0;

    if(vm->view->camera_dst_x == vm->view->camera_x) {
      vm->view->flags &= ~SCVM_VIEW_PAN;
      return 0;
    }
  }

  if(fast)
    x = vm->view->camera_dst_x;
  else {
    if(x < vm->view->camera_dst_x)
      x += 8;
    else
      x -= 8;
  }

  if(x < vm->var->camera_min_x)
    x = vm->var->camera_min_x;
  if(x > vm->var->camera_max_x)
    x = vm->var->camera_max_x;

  x &= ~7;

  if(x == vm->view->camera_x) {
    vm->view->flags &= ~SCVM_VIEW_PAN;
    return 0;
  }

  return scvm_set_camera_at(vm,x);
}
