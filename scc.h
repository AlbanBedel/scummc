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

#define OF_STATE_SHL 4
#define OF_OWNER_MASK 0x0F

typedef struct scc_res_list {
  uint32_t size;
  uint8_t* room_no;
  uint32_t* room_off;
} scc_res_list_t;

typedef struct scc_aary scc_aary_t;
struct scc_aary {
  scc_aary_t *next;
  uint16_t idx;
  uint16_t a,b,c;
};
  

typedef struct scc_res_idx {
  uint8_t rnam;
  uint32_t num_vars;
  uint32_t unk1;
  uint32_t num_bit_vars;
  uint32_t num_local_objs;
  uint32_t num_new_names;
  uint32_t num_array;
  uint32_t unk2;
  uint32_t num_verbs;
  uint32_t num_fl_objs;
  uint32_t num_inv;
  uint32_t num_rooms;
  uint32_t num_scr;
  uint32_t num_snds;
  uint32_t num_chst;
  uint32_t num_cost;
  uint32_t num_glob_objs;
  uint32_t num_glob_scr;
  uint32_t shd_pal_size;

  scc_res_list_t* room_list;
  scc_res_list_t* scr_list;
  scc_res_list_t* snd_list;
  scc_res_list_t* cost_list;
  scc_res_list_t* chst_list;

  uint32_t num_obj_owners;
  uint8_t* obj_owners;
  uint32_t* class_data;

  scc_aary_t* aary;
} scc_res_idx_t;

typedef struct scc_data_list scc_data_list_t;
struct scc_data_list {
  scc_data_list_t *next;
  uint32_t idx;
  uint32_t size;
  uint8_t data[0];
};

typedef struct scc_cycl scc_cycl_t;
struct scc_cycl {
  scc_cycl_t *next;
  uint8_t idx;
  uint16_t unk;
  uint16_t freq;
  uint16_t flags;
  uint8_t start;
  uint8_t end;
};

typedef struct scc_pal scc_pal_t;
struct scc_pal {
  scc_pal_t *next;
  uint8_t r[256];
  uint8_t g[256];
  uint8_t b[256];
};

typedef struct scc_rmim {
  uint16_t num_z_buf;
  uint8_t* smap;
  uint32_t smap_size;
  uint8_t* z_buf[SCC_MAX_IM_PLANES];
  uint32_t z_buf_size[SCC_MAX_IM_PLANES];
} scc_rmim_t;

typedef struct scc_imnn scc_imnn_t;
struct scc_imnn {
  scc_imnn_t *next;
  uint8_t idx;
  uint8_t* smap;
  uint32_t smap_size;
  uint8_t* z_buf[SCC_MAX_IM_PLANES];
  uint32_t z_buf_size[SCC_MAX_IM_PLANES];
};

typedef struct scc_obim scc_obim_t;
struct scc_obim {
  scc_obim_t *next;
  uint16_t idx;
  uint16_t num_imnn, num_zpnn;
  uint16_t unk;
  uint16_t x,y;
  uint16_t width,height;
  uint16_t num_hotspots;
  int16_t *hotspots; // x,y pairs
  scc_imnn_t * img; 
};

typedef struct scc_verb scc_verb_t;
struct scc_verb {
  scc_verb_t* next;
  // if code_idx is non zero redirect to it.
  uint8_t idx,code_idx;
  uint16_t len;
  uint8_t* code;
};

typedef struct scc_obcd scc_obcd_t;
struct scc_obcd {
  scc_obcd_t *next;
  uint16_t idx;
  uint16_t x,y;
  uint16_t width,height;
  uint8_t flags;
  uint8_t parent;
  uint16_t unk1,unk2;
  uint8_t actor_dir;
  scc_verb_t* verb;
  char* name;
};

#define SCC_BOX_INVISIBLE 0x80

typedef struct scc_boxd scc_boxd_t;
struct scc_boxd {
  scc_boxd_t* next;

  int16_t ulx,uly;
  int16_t urx,ury;
  int16_t lrx,lry;
  int16_t llx,lly;
  uint8_t mask;
  uint8_t flags;
  uint16_t scale;
};

typedef struct scc_room {
  uint8_t idx;  // RMHD
  uint32_t width,height;
  uint32_t num_objs;

  scc_cycl_t *cycl;           // CYCL
  uint16_t transp;            // TRNS
  scc_pal_t *pals;            // PALS
  scc_rmim_t *rmim;           // RMIM
  scc_obim_t *obim;           // OBIM
  scc_obcd_t *obcd;           // OBCD
  scc_data_t *excd;           // EXCD
  scc_data_t *encd;           // ENCD
  uint16_t nlsc;              // NLSC
  scc_data_list_t* lscr;      // LSCR
  scc_boxd_t *boxd;           // BOXD
  uint8_t **boxm;             // BOXM
  scc_data_t *scal;           // SCAL
} scc_room_t;


typedef struct scc_lfl scc_lfl_t;
struct scc_lfl {
  scc_lfl_t *next,*prev;
  scc_room_t room;

  scc_data_list_t* scr;
  scc_data_list_t* snd;
  scc_cost_t* cost;
  scc_data_list_t* chr;

};

typedef struct scc_res {
  scc_fd_t* fd;
  scc_res_idx_t* idx;
  scc_lfl_t* lfl;
} scc_res_t;

// write.h

// compute the size of various elements of the data files
int scc_res_list_size(scc_res_list_t* list);

int scc_aary_size(scc_aary_t* aary);

int scc_loff_size(scc_res_idx_t* idx);

int scc_cycl_size(scc_cycl_t* c);

int scc_pals_size(scc_pal_t* p);

int scc_rmim_size(scc_rmim_t* rmim);

int scc_imnn_size(scc_imnn_t* i);

int scc_obim_size(scc_obim_t* obim);

int scc_obcd_size(scc_obcd_t* obcd);

int scc_boxd_size(scc_boxd_t* boxd);

int scc_room_size(scc_room_t* r);

int scc_cost_size(scc_cost_t* c);

int scc_lfl_size(scc_lfl_t* l);

int scc_lecf_size(scc_res_t* r);

// writing data files
int scc_write_data_list(scc_fd_t* fd,uint32_t type,scc_data_list_t* l);

int scc_write_data(scc_fd_t* fd,uint32_t type,scc_data_t* d);

int scc_write_cycl(scc_fd_t* fd,scc_cycl_t* c);

int scc_write_pals(scc_fd_t* fd,scc_pal_t* p);

int scc_write_rmim(scc_fd_t* fd,scc_rmim_t* rmim);

int scc_write_imnn(scc_fd_t* fd,scc_imnn_t* img);

int scc_write_obim(scc_fd_t* fd,scc_obim_t* im);

int scc_write_obcd(scc_fd_t* fd,scc_obcd_t* o);

int scc_write_boxd(scc_fd_t* fd,scc_boxd_t* b);

int scc_write_room(scc_fd_t* fd,scc_room_t* room);

int scc_write_lfl(scc_fd_t* fd,scc_lfl_t* lfl);

// the main block of a data file
int scc_write_lecf(scc_fd_t* fd,scc_res_t* r);

// for the index file
int scc_write_res_list(scc_fd_t* fd,uint32_t type,scc_res_list_t* list);
// write the index file
int scc_write_res_idx(scc_fd_t* fd,scc_res_idx_t* idx);


// read.c

// load up an index file
scc_res_idx_t* scc_read_res_idx(scc_fd_t* fd);


// open a data file
scc_res_t* scc_open_res_file(scc_fd_t* fd,scc_res_idx_t* idx);

// random access to a ressource (type) in a given room (idx)
int scc_res_find(scc_res_t* res,uint32_t type,uint32_t idx);

// open and load a data file
scc_res_t* scc_parse_res(scc_fd_t* fd,scc_res_idx_t* idx);

// dump some stuff to various file, to be finished
// it should sort of untar the whole file
void scc_dump_res(scc_res_t* res);

// parse a pals block
scc_pal_t* scc_parse_pals(scc_fd_t* fd,int len);

// parse a boxd block
scc_boxd_t* scc_parse_boxd(scc_fd_t* fd,int len);

// parse a costume
scc_cost_t* scc_parse_cost(scc_fd_t* fd,int len);



