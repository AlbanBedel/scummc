
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


int scvm_view_draw(scvm_t* vm, scvm_view_t* view,
                   uint8_t* buffer, int stride,
                   unsigned width, unsigned height) {
  int sx,dx,w,h,y;

  if(width < 320 || height < 200 ||
     !vm->room) return 0;

  h = vm->room->height;
  if(view->room_start + h != view->room_end)
    scc_log(LOG_WARN,"View setup doesn't match the room height.\n");
  
  if(view->room_start + h > height)
    h = height - view->room_start;
  
  if(h > 0) {  
    w = vm->room->width;
    if(w > width) w = width;
  
    sx = vm->view->camera_x-w/2;
    if(sx + w/2 > vm->room->width) sx = vm->room->width-w;
    if(sx < 0) sx = 0;
  
    dx = (width-w)/2;

    for(y = 0 ; y < h ; y++)
      memcpy(buffer + (view->room_start+y)*stride + dx,
             vm->room->image.data + y*vm->room->width + sx,w);
  }
  
  return 1;
}
