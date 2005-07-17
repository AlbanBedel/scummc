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

// SCC name space implementation
typedef struct scc_ns_st {
  // global symbol tree
  scc_symbol_t *glob_sym;
  // current start point in the tree.
  // NULL is global
  scc_symbol_t *cur;
  // this is used for rid allocation
  uint16_t rids[SCC_RES_LAST];
  // this represent the address spaces, one bit per address
  uint8_t as[SCC_RES_LAST][0x10000/8];
} scc_ns_t;

scc_ns_t* scc_ns_new(void);

void scc_ns_free(scc_ns_t* ns);

scc_symbol_t* scc_ns_get_sym(scc_ns_t* ns, char* room, char* sym);

scc_symbol_t* scc_ns_get_sym_with_id(scc_ns_t* ns,int type, int id);

scc_symbol_t* scc_ns_add_sym(scc_ns_t* ns, char* sym, int type, int subtype,
			     int addr,char status);

scc_symbol_t* scc_ns_decl(scc_ns_t* ns, char* room, char* sym, 
			  int type, int subtype, int addr);

int scc_ns_get_rid(scc_ns_t* ns, scc_symbol_t* s);

int scc_ns_push(scc_ns_t* ns, scc_symbol_t* s);

void scc_ns_clear(scc_ns_t* ns,int type);

int scc_ns_pop(scc_ns_t* ns);

int scc_ns_set_sym_addr(scc_ns_t* ns, scc_symbol_t* s,int addr);

int scc_ns_alloc_sym_addr(scc_ns_t* ns,scc_symbol_t* s,int* cur);

int scc_ns_alloc_addr(scc_ns_t* ns);

int scc_ns_get_addr_from(scc_ns_t* ns,scc_ns_t* from);

int scc_ns_res_max(scc_ns_t* ns,int type);

int scc_ns_is_addr_alloc(scc_ns_t* ns,int type,int addr);

scc_symbol_t* scc_ns_get_sym_at(scc_ns_t* ns,int type,int addr);

int scc_sym_is_global(int type);

int scc_sym_is_var(int type);
