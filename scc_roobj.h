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

/**
 * @file scc_roobj.h
 * @ingroup scc
 * @brief ScummC object files
 */

typedef struct scc_roobj_res_st scc_roobj_res_t;
typedef struct scc_roobj_cycl_st scc_roobj_cycl_t;
typedef struct scc_roobj_state_st scc_roobj_state_t;
typedef struct scc_roobj_obj_st scc_roobj_obj_t;
typedef struct scc_roobj_st scc_roobj_t;

/// Generic data ressources like music.
struct scc_roobj_res_st {
  scc_roobj_res_t* next;

  scc_symbol_t* sym;
  uint8_t* data;
  unsigned data_len;
};

/// Palette cycle definition
struct scc_roobj_cycl_st {
  scc_roobj_cycl_t* next;

  scc_symbol_t* sym;
  int freq,flags;
  int start,end;
};

/// Object state definition
struct scc_roobj_state_st {
  scc_roobj_state_t* next;

  /// hotspot
  int hs_x,hs_y;
  scc_img_t* img;
  scc_img_t* zp[SCC_MAX_IM_PLANES];
};  

/// Object data
struct scc_roobj_obj_st {
  scc_roobj_obj_t* next;

  /// Symbol for this object
  scc_symbol_t* sym;
  /// Hotspot list for each state
  int hotspots[SCC_MAX_IM_PLANES+1];
  int x,y,w,h;
  /// Parent object
  scc_symbol_t* parent;
  /// Parent state wich enable this object
  int parent_state;
  /// Index (in the room) of the parent object
  int parent_id;
  /// Direction the actors should use when facing the object
  int dir;
  /// Default state
  int state;
  /// Name of the object in game
  char* name;

  /// hotspot used if no extra state is defined
  int hs_x,hs_y;
  /// state have image, zplanes and hotspot
  scc_roobj_state_t* states;
  /// verbs
  scc_script_t* verb;
  /// generated imnn
  scc_imnn_t* im;

  /// Initial owner of the object
  scc_symbol_t* owner;
  /// Classes the object belong to
  scc_symbol_t* class[SCC_MAX_CLASS];
};

/// Room data
struct scc_roobj_st {
  /// Chain when we have several room
  scc_roobj_t* next;

  /// Targeted VM version
  scc_target_t* target;

   /// Symbol of this romm
  scc_symbol_t* sym;

  /// Global scripts
  scc_script_t* scr;
  /// Local scripts
  scc_script_t* lscr;

  /// Object list
  scc_roobj_obj_t* obj,*last_obj;

  /// the defined ressources.
  scc_roobj_res_t *res;

  /// Palette cycles
  scc_roobj_cycl_t* cycl;
  /// Transparent color
  int trans;
  /// Background image
  scc_img_t* image;
  /// Z-Planes for masking actors
  scc_img_t* zplane[SCC_MAX_IM_PLANES];
  /// Box list
  scc_boxd_t* boxd;
  /// Box matrix
  scc_data_t* boxm;
  /// Scaling slots
  scc_data_t* scal;
};

scc_roobj_t* scc_roobj_new(scc_target_t* t, scc_symbol_t* sym);

void scc_roobj_free(scc_roobj_t* ro);

scc_roobj_res_t* scc_roobj_get_res(scc_roobj_t* ro,scc_symbol_t* sym);

scc_script_t* scc_roobj_get_scr(scc_roobj_t* ro, scc_symbol_t* sym);

int scc_roobj_add_scr(scc_roobj_t* ro,scc_script_t* scr);

scc_roobj_obj_t* scc_roobj_get_obj(scc_roobj_t* ro,scc_symbol_t* sym);

int scc_roobj_add_obj(scc_roobj_t* ro,scc_roobj_obj_t* obj);

scc_roobj_res_t* scc_roobj_add_res(scc_roobj_t* ro,scc_symbol_t* sym, 
				   char* val);

int scc_roobj_set_param(scc_roobj_t* ro,scc_ns_t* ns,char* p, char* val);

int scc_roobj_set_zplane(scc_roobj_t* ro, int idx,char* file);

int scc_roobj_add_voice(scc_roobj_t* ro, scc_symbol_t* sym, char* file,
                        int nsync, int* sync);

int scc_roobj_add_cycl(scc_roobj_t* ro, scc_symbol_t* sym,
                       int delay, int flags, int start, int end);

int scc_roobj_write(scc_roobj_t* ro,scc_ns_t* ns, scc_fd_t* fd);


scc_roobj_obj_t* scc_roobj_obj_new(scc_symbol_t* sym);

void scc_roobj_obj_free(scc_roobj_obj_t* obj);

int scc_roobj_obj_add_state(scc_roobj_obj_t* obj,int x, int y,
			    char *img_path,char** zp_paths);

scc_script_t* scc_roobj_obj_get_verb(scc_roobj_obj_t* obj,scc_symbol_t* sym);

int scc_roobj_obj_add_verb(scc_roobj_obj_t* obj,scc_script_t* scr);

int scc_roobj_obj_set_param(scc_roobj_obj_t* obj,char* sym,char* val);

int scc_roobj_obj_set_int_param(scc_roobj_obj_t* obj,char* sym,int val);

int scc_roobj_obj_set_class(scc_roobj_obj_t* obj, scc_symbol_t* sym);
