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
    return scvm_verb_init(vrb,id,0 /* vm->cur_charset */);
}

void scvm_kill_verb(scvm_t* vm, unsigned id) {
    scvm_verb_t* vrb = scvm_get_verb(vm,id,0);
    if(!vrb) return;
    if(vrb->name) free(vrb->name);
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

    return 0;
}
