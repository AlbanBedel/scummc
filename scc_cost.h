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
 * @file scc_cost.h
 * @ingroup scumm
 * @brief SCUMM costume parser and decoder
 */

#define SCC_WEST  0
#define SCC_EAST  1
#define SCC_SOUTH 2
#define SCC_NORTH 3

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
  unsigned id;

  uint8_t format;
  uint8_t flags;
  uint8_t pal_size;
  uint8_t* pal;
  uint16_t cmds_size;
  uint8_t* cmds; // finish with 0xFF
  
  scc_cost_pic_t* limb_pic[16];

  scc_cost_anim_t* anims;
};

typedef struct scc_cost_dec {
  scc_cost_t* cost;
  scc_cost_anim_t* anim;
  unsigned anim_id;

  uint16_t pc[16]; // limb pc
  uint8_t stopped;
} scc_cost_dec_t;


scc_cost_anim_t* scc_cost_new_anim(scc_cost_t* cost,uint8_t id);

scc_cost_anim_t* scc_cost_get_anim(scc_cost_t* cost,uint8_t id);

int scc_cost_add_pic(scc_cost_t* cost,uint8_t limb,scc_cost_pic_t* pic);

int scc_cost_decode_pic(scc_cost_t* cost,scc_cost_pic_t* pic,
			uint8_t* dst,int dst_stride, 
			int x_min,int x_max,int y_min,int y_max,
			int trans, int x_scale, int y_scale,
			int y_flip);

int scc_read_cost_pic(scc_fd_t* fd,scc_cost_t* cost,scc_cost_pic_t* pic,int len,int* posp);

scc_cost_pic_t* scc_cost_get_limb_pic(scc_cost_t* cost,uint8_t limb, 
				      uint8_t pic,int max_depth);

scc_cost_t* scc_parse_cost(scc_fd_t* fd,int len);

void scc_cost_dec_init(scc_cost_dec_t* dec);

int scc_cost_dec_load_anim(scc_cost_dec_t* dec,uint16_t aid);

int scc_cost_dec_step(scc_cost_dec_t* dec);

int scc_cost_dec_bbox(scc_cost_dec_t* dec,int* x1p,int* y1p,
		      int* x2p,int* y2p);

int scc_cost_dec_frame(scc_cost_dec_t* dec,uint8_t* dst,
		       int x, int y,
		       int dst_width, int dst_height,
		       int dst_stride,
		       int x_scale, int y_scale);
