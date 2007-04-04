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
 * @file scvm_res.c
 * @ingroup scvm
 * @brief SCVM ressources management
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
#include "scc_cost.h"
#include "scc_char.h"
#include "scc_codec.h"

#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

scc_fd_t* scvm_open_file(scvm_t* vm,unsigned num) {
  if(num >= vm->num_file) {
    scc_log(LOG_ERR,"Invalid file number %d\n",num);
    return NULL;
  }
  if(!vm->file[num]) {
    int l = strlen(vm->path) + strlen(vm->basename) + 32;
    char name[l+1];
    sprintf(name,"%s/%s.%03d",vm->path,vm->basename,num);
    vm->file[num] = new_scc_fd(name,O_RDONLY,vm->file_key);
    if(!vm->file[num])
      scc_log(LOG_ERR,"Failed to open %s: %s\n",name,strerror(errno));
  }
  return vm->file[num];
}

void scvm_close_file(scvm_t* vm,unsigned num) {
  if(num >= vm->num_file) {
    scc_log(LOG_ERR,"Invalid file number %d\n",num);
    return;
  }
  if(vm->file[num]) {
    scc_fd_close(vm->file[num]);
    vm->file[num] = NULL;
  }
}


void scvm_res_init(scvm_res_type_t* res, char* name, unsigned num,
                   scvm_res_load_f load, scvm_res_nuke_f nuke) {
  res->name = strdup(name);
  res->load = load;
  res->nuke = nuke;
  res->num = num;
  if(num > 0) res->idx = calloc(num,sizeof(scvm_res_t));
}

int scvm_res_load_index(scvm_t* vm, scvm_res_type_t* res,scc_fd_t* fd,
                        uint32_t ref_type) {
  int i,num;
  uint32_t type, len;
  
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != ref_type || len < 8+2 ||
     (num = scc_fd_r16le(fd))*5+2+8 != len ||
     num != res->num) {
    scc_log(LOG_ERR,"Invalid index file, bad %c%c%c%c block.\n",UNMKID(ref_type));
    return 0;
  }
  
  for(i = 0 ; i < res->num ; i++) {
    res->idx[i].room = scc_fd_r8(fd);
    if(res->idx[i].room >= vm->res[SCVM_RES_ROOM].num) {
      scc_log(LOG_WARN,"Invalid room number in index: %d.\n",res->idx[i].room);
      res->idx[i].room = 0;
      continue;
    }
    res->idx[i].file = vm->res[SCVM_RES_ROOM].idx[res->idx[i].room].file;
  }
  for(i = 0 ; i < res->num ; i++)
    res->idx[i].offset = scc_fd_r32le(fd) + 
    vm->res[SCVM_RES_ROOM].idx[res->idx[i].room].offset;
  return 1;
}

int scvm_lock_res(scvm_t* vm, unsigned type, unsigned num) {
  // check type and num
  if(type >= SCVM_RES_MAX) {
    scc_log(LOG_ERR,"Invalid resource type: %d\n",type);
    return 0;
  }
  if(num >= vm->res[type].num) {
    scc_log(LOG_ERR,"Invalid %s number: %d\n",vm->res[type].name,num);
    return 0;
  }
  vm->res[type].idx[num].flags |= SCVM_RES_LOCKED;
  return 1;
}

int scvm_unlock_res(scvm_t* vm, unsigned type, unsigned num) {
  // check type and num
  if(type >= SCVM_RES_MAX) {
    scc_log(LOG_ERR,"Invalid resource type: %d\n",type);
    return 0;
  }
  if(num >= vm->res[type].num) {
    scc_log(LOG_ERR,"Invalid %s number: %d\n",vm->res[type].name,num);
    return 0;
  }
  vm->res[type].idx[num].flags &= ~SCVM_RES_LOCKED;
  return 1;
}


void* scvm_load_res(scvm_t* vm, unsigned type, unsigned num) {
  // check type and num
  if(type >= SCVM_RES_MAX) {
    scc_log(LOG_ERR,"Invalid resource type: %d\n",type);
    return NULL;
  }
  if(num >= vm->res[type].num) {
    scc_log(LOG_ERR,"Invalid %s number: %d\n",vm->res[type].name,num);
    return NULL;
  }
  if(!vm->res[type].load && !vm->res[type].idx[num].data) {
    scc_log(LOG_ERR,"The %s loader is missing. Implement it!\n",
            vm->res[type].name);
    return NULL;
  }
  // load if needed
  if(!vm->res[type].idx[num].data) {
    scc_fd_t* fd = scvm_open_file(vm,vm->res[type].idx[num].file);
    unsigned offset = vm->res[type].idx[num].offset;
    if(!fd) return NULL;
    if(scc_fd_seek(fd,offset,SEEK_SET) != offset) {
      scc_log(LOG_ERR,"Failed to seek to %d in %s.\n",offset,fd->filename);
      scvm_close_file(vm,vm->res[type].idx[num].file);
      return NULL;
    }
    vm->res[type].idx[num].data = vm->res[type].load(vm,fd,num);
    if(!vm->res[type].idx[num].data)
      scc_log(LOG_ERR,"Failed to load %s %d\n",vm->res[type].name,num);
  }
  return vm->res[type].idx[num].data;
}

void* scvm_load_script(scvm_t* vm,scc_fd_t* fd, unsigned num) {
  uint32_t type,len;
  scvm_script_t* scrp;
  
  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);
  if(type != MKID('S','C','R','P') || len < 8) {
    scc_log(LOG_ERR,"Bad script block %d: %c%c%c%c %d\n",num,
            UNMKID(type),len);
    return NULL;
  }
  scrp = malloc(sizeof(scvm_script_t)+len-8);
  scrp->id = num;
  scrp->size = len-8;
  if(scrp->size > 0) {
    int total = 0, r;
    while(total < scrp->size) {
      r = scc_fd_read(fd,scrp->code+total,scrp->size-total);
      if(r < 0) {
        scc_log(LOG_ERR,"Error loading script %d: %s\n",num,strerror(errno));
        free(scrp);
        return NULL;
      }
      total += r;
    }
  }
  return scrp;
}

int scvm_load_image(unsigned width, unsigned height, unsigned num_zplane,
                    scvm_image_t* img,scc_fd_t* fd) {
  int i;
  uint32_t type = scc_fd_r32(fd);
  uint32_t size = scc_fd_r32be(fd);
  if(type != MKID('S','M','A','P') ||
     size < 8+(width/8)*4) return 0;
  else {
    uint8_t smap[size-8];
    if(scc_fd_read(fd,smap,size-8) != size-8)
      return 0;
    img->data = calloc(1,width*height);
    if(!scc_decode_image(img->data,width,
                         width,height,
                         smap,size-8,-1))
      return 0;
  }
  if(!num_zplane) return 1;
  img->zplane = calloc(num_zplane+1,sizeof(uint8_t*));
  for(i = 1 ; i <= num_zplane ; i++) {
    char buf[8];
    sprintf(buf,"%02x",i);
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
    if(type != MKID('Z','P',buf[0],buf[1]) ||
       size < 8+(width/8)*4) return 0;
    else {
      uint8_t zmap[size-8];
      if(scc_fd_read(fd,zmap,size-8) != size-8)
        return 0;
      img->zplane[i] = calloc(1,width/8*height);
      if(!scc_decode_zbuf(img->zplane[i],width/8,
                          width,height,
                          zmap,size-8,0))
        return 0;
    }
  }
  return 1;
}

scvm_object_t* scvm_load_obim(scvm_t* vm, scvm_room_t* room, scc_fd_t* fd) {
  uint32_t type = scc_fd_r32(fd);
  uint32_t size = scc_fd_r32be(fd);
  scvm_object_t* obj;
  unsigned id;
  int i;
  if(type != MKID('I','M','H','D') ||
     size < 8+20) return NULL;
  
  id = scc_fd_r16le(fd);
  if(id >= vm->num_object) return NULL;
  if(vm->res[SCVM_RES_OBJECT].idx[id].data)
    obj = vm->res[SCVM_RES_OBJECT].idx[id].data;
  else {
    obj = calloc(1,sizeof(scvm_object_t));
    obj->id = id;
    obj->pdata = &vm->object_pdata[id];
    vm->res[SCVM_RES_OBJECT].idx[id].data = obj;
  }
  // already loaded
  if(obj->loaded & SCVM_OBJ_OBIM) return obj;
  obj->loaded |= SCVM_OBJ_OBIM;
  
  obj->num_image = scc_fd_r16le(fd);
  obj->num_zplane = scc_fd_r16le(fd);
  scc_fd_r16le(fd); // unknown
  obj->x = scc_fd_r16le(fd);
  obj->y = scc_fd_r16le(fd);
  obj->width = scc_fd_r16le(fd);
  obj->height = scc_fd_r16le(fd);
  obj->num_hotspot = scc_fd_r16le(fd);
  if(obj->num_hotspot > 0) {
    obj->hotspot = malloc(2*sizeof(int)*obj->num_hotspot);
    for(i = 0 ; i < obj->num_hotspot ; i++) {
      obj->hotspot[2*i] = scc_fd_r16le(fd);
      obj->hotspot[2*i+1] = scc_fd_r16le(fd);
    }
  }
  
  // load the images
  if(!obj->num_image) return obj;
  
  obj->image = calloc(sizeof(scvm_image_t),obj->num_image+1);
  for(i = 1 ; i <= obj->num_image ; i++) {
    type = scc_fd_r32le(fd);
    size = scc_fd_r32be(fd);
    if((type & 0xFFFF) != ('I'|'M'<<8) ||
       size < 8) return 0;
    if(!scvm_load_image(obj->width,obj->height,obj->num_zplane,
                        &obj->image[i],fd)) return NULL;
  }
  
  return obj;
}

scvm_object_t* scvm_load_obcd(scvm_t* vm, scvm_room_t* room, scc_fd_t* fd) {
  uint32_t type = scc_fd_r32(fd);
  uint32_t size = scc_fd_r32be(fd);
  scvm_object_t* obj;
  unsigned id;
  
  if(type != MKID('C','D','H','D') ||
     size < 8+17) return NULL;
  
  id = scc_fd_r16le(fd);
  if(id >= vm->num_object) return NULL;
  if(vm->res[SCVM_RES_OBJECT].idx[id].data)
    obj = vm->res[SCVM_RES_OBJECT].idx[id].data;
  else {
    obj = calloc(1,sizeof(scvm_object_t));
    obj->id = id;
    obj->pdata = &vm->object_pdata[id];
    vm->res[SCVM_RES_OBJECT].idx[id].data = obj;
  }
  // already loaded
  if(obj->loaded & SCVM_OBJ_OBCD) return obj;
  obj->loaded |= SCVM_OBJ_OBCD;
  
  if(!(obj->loaded & SCVM_OBJ_OBIM)) {
    obj->x = scc_fd_r16le(fd);
    obj->y = scc_fd_r16le(fd);
    obj->width = scc_fd_r16le(fd);
    obj->height = scc_fd_r16le(fd);
  } else
    scc_fd_seek(fd,8,SEEK_CUR);
  obj->flags = scc_fd_r8(fd);
  // hack, the real pointer get resolved when all
  // object are loaded.
  obj->parent = (scvm_object_t*)((intptr_t)scc_fd_r8(fd));
  scc_fd_r32(fd); // unknown
  obj->actor_dir = scc_fd_r8(fd);

  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);

  if(type == MKID('V','E','R','B')) {
    unsigned entries[0x200];
    unsigned num_entries = 0,verb;
    if(size < 8+1) return NULL;
    size -= 8;
    while(1) {
      verb = entries[2*num_entries] = scc_fd_r8(fd);
      size--;
      if(!verb) break;
      entries[2*num_entries+1] = scc_fd_r16le(fd);
      size -= 2;
      num_entries++;
    }
    if(num_entries) {
      obj->verb_entries = malloc(2*num_entries*sizeof(unsigned));
      memcpy(obj->verb_entries,entries,2*num_entries*sizeof(unsigned));
    }
    if(size > 0) {
      obj->script = malloc(sizeof(scvm_script_t)+size);
      if(scc_fd_read(fd,obj->script->code,size) != size)
        return 0;
      obj->script->id = obj->id | 0x10000;
      obj->script->size = size;
    }
    
    type = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
  }
  
  if(type == MKID('O','B','N','A') &&
     !obj->pdata->name) {
    if(size < 8+1) return 0;
    size -= 8;
    obj->pdata->name = malloc(size);
    if(scc_fd_read(fd,obj->pdata->name,size));
    // it should be 0 terminated but make sure it is
    obj->pdata->name[size-1] = 0;
  }  
  
  return obj;
}

void* scvm_load_room(scvm_t* vm,scc_fd_t* fd, unsigned num) {
  uint32_t type,size,block_size,sub_block_size;
  unsigned len = 8,num_obim = 0, num_obcd = 0, num_lscr = 0;
  int i;
  scvm_room_t* room;
  off_t next_block;
  scvm_object_t *objlist[vm->num_local_object],*obj;

  memset(objlist,0,sizeof(scvm_object_t*)*vm->num_local_object);
  
  type = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  if(type != MKID('R','O','O','M') || size < 16) {
    scc_log(LOG_ERR,"Bad ROOM block %d: %c%c%c%c %d\n",num,
            UNMKID(type),size);
    return NULL;
  }
  room = calloc(1,sizeof(scvm_room_t));
  room->id = num;
  while(len < size) {
    type = scc_fd_r32(fd);
    block_size = scc_fd_r32be(fd);
    next_block = scc_fd_pos(fd)-8+block_size;
    len += block_size;
    switch(type) {
    case MKID('R','M','H','D'):
      if(block_size != 6+8) goto bad_block;
      room->width = scc_fd_r16le(fd);
      room->height = scc_fd_r16le(fd);
      room->num_object = scc_fd_r16le(fd);
      break;
      
    case MKID('C','Y','C','L'):
      if(block_size < 1+8) goto bad_block;
      room->num_cycle = 17;
      room->cycle = calloc(room->num_cycle,sizeof(scvm_cycle_t));
      while(1) {
        unsigned freq,id = scc_fd_r8(fd);
        if(!id) break;
        if(id >= room->num_cycle) goto bad_block;
        room->cycle[id].id = id;
        scc_fd_r16(fd); // unknown
        freq = scc_fd_r16be(fd);
        room->cycle[id].delay = freq ? 0x4000/freq : 0;
        room->cycle[id].flags = scc_fd_r16be(fd);
        room->cycle[id].start = scc_fd_r8(fd);
        room->cycle[id].end = scc_fd_r8(fd);
      }
      break;
      
    case MKID('T','R','N','S'):
      if(block_size != 2+8) goto bad_block;
      room->trans = scc_fd_r16le(fd);
      break;

    case MKID('P','A','L','S'):
      if(block_size < 16) goto bad_block;
      type = scc_fd_r32(fd);
      sub_block_size = scc_fd_r32be(fd);
      if(type != MKID('W','R','A','P') ||
         sub_block_size != block_size-8) goto bad_block;
      type = scc_fd_r32(fd);
      sub_block_size = scc_fd_r32be(fd);
      if(type != MKID('O','F','F','S') || sub_block_size<8) goto bad_block;
      else {
        off_t base_ptr = scc_fd_pos(fd)-8;
        int npal = (sub_block_size-8)/4;
        off_t offset[npal];
        for(i = 0 ; i < npal ; i++)
          offset[i] = base_ptr + scc_fd_r32le(fd);
        room->num_palette = npal;
        room->palette = malloc(npal*sizeof(scvm_palette_t));
        for(i = 0 ; i < npal ; i++) {
          scc_fd_seek(fd,offset[i],SEEK_SET);
          type = scc_fd_r32(fd);
          sub_block_size = scc_fd_r32be(fd);
          if(type != MKID('A','P','A','L') ||
             sub_block_size-8!=sizeof(scvm_palette_t)) goto bad_block;
          if(scc_fd_read(fd,room->palette[i],sizeof(scvm_palette_t)) != 
             sizeof(scvm_palette_t)) goto bad_block;
        }
      }
      break;
      
    case MKID('R','M','I','M'):
      if(block_size < 8+8+2+8+8+(room->width/8)*4) goto bad_block;
      type = scc_fd_r32(fd);
      sub_block_size = scc_fd_r32be(fd);
      if(type != MKID('R','M','I','H') ||
         sub_block_size != 8+2) goto bad_block;
      room->num_zplane = scc_fd_r16le(fd);
      type = scc_fd_r32(fd);
      sub_block_size = scc_fd_r32be(fd);
      if(type != MKID('I','M','0','0') ||
         sub_block_size < 8+8) goto bad_block;
      if(!scvm_load_image(room->width,room->height,room->num_zplane,
                          &room->image,fd))
        goto bad_block;
      break;
      
    case MKID('O','B','I','M'):
      if(num_obim >= vm->num_local_object) {
        scc_log(LOG_ERR,"Too many objects in room.\n");
        goto bad_block;
      }
      if(block_size < 8+8+20 ||
         !(obj = scvm_load_obim(vm,room,fd))) goto bad_block;
      if(!objlist[num_obim])
        objlist[num_obim] = obj;
      else if(objlist[num_obim] != obj) {
        scc_log(LOG_ERR,"OBIM and OBCD are badly ordered.\n");
        goto bad_block;
      }
      num_obim++;
      break;
      
    case MKID('O','B','C','D'):
      if(num_obcd >= vm->num_local_object) {
        scc_log(LOG_ERR,"Too many objects in room.\n");
        goto bad_block;
      }
      if(block_size < 8+8+17 ||
         !(obj = scvm_load_obcd(vm,room,fd))) goto bad_block;
      if(!objlist[num_obcd])
        objlist[num_obcd] = obj;
      else if(objlist[num_obcd] != obj) {
        scc_log(LOG_ERR,"OBIM and OBCD are badly ordered.\n");
        goto bad_block;
      }
      num_obcd++;
      break;
      
    case MKID('E','X','C','D'):
      if(block_size <= 8) {
        scc_log(LOG_WARN,"Ignoring empty EXCD.\n");
        break;
      }
      block_size -= 8;
      room->exit = malloc(sizeof(scvm_script_t)+block_size);
      room->exit->id = 0x1ECD0000;
      room->exit->size = block_size;
      if(scc_fd_read(fd,room->exit->code,block_size) != block_size)
        goto bad_block;
      break;

    case MKID('E','N','C','D'):
      if(block_size <= 8) {
        scc_log(LOG_WARN,"Ignoring empty ENCD.\n");
        break;
      }
      block_size -= 8;
      room->entry = malloc(sizeof(scvm_script_t)+block_size);
      room->entry->id = 0x0ECD0000;
      room->entry->size = block_size;
      if(scc_fd_read(fd,room->entry->code,block_size) != block_size)
        goto bad_block;
      break;

    case MKID('N','L','S','C'):
      if(block_size != 8+2)
        goto bad_block;
      room->num_script = scc_fd_r16le(fd);
      if(room->num_script)
        room->script = calloc(room->num_script,sizeof(scvm_script_t*));
      break;
      
    case MKID('L','S','C','R'):
      if(num_lscr >= room->num_script) {
        scc_log(LOG_ERR,"Too many local scripts in room.\n");
        goto bad_block;
      }
      if(block_size <= 8+1) {
        scc_log(LOG_WARN,"Ignoring empty LSCR.\n");
        break;
      }
      block_size -= 8+1;
      i = scc_fd_r8(fd);
      if(i < 200 || i-200 >= room->num_script)
        goto bad_block;
      i -= 200;
      room->script[i] = malloc(sizeof(scvm_script_t)+block_size);
      room->script[i]->id = i+200;
      room->script[i]->size = block_size;
      if(scc_fd_read(fd,room->script[i]->code,block_size) != block_size)
        goto bad_block;
      num_lscr++;
      break;
      
    default:
      scc_log(LOG_WARN,"Unhandled room block: %c%c%c%c %d\n",
              UNMKID(type),block_size);
    }
    scc_fd_seek(fd,next_block,SEEK_SET);
  }
  
  if(num_lscr < room->num_script)
    scc_log(LOG_WARN,"Room %d is missing some local scripts?\n",num);

  // Resolve the object parent
  for(i=0 ; i < num_obim ; i++) {
    uint8_t num = (uint8_t)((uintptr_t)objlist[i]->parent);
    if(!num) continue;
    if(num > num_obim) {
      scc_log(LOG_WARN,"Object %d has an invalid parent.\n",obj->id);
      continue;
    }
    objlist[i]->parent = objlist[num-1];
  }
  if(num_obcd > num_obim) num_obim = num_obcd;
  room->num_object = num_obim;
  room->object = malloc(num_obim*sizeof(scvm_object_t*));
  for(i=0 ; i < num_obim ; i++)
    room->object[i] = objlist[i];
  
  return room;
  
bad_block:
  scc_log(LOG_ERR,"Bad ROOM subblock %d: %c%c%c%c %d\n",num,
          UNMKID(type),block_size);
  free(room);
  return NULL;
}

void* scvm_load_costume(scvm_t* vm,scc_fd_t* fd, unsigned num) {
  uint32_t size,fmt;
  fmt = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  
  if(fmt != MKID('C','O','S','T') || size < 8+32 ) { // put the right size here
    scc_log(LOG_ERR,"Bad COST block %d: %c%c%c%c %d\n",num,
            UNMKID(fmt),size);
    return NULL;
  }
  return scc_parse_cost(fd,size-8);
}


void* scvm_load_charset(scvm_t* vm,scc_fd_t* fd, unsigned num) {
  uint32_t size,fmt;
  
  // read the scumm block header
  fmt = scc_fd_r32(fd);
  size = scc_fd_r32be(fd);
  
  if(fmt != MKID('C','H','A','R') || size < 8+25 ) {
    scc_log(LOG_ERR,"Bad CHAR block %d: %c%c%c%c %d\n",num,
            UNMKID(fmt),size);
    return NULL;
  }
  return scc_parse_charmap(fd,size-8);
}
