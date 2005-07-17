/* ScummC
 * Copyright (C) 2004-2005  Alban Bedel
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "scc_util.h"
#include "scc_parse.h"
#include "scc_ns.h"

static int scc_addr_max[] = {
  0,       // unused
  0x3FFF,  // VAR
  0xFF,    // ROOM
  SCC_MAX_GLOB_SCR-1, // SCR
  0xFFFF,  // COST
  0xFFFF,  // SOUND
  0xFFFF,  // CHSET
  0xFF,    // LSCR
  0xFE,    // VERB (0xFF is default)
  0xFFFF,  // OBJ
  0x0F,    // STATE
  0x400F,  // LVAR
  0xFFFF,  // BVAR
  0xFFFF,  // PALS
  0x0F,    // CYCL
  0x0F,    // ACTOR
  0xFF,    // BOX
  SCC_MAX_CLASS,  // CLASS
  0x7FFFFFFF      // VOICE
};

void scc_symbol_list_free(scc_symbol_t* s);

void scc_symbol_free(scc_symbol_t* s) {
  if(s->sym) free(s->sym);
  if(s->childs) scc_symbol_list_free(s->childs);
  free(s);
}

void scc_symbol_list_free(scc_symbol_t* s) {
  scc_symbol_t* n;

  while(s) {
    n = s->next;
    scc_symbol_free(s);
    s = n;
  }
}

int scc_sym_is_var(int type) {
  if(type == SCC_RES_VAR ||
     type == SCC_RES_BVAR ||
     type == SCC_RES_LVAR)
    return 1;
  return 0;
}

int scc_sym_is_global(int type) {
  if(type == SCC_RES_VAR ||
     type == SCC_RES_BVAR ||
     type == SCC_RES_ROOM ||
     type == SCC_RES_VERB ||
     type == SCC_RES_ACTOR ||
     type == SCC_RES_CLASS)
    return 1;
  return 0;
}

scc_ns_t* scc_ns_new(void) {
  scc_ns_t* ns = calloc(1,sizeof(scc_ns_t));
  return ns;
}

void scc_ns_free(scc_ns_t* ns) {
  scc_symbol_list_free(ns->glob_sym);
  free(ns);
}

scc_symbol_t* scc_ns_get_sym(scc_ns_t* ns, char* room, char* sym) {
  scc_symbol_t* r;
  scc_symbol_t* cur;

  if(room) {
    for(r = ns->glob_sym ; r ; r = r->next) {
      if(r->type != SCC_RES_ROOM) continue;
      if(!strcmp(r->sym,room)) break;
    }
    if(!r) return NULL;
    cur = r->childs;
  } else if(ns->cur) {
    if(ns->cur->childs)
      cur = ns->cur->childs;
    else if(ns->cur->parent)
      cur = ns->cur->parent->childs;
    else
      cur = ns->glob_sym;
  } else {
    cur = ns->glob_sym;
  }
    

  while(cur) {
    for(r = cur ; r ; r = r->next) {
      if(strcmp(r->sym,sym)) continue;
      return r;
    }
    // for full name we don't want to continue the search
    if(room) break;
    if(cur->parent) {
      if(cur->parent->parent)
	cur = cur->parent->parent->childs;
      else
	cur = ns->glob_sym;
    } else
      cur = NULL;
  }

  return NULL;
}

scc_symbol_t* scc_ns_get_sym_with_id(scc_ns_t* ns,int type, int id) {
  scc_symbol_t* r,*r2;

  // room, verbs and variables are in the global ns
  if(scc_sym_is_global(type)) {
    for(r = ns->glob_sym ; r ; r = r->next) {
      if(r->type == type && r->rid == id) return r;
    }
    return NULL;
  }

  // local vars are in the current sym
  if(type == SCC_RES_LVAR) {
    for(r = ns->cur ; r ; r = r->next) {
      if(r->type == SCC_RES_LVAR && r->rid == id) return r;
    }
    return NULL;
  }
      

  // the rest is in some room ... somewhere :)
  for(r = ns->glob_sym ; r ; r = r->next) {
    if(r->type != SCC_RES_ROOM) continue;
    for(r2 = r->childs ; r2 ; r2 = r2->next) {
      if(r2->type == type && r2->rid == id) return r2;
    }
  }

  return NULL;
}

// check if a redeclaration is compatible with the one we had before
static int scc_ns_redecl_sym(scc_ns_t* ns, scc_symbol_t* i, 
			     int type, int subtype,
			     int addr) {
  if(i->type != type || i->subtype != subtype) {
    printf("Symbole %s is alredy defined with another type.\n",
	   i->sym);
    return 0;
  }
  if(i->addr >= 0 && addr >= 0 && i->addr != addr) {
    printf("Symbole %s is alredy defined with address %d.\n",
	   i->sym,i->addr);
    return 0;
  }

  if(i->addr < 0 && addr)
    return scc_ns_set_sym_addr(ns,i,addr);

  return 1;
}

scc_symbol_t*  scc_ns_add_sym(scc_ns_t* ns, char* sym,
			      int type, int subtype,
			      int addr,char status) {
  scc_symbol_t* rr = scc_ns_get_sym(ns,NULL,sym);

  if(rr) {
    // check for address eventual type/address clash
    if(!scc_ns_redecl_sym(ns,rr,type,subtype,addr)) return 0;
    // the new sym export
    if(status == 'E') {
      // alredy exported ??
      if(rr->status == 'E') {
	printf("Symbol %s is exported several times.\n",sym);
	return 0;
      }
      rr->status = 'E';
    }
    return rr;
  }

  printf("Adding %s symbol %s (%d-%d @ %d)\n",
	 status == 'E' ? "exported" : "imported",
	 sym,type,subtype,addr);

  rr = calloc(1,sizeof(scc_symbol_t));
  rr->type = type;
  rr->subtype = subtype;
  rr->sym = strdup(sym);
  rr->addr = -1;
  rr->status = status;

  if(addr >= 0) {
    if(!scc_ns_set_sym_addr(ns,rr,addr)) {
      scc_symbol_free(rr);
      return NULL;
    }
  }

  if(ns->cur) {
    rr->next = ns->cur->childs;
    ns->cur->childs = rr;
    rr->parent = ns->cur;
  } else {
    rr->next = ns->glob_sym;
    ns->glob_sym = rr;
  }

  return rr;
}

static int scc_ns_make_addr(int type, int subtype, int addr) {

  if(addr < 0) return addr;

  if(type == SCC_RES_BVAR) addr |= 0x8000;
  else if(type == SCC_RES_LVAR) addr |= 0x4000;
  else if(type == SCC_RES_LSCR) addr += SCC_MAX_GLOB_SCR;

  return addr;
}

scc_symbol_t* scc_ns_decl(scc_ns_t* ns, char* room, char* sym, 
			  int type, int subtype, int addr) {
  scc_symbol_t* rr;
  scc_symbol_t* new;

  addr = scc_ns_make_addr(type,subtype,addr);

  if(room) {
      // go into a room ns
      rr = scc_ns_get_sym(ns,NULL,room);
      if(!rr || rr->type != SCC_RES_ROOM) {
	printf("%s is not a declared room.\n",room);
	return NULL;
      }
      // return scc_ns_decl_in_room(ns,rr,sym,type,subtype,addr);
  } else {
    rr = scc_ns_get_sym(ns,NULL,sym);
    if(rr) {
      if(scc_ns_redecl_sym(ns,rr,type,subtype,addr)) return rr;
      return NULL;
    }
  }
  
  new = calloc(1,sizeof(scc_symbol_t));
  new->type = type;
  new->subtype = subtype;
  new->sym = strdup(sym);

  if(addr >= 0) {
    if(!scc_ns_set_sym_addr(ns,new,addr)) {
      scc_symbol_free(new);
      return NULL;
    }
  } else
      new->addr = addr;

  if(room) {
    new->next = rr->childs;
    rr->childs = new;
    new->parent = rr;
  } else if(ns->cur) {
    new->next = ns->cur->childs;
    ns->cur->childs = new;
    new->parent = ns->cur;
  } else {
    new->next = ns->glob_sym;
    ns->glob_sym = new;
  }
    
  return new;
  
}

int scc_ns_push(scc_ns_t* ns, scc_symbol_t* s) {

  if(s->parent != ns->cur) {
    printf("Trying to push sym %s in the ns, but it doesn't belong to the current level %s\n",s->sym,ns->cur->sym);
    return 0;
  }
  
  ns->cur = s;
  return 1;
}

void scc_ns_clear(scc_ns_t* ns,int type) {
  scc_symbol_t *s,*o = NULL;

  if(!ns->cur) {
    printf("Trying to clear ns, but there is no pushed sym !!!\n");
    return;
  }

  s = ns->cur->childs;
  while(s) {
    
    if(s->type != type) {
      o = s;
      s = s->next;
      continue;
    }
     
    if(o) {
      o->next = s->next;
      scc_symbol_free(s);
      s = o->next;
    } else {
      ns->cur->childs = s->next;
      scc_symbol_free(s);
      s = ns->cur->childs;
    }
  }
  memset(&ns->as[type],0,0x10000/8);
}

int scc_ns_pop(scc_ns_t* ns) {


  if(!ns->cur) {
    printf("Trying to pop ns, but there is no pushed sym !!!\n");
    return 0;
  }

  ns->cur = ns->cur->parent;
  return 1;
}

// allocate a ressource id for this symbol
int scc_ns_get_rid(scc_ns_t* ns, scc_symbol_t* s) {
  printf("Get rid for %s\n",s->sym);
  // make sure the parent symbols all have rid allocated
  if(s->parent && !s->parent->rid) scc_ns_get_rid(ns,s->parent);

  if(s->rid > 0) return s->rid;

  // bit variable have there own address space
  ns->rids[s->type]++;
  s->rid = ns->rids[s->type];

  return s->rid;
}

int scc_ns_set_sym_addr(scc_ns_t* ns, scc_symbol_t* s,int addr) {
  uint8_t* as = ns->as[s->type];

  if(addr == s->addr) return 1;

  if(addr > scc_addr_max[s->type]) {
    printf("Address 0x%X is invalid for its type.\n",addr);
    return 0;
  }

  if(as[addr/8] & (1 << (addr%8))) {
    printf("Address 0x%X is alredy in use.\n",addr);
    return 0;
  }
  as[addr/8] |= (1 << (addr%8));

  s->addr = addr;
  return 1;
}

int scc_ns_alloc_sym_addr(scc_ns_t* ns,scc_symbol_t* s,int* cur) {
  uint8_t* as = ns->as[s->type];
  int i;

  if(s->addr >= 0) return 1;

  for(i = cur[0] ; i <= scc_addr_max[s->type] ; i++) {
    if(as[i/8] & (1 << (i%8))) continue;
    as[i/8] |= 1 << (i%8);
    cur[0]++;
    s->addr = i;
    return 1;
  }
  return 0;

}

int scc_ns_alloc_addr(scc_ns_t* ns) {
  scc_symbol_t* r,*s;
  int cur[] = {
    0, // nothing
    140, // VAR (don't use the engine vars)
    1,  // ROOM
    1,  // SCR
    1,  // COST
    1,  // SOUND
    1,  // CHSET
    SCC_MAX_GLOB_SCR, // LSCR
    1,  // VERB
    SCC_MAX_ACTOR, // OBJ
    1, // STATE
    0x4000, // LVAR
    0x8000, // BVAR
    1,  // PAL
    1,  // CYCL
    1,  // ACTOR
    0,  // BOX
    1,  // CLASS
    0   // VOICE
  };

  // a little self test in case SCC_RES_LAST change
  if(sizeof(cur)/sizeof(int) != SCC_RES_LAST) {
    printf("Pls chacek scc_ns_alloc_addr !!!!\n");
    return 0;
  }
    
  printf("Allocating address:\n");
  for(r = ns->glob_sym ; r ; r = r->next) {
    if(!scc_ns_alloc_sym_addr(ns,r,&cur[r->type])) {
      printf("Failed to allocate an address for %s.\n",r->sym);
      return 0;
    }
    printf("%s @ 0x%X\n",r->sym,r->addr);
    if(r->type == SCC_RES_ROOM) {
      for(s = r->childs ; s ; s = s->next) {
	if(!scc_ns_alloc_sym_addr(ns,s,&cur[s->type])) {
	  printf("Failed to allocate an address for %s::%s.\n",
		 r->sym,s->sym);
	  return 0;
	}
	printf("%s::%s @ 0x%X\n",r->sym,s->sym,s->addr);
      }
    }
  }

  return 1;
}

static int scc_symbol_get_addr_from(scc_symbol_t* s,scc_symbol_t* ref) {
  if(ref->addr < 0) {
    printf("The symbol %s have no address in the src ns.\n",s->sym);
    return 0;
  }

  if(s->addr >= 0 && s->addr != ref->addr) {
    printf("Got address clash on symbol %s: 0x%X != 0x%X ????\n",s->sym,
	   s->addr,ref->addr);
  }

  if(ref->type != s->type ||
     ref->subtype != s->subtype) {
    printf("The symbol %s is not of the same type in src ns.\n",s->sym);
    return 0;
  }
  s->addr = ref->addr;
  return 1;
}


int scc_ns_get_addr_from(scc_ns_t* ns,scc_ns_t* from) {
  scc_symbol_t* r,*s,*ref;

  for(r = ns->glob_sym ; r ; r = r->next) {
    ref = scc_ns_get_sym(from,NULL,r->sym);
    if(!ref) {
      printf("Failed to find symbol %s in the scr ns.\n",r->sym);
      return 0;
    }
    if(!scc_symbol_get_addr_from(r,ref)) return 0;
    if(r->type == SCC_RES_ROOM) {
      for(s = r->childs ; s ; s = s->next) {
	ref = scc_ns_get_sym(from,r->sym,s->sym);
	if(!ref) {
	  printf("Failed to find symbol %s::%s in the scr ns.\n",
		 r->sym,s->sym);
	  return 0;
	}
	if(!scc_symbol_get_addr_from(s,ref)) return 0;
      }
    }
  }
  return 1;
}


int scc_ns_res_max(scc_ns_t* ns,int type) {
  scc_symbol_t* r,*s;
  int n = 0;

  if(scc_sym_is_global(type)) {
    for(r = ns->glob_sym ; r ; r = r->next) {
      if(r->type == type && r->addr > n) n = r->addr;
    }
    if(type == SCC_RES_BVAR)
      n &= 0x7FFF;

    return n;
  }
   
  for(r = ns->glob_sym ; r ; r = r->next) {
    if(r->type != SCC_RES_ROOM) continue;
    for(s = r->childs ; s ; s = s->next) {
      if(s->type == type && s->addr > n) n = s->addr;
    }
  }
  return n;

}

int scc_ns_is_addr_alloc(scc_ns_t* ns,int type,int addr) {
  if(addr < 0 || type < 0 || type >= SCC_RES_LAST) return 0;

  return (ns->as[type][addr/8] & (1<<(addr%8))) ? 1 : 0;
}

scc_symbol_t* scc_ns_get_sym_at(scc_ns_t* ns,int type,int addr) {
  scc_symbol_t* r,*r2;

  // room, verbs and variables are in the global ns
  if(scc_sym_is_global(type)) {
    for(r = ns->glob_sym ; r ; r = r->next) {
      if(r->type == type && r->addr == addr) return r;
    }
    return NULL;
  }

  // local vars are in the current sym
  if(type == SCC_RES_LVAR) {
    for(r = ns->cur ; r ; r = r->next) {
      if(r->type == SCC_RES_LVAR && r->addr == addr) return r;
    }
    return NULL;
  }
      

  // the rest is in some room ... somewhere :)
  for(r = ns->glob_sym ; r ; r = r->next) {
    if(r->type != SCC_RES_ROOM) continue;
    for(r2 = r->childs ; r2 ; r2 = r2->next) {
      if(r2->type == type && r2->addr == addr) return r2;
    }
  }
  
  return NULL;
}
