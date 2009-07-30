/* ScummC
 * Copyright (C) 2008  Alban Bedel
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
 * @file scvm_verb.c
 * @ingroup scvm
 * @brief SCVM verbs
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"
#include "scc_box.h"
#include "scc_char.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

scvm_verb_t* scvm_verb_init(scvm_verb_t* v, unsigned id, unsigned charset) {
    memset(v,0,sizeof(*v));
    v->id         = id;
    v->color      = 2;
    v->hi_color   = 0;
    v->dim_color  = 8;
    v->charset    = charset;
    return v;
}

scvm_verb_t* scvm_get_verb(scvm_t* vm, unsigned id, unsigned save_id) {
    unsigned i;
    for(i = 0 ; i < vm->num_verb ; i++)
        if(vm->verb[i].id == id && vm->verb[i].save_id == save_id)
            return &vm->verb[i];
    return NULL;
}

scvm_verb_t* scvm_new_verb(scvm_t* vm, unsigned id) {
    scvm_verb_t* vrb = scvm_get_verb(vm,id,0);
    if(!vrb) {
        unsigned i;
        for(i = 0 ; i < vm->num_verb ; i++)
            if(!vm->verb[i].id) {
                vrb = &vm->verb[i];
                break;
            }
        if(!vrb) return NULL;
    }
    return scvm_verb_init(vrb,id,
                          vm->current_charset ? vm->current_charset->id : 0);
}

void scvm_kill_verb(scvm_t* vm, unsigned id) {
    scvm_verb_t* vrb = scvm_get_verb(vm,id,0);
    if(!vrb) return;
    if(vrb->name) free(vrb->name);
    if(vrb->img.data) free(vrb->img.data);
    memset(vrb,0,sizeof(*vrb));
}

int scvm_set_verb_image(scvm_t* vm, unsigned id,
                        unsigned room_id, unsigned obj_id) {
    scvm_verb_t* vrb;
    scvm_object_t* obj;
    if(!(vrb = scvm_get_verb(vm,id,0)) ||
       !(obj = scvm_load_object(vm,room_id,obj_id, SCVM_OBJ_OBIM)))
        return SCVM_ERR_BAD_RESOURCE;

    if(!obj->image[1].data)
        return SCVM_ERR_BAD_RESOURCE;

    vrb->width = obj->width;
    vrb->height = obj->height;
    vrb->img.data = realloc(vrb->img.data,obj->width*obj->height);
    memcpy(vrb->img.data,obj->image[1].data,obj->width*obj->height);
    vrb->img.have_trans = obj->image[1].have_trans;
    vrb->flags |= SCVM_VERB_HAS_IMG;

    return 0;
}

scvm_image_t* scvm_get_verb_image(scvm_t* vm, scvm_verb_t* vrb) {
    scvm_string_dc_t size_dc, draw_dc;;
    unsigned txt_w = 0, txt_h = 0;
    int vrb_x = vrb->x, vrb_y = vrb->y;

    if(vrb->flags & SCVM_VERB_HAS_IMG)
        return vrb->img.data ? &vrb->img : NULL;

    if(!vrb->name) return NULL;

    if(scvm_init_string_dc(vm,&size_dc,vrb->charset))
        return NULL;
    scvm_string_dc_copy(&draw_dc,&size_dc);

    if(scvm_string_get_size(vm,&size_dc,vrb->name,&txt_w,&txt_h) < 0 ||
       !txt_w || !txt_h)
        return NULL;

    if(vrb->flags & SCVM_VERB_CENTER)
        vrb_x -= vrb->width/2;

    if(vm->var->mouse_x >= vrb_x && vm->var->mouse_x < vrb_x+vrb->width &&
       vm->var->mouse_y >= vrb_y && vm->var->mouse_y < vrb_y+vrb->height)
        draw_dc.pal[0] = vrb->hi_color;
    else if(vrb->mode == SCVM_VERB_DIM)
        draw_dc.pal[0] = vrb->dim_color;
    else
        draw_dc.pal[0] = vrb->color;

    if(txt_w != vrb->width || txt_h != vrb->height) {
        vrb->width = txt_w;
        vrb->height = txt_h;
        vrb->img.data = realloc(vrb->img.data,vrb->width*vrb->height);
    }

    memset(vrb->img.data,vrb->back_color,vrb->width*vrb->height);
    vrb->img.have_trans = !vrb->back_color;
    scvm_string_draw(vm,&draw_dc,vrb->name,vrb->img.data,vrb->width,
                     0,0,vrb->width,vrb->height);
    return &vrb->img;
}
