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


void scvm_actor_init(scvm_actor_t* a) {
    
    a->width = 24;
    //a->height = ??;
    a->talk_color = 0x0F;
    a->talk_x = 0;
    a->talk_y = -80;
    a->talk_script = 0;
    
    a->scale_x = a->scale_y = 0xFF;
    // a->charset
    a->direction = 0;
    a->flags = 0;
    
    a->init_frame = 1;
    a->walk_frame = 2;
    a->stand_frame = 3;
    a->talk_start_frame = 4;
    a->talk_end_frame = 5;
}

void scvm_actor_put_at(scvm_actor_t* a, int x, int y, unsigned room) {
  a->x = x;
  a->y = y;
  if(room) a->room = room;
}

void scvm_actor_set_costume(scvm_actor_t* a, scc_cost_t* cost) {
  scc_cost_dec_init(&a->costdec);
  a->costdec.cost = cost;
  scc_cost_dec_load_anim(&a->costdec,a->init_frame<<2);
}

void scvm_actor_animate(scvm_actor_t* a, int anim) {
  if(anim >= 0xFC) {
    scc_cost_dec_load_anim(&a->costdec,a->stand_frame<<2);
    // stop moving
    return;
  }
  if(anim >= 0xF8) {
    // set direction
    return;
  }
  if(anim >= 0xF4) {
    // turn to direction
    return;
  }
  scc_cost_dec_load_anim(&a->costdec,anim);
}
