
typedef struct scc_cost_pic scc_cost_pic_t;
struct scc_cost_pic {
  scc_cost_pic_t* next;

  uint8_t id;

  uint16_t width,height;
  int16_t rel_x,rel_y;
  int16_t move_x,move_y;
  uint8_t redir_limb,redir_pic;
  uint8_t* data;
  uint32_t data_size;
};

#define SCC_COST_ANIM_LOOP 0x1
#define SCC_COST_HAS_SIZE 0x1

typedef struct scc_cost_anim_def {
  uint16_t start;
  uint16_t end;
  uint8_t flags;
} scc_cost_anim_def_t;

typedef struct scc_cost_anim scc_cost_anim_t;
struct scc_cost_anim {
  scc_cost_anim_t* next;

  uint8_t id;
  uint8_t redir;
  uint16_t mask;
  scc_cost_anim_def_t limb[16];
};



typedef struct scc_cost scc_cost_t;
struct scc_cost {
  scc_cost_t* next;

  uint8_t format;
  uint8_t flags;
  uint8_t pal_size;
  uint8_t* pal;
  uint16_t cmds_size;
  uint8_t* cmds; // finish with 0xFF
  
  scc_cost_pic_t* limb_pic[16];

  scc_cost_anim_t* anims;
};

scc_cost_anim_t* scc_cost_new_anim(scc_cost_t* cost,uint8_t id);

int scc_cost_add_pic(scc_cost_t* cost,uint8_t limb,scc_cost_pic_t* pic);

int scc_cost_decode_pic(scc_cost_t* cost,scc_cost_pic_t* pic,
			uint8_t* dst,int dst_stride, 
			int x_min,int x_max,int y_min,int y_max,
			int trans);

int scc_read_cost_pic(scc_fd_t* fd,scc_cost_t* cost,scc_cost_pic_t* pic,int len,int* posp);

scc_cost_pic_t* scc_cost_get_limb_pic(scc_cost_t* cost,uint8_t limb, 
				      uint8_t pic,int max_depth);

scc_cost_t* scc_parse_cost(scc_fd_t* fd,int len);
