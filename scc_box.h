/* ScummC
 * Copyright (C) 2004-2007  Alban Bedel
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
 * @file scc_box.h
 * @brief Common box stuff
 */

typedef struct scc_box_pts_st {
  int x,y;
  int selected;
} scc_box_pts_t;

typedef struct scc_scale_slot_st {
  int s1,y1;
  int s2,y2;
} scc_scale_slot_t;

#define SCC_BOX_INVISIBLE 0x80

typedef struct scc_box_st scc_box_t;
struct scc_box_st {
  scc_box_t* next;

  int npts,nsel;
  scc_box_pts_t pts[4];
  uint8_t mask;
  uint8_t flags;
  uint16_t scale;
  char* name;
};

void scc_box_list_free(scc_box_t* box);

int scc_box_add_pts(scc_box_t* box,int x,int y);

int scc_box_are_neighbors(scc_box_t* box,int n1,int n2);

int scc_box_get_matrix(scc_box_t* box,uint8_t** ret);

