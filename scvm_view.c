
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
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"


static void scale_copy(uint8_t* dst, int dst_stride,
                       unsigned clip_width, unsigned clip_height,
                       int x, int y,
                       int dst_width, int dst_height,
                       uint8_t* src, int src_stride,
                       int src_width, int src_height) {
  int sx = 0,sy = 0,dx = 0,dy = 0,xerr = 0,yerr = 0,skip = 0;

  dst += dst_stride*y;
  dst += x;

  while(dy < dst_height && dy+y < clip_height) {
    if(!skip && dy+y >= 0) {
      dx = sx = 0;
      while(dx < dst_width) {
        if(!skip && dx+x >= 0 && dx+x < clip_width)
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


int scvm_view_draw(scvm_t* vm, scvm_view_t* view,
                   uint8_t* buffer, int stride,
                   unsigned width, unsigned height) {
  int sx,dx,dy,w,h,a;

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
  
  dx = (view->screen_width-w)*width/view->screen_width/2;
  dy = view->room_start*height/view->screen_height;

  scale_copy(buffer,stride,width,height,
             dx,dy,
             w*width/view->screen_width,
             h*height/view->screen_height,
             vm->room->image.data+sx, vm->room->width,
             w,h);

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
               img->data, obj_w, obj_w, obj_h);
  }
  
  for(a = 0 ; a < vm->num_actor ; a++) {
    if(!vm->actor[a].room ||
       vm->actor[a].room != vm->room->id ||
       !vm->actor[a].costdec.cost) continue;
    scc_log(LOG_MSG,"Draw actor %d at %dx%d\n",a,vm->actor[a].x,vm->actor[a].y);
    scc_cost_dec_frame(&vm->actor[a].costdec,
                       buffer + dy*stride + dx,
                       (vm->actor[a].x-sx)*width/view->screen_width,
                       vm->actor[a].y*height/view->screen_height,
                       width,height,stride,
                       vm->actor[a].scale_x*width/view->screen_width,
                       vm->actor[a].scale_y*height/view->screen_height);
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
  int r, x = vm->view->camera_x;
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
