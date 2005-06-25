
typedef struct scc_roobj_res_st scc_roobj_res_t;
typedef struct scc_roobj_state_st scc_roobj_state_t;
typedef struct scc_roobj_obj_st scc_roobj_obj_t;
typedef struct scc_roobj_st scc_roobj_t;

struct scc_roobj_res_st {
  scc_roobj_res_t* next;

  scc_symbol_t* sym;
  uint8_t* data;
  unsigned data_len;
};

struct scc_roobj_state_st {
  scc_roobj_state_t* next;

  // hotspot
  int hs_x,hs_y;
  scc_img_t* img;
  scc_img_t* zp[SCC_MAX_IM_PLANES];
};  

struct scc_roobj_obj_st {
  scc_roobj_obj_t* next;

  scc_symbol_t* sym;
  int hotspots[SCC_MAX_IM_PLANES+1];
  int x,y,w,h;
  int flags;
  int dir;
  char* name;

  // hotspot used if no extra state is defined
  int hs_x,hs_y;
  // state have image, zplanes and hotspot
  scc_roobj_state_t* states;
  // verbs
  scc_script_t* verb;
  // generated imnn
  scc_imnn_t* im;
};

struct scc_roobj_st {
  scc_roobj_t* next; // chain when we have several room

  scc_symbol_t* sym; // our sym
  scc_symbol_t* cur; // currently parsed sym

  scc_script_t* scr;
  scc_script_t* lscr;

  scc_roobj_obj_t* obj;

  // the defined ressources.
  scc_roobj_res_t *res,*last_res;

  // cycles: TODO may need gramm. extension
  // transparent color
  int trans;
  // room image
  scc_img_t* image;
  scc_img_t* zplane[SCC_MAX_IM_PLANES];
  // boxd
  scc_boxd_t* boxd;
  // boxm
  scc_data_t* boxm;
  // scal
  scc_data_t* scal;
};

scc_roobj_t* scc_roobj_new(scc_symbol_t* sym);

scc_roobj_res_t* scc_roobj_get_res(scc_roobj_t* ro,scc_symbol_t* sym);

scc_script_t* scc_roobj_get_scr(scc_roobj_t* ro, scc_symbol_t* sym);

int scc_roobj_add_scr(scc_roobj_t* ro,scc_script_t* scr);

scc_roobj_obj_t* scc_roobj_get_obj(scc_roobj_t* ro,scc_symbol_t* sym);

int scc_roobj_add_obj(scc_roobj_t* ro,scc_roobj_obj_t* obj);

scc_roobj_res_t* scc_roobj_add_res(scc_roobj_t* ro,scc_symbol_t* sym, 
				   char* val);

int scc_roobj_set_param(scc_roobj_t* ro,scc_ns_t* ns,char* p,int idx, char* val);

int scc_roobj_write(scc_roobj_t* ro,scc_ns_t* ns, scc_fd_t* fd);


scc_roobj_obj_t* scc_roobj_obj_new(scc_symbol_t* sym);

int scc_roobj_obj_add_state(scc_roobj_obj_t* obj,int x, int y,
			    char *img_path,char** zp_paths);

scc_script_t* scc_roobj_obj_get_verb(scc_roobj_obj_t* obj,scc_symbol_t* sym);

int scc_roobj_obj_add_verb(scc_roobj_obj_t* obj,scc_script_t* scr);

int scc_roobj_obj_set_param(scc_roobj_obj_t* obj,char* sym,char* val);

int scc_roobj_obj_set_int_param(scc_roobj_obj_t* obj,char* sym,int val);
