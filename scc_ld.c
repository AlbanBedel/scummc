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

/// @defgroup sld Linker
/**
 * @file scc_ld.c
 * @ingroup sld
 * @brief ScummC linker
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

#include "scc_parse.h"
#include "scc_ns.h"
#include "scc_code.h"
#include "scc_param.h"
#include "sld_help.h"



static scc_ns_t* scc_ns = NULL;
static uint8_t* obj_owner;
static uint8_t* obj_state;
static uint8_t* obj_room;
static uint32_t* obj_class;

typedef struct scc_ld_block_st scc_ld_block_t;
struct scc_ld_block_st {
  scc_ld_block_t* next;

  uint32_t type;
  /// Can be copied as is, it won't need any patching
  int asis;
  /// Ressource address, used for scripts
  int addr;


  int data_len;
  uint8_t data[0];
};

typedef struct scc_ld_room_st scc_ld_room_t;
struct scc_ld_room_st {
  scc_ld_room_t* next;

  /// Targeted VM version
  int vm_version;

  /// Room ns used to resolve the rid during code patching
  scc_ns_t* ns;
  /// Room symbol from the local ns
  scc_symbol_t* sym;
  
  scc_ld_block_t* room;
  scc_ld_block_t* scr;
  scc_ld_block_t* snd;
  scc_ld_block_t* cost;
  scc_ld_block_t* chset;
};

typedef struct scc_ld_voice_st scc_ld_voice_t;
struct scc_ld_voice_st {
  scc_ld_voice_t* next;

  scc_symbol_t* sym;
  unsigned offset;
  unsigned vctl_size;
  
  int data_len;
  char data[0];
};

static scc_ld_room_t* scc_room = NULL;
static scc_ld_voice_t* scc_voice = NULL, *scc_last_voice = NULL;
static unsigned scc_voice_off = 8;

static int scc_fd_get_block(scc_fd_t* fd, int max_len,uint32_t* type) {
  int len;

  if(max_len < 8) return -1;

  type[0] = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);

  if(len > max_len) return -1;

  return len-8;
}

static scc_ld_block_t* scc_fd_read_block(scc_fd_t* fd,uint32_t type, int len) {
  scc_ld_block_t* blk = malloc(sizeof(scc_ld_block_t) + len);

  blk->next = NULL;
  blk->type = type;
  blk->data_len = len;
  blk->asis = 0;
  if(scc_fd_read(fd,blk->data,len) != len) {
    scc_log(LOG_ERR,"Error while reading block.\n");
    return NULL;
  }

  return blk;
}

static int scc_ld_write_block(scc_ld_block_t* blk,scc_fd_t* fd) {
  
  scc_fd_w32(fd,blk->type);
  scc_fd_w32be(fd,blk->data_len + 8);

  if(scc_fd_write(fd,blk->data,blk->data_len) != blk->data_len) {
    scc_log(LOG_ERR,"Error while writing block.\n");
    return 0;
  }

  return 1;
}

static int scc_ld_write_block_list(scc_ld_block_t* blk,scc_fd_t* fd) {

  while(blk) {
    if(!blk->asis) {
      scc_log(LOG_WARN,"Warning: skipping non-patched block %c%c%c%c.\n",
              UNMKID(blk->type));
      blk = blk->next;
      continue;
    }
    if(!scc_ld_write_block(blk,fd)) return 0;
    blk = blk->next;
  }

  return 1;
}

void scc_ld_block_list_free(scc_ld_block_t* blk) {
  scc_ld_block_t* n;

  while(blk) {
    n = blk->next;
    free(blk);
    blk = n;
  }

}

void scc_ld_room_free(scc_ld_room_t* room) {
  
  scc_ns_free(room->ns);
  scc_ld_block_list_free(room->room);
  scc_ld_block_list_free(room->scr);
  scc_ld_block_list_free(room->snd);
  scc_ld_block_list_free(room->cost);

  free(room);
}

scc_ld_voice_t* scc_ld_get_voice(scc_symbol_t* sym) {
  scc_ld_voice_t* v;
  for(v = scc_voice ; v ; v = v->next)
    if(sym == v->sym) return v;
  return NULL;
}

int scc_ld_parse_sym_block(scc_fd_t* fd, scc_ld_room_t* room, int len) {
  int pos = 0;
  char status;
  int nlen;
  char name[256];
  int type,subtype;
  int rid,addr;
  scc_symbol_t* sym;

  while(1) {

    if(pos + 2 >= len) {
      scc_log(LOG_ERR,"Invalid symbol definition.\n");
      return 0;
    }

    status = scc_fd_r8(fd); pos++;
    if(status != 'I' && status != 'E') {
      scc_log(LOG_ERR,"Invalid symbol status: %c\n",status);
      return 0;
    }
    nlen = scc_fd_r8(fd); pos++;
    // check that all the data is there
    if(pos + nlen + 6 > len) {
      scc_log(LOG_ERR,"Invalid symbol definition.\n");
      return 0;
    }
    if(scc_fd_read(fd,name,nlen) != nlen) {
      scc_log(LOG_ERR,"Error while reading symbol name.\n");
      return 0;
    }
    pos += nlen;
    name[nlen] = '\0';

    type = scc_fd_r8(fd); pos++;
    subtype = scc_fd_r8(fd); pos++;

    rid = scc_fd_r16le(fd); pos += 2;
    addr = scc_fd_r16le(fd); pos += 2;
    if(addr == 0xFFFF) addr = -1;

    // add it to the global ns
    sym = scc_ns_add_sym(scc_ns,name,type,subtype,addr,status);
    if(!sym) return 0;
    // add it to the room ns
    sym = scc_ns_add_sym(room->ns,name,type,subtype,addr,status);
    if(!sym) return 0;
    // set its rid
    sym->rid = rid;
    // the first exported room should be ourself.
    if(type == SCC_RES_ROOM && status == 'E') {
      if(room->sym) 
	scc_log(LOG_WARN,"Warning: multiple rooms are exported in the same STAB!!!!\n");
      else
	room->sym = sym;
    }
    if(pos >= len) break;
  }

  return 1;
}

// parse all sym. decl
int scc_ld_parse_stab(scc_fd_t* fd, scc_ld_room_t* room, int max_len) {
  uint32_t type;
  int len,rid;
  int pos = 0;
  scc_symbol_t *sym,*gsym;

  len = scc_fd_get_block(fd,max_len,&type); pos += 8;
  if(len < 0 || type != MKID('G','S','Y','M')) {
    scc_log(LOG_ERR,"Invalid STAB block.\n");
    return 0;
  }
  
  // parse gsyms
  if(!scc_ld_parse_sym_block(fd,room,len)) return 0;
  pos += len;
  
  while(pos < max_len) {
    len = scc_fd_get_block(fd,max_len-pos,&type); pos += 8;
    if(len < 2 || type != MKID('R','S','Y','M')) {
      scc_log(LOG_ERR,"Invalid STAB block.\n");
      return 0;
    }
    // find the room
    rid = scc_fd_r16le(fd); pos += 2;
    sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_ROOM,rid);
    if(!sym) {
      scc_log(LOG_ERR,"Invalid RSYM block.\n");
      return 0;
    }
    gsym = scc_ns_get_sym(scc_ns,NULL,sym->sym);
    if(!gsym) {
      scc_log(LOG_ERR,"NS error!!!!\n");
      return 0;
    }
    // push the room in the ns
    scc_ns_push(room->ns,sym);
    scc_ns_push(scc_ns,gsym);
    scc_log(LOG_DBG,"Loading symbols for room %s:\n",sym->sym);
    // parse the syms
    if(!scc_ld_parse_sym_block(fd,room,len-2)) return 0;
    pos += len-2;
    // pop ns
    scc_ns_pop(room->ns);
    scc_ns_pop(scc_ns);
  }

  return 1;
}

int scc_ld_parse_voice(scc_fd_t* fd, scc_ld_room_t* room, int len) {
  int rid;
  scc_symbol_t* sym;
  scc_ld_voice_t* v;

  if(len < 10) {
    scc_log(LOG_ERR,"Too small voic block.\n");
    return 0;
  }

  rid = scc_fd_r16le(fd);
  sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_VOICE,rid);
  if(!sym) {
    scc_log(LOG_ERR,"Invalid voic block.\n");
    return 0;
  }

  len -= 2;
  
  v = calloc(1,sizeof(scc_ld_voice_t) + len);
  v->data_len = len;
  if(scc_fd_read(fd,v->data,len) != len) {
    scc_log(LOG_ERR,"Error while reading voic block.\n");
    return 0;
  }

  v->sym = sym;
  v->offset = scc_voice_off;
  scc_voice_off += len;
  v->vctl_size = SCC_GET_32BE(v->data,4);

  SCC_LIST_ADD(scc_voice,scc_last_voice,v);

  return 1;
}

scc_ld_room_t* scc_ld_parse_room(scc_fd_t* fd,int max_len) {
  int addr,len,pos = 0;
  uint32_t type;
  scc_ld_room_t* room;
  scc_ld_block_t* blk;
  scc_ld_block_t* room_last = NULL,*scr_last = NULL, *cost_last = NULL;
  scc_ld_block_t* chset_last = NULL, *snd_last = NULL;
  int vm_version = scc_fd_r8(fd); pos += 1;

  if(!scc_ns) {
    scc_target_t* target = scc_get_target(vm_version);
    if(!target) {
      scc_log(LOG_ERR,"Unsuported VM version: %d\n",vm_version);
      return NULL;
    }
    scc_ns = scc_ns_new(target);
  } else if(vm_version != scc_ns->target->version) {
      scc_log(LOG_ERR,"Bad VM version: %d (started with %d)\n",
              vm_version,scc_ns->target->version);
      return NULL;
  }

  room = calloc(1,sizeof(scc_ld_room_t));
  room->vm_version = vm_version;
  room->ns = scc_ns_new(scc_ns->target);

  while(pos < max_len) {
    len = scc_fd_get_block(fd,max_len-pos,&type); pos += 8;
    if(len < 0) {
      scc_log(LOG_ERR,"Invalid room block.\n");
      scc_ld_room_free(room);
      return NULL;
    }
    scc_log(LOG_DBG,"Parsing %c%c%c%c block.\n",UNMKID(type));

    switch(type) {
    case MKID('S','T','A','B'):
      if(!scc_ld_parse_stab(fd,room,len)) {
	scc_ld_room_free(room);
	return NULL;
      }
      break;

    case MKID('o','b','i','m'):
    case MKID('o','b','c','d'):
    case MKID('e','x','c','d'):
    case MKID('e','n','c','d'):
    case MKID('l','s','c','r'):
      blk = scc_fd_read_block(fd,type,len);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      SCC_LIST_ADD(room->room,room_last,blk);
      break;

    case MKID('s','c','r','p'):
      blk = scc_fd_read_block(fd,type,len);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      SCC_LIST_ADD(room->scr,scr_last,blk);
      break;

    case MKID('c','o','s','t'):
    case MKID('a','k','o','s'):
      addr = scc_fd_r16le(fd);
      blk = scc_fd_read_block(fd,type,len-2);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      blk->addr = addr;
      blk->asis = 1;
      SCC_LIST_ADD(room->cost,cost_last,blk);
      break;

    case MKID('c','h','a','r'):
      addr = scc_fd_r16le(fd);
      blk = scc_fd_read_block(fd,type,len-2);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      blk->addr = addr;
      blk->asis = 1;
      SCC_LIST_ADD(room->chset,chset_last,blk);
      break;

    case MKID('s','o','u','n'):
      addr = scc_fd_r16le(fd);
      blk = scc_fd_read_block(fd,type,len-2);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      blk->addr = addr;
      blk->asis = 1;
      SCC_LIST_ADD(room->snd,snd_last,blk);
      break;

    case MKID('v','o','i','c'):
      if(!scc_ld_parse_voice(fd,room,len)) {
        scc_ld_room_free(room);
	return NULL;
      }      
      break;

    default:
      blk = scc_fd_read_block(fd,type,len);
      if(!blk) {
	scc_ld_room_free(room);
	return NULL;
      }
      blk->asis = 1;
      SCC_LIST_ADD(room->room,room_last,blk);
      break;
    }
    pos += len;
  }

  return room;
}


int scc_ld_load_roobj(char* path) {
  scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
  int flen,len,pos = 0;
  uint32_t type;
  scc_ld_room_t* ro;

  if(!fd) {
    scc_log(LOG_ERR,"Failed to open %s.\n",path);
    return 0;
  }

  scc_fd_seek(fd,0,SEEK_END);
  flen = scc_fd_pos(fd);
  scc_fd_seek(fd,0,SEEK_SET);

  while(pos < flen) {
    len = scc_fd_get_block(fd,flen-pos,&type); pos += 8;
    if(type != MKID('r','o','o','m')) {
      scc_log(LOG_ERR,"Got unknown block %c%c%c%c in roobj file.\n",UNMKID(type));
      return 0;
    }
    ro = scc_ld_parse_room(fd,len);
    if(!ro) return 0;
    pos += len;
    ro->next = scc_room;
    scc_room = ro;
  }
  
  return 1;
}

// verify that all none-variable resouce are now exported
int scc_ld_check_ns(scc_ns_t* ns,scc_symbol_t* sym) {
  int r = 1;

  if(!sym) sym = ns->glob_sym;

  while(sym) {
    if((!(sym->type == SCC_RES_VAR ||
          sym->type == SCC_RES_BVAR ||
          sym->type == SCC_RES_VERB ||
          sym->type == SCC_RES_ACTOR ||
          sym->type == SCC_RES_CLASS)) &&
       sym->status == 'I') {
      if(sym->parent)
	scc_log(LOG_ERR,"Found unresolved symbol %s in room %s.\n",
                sym->sym,sym->parent->sym);
      else
	scc_log(LOG_ERR,"Found unresolved symbol: %s\n",sym->sym);
      r = 0;
    }
      
    if(sym->type == SCC_RES_ROOM && sym->childs) {
      if(!scc_ld_check_ns(ns,sym->childs)) r = 0;
    }
    sym = sym->next;
  }
  
  return r;
}


int scc_ld_find_main(scc_ns_t* ns) {
  scc_symbol_t* r,*s;
  scc_symbol_t* m = NULL;

  for(r = ns->glob_sym ; r ; r = r->next) {
    if(r->type != SCC_RES_ROOM) continue;
    for(s = r->childs ; s ; s = s->next) {
      if(s->type != SCC_RES_SCR) continue;
      // Found an explicit main
      if(s->addr == 1)
	return 1;
      // Found a script named main
      if(!strcmp(s->sym,"main")) {
	if(m)
	  scc_log(LOG_WARN,"Warning: multiple rooms have a main script!!!!\n");
	else
	  m = s;
      }
    }
  }

  if(m) {
    if(!scc_ns_set_sym_addr(scc_ns,m,1)) {
      scc_log(LOG_ERR,"Failed to set main script address????\n");
      return 0;
    }
    return 1;
  }
  return 0;
}

static scc_script_t* scc_ld_parse_scob(scc_ld_room_t* room,
				       scc_symbol_t* sym,uint8_t* data,
				       int len) {
  uint32_t type; 
  uint32_t  size;
  int pos = 0;
  scc_sym_fix_t* fix_list = NULL,*fix;
  int sym_type,sym_id,off;
  scc_script_t* scr;
  scc_symbol_t* s;
  uint8_t* scr_data;

  if(len < 8) {
    scc_log(LOG_ERR,"Invalid scob block.\n");
    return NULL;
  }

  type = SCC_GET_32(data,0);
  size = SCC_GET_32BE(data,4);

  // fix list
  if(type == MKID('S','F','I','X')) {
    if((size/8)*8 != size || size > len) {
      scc_log(LOG_ERR,"SFIX block has invalid length.\n");
      return NULL;
    }
    for(pos = 8 ; pos < size ; pos += 8) {
      if(pos + 8 > size) {
	scc_log(LOG_ERR,"Invalid SFIX block.\n");
	return NULL;
      }
      sym_type = data[pos];
      sym_id = SCC_GET_16LE(data,pos+2);
      off = SCC_GET_32LE(data,pos+4);
      s = scc_ns_get_sym_with_id(room->ns,sym_type,sym_id);
      if(!s) {
	scc_log(LOG_ERR,"SFIX entry with invalid id????\n");
	return NULL;
      }
      fix = malloc(sizeof(scc_sym_fix_t));
      fix->sym = s;
      fix->off = off;
      fix->next = fix_list;
      fix_list = fix;
    }

    if(pos+8 > len) {
      scc_log(LOG_ERR,"Invalid scob block????\n");
      return NULL;
    }
    type = SCC_GET_32(data,pos);
    size = SCC_GET_32BE(data,pos+4);
  }

  if(type != MKID('s','c','o','b') || size < 8 || pos+size != len) {
    scc_log(LOG_ERR,"Invalid scob block????\n");
    return NULL;
  }

  if(size == 8) {
    scr = calloc(1,sizeof(scc_script_t));
    scr->sym = sym;
    return scr;
  }

  pos += 8;
  size -= 8;

  scr_data = malloc(size);
  memcpy(scr_data,&data[pos],size);
  for(fix = fix_list ; fix ; fix = fix->next) {
    if(fix->off >= size) {
      scc_log(LOG_ERR,"Got invalid fix offset.\n");
      return NULL;
    }
    if(fix->sym->type == SCC_RES_VOICE) {
      scc_ld_voice_t* v = scc_ld_get_voice(fix->sym);
      if(!v) {
        scc_log(LOG_ERR,"Got invalid voice sym fix???\n");
        return NULL;
      }
      scr_data[fix->off] = v->offset & 0xFF;
      scr_data[fix->off+1] = (v->offset >> 8) & 0xFF;
      scr_data[fix->off+2] = 0xFF;
      scr_data[fix->off+3] = 0x0A;
      scr_data[fix->off+4] = (v->offset >> 16) & 0xFF;
      scr_data[fix->off+5] = (v->offset >> 24) & 0xFF;

      scr_data[fix->off+6] = 0xFF;
      scr_data[fix->off+7] = 0x0A;
      scr_data[fix->off+8] = v->vctl_size & 0xFF;
      scr_data[fix->off+9] = (v->vctl_size >> 8) & 0xFF;
      scr_data[fix->off+10] = 0xFF;
      scr_data[fix->off+11] = 0x0A;
      scr_data[fix->off+12] = (v->vctl_size >> 16) & 0xFF;
      scr_data[fix->off+13] = (v->vctl_size >> 24) & 0xFF;
    } else {
      SCC_SET_16LE(scr_data,fix->off,fix->sym->addr);
    }
  }

  SCC_LIST_FREE(fix_list,fix);

  scr = calloc(1,sizeof(scc_script_t));
  scr->sym = sym;
  scr->code = scr_data;
  scr->code_len = size;
  
  return scr;
}

static scc_ld_block_t* scc_ld_obim_patch(scc_ld_room_t* room,
					 scc_ld_block_t* blk) {
  uint32_t type = SCC_GET_32(blk->data,0);
  uint32_t  size = SCC_GET_32BE(blk->data,4);
  uint16_t id;
  int id_pos = scc_ns->target->version == 7 ? 12 : 8;
  scc_symbol_t* sym;

  if(type != MKID('i','m','h','d') || size < 26) {
    scc_log(LOG_ERR,"Invalid obim block.\n");
    return NULL;
  }

  id = SCC_GET_16LE(blk->data,id_pos);
  sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_OBJ,id);
  if(!sym) {
    scc_log(LOG_ERR,"imhd block contains an invalid id????\n");
    return NULL;
  }
  
  SCC_SET_32(blk->data,0,MKID('I','M','H','D'));
  SCC_SET_16LE(blk->data,id_pos,sym->addr);

  blk->type = MKID('O','B','I','M');
  blk->asis = 1;
  return blk;
}

static scc_ld_block_t* scc_ld_obcd_patch(scc_ld_room_t* room,
					 scc_ld_block_t* blk) {
  uint32_t type = SCC_GET_32(blk->data,0);
  uint32_t  size = SCC_GET_32BE(blk->data,4);
  uint16_t vid,oid,cid,id,addr;
  scc_symbol_t* sym,*osym,*csym;
  int nverb=0,verb_size=0,len,pos = 0,new_size,vbase,vpos,vn,i;
  scc_script_t* scr=NULL,*scr_last=NULL,*new_scr;
  scc_ld_block_t* new;
  int cdhd_size = scc_ns->target->version == 7 ? 8 : 17;

  scc_log(LOG_DBG,"Patching obcd.\n");

  if(type != MKID('c','d','h','d') ||
     size != 8 + 1+2+2*SCC_MAX_CLASS+cdhd_size) {
    scc_log(LOG_ERR,"Invalid cdhd block.\n");
    return NULL;
  }
  // find the object
  vid = SCC_GET_16LE(blk->data,8);
  sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_OBJ,vid);
  if(!sym) {
    scc_log(LOG_ERR,"cdhd block contains an invalid id (0x%x)????\n",vid);
    return NULL;
  }
  addr = sym->addr;
  // initial state
  obj_state[sym->addr] = blk->data[10];
  // owner
  oid = SCC_GET_16LE(blk->data,11);
  if(oid) {
    osym = scc_ns_get_sym_with_id(room->ns,SCC_RES_ACTOR,oid);
    if(!osym) {
      scc_log(LOG_ERR,"cdhd block contains an invalid owner id (0x%x)????\n",oid);
      return NULL;
    }
    obj_owner[sym->addr] = osym->addr;
  } else
    obj_owner[sym->addr] = 0x0F;
  // room
  obj_room[sym->addr] = room->sym->addr;

  for(i = 0 ; i < SCC_MAX_CLASS ; i++) {
    cid = SCC_GET_16LE(blk->data,8+2+1+2+2*i);
    if(!cid) continue;
    csym = scc_ns_get_sym_with_id(room->ns,SCC_RES_CLASS,cid);
    if(!csym) {
      scc_log(LOG_ERR,"cdhd block contains an invalid class id (0x%x)????\n",cid);
      return NULL;
    }
    obj_class[sym->addr] |= (1 << (csym->addr-1));
  }
  
  pos = size;
  while(pos < blk->data_len) {
    type = SCC_GET_32(blk->data,pos);
    len = SCC_GET_32BE(blk->data,pos+4);

    if(type == MKID('O','B','N','A')) {
      break;
    } else if(type != MKID('v','e','r','b')) {
      scc_log(LOG_ERR,"Got unknown block inside obcd block!!!!\n");
      return NULL;
    }
    id = SCC_GET_16LE(blk->data,pos + 8);
    if(id) {
      sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_VERB,id);
      if(!sym) {
        scc_log(LOG_ERR,"verb block contains an invalid id: %d????\n",id);
        return NULL;
      }
    } else
      sym = NULL;
    new_scr = scc_ld_parse_scob(room,sym,&blk->data[pos+10],len-10);
    if(!new_scr) {
      scc_log(LOG_ERR,"Failed to create verb %s.\n",sym->sym);
      return NULL;
    }
    if(verb_size + new_scr->code_len > 0xFFFF) {
      scc_log(LOG_ERR,"verb block has too much code.");
      return NULL;
    }

    SCC_LIST_ADD(scr,scr_last,new_scr);
    nverb++;
    pos += len;
    verb_size += new_scr->code_len;
  } 
  
  if(pos + len != blk->data_len) {
    scc_log(LOG_ERR,"Invalid obcd block: %d >= %d.\n",pos,size);
    return NULL;
  }
  
  // build verb block
  scc_log(LOG_DBG,"Building the verb block: %d.\n",nverb*3+1+verb_size);  
  new_size = 8 + cdhd_size + 8 + nverb*3+1+verb_size + len;

  new = malloc(sizeof(scc_ld_block_t) + new_size);
  new->type = MKID('O','B','C','D');
  new->next = blk->next;
  new->asis = 1;
  new->data_len = new_size;
  
  // make the cdhd header
  SCC_SET_32(new->data,0,MKID('C','D','H','D'));
  if(scc_ns->target->version == 7) {
    SCC_SET_32BE(new->data,4,8+cdhd_size);
    // copy the version
    memcpy(new->data + 8,blk->data + 8 + 2 + 1 + 2 + SCC_MAX_CLASS*2,4);
    // patch the id
    SCC_SET_16LE(new->data,8+4,addr);
    // copy parent and parent state
    memcpy(new->data + 8+4+2,blk->data + 8 + 2 + 1 + 2 + SCC_MAX_CLASS*2+4,2);
  } else {
    SCC_SET_32BE(new->data,4,8+17);
    SCC_SET_16LE(new->data,8,addr);
    // copy cdhd
    memcpy(new->data + 8 + 2,blk->data + 8 + 2 + 1 + 2 + SCC_MAX_CLASS*2,15);
  }
  // write VERB
  SCC_SET_32(new->data,8+cdhd_size,MKID('V','E','R','B'));
  SCC_SET_32BE(new->data,8+cdhd_size+4,8 + nverb*3+1+verb_size);

  vbase = 8+cdhd_size+8;
  vpos = vbase+nverb*3+1;
  vn = 0;
  while(scr) {
    if(scr->sym)
      new->data[vbase+vn*3] = scr->sym->addr;
    else
      new->data[vbase+vn*3] = 0xFF;
    SCC_SET_16LE(new->data,vbase+vn*3+1,vpos-cdhd_size-8);
    if(scr->code_len) memcpy(&new->data[vpos],scr->code,scr->code_len);
    vpos += scr->code_len;
    vn++;
    // we should free it too
    scr = scr->next;
  }
  new->data[vbase+vn*3] = 0;
  memcpy(&new->data[vpos],&blk->data[pos],len);
  vpos += len;

  if(vpos != new_size) {
    scc_log(LOG_ERR,"Something went wrong %d %d???\n",vpos,new_size);
    return NULL;
  }

  return new;
}


static scc_ld_block_t* scc_ld_ecd_patch(scc_ld_room_t* room,
					scc_ld_block_t* blk,int type) {
  scc_script_t* scr;
  scc_ld_block_t* new;

  scr = scc_ld_parse_scob(room,NULL,blk->data,blk->data_len);

  if(!scr) return NULL;

  new = malloc(sizeof(scc_ld_block_t) + scr->code_len);
  new->type = type;
  new->asis = 1;
  new->next = blk->next;
  new->data_len = scr->code_len;
  memcpy(new->data,scr->code,scr->code_len);

  scc_script_free(scr);

  return new;
}

static scc_ld_block_t* scc_ld_lscr_patch(scc_ld_room_t* room,
					scc_ld_block_t* blk) {
  scc_script_t* scr;
  scc_ld_block_t* new;
  int id_size = scc_ns->target->version == 7 ? 2 : 1;
   
  scr = scc_ld_parse_scob(room,NULL,&blk->data[2],blk->data_len-2);

  if(!scr) return NULL;

  new = malloc(sizeof(scc_ld_block_t) + id_size + scr->code_len);
  new->type = MKID('L','S','C','R');
  new->asis = 1;
  new->next = blk->next;
  new->data_len = scr->code_len+id_size;
  memcpy(new->data,blk->data,id_size);
  memcpy(&new->data[id_size],scr->code,scr->code_len);

  scc_script_free(scr);

  return new;
}

static scc_ld_block_t* scc_ld_scrp_patch(scc_ld_room_t* room,
					 scc_ld_block_t* blk) {
  scc_script_t* scr;
  scc_ld_block_t* new;
  scc_symbol_t* sym;
  uint16_t id = SCC_GET_16LE(blk->data,0);

  sym = scc_ns_get_sym_with_id(room->ns,SCC_RES_SCR,id);
  if(!sym) {
    scc_log(LOG_ERR,"Invalid id in scrp block.\n");
    return NULL;
  }

  scr = scc_ld_parse_scob(room,NULL,&blk->data[2],blk->data_len-2);

  if(!scr) return NULL;

  new = malloc(sizeof(scc_ld_block_t) + scr->code_len);
  new->type = MKID('S','C','R','P');
  new->asis = 1;
  new->addr = sym->addr;
  new->next = blk->next;
  new->data_len = scr->code_len;
  memcpy(new->data,scr->code,scr->code_len);

  scc_script_free(scr);

  return new;
}

int scc_ld_res_patch(scc_ld_room_t* room, scc_ld_block_t* list,
                     int type, char* name) {
  scc_ld_block_t* blk;
  unsigned newid,i;

  for(blk = list ; blk ; blk = blk->next) {
    scc_symbol_t* s = scc_ns_get_sym_with_id(room->ns,type,blk->addr);
    if(!s) {
      scc_log(LOG_ERR,"Got %s with invalid id!!!!\n",name);
      return 0;
    }
    newid = 0;
    for(i = 0 ; i < 4 ; i++)
      newid |= SCC_TOUPPER((blk->type>>(i*8))&0xFF)<<(i*8);
    blk->type = newid;
    blk->addr = s->addr;
  }
  return 1;
}


int scc_ld_room_patch(scc_ld_room_t* room) {
  scc_ld_block_t* blk,*new,*last = NULL;

  if(!scc_ns_get_addr_from(room->ns,scc_ns)) {
    scc_log(LOG_ERR,"Failed to import the address in the room ns.\n");
    return 0;
  }

  for(blk = room->room ; blk ; blk = blk->next) {
    if(blk->asis) {
      last = blk;
      continue;
    }

    scc_log(LOG_DBG,"Patching %c%c%c%c block.\n",UNMKID(blk->type));
    switch(blk->type) {
    case MKID('o','b','i','m'):
      new = scc_ld_obim_patch(room,blk);
      break;
    case MKID('o','b','c','d'):
      new = scc_ld_obcd_patch(room,blk);
      break;
    case MKID('e','n','c','d'):
      new = scc_ld_ecd_patch(room,blk,MKID('E','N','C','D'));
      break;
    case MKID('e','x','c','d'):
      new = scc_ld_ecd_patch(room,blk,MKID('E','X','C','D'));
      break;
    case MKID('l','s','c','r'):
      new = scc_ld_lscr_patch(room,blk);
      break;
    default:
      scc_log(LOG_ERR,"Got unhandled room block!!!\n");
      return 0;
    }

    if(!new) return 0;
    if(blk != new) {
      if(last) last->next = new;
      else room->room = new;
      blk = new;
    }
    last = new;
  }

  last = NULL;
  for(blk = room->scr ; blk ; blk = blk->next) {
    if(blk->asis) continue;

    scc_log(LOG_DBG,"Patching %c%c%c%c block.\n",UNMKID(blk->type));
    switch(blk->type) {
      case MKID('s','c','r','p'):
	new = scc_ld_scrp_patch(room,blk);
	break;
    default:
      scc_log(LOG_ERR,"Got unhandled script block!!!\n");
      return 0;
    }

    if(!new) return 0;
    if(blk != new) {
      if(last) last->next = new;
      else room->scr = new;
      blk = new;
    }
    last = new;
  }

  if(!scc_ld_res_patch(room,room->cost,SCC_RES_COST,"costume"))
    return 0;

  if(!scc_ld_res_patch(room,room->chset,SCC_RES_CHSET,"charset"))
    return 0;

  if(!scc_ld_res_patch(room,room->snd,SCC_RES_SOUND,"sound"))
    return 0;

  return 1;
}

static int scc_ld_block_list_size(scc_ld_block_t* blk) {
  int size = 0;

  for( ; blk ; blk = blk->next) {
    if(!blk->asis) continue;
    size += 8 + blk->data_len;
  }

  return size;
}

int scc_ld_lflf_size(scc_ld_room_t* room) {
  int size = 8; // ROOM

  size += scc_ld_block_list_size(room->room);

  size += scc_ld_block_list_size(room->scr);

  size += scc_ld_block_list_size(room->snd);

  size += scc_ld_block_list_size(room->cost);

  size += scc_ld_block_list_size(room->chset);

  return size;
}

int scc_ld_write_lflf(scc_ld_room_t* room, scc_fd_t* fd) {
  
  scc_fd_w32(fd,MKID('L','F','L','F'));
  scc_fd_w32be(fd,8 + scc_ld_lflf_size(room));

  scc_fd_w32(fd,MKID('R','O','O','M'));
  scc_fd_w32be(fd,8 + scc_ld_block_list_size(room->room));
  if(!scc_ld_write_block_list(room->room,fd)) return 0;

  if(!scc_ld_write_block_list(room->scr,fd)) return 0;

  if(!scc_ld_write_block_list(room->snd,fd)) return 0;

  if(!scc_ld_write_block_list(room->cost,fd)) return 0;

  if(!scc_ld_write_block_list(room->chset,fd)) return 0;

  return 1;  
}

int scc_ld_lecf_size(scc_ld_room_t* room) {
  int size = 0;
  int n = 0;
  
  // LFLF blocks
  while(room) {
    size += 8 + scc_ld_lflf_size(room);
    n++;
    room = room->next;
  }

  // LOFF
  size += 8 + 1 + 5*n;

  return size;
}

int scc_ld_write_loff(scc_ld_room_t* room, scc_fd_t* fd) {
  int n;
  scc_ld_room_t* r;
  int off = 8 + 8 + 8; // LECF LOFF LFLF

  for(n = 0, r = room ; r ; r = r->next) n++;

  scc_fd_w32(fd,MKID('L','O','F','F'));
  scc_fd_w32be(fd,8 + 1 + 5*n);

  scc_fd_w8(fd,n);
  off += 1+5*n;
  for(r = room ; r ; r = r->next) {
    scc_fd_w8(fd,r->sym->addr);
    scc_fd_w32le(fd,off);
    off += 8 + scc_ld_lflf_size(r);
  }

  return 1;
}

int scc_ld_write_lecf(scc_ld_room_t* room, scc_fd_t* fd) {
  
  scc_fd_w32(fd,MKID('L','E','C','F'));
  scc_fd_w32be(fd,8 + scc_ld_lecf_size(room));

  if(!scc_ld_write_loff(room,fd)) return 0;

  while(room) {
    if(!scc_ld_write_lflf(room,fd)) return 0;
    room = room->next;
  }

  return 1;
}

scc_ld_room_t* scc_ld_get_room(scc_symbol_t* s) {
  scc_ld_room_t* r;

  for(r = scc_room ; r ; r = r->next) {
    if(r->sym->type == s->type &&
       r->sym->subtype == s->subtype &&
       r->sym->addr == s->addr) return r;
  }
  return NULL;
}

static scc_ld_block_t* scc_ld_room_get_res_list(scc_ld_room_t* r, int rtype) {
  switch(rtype) {
    case SCC_RES_CHSET:
      return r->chset;
  case SCC_RES_COST:
    return r->cost;
  case SCC_RES_SOUND:
    return r->snd;
  case SCC_RES_SCR:
    return r->scr;
  }
  return NULL;
}

int scc_ld_write_res_idx(scc_fd_t* fd, int n,char* name,int rtype) {
  uint8_t* room_no = calloc(1,n);
  uint32_t* room_off = calloc(4,n);
  scc_symbol_t* s;
  scc_ld_room_t* r;
  scc_ld_block_t* blk;
  int a;

  for(a = 1 ; a < n ; a++) {
    int off;
    if(!scc_ns_is_addr_alloc(scc_ns,rtype,a)) continue;
    s = scc_ns_get_sym_at(scc_ns,rtype,a);
    if(!s || !s->parent) {
      scc_log(LOG_ERR,"Error while writing %s index.\n",name);
      return 0;
    }
    r = scc_ld_get_room(s->parent);
    if(!r) {
      scc_log(LOG_ERR,"Error while writing %s index.\n",name);
      return 0;
    }

    off = 8;
    switch(rtype) {
    case SCC_RES_CHSET:
      off += scc_ld_block_list_size(r->cost);
    case SCC_RES_COST:
      off += scc_ld_block_list_size(r->snd);
    case SCC_RES_SOUND:
      off += scc_ld_block_list_size(r->scr);
    case SCC_RES_SCR:
      off += scc_ld_block_list_size(r->room);
    }

    for(blk = scc_ld_room_get_res_list(r,rtype) ; 
        blk && blk->addr != a ; blk = blk->next) {
      off += 8 + blk->data_len;
    }
    if(!blk) {
      scc_log(LOG_ERR,"Error while writing script index (type = %i).\n", rtype);
      return 0;
    }
    room_no[a] = r->sym->addr;
    room_off[a] = SCC_TO_32LE(off);
  }
  scc_fd_write(fd,room_no,n);
  scc_fd_write(fd,room_off,n*4);
  free(room_no);
  free(room_off);

  return 1;
}

int scc_ld_write_idx(scc_ld_room_t* room, scc_fd_t* fd,
                     int local_n,int array_n,int flobj_n,
                     int inv_n, int write_room_names) {
  int room_n = 0,scr_n = scc_ns_res_max(scc_ns,SCC_RES_SCR) + 1;
  int chset_n = scc_ns_res_max(scc_ns,SCC_RES_CHSET) + 1;
  int obj_n = scc_ns_res_max(scc_ns,SCC_RES_OBJ) + 1;
  int cost_n = scc_ns_res_max(scc_ns,SCC_RES_COST)+1;
  int snd_n = scc_ns_res_max(scc_ns,SCC_RES_SOUND)+1;
  int var_n = scc_ns_res_max(scc_ns,SCC_RES_VAR)+1;
  int bvar_n = (scc_ns_res_max(scc_ns,SCC_RES_BVAR)+1)&0x7FFF;
  int i;
  char rnam_buf[10];
  scc_ld_room_t* r;


  for(r = room ; r ; r = r->next) room_n++;
  room_n++;

  // we must give enouth space for all the engine vars
  // if we didn't used them all.
  if(var_n < 140) var_n = 140;

  // the orginal engine seems to just divide by 8 :(
  bvar_n = ((bvar_n+7)/8)*8;

  scc_fd_w32(fd,MKID('R','N','A','M'));
  if (write_room_names)
  {
     // NOTE: ScummVM uses a different codepath for HE v8 games as the format
     //       is slightly different. But for now we will just implement
     //       this format.
     scc_fd_w32be(fd, 8 + ((room_n-1)*10) + 1);
     
     for (r = room ; r ; r = r->next)
     {
        scc_fd_w8(fd, r->sym->addr);
        strncpy(rnam_buf, r->sym->sym, 9);
        for (i=0; i<10; i++)
            rnam_buf[i] ^= 0xFF; 
        scc_fd_write(fd, rnam_buf, 9);
     }

     scc_fd_w8(fd, 0);
  }
  else
  {
     scc_fd_w32be(fd,8 + 1);
     scc_fd_w8(fd,0);
  }

  scc_fd_w32(fd,MKID('M','A','X','S'));
  if(room->vm_version == 7) {
    char dummy_version[50] = { 0 };
    scc_fd_w32be(fd,8 + 50*2+15*2);

    scc_fd_write(fd,dummy_version,50);
    scc_fd_write(fd,dummy_version,50);
    scc_fd_w16le(fd,var_n);
    scc_fd_w16le(fd,bvar_n);
    scc_fd_w16le(fd,0); // unk
    scc_fd_w16le(fd,obj_n);
    scc_fd_w16le(fd,local_n);
    scc_fd_w16le(fd,20); // new name
    scc_fd_w16le(fd,scc_ns_res_max(scc_ns,SCC_RES_VERB)+1);
    scc_fd_w16le(fd,flobj_n); // fl objects
    scc_fd_w16le(fd,inv_n); // inventory
    scc_fd_w16le(fd,array_n);  // num array we should count them
    scc_fd_w16le(fd,room_n);
    scc_fd_w16le(fd,scr_n);
    scc_fd_w16le(fd,snd_n);
    scc_fd_w16le(fd,chset_n);
    scc_fd_w16le(fd,cost_n);
  } else {
    scc_fd_w32be(fd,8 + 15*2);

    scc_fd_w16le(fd,var_n);
    scc_fd_w16le(fd,0); // unk
    scc_fd_w16le(fd,bvar_n);
    scc_fd_w16le(fd,local_n); // local obj, dunno what's that exactly
    scc_fd_w16le(fd,array_n);  // num array we should count them
    scc_fd_w16le(fd,0); // unk
    scc_fd_w16le(fd,scc_ns_res_max(scc_ns,SCC_RES_VERB)+1);
    scc_fd_w16le(fd,flobj_n); // fl objects
    scc_fd_w16le(fd,inv_n); // inventory
    scc_fd_w16le(fd,room_n);
    scc_fd_w16le(fd,scr_n);
    scc_fd_w16le(fd,snd_n);
    scc_fd_w16le(fd,chset_n);
    scc_fd_w16le(fd,cost_n);
    scc_fd_w16le(fd,obj_n);
  }

  scc_fd_w32(fd,MKID('D','R','O','O'));
  scc_fd_w32be(fd,8 + 2 + room_n*5);
  scc_fd_w16le(fd,room_n);
  scc_fd_w8(fd,0);
  for(r = room ; r ; r = r->next)
    scc_fd_w8(fd,1);
  scc_fd_w32(fd,0);
  for(r = room ; r ; r = r->next)
    scc_fd_w32(fd,0);
  

  scc_fd_w32(fd,MKID('D','S','C','R'));
  scc_fd_w32be(fd,8 + 2 + scr_n*5);
  scc_fd_w16le(fd,scr_n);
  scc_ld_write_res_idx(fd,scr_n,"script", SCC_RES_SCR);


    
  scc_fd_w32(fd,MKID('D','S','O','U'));
  scc_fd_w32be(fd,8 + 2 + snd_n*5);
  scc_fd_w16le(fd,snd_n);
  scc_ld_write_res_idx(fd,snd_n,"sound", SCC_RES_SOUND);

  scc_fd_w32(fd,MKID('D','C','O','S'));
  scc_fd_w32be(fd,8 + 2 + cost_n*5);
  scc_fd_w16le(fd,cost_n);
  scc_ld_write_res_idx(fd,cost_n,"costume", SCC_RES_COST);



  scc_fd_w32(fd,MKID('D','C','H','R'));
  scc_fd_w32be(fd,8 + 2 + chset_n*5);
  scc_fd_w16le(fd,chset_n);
  scc_ld_write_res_idx(fd,chset_n,"charset", SCC_RES_CHSET);



  scc_fd_w32(fd,MKID('D','O','B','J'));
  if(room->vm_version == 7) {
    scc_fd_w32be(fd,8 + 2 + obj_n*6);
    scc_fd_w16le(fd,obj_n);
    scc_fd_write(fd,obj_state,obj_n);
    scc_fd_write(fd,obj_room,obj_n);
  } else {
    scc_fd_w32be(fd,8 + 2 + obj_n*5);
    scc_fd_w16le(fd,obj_n);
    for(i = 0 ; i < obj_n ; i++)
      scc_fd_w8(fd,(obj_owner[i]&0xF)|(obj_state[i]<<4));
  }
  for(i = 0 ; i < obj_n ; i++)
    scc_fd_w32le(fd,obj_class[i]);

  scc_fd_w32(fd,MKID('A','A','R','Y'));
  scc_fd_w32be(fd,8 + 2);
  scc_fd_w16le(fd,0);

  if(room->vm_version == 7) {
    scc_fd_w32(fd,MKID('A','N','A','M'));
    scc_fd_w32be(fd,8 + 2);
    scc_fd_w16le(fd,0);
  }

  return 1;
}

int scc_ld_write_sou(scc_ld_voice_t* v,scc_fd_t* fd) {

  scc_fd_w32(fd,MKID('S','O','U',' '));
  scc_fd_w32(fd,0);

  while(v) {
    if(scc_fd_write(fd,v->data,v->data_len) != v->data_len) {
      scc_log(LOG_ERR,"Write error.\n");
      return 0;
    }
    v = v->next;
  }

  return 1;
}


static char* out_file = NULL;
static int dump_rooms = 0;
static int write_room_names = 0;
static int enckey = 0;
static int max_local = 200;
static int max_array = 100;
static int max_flobj = 20;
static int max_inventory = 20;


static scc_param_t scc_ld_params[] = {
  { "o", SCC_PARAM_STR, 0, 0, &out_file },
  { "rooms", SCC_PARAM_FLAG, 0, 1, &dump_rooms },
  { "key", SCC_PARAM_INT, 0, 255, &enckey },
  { "max-local", SCC_PARAM_INT, 0, 0xFFFF, &max_local },
  { "max-array", SCC_PARAM_INT, 0, 0xFFFF, &max_array },
  { "max-flobj", SCC_PARAM_INT, 0, 0xFFFF, &max_flobj },
  { "max-inventory", SCC_PARAM_INT, 0, 0xFFFF, &max_inventory },
  { "v", SCC_PARAM_FLAG, LOG_MSG, LOG_V, &scc_log_level },
  { "vv", SCC_PARAM_FLAG, LOG_MSG, LOG_DBG, &scc_log_level },
  { "write-room-names", SCC_PARAM_FLAG, 0, 1, &write_room_names },
  { "help", SCC_PARAM_HELP, 0, 0, &sld_help },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_ld_room_t* r;
  scc_cl_arg_t* files,*f;
  int obj_n;
  char default_name[255];

  files = scc_param_parse_argv(scc_ld_params,argc-1,&argv[1]);
  if(!files) scc_print_help(&sld_help,1);

  // The global ns is now created just before loading the
  // the first to get the target vm version.
  //scc_ns = scc_ns_new(vm_version);

  for(f = files ; f ; f = f->next) {
    scc_log(LOG_V,"Loading %s.\n",f->val);
    if(!scc_ld_load_roobj(f->val))
      return 1;
  }
  scc_log(LOG_DBG,"All files loaded.\n");

  if(!scc_ld_check_ns(scc_ns,NULL)) {
    scc_log(LOG_ERR,"Some symbols are unresolved, aborting.\n");
    return 2;
  }

  // Look for the main script, and set it's address
  if(!scc_ld_find_main(scc_ns)) {
    scc_log(LOG_ERR,"Unable to find a suitable main script.\n");
    return 3;
  }

  // allocate the other address
  if(!scc_ns_alloc_addr(scc_ns)) {
    scc_log(LOG_ERR,"Address allocation failed.\n");
    return 4;
  }

  // allocate the object tables
  obj_n = scc_ns_res_max(scc_ns,SCC_RES_OBJ) + 1;
  obj_owner = calloc(1,obj_n);
  obj_state = calloc(1,obj_n);
  obj_room  = calloc(1,obj_n);
  obj_class = calloc(4,obj_n);

  // patch the rooms
  for(r = scc_room ; r ; r = r->next) {
    scc_log(LOG_V,"Patching room %s\n",r->sym->sym);
    if(!scc_ld_room_patch(r)) return 5;
  }

  // Compute our basename
  if(!out_file) {
      sprintf(default_name,"scummc%d",scc_ns->target->version);
      out_file = default_name;
  }

  // write the voice file
  if(scc_voice) {
    char name[255];
    scc_fd_t* fd;

    sprintf(name,"%s.sou",out_file);

    scc_log(LOG_V,"Outputting voice file %s\n",name);
    fd = new_scc_fd(name,O_WRONLY|O_CREAT|O_TRUNC,0);
    if(!fd) {
      scc_log(LOG_ERR,"Failed to open file %s\n",name);
      return 5;
    }
    if(!scc_ld_write_sou(scc_voice,fd)) {
      scc_log(LOG_ERR,"Failed to write SOU file %s\n",name);
      return 6;
    }
    scc_fd_close(fd);
  }

  // dump rooms
  if(dump_rooms) {
    for(r = scc_room ; r ; r = r->next) {
      char name[255];
      scc_fd_t* fd;
      sprintf(name,"%s-%03d.lflf",out_file,r->sym->addr);
      scc_log(LOG_V,"Dumping room %s to %s\n",r->sym->sym,name);
      
      fd = new_scc_fd(name,O_WRONLY|O_CREAT|O_TRUNC,enckey);
      if(!fd) {
	scc_log(LOG_ERR,"Failed to open dump file %s\n",name);
	continue;
      }
      if(!scc_ld_write_lflf(r,fd))
	scc_log(LOG_ERR,"Failed to write LFLF file %s\n",name);
      scc_fd_close(fd);
    }
  } else {
    char name[255];
    scc_fd_t* fd;

    sprintf(name,"%s.001",out_file);

    fd = new_scc_fd(name,O_WRONLY|O_CREAT|O_TRUNC,enckey);
    scc_log(LOG_V,"Outputting data file %s\n",name);

    if(!fd) {
      scc_log(LOG_ERR,"Failed to open file %s\n",name);
      return 5;
    }
    if(!scc_ld_write_lecf(scc_room,fd)) {
      scc_log(LOG_ERR,"Failed to write data file.\n");
      return 6;
    }
    scc_fd_close(fd);

    sprintf(name,"%s.000",out_file);
    scc_log(LOG_V,"Outputting index file %s\n",name);
    fd = new_scc_fd(name,O_WRONLY|O_CREAT|O_TRUNC,enckey);
    if(!fd) {
      scc_log(LOG_ERR,"Failed to open file %s\n",name);
      return 5;
    }
    if(!scc_ld_write_idx(scc_room,fd,max_local,max_array,max_flobj,max_inventory, write_room_names)) {
      scc_log(LOG_ERR,"Failed to write index file.\n");
      return 6;
    }
    scc_fd_close(fd);

  }
      

  return 0;
}
