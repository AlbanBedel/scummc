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
 * @file boxedit.c
 * @brief Gtk based box editor
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"
#include "scc.h"
#include "scc_param.h"
#include "scc_img.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

typedef struct scc_box_pts_st {
  int x,y;
  int selected;
} scc_box_pts_t;

typedef struct scc_scale_slot_st {
  int s1,y1;
  int s2,y2;
} scc_scale_slot_t;

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

#define SCC_UNDO_NEW_PTS           1
#define SCC_UNDO_SELECT_PTS        2
#define SCC_UNDO_UNSELECT_PTS      3
#define SCC_UNDO_SELECT_NEXT_PTS   4
#define SCC_UNDO_NEW_BOX           5
#define SCC_UNDO_SELECT_BOX        6
#define SCC_UNDO_UNSELECT_BOX      7
#define SCC_UNDO_SELECT_NEXT_BOX   8
#define SCC_UNDO_MOVE              9
#define SCC_UNDO_UNSELECT_ALL     10
#define SCC_UNDO_DESTROY          11
#define SCC_UNDO_FLAG             12
#define SCC_UNDO_NAME             13
#define SCC_UNDO_SCALE            14
#define SCC_UNDO_MASK             15
#define SCC_UNDO_UNDO             16

typedef struct scc_boxedit_undo_st scc_boxedit_undo_t;
struct scc_boxedit_undo_st {
  scc_boxedit_undo_t* next;
  int op;

  scc_box_t* box;
  scc_box_t* box_b;
  scc_box_t* box_c;
  int pts[4];
  int undone;
};

#define SCC_NUM_SCALE_SLOT 4

typedef struct scc_boxedit_st {
  // box data
  scc_box_t* boxes;
  // scale slots
  scc_scale_slot_t scale_slots[SCC_NUM_SCALE_SLOT];
  // room image
  scc_img_t* rmim;
  // the view
  uint8_t* view_data;
  int view_w,view_h;
  int  view_off_x,view_off_y;
  int zoom;

  // the main gtk window
  GtkWidget* win;
  // the drawing area
  GtkWidget* da;
  // the da scrollbox, we need bcs it take the key events
  GtkWidget* da_scroll;
  // the status bar
  GtkWidget* sbar;
  unsigned int sbar_ctx_coord;
  unsigned int sbar_ctx_msg;
  int sbar_have_msg;
  
  // the scale slots window
  GtkWidget* scale_win;

  // the box entries
  scc_box_t* edit_box;
  GtkWidget* name_entry;
  GtkWidget* mask_entry;
  GtkWidget* scale_val;
  GtkWidget* scale_slot;
  GtkWidget* scale_slot_entries[4*SCC_NUM_SCALE_SLOT];
  GtkWidget* scale_entry;
  GtkWidget* flag_toggle[8];
  

  int hide_plane[SCC_MAX_IM_PLANES];

  // gc stuff
  GdkGC* img_gc;
  GdkRgbCmap *img_cmap;
  GdkColor box_color;  // box colors
  GdkColor pts_color;  // point color
  GdkColor sel_color;  // selected element color
  GdkColor edit_color; // edit box color

  int nsel;
  int drag,drag_x,drag_y;
  int drag_move_x,drag_move_y;

  // undo
  scc_boxedit_undo_t* undo;
  // undo level at last save
  scc_boxedit_undo_t* last_save;
  int changed;

  // current file
  char* filename;
  int save_as;
  int open_img;
} scc_boxedit_t;

void scc_box_list_free(scc_box_t* box) {
  scc_box_t* n;
  while(box) {
    n = box->next;
    free(box);
    box = n;
  }
}

int scc_box_add_pts(scc_box_t* box,int x,int y) {
  int i;
  if(box->npts >= 4) return 0;

  // look if don't alredy have this point
  for(i = 0 ; i < box->npts ; i++)
    if(box->pts[i].x == x && box->pts[i].y == y) return 1;

  box->pts[i].x = x;
  box->pts[i].y = y;

  box->npts++;
  return 1;
}

static int scc_box_are_neighbors(scc_box_t* box,int n1,int n2) {
  scc_box_t *a = NULL,*b = NULL,*c;
  int n,m;
  int i,j;
  int f,f2,l,l2;

  // box 0 is connected to nothing
  if((!n1) || (!n2)) return 0;

  for(n = 1, c = box ; c ; n++,c = c->next) {
    if(n == n1) a = c;
    if(n == n2) b = c;
  }

  if(!(a && b)) {
    printf("Failed to find boxes %d and/or %d.\n",n1,n2);
    return 0;
  }

  if((a->flags & SCC_BOX_INVISIBLE) || (b->flags & SCC_BOX_INVISIBLE))
    return 0;

  for(n = 0 ; n < a->npts ; n++) {
    m = (n+1)%a->npts;
    // is it an vertical side
    if(a->pts[n].x == a->pts[m].x) {
      // look at tho other box
      for(i = 0 ; i < b->npts ; i++) {
	j = (i+1)%b->npts;
	// ignore side which are not on the same vertical
	if((b->pts[i].x != a->pts[n].x) ||
	   (b->pts[j].x != a->pts[n].x)) continue;

	if(a->pts[n].y < a->pts[m].y)
	  f = n, l = m;
	else
	  f = m, l = n;

	if(b->pts[i].y < b->pts[j].y)
	  f2 = i, l2 = j;
	else
	  f2 = j, l2 = i;

	if((a->pts[l].y < b->pts[f2].y) || 
	   (a->pts[f].y > b->pts[l2].y)) continue;
	return 1;
      }
      
    }
    // is it an horizontal line
    if(a->pts[n].y == a->pts[m].y) {
      // look at the other box
      for(i = 0 ; i < b->npts ; i++) {
	j = (i+1)%b->npts;
	if((b->pts[i].y != a->pts[n].y) ||
	   (b->pts[j].y != a->pts[n].y)) continue;

	if(a->pts[n].x < a->pts[m].x)
	  f = n, l = m;
	else
	  f = m, l = n;

	if(b->pts[i].x < b->pts[j].x)
	  f2 = i, l2 = j;
	else
	  f2 = j, l2 = i;

	if((a->pts[l].x < b->pts[f2].x) || 
	   (a->pts[f].x > b->pts[l2].x)) continue;
	return 1;
      }
    }
  }
  return 0;
}

// code pumped from scummvm
// changed to make it work with any matrix size
int scc_box_get_matrix(scc_box_t* box,uint8_t** ret) {
  scc_box_t* b;
  int i,j,k,num = 0;
  uint8_t *adjacentMatrix, *itineraryMatrix;
  
  // count the boxes
  for(b = box ; b ; b = b->next) num++;
  // add the box 0
  num++;

  // Allocate the adjacent & itinerary matrices
  adjacentMatrix = malloc(num * num);
  itineraryMatrix = malloc(num * num);

  // Initialise the adjacent matrix: each box has distance 0 to itself,
  // and distance 1 to its direct neighbors. Initially, it has distance
  // 255 (= infinity) to all other boxes.
  for (i = 0; i < num; i++) {
    for (j = 0; j < num; j++) {
      if (i == j) {
	adjacentMatrix[i*num+j] = 0;
	itineraryMatrix[i*num+j] = j;
      } else if (scc_box_are_neighbors(box,i,j)) {
	adjacentMatrix[i*num+j] = 1;
	itineraryMatrix[i*num+j] = j;
      } else {
	adjacentMatrix[i*num+j] = 255;
	itineraryMatrix[i*num+j] = 255;
      }
    }
  }

  // Compute the shortest routes between boxes via Kleene's algorithm.
  for (k = 0; k < num; k++) {
    for (i = 0; i < num; i++) {
      for (j = 0; j < num; j++) {
	uint8_t distIK,distKJ;
	if (i == j)
	  continue;
	distIK = adjacentMatrix[num*i+k];
	distKJ = adjacentMatrix[num*k+j];
	if (adjacentMatrix[num*i+j] > distIK + distKJ) {
	  adjacentMatrix[num*i+j] = distIK + distKJ;
	  itineraryMatrix[num*i+j] = itineraryMatrix[num*i+k];
	}
      }
    }  
  }

  free(adjacentMatrix);
  ret[0] = itineraryMatrix;
  return num;
}

static int scc_boxm_size_from_matrix(uint8_t* matrix,int size) {
  int i,j,pos = 0;

  for(i = 0 ; i < size ; i++) {
    pos++;
    for(j = 0 ; j < size ; j++) {
      uint8_t v = matrix[i*size+j];
      if(v == 255) continue;
      while(j < size-1 && v == matrix[j]) j++;
      pos += 3;
    }
  }
  pos++;
  return pos;
}

int scc_boxedit_read_scale(scc_boxedit_t* be,
                           scc_fd_t* fd) {
  uint32_t type,len;
  int i;

  while(1) {
    type = scc_fd_r32(fd);
    len = scc_fd_r32be(fd);

    if(type == MKID('B','O','X','M') && len > 8) {
      scc_fd_seek(fd,len-8,SEEK_CUR);
      continue;
    }
    if(type == MKID('S','C','A','L'))
      break;
    return 0;
  }

  if(len != SCC_NUM_SCALE_SLOT*8+8) {
    printf("SCAL block has an invalid length: %d, should be 40.\n",len);
    return 0;
  }
  
  for(i = 0 ; i < SCC_NUM_SCALE_SLOT ; i++) {
    be->scale_slots[i].s1 = scc_fd_r16le(fd);
    be->scale_slots[i].y1 = scc_fd_r16le(fd);
    be->scale_slots[i].s2 = scc_fd_r16le(fd);
    be->scale_slots[i].y2 = scc_fd_r16le(fd);
  }
  return 1;
}

scc_box_t* scc_boxedit_load_box(scc_boxedit_t* be, char* path) {
  scc_box_t* list=NULL,*last=NULL,*b;
  scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
  uint32_t type,len,pos = 8;
  int nlen,x,y;

  if(!fd) {
    printf("Failed to open %s.\n",path);
    return NULL;
  }

  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);

  if((type != MKID('B','O','X','D')) && (type != MKID('b','o','x','d'))) {
    printf("%s is not a boxd file.\n",path);
    scc_fd_close(fd);
    return NULL;
  }

  // skip the first box on the original format
  if(type == MKID('B','O','X','D')) {
    scc_fd_r16le(fd); pos += 2;
    scc_fd_seek(fd,20,SEEK_CUR);
    pos += 20;
  }

  while(pos < len) {
    b = calloc(1,sizeof(scc_box_t));
    if(type == MKID('b','o','x','d')) {
      nlen = scc_fd_r8(fd); pos++;
      if(nlen > 0) {
	b->name = malloc(nlen+1);
	scc_fd_read(fd,b->name,nlen); pos += nlen;
	b->name[nlen] = '\0';
      }
    }
    x = (int16_t)scc_fd_r16le(fd); pos += 2;
    y = (int16_t)scc_fd_r16le(fd); pos += 2;
    scc_box_add_pts(b,x,y);
    x = (int16_t)scc_fd_r16le(fd); pos += 2;
    y = (int16_t)scc_fd_r16le(fd); pos += 2;
    scc_box_add_pts(b,x,y);
    x = (int16_t)scc_fd_r16le(fd); pos += 2;
    y = (int16_t)scc_fd_r16le(fd); pos += 2;
    scc_box_add_pts(b,x,y);
    x = (int16_t)scc_fd_r16le(fd); pos += 2;
    y = (int16_t)scc_fd_r16le(fd); pos += 2;
    scc_box_add_pts(b,x,y);
    b->mask = scc_fd_r8(fd); pos++;
    b->flags = scc_fd_r8(fd); pos++;
    b->scale = scc_fd_r16le(fd); pos += 2;
    SCC_LIST_ADD(list,last,b);


#if 0
    printf("load box:");
    for(x = 0 ; x < b->npts ; x++)
      printf(" (%dx%d)",b->pts[x].x,b->pts[x].y);
    printf("\n");
#endif
  }

  // try to read scal, it might be there
  scc_boxedit_read_scale(be,fd);

  scc_fd_close(fd);
  
  if(be->filename) free(be->filename);
  be->filename = strdup(path);
  if(be->boxes) scc_box_list_free(be->boxes);
  be->boxes = list;
  
  return list;
}

int scc_box_size(scc_box_t* box) {
  int size = 0;

  while(box) {
    size += 1;
    if(box->name) size += strlen(box->name);
    size += 2*8 + 1 + 1 + 2;
    box = box->next;
  }
  return size;
}

int scc_boxedit_save(scc_boxedit_t* be, char* path) {
  scc_fd_t* fd;
  scc_box_t* box;
  int i,j,up,up2;
  int len;
  uint8_t* boxm;

  if(!be->boxes) {
    printf("No box to save.\n");
    return 0;
  }

  fd = new_scc_fd(path,O_WRONLY|O_CREAT|O_TRUNC,0);
  if(!fd) {
    printf("Failed to open %s\n",path);
    return 0;
  }

  scc_fd_w32(fd,MKID('b','o','x','d'));
  scc_fd_w32be(fd,8 + scc_box_size(be->boxes));

  for(box = be->boxes ; box ; box = box->next) {
    if(!box->name)
      scc_fd_w8(fd,0);
    else {
      scc_fd_w8(fd,strlen(box->name));
      scc_fd_write(fd,box->name,strlen(box->name));
    }

    // find the top point
    up = 0;
    for(i = 1 ; i < box->npts ; i++)
      if(box->pts[i].y < box->pts[up].y) up = i;
    // find the 2 top point
    up2 = -1;
    for(i = 0 ; i < box->npts ; i++) {
      if(i == up) continue;
      if(up2 == -1 || box->pts[i].y < box->pts[up2].y) up2 = i;
    }
    // take the leftest, note that if both are equal we still keep the
    // highest
    if(box->pts[up2].x < box->pts[up].x) up = up2;

    if(box->npts == 2) {
      for(i = 0 ; i < 2 ; i++) {
	scc_fd_w16le(fd,box->pts[up].x);
	scc_fd_w16le(fd,box->pts[up].y);
      }
      up = (up+1)%box->npts;
      for(i = 0 ; i < 2 ; i++) {
	scc_fd_w16le(fd,box->pts[up].x);
	scc_fd_w16le(fd,box->pts[up].y);
      }
    } else {
      for(i = up ; i < up + 4 ; i++) {
	scc_fd_w16le(fd,box->pts[i%box->npts].x);
	scc_fd_w16le(fd,box->pts[i%box->npts].y);
      }
    }

    scc_fd_w8(fd,box->mask);
    scc_fd_w8(fd,box->flags);
    scc_fd_w16le(fd,box->scale);
  }

  // boxm generation
  len = scc_box_get_matrix(be->boxes,&boxm);
#if 0
  for(i = 0 ; i < len ; i++) {
    printf("Box %2d :",i);
    for(j = 0 ; j < len ; j++)
      printf(" %d",boxm[i*len+j]);
    printf("\n");
  }
#endif

  scc_fd_w32(fd,MKID('B','O','X','M'));
  scc_fd_w32be(fd,8 + scc_boxm_size_from_matrix(boxm,len));

  for(i = 0 ; i < len ; i++) {
    scc_fd_w8(fd,0xFF);
    for(j = 0 ; j < len ; j++) {
      uint8_t v = boxm[i*len+j];
      if(v == 255) continue;
      scc_fd_w8(fd,j);
      while(j < len-1 && v == boxm[j]) j++;
      scc_fd_w8(fd,j);
      scc_fd_w8(fd,v);
    }
  }
  scc_fd_w8(fd,0xFF);

  // scal
  scc_fd_w32(fd,MKID('S','C','A','L'));
  scc_fd_w32be(fd,8 + 8*SCC_NUM_SCALE_SLOT);
  
  for(i = 0 ; i < SCC_NUM_SCALE_SLOT ; i++) {
    scc_fd_w16le(fd,be->scale_slots[i].s1);
    scc_fd_w16le(fd,be->scale_slots[i].y1);
    scc_fd_w16le(fd,be->scale_slots[i].s2);
    scc_fd_w16le(fd,be->scale_slots[i].y2);
  }

  scc_fd_close(fd);
  return 1;
}


#define SWAP(a,b) t = a; a = b; b = t;

static int is_near_line(scc_box_pts_t* a,scc_box_pts_t* b, int x, int y) {
  float s;
  int th = 4,r = 0,c,z,w,swap = 0;
  
  if(abs(a->x - b->x) < abs(a->y - b->y)) {
    int t;
    swap = 1;
    SWAP(a->x,a->y);
    SWAP(b->x,b->y);
    SWAP(x,y);
  }

  if(a->x > b->x) {
    scc_box_pts_t* t;
    SWAP(a,b);
  }

  do {
    if(x < a->x - th || x > b->x + th) break;

    if(a->x == b->x) {
      if(a->y < b->y) {
	if(y < a->y - th || y > b->y + th) break;
      } else {
	if(y < b->y - th || y > a->y + th) break;
      }
      r = 1;
      break;
    }
    
    s = ((float)(b->y-a->y))/(b->x-a->x);
    c = a->y - s*a->x;
    
    w = x*s+c;
    z = sqrt((s*s)+1);
    
    if(abs((y-w)*z) > th) break;
    r = 1;
  } while(0);
  
  if(swap) {
    int t;
    SWAP(a->x,a->y);
    SWAP(b->x,b->y);
  }
  return r;
}

static int is_near_point(scc_box_pts_t* a,int x, int y,int th) {
  int dx = a->x - x;
  int dy = a->y - y;
  if(th < 1) th = 1;
  return (sqrt((dx*dx)+(dy*dy)) > th) ? 0 : 1;
}

static int is_box_visible(scc_boxedit_t* be, scc_box_t* box) {
  int m = box->mask;
  if(m < 0) m = 0;
  if(m >= SCC_MAX_IM_PLANES) m = SCC_MAX_IM_PLANES-1;
  return be->hide_plane[m] ? 0 : 1;  
}

static int can_move_box(scc_box_t* box,int x, int y) {
  scc_box_pts_t pts[4];
  int i;

  // it's not working properly and a bit useless currently
  // so disable it for now
  return 1;

  // non-square box can't be bad
  if(box->npts < 4) return 1;

  for(i = 0 ; i < box->npts ; i++) {
    pts[i] = box->pts[i];
    if(!box->pts[i].selected) continue;
    pts[i].x += x;
    pts[i].y += y;
  }

  if(pts[0].x > pts[1].x || pts[1].y > pts[2].y ||
     pts[2].x < pts[3].x || pts[3].y < pts[0].y) return 0;
  
  return 1;
}

static void activate_name_cb(GtkEntry *entry,scc_boxedit_t* be);

static void scc_boxedit_set_edit_box(scc_boxedit_t* be, scc_box_t* box) {
  int i;

  if(be->edit_box == box) return;

  // it's set to null so the callback doesn't touch it
  be->edit_box = NULL;

  if(!box) {
    gtk_widget_set_sensitive(be->name_entry,0);
    gtk_entry_set_text(GTK_ENTRY(be->name_entry),"");
    gtk_widget_set_sensitive(be->mask_entry,0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->mask_entry),0);
    gtk_widget_set_sensitive(be->scale_entry,0);
    gtk_widget_set_sensitive(be->scale_val,0);
    gtk_widget_set_sensitive(be->scale_slot,0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_entry),0);
    for(i = 3 ; i < 8 ; i++) {
      gtk_widget_set_sensitive(be->flag_toggle[i],0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->flag_toggle[i]),0);
    }
    return;
  }
  gtk_widget_set_sensitive(be->name_entry,1);
  gtk_entry_set_text(GTK_ENTRY(be->name_entry),box->name ? box->name : "");
  gtk_widget_set_sensitive(be->mask_entry,1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->mask_entry),box->mask);
  gtk_widget_set_sensitive(be->scale_entry,1);
  gtk_widget_set_sensitive(be->scale_val,1);
  gtk_widget_set_sensitive(be->scale_slot,1);
  if(box->scale & 0x8000) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->scale_slot),1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),0,3);
  } else {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->scale_val),1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),1,255);
  }
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_entry),box->scale & 0xFF);
  for(i = 3 ; i < 8 ; i++) {
    gtk_widget_set_sensitive(be->flag_toggle[i],1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->flag_toggle[i]),box->flags & (1 << i));
  }
  be->edit_box = box;
}

static void scc_boxedit_status_msg(scc_boxedit_t* be,char* msg, ...) {
  char txt[1024];
  va_list ap;

  va_start(ap, msg);
  vsprintf(txt,msg,ap);
  va_end(ap);

  if(be->sbar_have_msg)
    gtk_statusbar_pop(GTK_STATUSBAR(be->sbar),be->sbar_ctx_msg);
  gtk_statusbar_push(GTK_STATUSBAR(be->sbar),be->sbar_ctx_msg,txt);
  be->sbar_have_msg = 1;
}

static int scc_boxedit_sel_pts(scc_boxedit_t* be,scc_box_t* box,int p) {

  if(p < 0 || p > 3) return 0;
  
  if(box->pts[p].selected) return 0;
  box->pts[p].selected = 1;
  box->nsel++;
  be->nsel++;

  //if(box->nsel == box->npts || be->edit_box == NULL)
  scc_boxedit_set_edit_box(be,box);

  return 1;
}

static int scc_boxedit_unsel_pts(scc_boxedit_t* be,scc_box_t* box,int p) {

  if(p < 0 || p > 3) return 0;
  
  if(!box->pts[p].selected) return 0;
  box->pts[p].selected = 0;
  box->nsel--;
  be->nsel--;

  if(box == be->edit_box && box->nsel == 0)
    scc_boxedit_set_edit_box(be,NULL);

  return 1;
}

static int scc_boxedit_sel_next_pts(scc_boxedit_t* be,scc_box_t* box_a,int p_a,
				 scc_box_t* box_b,int p_b) {
  if(scc_boxedit_unsel_pts(be,box_a,p_a) &&
     scc_boxedit_sel_pts(be,box_b,p_b)) return 1;
  return 0;
}

static int scc_boxedit_sel_box(scc_boxedit_t* be,scc_box_t* box) {
  int i,r=0;

  for(i = 0 ; i < box->npts ; i++) {
    if(box->pts[i].selected) continue;
    r=1;
    box->pts[i].selected = 1;
    box->nsel++;
    be->nsel++;
  }

  scc_boxedit_set_edit_box(be,box);
    
  return r;
}

static int scc_boxedit_unsel_box(scc_boxedit_t* be,scc_box_t* box) {
  int i,r=0;

  for(i = 0 ; i < box->npts ; i++) {
    if(!box->pts[i].selected) continue;
    r=1;
    box->pts[i].selected = 0;
    box->nsel--;
    be->nsel--;
  }

  if(box == be->edit_box)
    scc_boxedit_set_edit_box(be,NULL);
    
  return r;
}

static int scc_boxedit_sel_next_box(scc_boxedit_t* be,scc_box_t* box_a,
				    scc_box_t* box_b) {
  if(scc_boxedit_unsel_box(be,box_a) &&
     scc_boxedit_sel_box(be,box_b)) return 1;
  return 0;
}

static int scc_boxedit_sel_move(scc_boxedit_t* be,int x, int y) {
  scc_box_t* box;
  int i;

  // first check if the move is valid
  for(box = be->boxes ; box ; box = box->next)
    if(!can_move_box(box,x,y)) return 0;
  
  for(box = be->boxes ; box ; box = box->next) {
    for(i = 0 ; i < box->npts ; i++) {
      if(!box->pts[i].selected) continue;
      box->pts[i].x += x;
      box->pts[i].y += y;
    }
  }

  return 1;
}

static scc_box_t* scc_boxedit_new_box(scc_boxedit_t* be) {
  scc_box_t *b,* box = calloc(1,sizeof(scc_box_t));

  if(be->boxes) {
    for(b = be->boxes ; b->next ; b = b->next);
    b->next = box;
  } else
    be->boxes = box;

  box->npts = 4;
  box->pts[1].x = 10;
  box->pts[2].x = 10;
  box->pts[2].y = 10;
  box->pts[3].y = 10;
  box->scale = 255;

  return box;
}

static scc_boxedit_undo_t* scc_boxedit_get_undo(scc_boxedit_t* be,int op) {
  scc_boxedit_undo_t* undo = calloc(1,sizeof(scc_boxedit_undo_t));
  be->changed = 1;
  undo->op = op;
  undo->next = be->undo;
  be->undo = undo;
  return undo;
}

static int scc_boxedit_undo_action(scc_boxedit_t* be,
				   scc_boxedit_undo_t* undo) {
  int n,end;
  scc_boxedit_undo_t* u2;
  scc_box_t* b;
  char* txt;

  switch(undo->op) {
  case SCC_UNDO_SELECT_PTS:
    if(undo->undone)
      scc_boxedit_sel_pts(be,undo->box,undo->pts[0]);
    else {
      scc_boxedit_unsel_pts(be,undo->box,undo->pts[0]);
      scc_boxedit_set_edit_box(be,undo->box_b);
    }    
    break;

  case SCC_UNDO_UNSELECT_PTS:
    if(undo->undone)
      scc_boxedit_unsel_pts(be,undo->box,undo->pts[0]);
    else {
      scc_boxedit_sel_pts(be,undo->box,undo->pts[0]);
      scc_boxedit_set_edit_box(be,undo->box_b);
    }
    break;

  case SCC_UNDO_SELECT_NEXT_PTS:
    if(undo->undone)
      scc_boxedit_sel_next_pts(be,undo->box,undo->pts[0],
			       undo->box_b,undo->pts[1]);
    else {
      scc_boxedit_sel_next_pts(be,undo->box_b,undo->pts[1],
			       undo->box,undo->pts[0]);
      scc_boxedit_set_edit_box(be,undo->box_c);
    }
    break;

  case SCC_UNDO_NEW_BOX:
    if(undo->undone) {
      // re-add it
      if(!be->boxes || be->boxes == undo->box->next)
	be->boxes = undo->box;
      else {
	for(b = be->boxes ; b->next != undo->box->next ; b = b->next);
	b->next = undo->box;
      }
      scc_boxedit_set_edit_box(be,undo->box);
      be->nsel += 4;
    } else {
      // remove it from the list
      if(undo->box == be->boxes)
	be->boxes = undo->box->next;
      else {
	for(b = be->boxes ; b->next != undo->box ; b = b->next);
	b->next = undo->box->next;
      }
      scc_boxedit_set_edit_box(be,undo->box_b);
      be->nsel -= 4;
      //undo->box->next = NULL;
    }
    break;

  case SCC_UNDO_SELECT_BOX:
    if(undo->undone)
      scc_boxedit_sel_box(be,undo->box);
    else {
      for(n = 0 ; n < 4 && undo->pts[n] >= 0 ; n++)
	scc_boxedit_unsel_pts(be,undo->box,undo->pts[n]);
      scc_boxedit_set_edit_box(be,undo->box_b);
    }
    break;

  case SCC_UNDO_UNSELECT_BOX:
    if(undo->undone)
      scc_boxedit_unsel_box(be,undo->box);
    else {
      for(n = 0 ; n < 4 && undo->pts[n] >= 0 ; n++)
	scc_boxedit_sel_pts(be,undo->box,undo->pts[n]);
      scc_boxedit_set_edit_box(be,undo->box_b);
    }
    break;

  case SCC_UNDO_SELECT_NEXT_BOX:
    if(undo->undone)
      scc_boxedit_sel_next_box(be,undo->box,undo->box_b);
    else {
      scc_boxedit_sel_next_box(be,undo->box_b,undo->box);
      for(n = 0 ; n < 4 && undo->pts[n] >= 0 ; n++)
	scc_boxedit_sel_pts(be,undo->box_b,undo->pts[n]);
      scc_boxedit_set_edit_box(be,undo->box_c);
    }
    break;

  case SCC_UNDO_MOVE:
    if(undo->undone)
      scc_boxedit_sel_move(be,undo->pts[0],undo->pts[1]);
    else
      scc_boxedit_sel_move(be,-undo->pts[0],-undo->pts[1]);
    break;

  case SCC_UNDO_UNSELECT_ALL:
  case SCC_UNDO_DESTROY:
    b = be->boxes;
    be->boxes = undo->box;
    undo->box = b;

    n = be->nsel;
    be->nsel = undo->pts[0];
    undo->pts[0] = n;

    if(be->edit_box || undo->box_b) {
      b = be->edit_box;
      scc_boxedit_set_edit_box(be,undo->box_b);
      undo->box_b = b;
    }
    break;

  case SCC_UNDO_NAME:
    b = be->edit_box;
    be->edit_box = NULL;

    txt = b->name;
    gtk_entry_set_text(GTK_ENTRY(be->name_entry),(undo->box) ? (char*)undo->box : "");
    b->name = (char*)undo->box;
    undo->box = (scc_box_t*)txt;

    be->edit_box = b;
    break;

  case SCC_UNDO_FLAG:
    b = be->edit_box;
    be->edit_box = NULL;
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->flag_toggle[undo->pts[0]]),(b->flags & (1<<undo->pts[0])) ? 0 : 1);
    if(b->flags & (1<<undo->pts[0]))
      b->flags &= ~(1<<undo->pts[0]);
    else
      b->flags |= (1<<undo->pts[0]);

    be->edit_box = b;
    break;

  case SCC_UNDO_SCALE:
    n = be->edit_box->scale;
    b = be->edit_box;
    be->edit_box = NULL;

    b->scale = undo->pts[0];
    undo->pts[0] = n;

    if(b->scale & 0x8000) {
      if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(be->scale_slot))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->scale_slot),1);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),0,3);
      }
    } else {
      if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(be->scale_val))) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(be->scale_val),1);
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),1,255);
      }
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_entry),b->scale & 0xFF);

    be->edit_box = b;
    break;

  case SCC_UNDO_MASK:
    n = be->edit_box->mask;
    b = be->edit_box;
    be->edit_box = NULL;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->mask_entry),undo->pts[0]);
    b->mask = undo->pts[0];
    undo->pts[0] = n;

    be->edit_box = b;
    break;

  case SCC_UNDO_UNDO:
    end = undo->pts[0];
    if(undo->undone) {
      for(u2 = undo->next,n = 0 ; u2 && n < end ; n++, u2= u2->next)
	scc_boxedit_undo_action(be,u2);

    } else {
      do {
	for(u2 = undo->next,n = 1 ; u2 && n < end ; n++, u2= u2->next);      
	scc_boxedit_undo_action(be,u2);
	end--;
      } while(end > 0);
    }
    break;
  }

  undo->undone ^= 1;

  return 1;
}

static void scc_undo_free(scc_boxedit_undo_t* u) {
  scc_boxedit_undo_t* n;

  while(u) {
    n = u->next;
    switch(u->op) {
    case SCC_UNDO_NEW_BOX:
      if(u->undone) free(u->box);
      break;
    case SCC_UNDO_UNSELECT_ALL:
    case SCC_UNDO_DESTROY:
      scc_box_list_free(u->box);
      break;
    case SCC_UNDO_NAME:
      free(u->box);
      break;
    }
    free(u);
    u = n;
  }

}

static char* scc_undo_name(scc_boxedit_undo_t* u) {
  char* name;

  switch(u->op) {
  case SCC_UNDO_SELECT_PTS:
    name = "point selection";
    break;
  case SCC_UNDO_UNSELECT_PTS:
    name = "point unselection";
    break;
  case SCC_UNDO_SELECT_NEXT_PTS:
    name = "next point selection";
    break;
  case SCC_UNDO_NEW_BOX:
    name = "new box creation";
    break;
  case SCC_UNDO_SELECT_BOX:
    name = "box selection";
    break;
  case SCC_UNDO_UNSELECT_BOX:
    name = "box unselection";
    break;
  case SCC_UNDO_SELECT_NEXT_BOX:
    name = "next box selection";
    break;
  case SCC_UNDO_MOVE:
    name = "selection move";
    break;
  case SCC_UNDO_UNSELECT_ALL:
    name = "unselecting all";
    break;
  case SCC_UNDO_DESTROY:
    name = "destroy selection";
    break;
  case SCC_UNDO_NAME:
    name = "name setting";
    break;
  case SCC_UNDO_FLAG:
    name = "flag setting";
    break;
  case SCC_UNDO_SCALE:
    name = "scale setting";
    break;
  case SCC_UNDO_MASK:
    name = "mask setting";
    break;
  case SCC_UNDO_UNDO:
    name = "undo";
    break;
  default:
    name = "some op";
  }

  return name;

}

static void scc_undo_print(scc_boxedit_undo_t* u) {
  printf("%s %d %d\n",scc_undo_name(u),u->undone,u->pts[0]);
}

int scc_boxedit_undo(scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo = be->undo,*u2;
  int skip = 0;

  if(!undo) {
    scc_boxedit_status_msg(be,"Nothing to undo");
    return 0;
  }
  
  // if the last op is an undo, try to add another step
  // to it
  if(undo->op == SCC_UNDO_UNDO) {
    skip = undo->pts[0]+1;
    while(undo && skip > 0) {
      skip--;
      undo = undo->next;
    }
    if(!undo) {
      scc_boxedit_status_msg(be,"Nothing left to undo");
      return 0;
    }
  }

  scc_boxedit_status_msg(be,"Undo %s",scc_undo_name(undo));
  scc_boxedit_undo_action(be,undo);

  if(be->undo->op == SCC_UNDO_UNDO) {
    be->undo->pts[0]++;
  } else {
    u2 = scc_boxedit_get_undo(be,SCC_UNDO_UNDO);
    u2->pts[0] = 1;
  }

  if(undo->next == be->last_save) be->changed = 0;
  else be->changed = 1;

  return 1;
}

int scc_boxedit_redo(scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo = be->undo, *u2;
  int n;

  if(!undo || undo->op != SCC_UNDO_UNDO) {
    scc_boxedit_status_msg(be,"Nothing to redo");
    return 0;
  }
  undo->pts[0]--;

  for(u2 = undo->next,n = 0 ; u2 && n < undo->pts[0] ; n++, u2= u2->next);
  if(!u2) {
    scc_boxedit_status_msg(be,"Hmm, something went wrong with the redo");
    return 0;
  }

  scc_boxedit_status_msg(be,"Redo %s",scc_undo_name(u2));
  scc_boxedit_undo_action(be,u2);

  be->changed = (u2 == be->last_save) ? 0 : 1;

  if(undo->pts[0] <= 0) {
    be->undo = undo->next;
    free(undo);
  }

  return 1;
}


static void scc_boxedit_sel_clear(scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box;
  scc_box_t* last = NULL,*new;
  int i;

  if(!be->nsel) return;

  // save all the boxes
  undo = scc_boxedit_get_undo(be,SCC_UNDO_UNSELECT_ALL);
  undo->box = be->boxes;
  undo->box_b = be->edit_box;
  undo->pts[0] = be->nsel;

  be->boxes = NULL;
  for(box = undo->box ; box ; box = box->next) {
    new = malloc(sizeof(scc_box_t));
    memcpy(new,box,sizeof(scc_box_t));
    new->next = NULL;
    new->nsel = 0;
    for(i = 0 ; i < new->npts ; i++)
      new->pts[i].selected = 0;
    SCC_LIST_ADD(be->boxes,last,new);
  }
  be->nsel = 0;
  scc_boxedit_set_edit_box(be,NULL);
}

static int scc_boxedit_sel_add_from(scc_boxedit_t* be,int x, int y) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box;
  int i;

  for(box = be->boxes ; box ; box = box->next) {
    for(i = 0 ; i < box->npts ; i++) {
      if(!is_box_visible(be,box)) continue;
      if(!is_near_point(&box->pts[i],x,y,5-be->zoom)) continue;
      if(box->pts[i].selected) continue;

      undo = scc_boxedit_get_undo(be,SCC_UNDO_SELECT_PTS);
      undo->box = box;
      undo->box_b = be->edit_box;
      undo->pts[0] = i;
      return scc_boxedit_sel_pts(be,box,i);
    }
  }
  return 0;	
}



static int scc_boxedit_sel_rem_from(scc_boxedit_t* be,int x, int y) {
  scc_box_t* box,*last = NULL;
  scc_boxedit_undo_t* undo;
  int i;

  if(!be->boxes) return 0;

  do {
    for(box = be->boxes ; box->next != last ; box = box->next);
    for(i = box->npts-1 ; i >= 0 ; i--) {
      if(!is_box_visible(be,box)) continue;
      if(!is_near_point(&box->pts[i],x,y,5-be->zoom)) continue;
      if(!box->pts[i].selected) continue;

      undo = scc_boxedit_get_undo(be,SCC_UNDO_UNSELECT_PTS);
      undo->box = box;
      undo->pts[0] = i;
      undo->box_b = be->edit_box;
      return scc_boxedit_unsel_pts(be,box,i);
    }
    last = box;
  } while(box != be->boxes);
  return 0;
}

static int scc_boxedit_sel_turn_from(scc_boxedit_t* be,int x, int y) {
  scc_box_t* box,*ubox = NULL;
  scc_boxedit_undo_t* undo;
  int i,u;

  do {

    for(box = be->boxes ; box ; box = box->next) {
      for(i = 0 ; i < box->npts ; i++) {
	if(!is_box_visible(be,box)) continue;
	if(!is_near_point(&box->pts[i],x,y,5-be->zoom)) continue;
	if(!ubox) {
	  if(!box->pts[i].selected) continue;
	  ubox = box;
	  u = i;
	  continue;
	}
	if(box == ubox && i == u) return 0;
	if(box->pts[i].selected) continue;

	undo = scc_boxedit_get_undo(be,SCC_UNDO_SELECT_NEXT_PTS);
	undo->box = ubox;
	undo->pts[0] = u;
	undo->box_b = box;
	undo->pts[1] = i;
	undo->box_c = be->edit_box;
  
	return scc_boxedit_sel_next_pts(be,ubox,u,box,i);
      }
    }
    // if ubox is set then retry
  } while(ubox);
  return 0;
}

static int scc_boxedit_sel_add_box_from(scc_boxedit_t* be,int x, int y) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box,*sbox = NULL;
  int i,ns = 0;

  for(box = be->boxes ; box ; box = box->next) {
    for(i = 0 ; i < box->npts ; i++) {
      if(!is_box_visible(be,box)) continue;
      if(!box->pts[i].selected) ns++;
      if(is_near_point(&box->pts[i],x,y,5-be->zoom)) sbox = box;
    }

    if(sbox && ns) {
      undo = scc_boxedit_get_undo(be,SCC_UNDO_SELECT_BOX);
      undo->box = sbox;
      // mark the points that we add to the selection
      for(i = 0,ns = 0 ; i < sbox->npts ; i++) {
	if(sbox->pts[i].selected) continue;
	undo->pts[ns] = i;
	ns++;
      }
      undo->box_b = be->edit_box;
      if(ns < 4) undo->pts[ns] = -1;
      return scc_boxedit_sel_box(be,sbox);
    }

    sbox = NULL;
    ns = 0;
  }
  return 0;
}

static int scc_boxedit_sel_rem_box_from(scc_boxedit_t* be,int x, int y) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box,*last = NULL,*sbox;
  int i,ns;

  if(!be->boxes) return 0;

  do {
    sbox = NULL;
    ns = 0;
    for(box = be->boxes ; box->next != last ; box = box->next);
    for(i = box->npts-1 ; i >= 0 ; i--) {
      if(!is_box_visible(be,box)) continue;
      if(box->pts[i].selected) ns++;
      if(!is_near_point(&box->pts[i],x,y,5-be->zoom)) continue;
      sbox = box;
    }

    if(sbox && ns) {
      undo = scc_boxedit_get_undo(be,SCC_UNDO_UNSELECT_BOX);
      undo->box = sbox;
      // mark the points that was in the selection
      for(i = 0,ns = 0 ; i < sbox->npts ; i++) {
	if(!sbox->pts[i].selected) continue;
	undo->pts[ns] = i;
	ns++;
      }
      undo->box_b = be->edit_box;
      return scc_boxedit_unsel_box(be,sbox);
    }

    last = box;
  } while(box != be->boxes);
  return 0;
}

static int scc_boxedit_sel_turn_box_from(scc_boxedit_t* be,int x, int y) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box,*sbox = NULL,*dbox=NULL;
  int i,ns;

  if(!be->boxes) return 0;

  do {
    for(box = be->boxes ; box ; box = box->next) {
      if(dbox && box == dbox) return 0;

      ns = 0;
      sbox = NULL;
      for(i = box->npts-1 ; i >= 0 ; i--) {
	if(!is_box_visible(be,box)) continue;
	if(box->pts[i].selected) ns++;
	if(is_near_point(&box->pts[i],x,y,5-be->zoom)) sbox = box;
      }
    
      if(dbox && sbox && ns < sbox->npts) {
	undo = scc_boxedit_get_undo(be,SCC_UNDO_SELECT_NEXT_BOX);
	undo->box = dbox;
	// mark the points were alredy in the selection
	for(i = 0,ns = 0 ; i < sbox->npts ; i++) {
	  if(!sbox->pts[i].selected) continue;
	  undo->pts[ns] = i;
	  ns++;
	}
	if(ns < 4) undo->pts[ns] = -1;
	undo->box_b = sbox;
	undo->box_c = be->edit_box;
	return scc_boxedit_sel_next_box(be,dbox,sbox);
      }
      // ok we found a candidate
      if(sbox && ns == sbox->npts)
	dbox = sbox;    
    }
  } while(dbox);

  return 0;
}

static int scc_boxedit_new_box_at(scc_boxedit_t* be,int x, int y) {
  scc_box_t* b = scc_boxedit_new_box(be);
  scc_boxedit_undo_t* undo = scc_boxedit_get_undo(be,SCC_UNDO_NEW_BOX);

  undo->box = b;
  undo->box_b = be->edit_box;

  x -= be->view_off_x;
  x /= be->zoom;
  y -= be->view_off_y;
  y /= be->zoom;

  b->pts[0].x = x-10;
  b->pts[0].y = y-10;
  b->pts[0].selected = 1;

  b->pts[1].x = x+10;
  b->pts[1].y = y-10;
  b->pts[1].selected = 1;

  b->pts[2].x = x+10;
  b->pts[2].y = y+10;
  b->pts[2].selected = 1;

  b->pts[3].x = x-10;
  b->pts[3].y = y+10;
  b->pts[3].selected = 1;

  b->nsel = 4;
  be->nsel += 4;

  scc_boxedit_set_edit_box(be,b);

  return 1;
}

static int scc_boxedit_destroy_sel(scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo;
  scc_box_t* box,*last = NULL,*new;
  int i;

  if(!be->nsel) return 0;

  undo = scc_boxedit_get_undo(be,SCC_UNDO_DESTROY);
  undo->box = be->boxes;
  undo->box_b = be->edit_box;
  undo->pts[0] = be->nsel;

  be->boxes = NULL;
  for(box = undo->box ; box ; box = box->next) {
    if(box->nsel > box->npts-2) continue; // destroy completly the +2 selected pts box

    // create the new box
    new = calloc(1,sizeof(scc_box_t));
    memcpy(new,box,sizeof(scc_box_t));
    new->next = NULL;
    // kill the resting points
    if(new->nsel > 0) {
      for(i = 0 ; i < new->npts ; i++) {
	if(!new->pts[i].selected) continue;
	if(i+1 < new->npts) {
	  memmove(&new->pts[i],&new->pts[i+1],
		  sizeof(scc_box_pts_t)*(new->npts-i-1));
	  i--;
	}
	new->npts--;
      }
      new->nsel = 0;
    }
    SCC_LIST_ADD(be->boxes,last,new);
  }
  scc_boxedit_set_edit_box(be,NULL);
  be->nsel = 0;
  return 1;
}

void scc_boxedit_set_zoom(scc_boxedit_t* be, int z) {
  scc_img_t* rmim = be->rmim;
  uint8_t* dst,*src;
  int i,j,l;
  GtkAdjustment* hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
  GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
  int x = (hadj->value+(hadj->page_size/2))/be->zoom;
  int y = (vadj->value+(vadj->page_size/2))/be->zoom;

  // clamp
  if(z < 1) z = 1;
  else if(z > 10) z = 10;

  //if(z == be->zoom) return;

  be->zoom = z;

  if(!rmim) {
    be->view_w = 800;
    be->view_h = 500;
  } else {
    if(be->view_data) free(be->view_data);
    be->view_w = be->rmim->w*z;
    be->view_h = be->rmim->h*z;
  }    

  // allocate some space on the view
  gtk_widget_set_size_request(be->da,2*be->view_w,2*be->view_h);
  be->view_off_x = be->view_w/2;
  be->view_off_y = be->view_h/2;

  if(rmim) {
    src = rmim->data;
    dst = be->view_data = malloc(be->view_w*be->view_h);

    for(i = 0 ; i < rmim->h ; i++) {
      // copy the first line
      for(j = 0 ; j < rmim->w ; j++) {
	for(l = 0 ; l < be->zoom ; l++)
	  dst[z*j+l] = src[j];
      }
      for(l = 1 ; l < z ; l++)
	memcpy(&dst[be->view_w*l],dst,be->view_w);
      dst += z*be->view_w;
      src += rmim->w;
    }
  }
  
  // try to reput the view at the same place
  x = x*z-hadj->page_size/2;
  if(x > hadj->upper-hadj->page_size) x = hadj->upper-hadj->page_size;
  if(x < 0) x = 0;
  gtk_adjustment_set_value(hadj,x);

  y = y*z-vadj->page_size/2;
  if(y > vadj->upper-vadj->page_size) y = vadj->upper-vadj->page_size;
  if(y < 0) y = 0;
  gtk_adjustment_set_value(vadj,y);
}

void scc_boxedit_draw_box(scc_boxedit_t* be, scc_box_t* box,
			  int selected) {
  int off_x = be->view_off_x;
  int off_y = be->view_off_y;
  int i,m = box->mask;

  //gdk_gc_set_rgb_fg_color(be->img_gc,c);
  if(m >= SCC_MAX_IM_PLANES) m = SCC_MAX_IM_PLANES-1;
  if(be->hide_plane[m]) return;


  if(selected) {
    if(box == be->edit_box)
      gdk_gc_set_rgb_fg_color(be->img_gc,&be->edit_color);
    else
      gdk_gc_set_rgb_fg_color(be->img_gc,&be->sel_color);
  } else
    gdk_gc_set_rgb_fg_color(be->img_gc,&be->box_color);
    

  for(i = 1 ; i < box->npts ; i++) {
    GdkPoint pts[2] = {
      { off_x+box->pts[i-1].x*be->zoom, off_y+box->pts[i-1].y*be->zoom },
      { off_x+box->pts[i].x*be->zoom, off_y+box->pts[i].y*be->zoom }
    };
    if(selected ^ (box->pts[i].selected || box->pts[i-1].selected)) continue;
    gdk_draw_lines(be->da->window,be->img_gc,
		   pts,2);
  }

  // close the polygone
  if(i > 1 &&
     (!(selected ^ (box->pts[0].selected || box->pts[i-1].selected)))) {
    GdkPoint pts[2] = {
      { off_x+box->pts[0].x*be->zoom, off_y+box->pts[0].y*be->zoom },
      { off_x+box->pts[i-1].x*be->zoom, off_y+box->pts[i-1].y*be->zoom }
    };

    gdk_draw_lines(be->da->window,be->img_gc,
		   pts,2);
  }
  // then the points
  if(!selected)
    gdk_gc_set_rgb_fg_color(be->img_gc,&be->pts_color);
  
  for(i = 0 ; i < box->npts ; i++) {
    GdkPoint pts[5] = {
      { off_x+box->pts[i].x*be->zoom - 2, off_y+box->pts[i].y*be->zoom - 2 },
      { off_x+box->pts[i].x*be->zoom + 2, off_y+box->pts[i].y*be->zoom - 2 },
      { off_x+box->pts[i].x*be->zoom + 2, off_y+box->pts[i].y*be->zoom + 2 },
      { off_x+box->pts[i].x*be->zoom - 2, off_y+box->pts[i].y*be->zoom + 2 },
      { off_x+box->pts[i].x*be->zoom - 2, off_y+box->pts[i].y*be->zoom - 2 }
    };
    if(selected ^ box->pts[i].selected) continue;

    gdk_draw_lines(be->da->window,be->img_gc,
		   pts,5);
  }

}

static int motion_img_cb(GtkWidget *img,GdkEventMotion *ev,
			      scc_boxedit_t* be) {
  char txt[50];
  GtkAdjustment* adj;
  double val;

  // kill status msg as soon as the mouse move
  if(be->sbar_have_msg) {
    gtk_statusbar_pop(GTK_STATUSBAR(be->sbar),be->sbar_ctx_msg);
    be->sbar_have_msg = 0;
  }
  // no drag, so just display the coordinates
  if(!be->drag) {
    if(be->nsel)
      sprintf(txt,"%c %d x %d (%d)",be->changed ? '*':' ',
	      (int)(ev->x - be->view_off_x)/be->zoom,
	      (int)(ev->y - be->view_off_y)/be->zoom,be->nsel);
    else
      sprintf(txt,"%c %d x %d",be->changed ? '*':' ',
	      (int)(ev->x - be->view_off_x)/be->zoom,
	      (int)(ev->y - be->view_off_y)/be->zoom);
    gtk_statusbar_pop(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord);
    gtk_statusbar_push(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord,txt);
    return 0;
  }
  // drag with the first button
  if(be->drag == 1) {
    // we do smthg only if their is a selection
    if(be->nsel > 0) { // get the relative move
      int x = (ev->x - be->drag_x)/be->zoom;
      int y = (ev->y - be->drag_y)/be->zoom;

      // if the box didn't moved at all just ignore
      if(x == 0 && y == 0) return 0;
      // move them, return if tho move was not possible
      if(!scc_boxedit_sel_move(be,x,y)) {
	be->drag_x = ev->x;
	be->drag_y = ev->y;
	return 0;
      }
      // store the move for the undo
      be->drag_move_x += x;
      be->drag_move_y += y;
      // ask a redraw
      gtk_widget_queue_draw(img);
      // move the origin, we do that only if we moved the boxes along that axis
      // otherwise rouding accumulate when one drag slowly
      if(x) be->drag_x = ev->x;
      if(y) be->drag_y = ev->y;

      // display coord and move
      sprintf(txt,"%c %d x %d (%d x %d)",be->changed ? '*':' ',
	      (int)(ev->x - be->view_off_x)/be->zoom,
	      (int)(ev->y - be->view_off_y)/be->zoom,
	      be->drag_move_x,be->drag_move_y);
      gtk_statusbar_pop(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord);
      gtk_statusbar_push(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord,txt);

      // see if we need to scroll the window horizontaly
      adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
      val = gtk_adjustment_get_value(adj);
      if(val + adj->page_size < ev->x) {
	val = ev->x - adj->page_size;
	if(val > adj->upper-adj->page_size) val = adj->upper-adj->page_size;
	gtk_adjustment_set_value(adj,val);	
      } else if(val > ev->x) {
	val = ev->x;
	if(val < 0) val = 0;
	gtk_adjustment_set_value(adj,val);
      }

      // the same verticaly
      adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
      val = gtk_adjustment_get_value(adj);
      if(val + adj->page_size < ev->y) {
	val = ev->y - adj->page_size;
	if(val > adj->upper-adj->page_size) val = adj->upper-adj->page_size;
	gtk_adjustment_set_value(adj,val);	
      } else if(val > ev->y) {
	val = ev->y;
	if(val < 0) val = 0;
	gtk_adjustment_set_value(adj,val);
      }
	
    }

    // drag with button 2 just move the scroll around
    // note that we have to use root coordinate
  } else if(be->drag == 2) {
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
    val = gtk_adjustment_get_value(adj)-(ev->x_root-be->drag_x);

    if(val > adj->upper-adj->page_size) val = adj->upper-adj->page_size;
    if(val < adj->lower) val = adj->lower;
    gtk_adjustment_set_value(adj,val);

    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(be->da_scroll));
    val = gtk_adjustment_get_value(adj)-(ev->y_root-be->drag_y);

    if(val > adj->upper-adj->page_size) val = adj->upper-adj->page_size;
    if(val < adj->lower) val = adj->lower;
    gtk_adjustment_set_value(adj,val);

    be->drag_x = ev->x_root;
    be->drag_y = ev->y_root;
  }

  return 0;
}

static int btn_press_img_cb(GtkWidget *img,GdkEventButton *ev,
			      scc_boxedit_t* be) {
  int off_x = be->view_off_x;
  int off_y = be->view_off_y;
  int r = 0;

  // ignore other button press while dragging
  if(be->drag) return 0;
  
  if(ev->button == 1) {

    if(ev->state & GDK_CONTROL_MASK)
      r = scc_boxedit_sel_turn_from(be,(ev->x-off_x)/be->zoom,
				    (ev->y-off_y)/be->zoom);
    else if(ev->state & GDK_SHIFT_MASK)
      r = scc_boxedit_sel_add_box_from(be,(ev->x-off_x)/be->zoom,
				       (ev->y-off_y)/be->zoom);
    else
      r = scc_boxedit_sel_add_from(be,(ev->x-off_x)/be->zoom,
				   (ev->y-off_y)/be->zoom);

    be->drag = 1;
    be->drag_x = ev->x;
    be->drag_y = ev->y;
    be->drag_move_x = 0;
    be->drag_move_y = 0;

  } else if(ev->button == 2) {
    // we have to use root coordinates, as the img will be moving inside
    // the scroll
    be->drag = 2;
    be->drag_x = ev->x_root;
    be->drag_y = ev->y_root;

  } else if(ev->button == 3) {
    if(ev->state & GDK_CONTROL_MASK)
      r = scc_boxedit_sel_turn_box_from(be,(ev->x-off_x)/be->zoom,
					(ev->y-off_y)/be->zoom);
    else if(ev->state & GDK_SHIFT_MASK)
      r = scc_boxedit_sel_rem_box_from(be,(ev->x-off_x)/be->zoom,
				       (ev->y-off_y)/be->zoom);
    else
      r = scc_boxedit_sel_rem_from(be,(ev->x-off_x)/be->zoom,
				   (ev->y-off_y)/be->zoom);

  }    
    
  if(r) gtk_widget_queue_draw(img);

  return 0;
}

static int btn_release_img_cb(GtkWidget *img,GdkEventButton *ev,
			      scc_boxedit_t* be) {
  // ignore release which are not for the current drag
  if(ev->button != be->drag) return 0;
  // box dragging is over, store the undo
  if(be->drag == 1 && 
     (be->drag_move_x != 0 || be->drag_move_y != 0)) {
    scc_boxedit_undo_t* undo = scc_boxedit_get_undo(be,SCC_UNDO_MOVE);
    undo->pts[0] = be->drag_move_x;
    undo->pts[1] = be->drag_move_y;
  }
  be->drag = 0;
  return 0;
}

static void expose_img_cb(GtkWidget *img,GdkEventExpose* ev,
			  scc_boxedit_t* be) {
  scc_box_t* box;
  int x,y,h,w;
  int off_x = be->view_off_x;
  int off_y = be->view_off_y;

  // the window must be realized to give a gc, so do it here
  if(!be->img_gc) be->img_gc = gdk_gc_new(img->window);

  if(be->view_data) {
  // compute wich part of the image need to be drawn
  x = (off_x < ev->area.x) ? ev->area.x - off_x : 0;
  y = (off_y < ev->area.y) ? ev->area.y - off_y : 0;

  w = (be->view_w-x > ev->area.width) ? ev->area.width : be->view_w-x;
  h = (be->view_h-y > ev->area.height) ? ev->area.height : be->view_h-y;
    
  
  // First draw the image
  gdk_draw_indexed_image(img->window,be->img_gc,
			 off_x+x,off_y+y,
			 w,h,GDK_RGB_DITHER_NONE,
			 &be->view_data[be->view_w*y+x],be->view_w,be->img_cmap);
  }
  // Then the boxes on top of it

  // first the non-selected stuff
  for(box = be->boxes ; box ; box = box->next)
    scc_boxedit_draw_box(be,box,0);
  // then the selected stuff
  for(box = be->boxes ; box ; box = box->next)
    scc_boxedit_draw_box(be,box,1);


}

// grab the focus on the scroll when we enter it
static int enter_img_cb(GtkWidget *img,
			GdkEventCrossing *ev,
			scc_boxedit_t* be) {
  char msg[50]; // set the status bar
  if(be->nsel)
    sprintf(msg,"%c %d x %d (%d)",be->changed ? '*':' ',
	    (int)(ev->x - be->view_off_x)/be->zoom,
	    (int)(ev->y - be->view_off_y)/be->zoom,be->nsel);
  else
    sprintf(msg,"%c %d x %d",be->changed ? '*':' ',
	    (int)(ev->x - be->view_off_x)/be->zoom,
	    (int)(ev->y - be->view_off_y)/be->zoom);
  gtk_statusbar_push(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord,msg);

  // hack to autosave the name entry
  if(be->edit_box) {
    int set = 0;
    const char* txt = gtk_entry_get_text(GTK_ENTRY(be->name_entry));
    if(txt[0] == '\0') txt = NULL;
    if(txt && be->edit_box->name) {
      if(strcmp(txt,be->edit_box->name)) set = 1;
    } else if(txt || be->edit_box->name) set = 1;
    if(set) activate_name_cb(GTK_ENTRY(be->name_entry),be);
  }
  
  gtk_widget_grab_focus(be->da_scroll);
  return 1;
}

static int leave_img_cb(GtkWidget *img,
			GdkEventCrossing *ev,
			scc_boxedit_t* be) {
  gtk_statusbar_pop(GTK_STATUSBAR(be->sbar),be->sbar_ctx_coord);
  return 1;
}

//int scc_edit_save_box(scc_boxedit_t* be, char* path);
static void clicked_save_btn_cb(GtkButton *btn,scc_boxedit_t* be);

static int key_press_scroll_cb(GtkWidget *scrl,
			    GdkEventKey *ev,
			    scc_boxedit_t* be) {
  int r = 0;
  
  switch(ev->keyval) {
    // kill the selection with escape
  case GDK_Escape:
    scc_boxedit_sel_clear(be);
    r = 1;
    break;
  case GDK_plus:
  case GDK_KP_Add:
  case GDK_greater:
    if(be->zoom < 2) scc_boxedit_set_zoom(be,2), r = 1;
    else if(be->zoom < 5) scc_boxedit_set_zoom(be,be->zoom+1), r = 1;
    break;
  case GDK_minus:
  case GDK_KP_Subtract:
  case GDK_less:
    if(be->zoom > 1) scc_boxedit_set_zoom(be,be->zoom-1), r = 1;
    break;

  case GDK_b: {
    int x,y;
    gtk_widget_get_pointer(be->da,&x,&y);
    r = scc_boxedit_new_box_at(be,x,y);    
  } break;

  case GDK_d:
  case GDK_KP_Delete:
  case GDK_Delete:
    r = scc_boxedit_destroy_sel(be);
    break;

  case GDK_u:
  case GDK_q:
    r = scc_boxedit_undo(be);
    break;

  case GDK_r:
  case GDK_o:
    r = scc_boxedit_redo(be);
    break;

  case GDK_D: {
    scc_boxedit_undo_t* u;
    printf("\n------- Undo dump --------\n");
    for(u = be->undo ; u ; u = u->next)
      scc_undo_print(u);
    } break;

  case GDK_S:
    be->save_as = 1;
    clicked_save_btn_cb(NULL,be);
    be->save_as = 0;
    break;

  case GDK_s:
    clicked_save_btn_cb(NULL,be);
    break;
  }

  if(r) gtk_widget_queue_draw(be->da);

  return 0;
}

static void toggled_plane_cb(GtkToggleButton *btn,
			     scc_boxedit_t* be) {
  int s = gtk_toggle_button_get_active(btn);
  int* plane = g_object_get_data(G_OBJECT(btn),"plane");
  plane[0] = !s;
  gtk_widget_queue_draw(be->da);
}

static void activate_name_cb(GtkEntry *entry,
			     scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo;
  const char* txt;
  if(!be->edit_box) return;
  
  undo = scc_boxedit_get_undo(be,SCC_UNDO_NAME);
  undo->box = (scc_box_t*)be->edit_box->name;

  txt = gtk_entry_get_text(GTK_ENTRY(be->name_entry));
  if(txt[0] == '\0')
    be->edit_box->name = NULL;
  else
    be->edit_box->name = strdup(txt);
}

static void value_changed_mask_cb(GtkSpinButton *btn,
				   scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo;
  if(!be->edit_box) return;

  undo = scc_boxedit_get_undo(be,SCC_UNDO_MASK);
  undo->pts[0] = be->edit_box->mask;

  be->edit_box->mask = gtk_spin_button_get_value(btn);
  gtk_widget_queue_draw(be->da);
}

static void toggled_scale_type(GtkWidget* btn,
                               scc_boxedit_t* be) {
  int s = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn));
  scc_boxedit_undo_t* undo;
  if(!s || !be->edit_box) return;

  s = be->edit_box->scale;

  if(btn == be->scale_val) {
    be->edit_box->scale = 0x00FF;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),1,255);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_entry),255);
  } else if(btn == be->scale_slot) {
    be->edit_box->scale = 0x8000;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(be->scale_entry),0,3);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_entry),0);
  } else {
    printf("Error: toggle scale type from unknown button ??\n");
    return;
  }

  if(s == be->edit_box->scale) return;

  undo = scc_boxedit_get_undo(be,SCC_UNDO_SCALE);
  undo->pts[0] = s;

}

static void value_changed_scale_cb(GtkSpinButton *btn,
				   scc_boxedit_t* be) {
  scc_boxedit_undo_t* undo;
  int val;
  if(!be->edit_box) return;

  val = (int)gtk_spin_button_get_value(btn) |
    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(be->scale_slot)) ? 0x8000 : 0);
  
  if(val == be->edit_box->scale) return;

  undo = scc_boxedit_get_undo(be,SCC_UNDO_SCALE);
  undo->pts[0] = be->edit_box->scale;

  be->edit_box->scale = val;
}

static void toggled_flag_cb(GtkToggleButton *btn,
			    scc_boxedit_t* be) {
  int s = gtk_toggle_button_get_active(btn);
  int bit = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),"bit"));
  scc_boxedit_undo_t* undo;

  if(!be->edit_box) return;

  undo = scc_boxedit_get_undo(be,SCC_UNDO_FLAG);
  undo->pts[0] = bit;

  if(s)
    be->edit_box->flags |= (1<<bit);
  else
    be->edit_box->flags &= ~(1<<bit);
}

void scc_boxedit_set_zoom(scc_boxedit_t* be, int z);
void scc_boxedit_load_pal(scc_boxedit_t* be,scc_img_t* img);

static void clicked_open_btn_cb(GtkButton *btn,scc_boxedit_t* be) {
  scc_box_t* new;
  char* fname;
  int open_img = be->open_img;
  GtkWidget* dial =
    gtk_file_chooser_dialog_new (open_img ? "Open image" : "Open boxes",
				 GTK_WINDOW(be->win),
				 GTK_FILE_CHOOSER_ACTION_OPEN,
				 GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
				 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				 NULL);
  if(gtk_dialog_run(GTK_DIALOG(dial)) != GTK_RESPONSE_ACCEPT) {
      gtk_widget_destroy(dial);
      return;
  }
      
  fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dial));
  gtk_widget_destroy(dial);
  if(!fname) return;

  if(open_img) {
    scc_img_t* rmim = scc_img_open(fname);
    if(!rmim) {
      scc_boxedit_status_msg(be,"Failed to load image %s\n",fname);
      free(fname);
      return;
    }
    be->rmim = rmim;
    scc_boxedit_load_pal(be,rmim);
    scc_boxedit_set_zoom(be,be->zoom);  
  } else {
    new = scc_boxedit_load_box(be,fname);
    free(fname);
    if(!new) {
      scc_boxedit_status_msg(be,"Failed to open %s\n",fname);
      return;
    }
    // free undo
    scc_undo_free(be->undo);
    be->undo = NULL;
    be->last_save = NULL;
    be->changed = 0;
    // unset edit box
    scc_boxedit_set_edit_box(be,NULL);
    be->nsel = 0;
  }
}


static void clicked_save_btn_cb(GtkButton *btn,scc_boxedit_t* be) {

  if((!be->boxes) || (!be->changed)) {
    scc_boxedit_status_msg(be,"There is nothing to save");
    return;
  }
  
  if(!be->filename || be->save_as) {
    char* fname;
    GtkWidget* dial = 
      gtk_file_chooser_dialog_new ("Save boxes",
				   GTK_WINDOW(be->win),
				   GTK_FILE_CHOOSER_ACTION_SAVE,
				   GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
				   GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				   NULL);
    if(gtk_dialog_run(GTK_DIALOG(dial)) != GTK_RESPONSE_ACCEPT) {
      gtk_widget_destroy(dial);
      return;
    }
    fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dial));
    gtk_widget_destroy(dial);
    if(!fname) return;
    if(be->filename) free(be->filename);
    be->filename = fname;
  }

  if(scc_boxedit_save(be,be->filename)) {
    scc_boxedit_status_msg(be,"Saved to %s",be->filename);
    if(be->undo && be->undo->op == SCC_UNDO_UNDO) {
      int n = be->undo->pts[0];
      for(be->last_save = be->undo ; n >= 0 && be->last_save ;
	  be->last_save = be->last_save->next) n--;
    } else
      be->last_save = be->undo;
    be->changed = 0;
  } else
    scc_boxedit_status_msg(be,"Save to %s failed",be->filename);
  
}

// generic stuff for the buttons

// grab the focus, so we can get key event
static int enter_btn_cb(GtkWidget *btn,
			GdkEventCrossing *ev,
			scc_boxedit_t* be) {
  gtk_widget_grab_focus(btn);
  return 0;
}

// return to "normal" state if we leave and shift 
// has not been released yet
static int leave_btn_cb(GtkWidget *btn,
			GdkEventCrossing *ev,
			scc_boxedit_t* be) {
  int* ptr = g_object_get_data(G_OBJECT(btn),"flag");
  char* txt = g_object_get_data(G_OBJECT(btn),"label");
  if(ptr[0]) {
    ptr[0] = 0;
    gtk_container_remove(GTK_CONTAINER(btn),gtk_bin_get_child(GTK_BIN(btn)));
    gtk_container_add(GTK_CONTAINER(btn),gtk_label_new(txt));
    gtk_widget_show_all(btn);
  }
  gtk_widget_grab_focus(be->da_scroll);
  return 0;
}

// when shift is pressed go to the alt state
static int key_press_btn_cb(GtkWidget *btn,GdkEventKey *ev,
			    scc_boxedit_t* be) {
  int* ptr = g_object_get_data(G_OBJECT(btn),"flag");
  if(ptr[0]) return 0;
  if(ev->keyval == GDK_Shift_L ||
     ev->keyval == GDK_Shift_R) {
    char* txt = g_object_get_data(G_OBJECT(btn),"alt_label");
    ptr[0] = 1;
    gtk_container_remove(GTK_CONTAINER(btn),gtk_bin_get_child(GTK_BIN(btn)));
    gtk_container_add(GTK_CONTAINER(btn),gtk_label_new(txt));
    gtk_widget_show_all(btn);
    return 1;
  }
  return 0;
}

// return to normal on release
static int key_release_btn_cb(GtkWidget *btn, GdkEventKey *ev,
			      scc_boxedit_t* be) {
  int* ptr = g_object_get_data(G_OBJECT(btn),"flag");
  if(!ptr[0]) return 0;
  if(ev->keyval == GDK_Shift_L ||
     ev->keyval == GDK_Shift_R) {
    char* txt = g_object_get_data(G_OBJECT(btn),"label");
    ptr[0] = 0;
    gtk_container_remove(GTK_CONTAINER(btn),gtk_bin_get_child(GTK_BIN(btn)));
    gtk_container_add(GTK_CONTAINER(btn),gtk_label_new(txt));
    gtk_widget_show_all(btn);
    return 1;
  }
  return 0;
}

static void clicked_edit_slot_btn_cb(GtkButton *btn,scc_boxedit_t* be) {
  int i;

  for(i = 0 ; i < SCC_NUM_SCALE_SLOT ; i++) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_slot_entries[4*i]),
                              be->scale_slots[i].y1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_slot_entries[4*i+1]),
                              be->scale_slots[i].s1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_slot_entries[4*i+2]),
                              be->scale_slots[i].y2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(be->scale_slot_entries[4*i+3]),
                              be->scale_slots[i].s2);
  }

  gtk_widget_show_all(be->scale_win);
}

static void value_changed_scale_slot_cb(GtkSpinButton *btn,
                                        int* entry) {
  int val = (int)gtk_spin_button_get_value(btn);
  entry[0] = val;  
}

static int delete_scale_win_cb(GtkWidget *widget,GdkEvent *event,
                               scc_boxedit_t* be) {
  gtk_widget_hide(be->scale_win);
  return 1;
}

static int delete_win_cb(GtkWidget *widget,GdkEvent *event,
			 scc_boxedit_t* be) {
  
  if(be->changed) {
    int r;
    GtkWidget* dial =gtk_message_dialog_new(GTK_WINDOW(be->win),
					    GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_QUESTION,
					    GTK_BUTTONS_NONE,
					    "Do you want to save before quitting?");
    gtk_dialog_add_buttons(GTK_DIALOG(dial),GTK_STOCK_YES,GTK_RESPONSE_YES,
			   GTK_STOCK_NO,GTK_RESPONSE_NO,
			   GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,NULL);
    r = gtk_dialog_run(GTK_DIALOG(dial));
    gtk_widget_destroy(dial);
    switch(r) {
    case GTK_RESPONSE_YES:
      clicked_save_btn_cb(NULL,be);
      if(be->changed) return 1;
      break;
    case GTK_RESPONSE_CANCEL:
      return 1;
    }
  }

  gtk_main_quit();
  return 0;
}

GtkWidget* scc_boxedit_list_new(scc_boxedit_t* be) {
  GtkWidget* vbox,*frame;
  GtkWidget* hbox,*box,*w;
  int i;

  box = gtk_vbox_new(0,2);

  hbox = gtk_hbox_new(0,2);

  w = gtk_button_new_with_label("Open");
  g_signal_connect(G_OBJECT(w),"clicked",
		     G_CALLBACK(clicked_open_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"enter-notify-event",
  		   G_CALLBACK(enter_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"leave-notify-event",
  		   G_CALLBACK(leave_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"key-press-event",
  		   G_CALLBACK(key_press_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"key-release-event",
  		   G_CALLBACK(key_release_btn_cb),be);
  g_object_set_data(G_OBJECT(w),"flag",&be->open_img);
  g_object_set_data(G_OBJECT(w),"label","Open");
  g_object_set_data(G_OBJECT(w),"alt_label","Open img");
  gtk_widget_set_size_request(w,90,30);
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);

  w = gtk_button_new_with_label("Save");
  g_signal_connect(G_OBJECT(w),"clicked",
		     G_CALLBACK(clicked_save_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"enter-notify-event",
  		   G_CALLBACK(enter_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"leave-notify-event",
  		   G_CALLBACK(leave_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"key-press-event",
  		   G_CALLBACK(key_press_btn_cb),be);
  g_signal_connect(G_OBJECT(w),"key-release-event",
  		   G_CALLBACK(key_release_btn_cb),be);
  g_object_set_data(G_OBJECT(w),"flag",&be->save_as);
  g_object_set_data(G_OBJECT(w),"label","Save");
  g_object_set_data(G_OBJECT(w),"alt_label","Save as");
  gtk_widget_set_size_request(w,90,30);
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);

  gtk_box_pack_start(GTK_BOX(box),hbox,0,0,5);

  frame = gtk_frame_new("Planes");
  gtk_container_set_border_width(GTK_CONTAINER(frame),5);

  hbox = gtk_hbox_new(0,0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox),5);
  for(i = 0 ; i < SCC_MAX_IM_PLANES ; i++) {
    char name[50];
    sprintf(name,"%d\n",i);
    w = gtk_toggle_button_new_with_label(name);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),1);
    gtk_box_pack_start(GTK_BOX(hbox),w,0,1,0);
    g_object_set_data(G_OBJECT(w),"plane",&be->hide_plane[i]);
    g_signal_connect(G_OBJECT(w),"toggled",
		     G_CALLBACK(toggled_plane_cb),be);
  }
  gtk_container_add(GTK_CONTAINER(frame),hbox);
  gtk_box_pack_start(GTK_BOX(box),frame,0,0,0);

  frame = gtk_frame_new("Box parameters");
  gtk_container_set_border_width(GTK_CONTAINER(frame),5);

  vbox = gtk_vbox_new(0,2);
  hbox = gtk_hbox_new(0,2);
  w = gtk_label_new("Name");
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
  be->name_entry = gtk_entry_new();
  g_signal_connect(G_OBJECT(be->name_entry),"activate",
		   G_CALLBACK(activate_name_cb),be);
  gtk_widget_set_sensitive(be->name_entry,0);
  gtk_widget_set_size_request(be->name_entry,120,25);
  gtk_box_pack_start(GTK_BOX(hbox),be->name_entry,1,1,5);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);
  

  hbox = gtk_hbox_new(0,2);
  w = gtk_label_new("Plane");
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
  be->mask_entry = 
    gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,15,
							  1,1,1)),
			1,0);
  g_signal_connect(G_OBJECT(be->mask_entry),"value-changed",
		   G_CALLBACK(value_changed_mask_cb),be);
  gtk_widget_set_sensitive(be->mask_entry,0);
  gtk_box_pack_start(GTK_BOX(hbox),be->mask_entry,0,0,5);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);


  hbox = gtk_hbox_new(0,2);
  w = gtk_label_new("Scale");
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
  be->scale_entry =
    gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(1,1,255,
							  1,1,10)),
			1,0);
  g_signal_connect(G_OBJECT(be->scale_entry),"value-changed",
		   G_CALLBACK(value_changed_scale_cb),be);
  gtk_widget_set_sensitive(be->scale_entry,0);
  gtk_box_pack_start(GTK_BOX(hbox),be->scale_entry,0,0,5);
  w = gtk_button_new_with_label("Edit slots");
  g_signal_connect(G_OBJECT(w),"clicked",
                   G_CALLBACK(clicked_edit_slot_btn_cb),be);
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);

  hbox = gtk_hbox_new(0,2);
  be->scale_val = gtk_radio_button_new_with_label(NULL,"Fixed scale");
  gtk_widget_set_sensitive(be->scale_val,0);
  g_signal_connect(G_OBJECT(be->scale_val),"toggled",
		   G_CALLBACK(toggled_scale_type),be);
  gtk_box_pack_start(GTK_BOX(hbox),be->scale_val,0,0,5);
  be->scale_slot = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(be->scale_val),
                                                               "Slot");
  gtk_widget_set_sensitive(be->scale_slot,0);
  g_signal_connect(G_OBJECT(be->scale_slot),"toggled",
		   G_CALLBACK(toggled_scale_type),be);
  gtk_box_pack_start(GTK_BOX(hbox),be->scale_slot,0,0,5);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);

  be->flag_toggle[7] = w = gtk_check_button_new_with_label("Invisible");
  g_object_set_data(G_OBJECT(w),"bit",GINT_TO_POINTER(7));
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(toggled_flag_cb),be);
  gtk_widget_set_sensitive(w,0);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,5);
  
  be->flag_toggle[6] = w = gtk_check_button_new_with_label("Locked");
  g_object_set_data(G_OBJECT(w),"bit",GINT_TO_POINTER(6));
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(toggled_flag_cb),be);
  gtk_widget_set_sensitive(w,0);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,5);

  be->flag_toggle[5] = w = gtk_check_button_new_with_label("Player only / Ignore scale");
  g_object_set_data(G_OBJECT(w),"bit",GINT_TO_POINTER(5));
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(toggled_flag_cb),be);
  gtk_widget_set_sensitive(w,0);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,5);

  be->flag_toggle[4] = w = gtk_check_button_new_with_label("Y flip actors");
  g_object_set_data(G_OBJECT(w),"bit",GINT_TO_POINTER(4));
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(toggled_flag_cb),be);
  gtk_widget_set_sensitive(w,0);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,5);

  be->flag_toggle[3] = w = gtk_check_button_new_with_label("X flip actors");
  g_object_set_data(G_OBJECT(w),"bit",GINT_TO_POINTER(3));
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(toggled_flag_cb),be);
  gtk_widget_set_sensitive(w,0);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,5);

  gtk_container_add(GTK_CONTAINER(frame),vbox);
  gtk_box_pack_start(GTK_BOX(box),frame,0,0,0);

  return box;

}

GtkWidget* scc_boxedit_scale_win_new(scc_boxedit_t* be) {
  GtkWidget *w,*vbox,*hbox,*frame,*fbox,*win;
  char lbl[32];
  int i;

  fbox = gtk_vbox_new(0,2);

  for(i = 0 ; i < SCC_NUM_SCALE_SLOT ; i++) {
    sprintf(lbl,"Slot %d",i);
    frame = gtk_frame_new(lbl);
    
    vbox = gtk_vbox_new(0,2);
    
    hbox = gtk_hbox_new(0,2);
    w = gtk_label_new("Y1");
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0xFFFF,
                                                              1,1,10)),
                            1,0);
    be->scale_slot_entries[4*i] = w;
    g_signal_connect(G_OBJECT(w),"value-changed",
                     G_CALLBACK(value_changed_scale_slot_cb),
                     &be->scale_slots[i].y1);
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);

    w = gtk_label_new("S1");
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0xFFFF,
                                                              1,1,10)),
                            1,0);
    be->scale_slot_entries[4*i+1] = w;
    g_signal_connect(G_OBJECT(w),"value-changed",
                     G_CALLBACK(value_changed_scale_slot_cb),
                     &be->scale_slots[i].s1);
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    
    gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);

    hbox = gtk_hbox_new(0,2);
    w = gtk_label_new("Y2");
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0xFFFF,
                                                              1,1,10)),
                            1,0);
    be->scale_slot_entries[4*i+2] = w;
    g_signal_connect(G_OBJECT(w),"value-changed",
                     G_CALLBACK(value_changed_scale_slot_cb),
                     &be->scale_slots[i].y2);
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);

    w = gtk_label_new("S2");
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0xFFFF,
                                                              1,1,10)),
                            1,0);
    be->scale_slot_entries[4*i+3] = w;
    g_signal_connect(G_OBJECT(w),"value-changed",
                     G_CALLBACK(value_changed_scale_slot_cb),
                     &be->scale_slots[i].s2);
    gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
    
    gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);
    
    gtk_container_add(GTK_CONTAINER(frame),vbox);

    gtk_box_pack_start(GTK_BOX(fbox),frame,0,0,5);
  }

  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width(GTK_CONTAINER(win),5);
  gtk_container_add(GTK_CONTAINER(win),fbox);
  

  g_signal_connect(G_OBJECT(win),"delete-event",
                   G_CALLBACK(delete_scale_win_cb),be);

  //gtk_widget_show_all(win);

  return win;
}

void scc_boxedit_load_pal(scc_boxedit_t* be,scc_img_t* img) {
  int i;

  if(be->img_cmap) gdk_rgb_cmap_free(be->img_cmap);

  be->img_cmap = malloc(sizeof(GdkRgbCmap));

  be->img_cmap->n_colors = img->ncol;

  for(i = 0; i < img->ncol ; i++)
    be->img_cmap->colors[i] = ((img->pal[i*3])<<16) |
      ((img->pal[i*3+1])<<8) | img->pal[i*3+2];
}

scc_boxedit_t* scc_boxedit_new(scc_img_t* rmim) {
  scc_boxedit_t* be = calloc(1,sizeof(scc_boxedit_t));
  GtkWidget* hpan;
  GtkWidget *w,*box;

  be->rmim = rmim;
  // set some default colors
  be->box_color.red = 0xFFFF;
  be->pts_color.green = 0xFFFF;
  be->edit_color.blue = 0xFFFF;
  be->edit_color.red = 0xFFFF;
  be->sel_color.blue = 0xFFFF;
  // load the palette
  if(rmim) scc_boxedit_load_pal(be,rmim);

  // create the main win
  be->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(be->win),"delete-event",
  		   G_CALLBACK(delete_win_cb),be);
  // the box
  box = gtk_vbox_new(0,2);

  // the pane
  hpan = gtk_hpaned_new();

  // the viewing area
  be->da_scroll = w = gtk_scrolled_window_new(NULL,NULL);
  be->da = gtk_drawing_area_new();
  if(rmim)
    gtk_widget_set_size_request(be->da,rmim->w*2,rmim->h*2);
  else
    gtk_widget_set_size_request(be->da,800,500);
  g_signal_connect(G_OBJECT(be->da),"expose-event",
  		   G_CALLBACK(expose_img_cb),be);
  g_signal_connect(G_OBJECT(be->da),"motion-notify-event",
  		   G_CALLBACK(motion_img_cb),be);
  g_signal_connect(G_OBJECT(be->da),"button-press-event",
  		   G_CALLBACK(btn_press_img_cb),be);
  g_signal_connect(G_OBJECT(be->da),"button-release-event",
  		   G_CALLBACK(btn_release_img_cb),be);
  g_signal_connect(G_OBJECT(be->da),"enter-notify-event",
  		   G_CALLBACK(enter_img_cb),be);
  g_signal_connect(G_OBJECT(be->da),"leave-notify-event",
  		   G_CALLBACK(leave_img_cb),be);
  gtk_widget_add_events(be->da,GDK_POINTER_MOTION_MASK|
			GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK|
			GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_sensitive(be->da,1);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(w),be->da);
  g_signal_connect(G_OBJECT(w),"key-press-event",
  		   G_CALLBACK(key_press_scroll_cb),be);


  gtk_paned_add2(GTK_PANED(hpan),w);

  w = scc_boxedit_list_new(be);
  gtk_paned_add1(GTK_PANED(hpan),w);
  gtk_box_pack_start(GTK_BOX(box),hpan,1,1,0);

  be->sbar = gtk_statusbar_new();
  be->sbar_ctx_coord = gtk_statusbar_get_context_id(GTK_STATUSBAR(be->sbar),"Coordinates");
  be->sbar_ctx_msg = gtk_statusbar_get_context_id(GTK_STATUSBAR(be->sbar),"Messages");
  gtk_box_pack_start(GTK_BOX(box),be->sbar,0,0,0);

  gtk_container_add(GTK_CONTAINER(be->win),box);

  gtk_widget_set_size_request(be->win,800,500);

  be->scale_win = scc_boxedit_scale_win_new(be);

  scc_boxedit_set_zoom(be,1);

  gtk_widget_show_all(be->win);
  
  return be;
}

static void usage(char* prog) {
  printf("Usage: %s [-img file.bmp] [file.boxd]\n",prog);
  exit(-1);
}

static char* rmim_file = NULL;
static char* scal_file = NULL;

static scc_param_t boxedit_params[] = {
  { "img", SCC_PARAM_STR, 0, 0, &rmim_file },
  { "scal", SCC_PARAM_STR, 0, 0, &scal_file },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_cl_arg_t* files = NULL;
  scc_img_t* rmim = NULL;
  scc_boxedit_t* be;

  // let gtk pump his args
  gtk_init(&argc,&argv);

  if(argc > 1)
    files = scc_param_parse_argv(boxedit_params,argc-1,&argv[1]);

  if(rmim_file) {
    rmim = scc_img_open(rmim_file);
    if(!rmim) printf("Failed to load %s.\n",rmim_file);
  }

  be = scc_boxedit_new(rmim);
  
  if(files && !scc_boxedit_load_box(be,files->val))
    printf("Failed to load %s.\n",files->val);

  if(scal_file) {
    scc_fd_t* fd = new_scc_fd(scal_file,O_RDONLY,0);
    if(!fd)
      printf("Failed to open %s.\n",scal_file);
    else {
      if(!scc_boxedit_read_scale(be,fd))
        printf("Failed to load scal file %s.\n",scal_file);
      scc_fd_close(fd);
    }
  }

  gtk_main();
  return 0;
}
