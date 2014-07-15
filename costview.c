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
 * @file costview.c
 * @brief Gtk based costume viewer
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#include "scc_fd.h"
#include "scc_util.h"
#include "scc_cost.h"
#include "scc_img.h"
#include "scc.h"
#include "scc_param.h"

#include "costview_help.h"

#include <gtk/gtk.h>


typedef struct cost_view_anim_def {
  int width,height;
  int step_width;
  int start,end;

  int drag;

  uint16_t* pc;
  scc_cost_anim_def_t* def;
} cost_view_anim_def_t;

static int motion_anim_def_cb(GtkWidget *area,GdkEventMotion *ev,
			      cost_view_anim_def_t* d) {
  int r = 0;
  if(!d->drag) return 0;

  //printf("Motion: %d %d => [%d %d]\n",ev->x,ev->y,d->start,d->end);

  if(d->drag == 1) { // start side
    if(ev->x < d->start-(3*d->step_width/4) &&
       d->start-d->step_width >= 0)
      d->start -= d->step_width, r = 1;
    else if(ev->x > d->start+(3*d->step_width/4) &&
	    d->start+d->step_width < d->end)
      d->start += d->step_width, r = 1;
  } else {
    if(ev->x < d->end-(3*d->step_width/4) &&
       d->end-d->step_width > d->start)
      d->end -= d->step_width, r = 1;
    else if(ev->x > d->end + (3*d->step_width/4) &&
	    d->end+d->step_width < d->width)
      d->end += d->step_width, r = 1;
  }

  if(r) {
    d->def->start = (d->start-2)/d->step_width;
    d->def->end = (d->end-d->step_width-2)/d->step_width;

    if(*d->pc < d->def->start) *d->pc = d->def->start;
    if(*d->pc > d->def->end) *d->pc = d->def->end;

    gtk_widget_queue_draw(area);
  }
  return 0;
}

static int btn_press_anim_def_cb(GtkWidget *area,GdkEventButton *ev,
				 cost_view_anim_def_t* d) {
  printf("Press %d\n",ev->button);
  if(ev->button != 1) return 0;

  if(ev->y < 2 || ev->y >= d->height-2 ||
     ev->x < d->start-2 || ev->x > d->end+2) return 0;

  if(ev->x <= d->start+8)
    d->drag = 1;
  else if(ev->x >= d->end-8)
    d->drag = 2;

  return 0;
}

static int btn_release_anim_def_cb(GtkWidget *area,GdkEventButton *ev,
				 cost_view_anim_def_t* d) {
  if(ev->button != 1) return 0;

  d->drag = 0;

  return 0;
}

static void expose_anim_def_cb(GtkWidget *area,GdkEvent* ev,
			       cost_view_anim_def_t* d) {
  int i;
  GdkRectangle rec;

  if(d->start >= d->width) return;

  if(d->end >= d->width) d->end = d->width-1;

  rec.x = 0;
  rec.y = 0;
  rec.width = d->width;
  rec.height = d->height;

  for(i = 2+d->step_width/2 ; i < d->width ; i += d->step_width)
    gdk_draw_line(area->window,
		   area->style->fg_gc[GTK_WIDGET_STATE(area)],
		   i,0,i,d->height);

  gtk_paint_box(area->style,
		area->window,
		GTK_WIDGET_STATE(area),
		GTK_SHADOW_OUT,
		&rec,
		area,
		NULL,
		d->start,2,
		d->end-d->start,d->height-4);
  gtk_paint_resize_grip(area->style,
			area->window,
			GTK_WIDGET_STATE(area),
			&rec,
			area,
			NULL,
			GDK_WINDOW_EDGE_WEST,
			d->start+2,4,
			2,d->height-10);
  gtk_paint_resize_grip(area->style,
			area->window,
			GTK_WIDGET_STATE(area),
			&rec,
			area,
			NULL,
			GDK_WINDOW_EDGE_EAST,
			d->end-4,4,
			2,d->height-10);

  if(d->pc) {
    uint16_t pc = *d->pc;

    if(pc >= d->def->start && pc <= d->def->end)
      gtk_paint_vline(area->style,
		      area->window,
		      GTK_STATE_SELECTED,
		      &rec,
		      area,
		      NULL,
		      4,d->height-6,pc*d->step_width+(d->step_width/2));
  }
}

void destroy_anim_def_cb(GtkWidget* area,cost_view_anim_def_t* d) {
  if(d) free(d);
}

GtkWidget* cost_view_anim_def_new(scc_cost_anim_def_t* def,uint16_t* pc,
				  int width,int height,int steps) {
  GtkWidget* area;
  cost_view_anim_def_t* d = calloc(1,sizeof(cost_view_anim_def_t));

  d->pc = pc;
  d->def = def;

  d->width = width;
  d->height = height;

  area = gtk_drawing_area_new();
  gtk_widget_set_size_request(area,width,height);
  g_signal_connect(G_OBJECT(area),"expose-event",
		   G_CALLBACK(expose_anim_def_cb),d);
  g_signal_connect(G_OBJECT(area),"motion-notify-event",
		   G_CALLBACK(motion_anim_def_cb),d);
  g_signal_connect(G_OBJECT(area),"button-press-event",
		   G_CALLBACK(btn_press_anim_def_cb),d);
  g_signal_connect(G_OBJECT(area),"button-release-event",
		   G_CALLBACK(btn_release_anim_def_cb),d);
  g_signal_connect(G_OBJECT(area),"destroy",
  		   G_CALLBACK(destroy_anim_def_cb),d);
  gtk_widget_add_events(area,GDK_POINTER_MOTION_MASK|
			GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK);
  d->step_width = steps;

  d->start = 2+d->def->start*d->step_width;
  d->end = 2+(d->def->end+1)*d->step_width;

  return area;
}


typedef struct cost_view_anim_def_tbl {
  scc_cost_dec_t* dec;
  cost_view_anim_def_t* limbs[16];
  int step_size;
  GtkWidget *cmd_win;
  GtkWidget *def_win;
  GtkWidget* loop_win;
  GtkAdjustment* hadj;
} cost_view_anim_def_tbl_t;

GtkWidget* cost_view_anim_def_tbl_header(scc_cost_dec_t* dec,int step_size) {
  GtkWidget *box,*lbl;
  int i;
  char buf[20];

  box = gtk_hbox_new(0,0);
  for(i = 0 ; i < dec->cost->cmds_size ; i++) {
    sprintf(buf,"%u",dec->cost->cmds[i]);
    lbl = gtk_label_new(buf);
    gtk_widget_set_size_request(lbl,step_size,20);
    gtk_label_set_justify(GTK_LABEL(lbl),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box),lbl,0,0,0);
  }

  return box;
}

static void toogle_anim_loop_cb(GtkToggleButton* btn,uint8_t* flags) {
  int mode = gtk_toggle_button_get_active(btn);

  if(mode)
    *flags |= 1;
  else
    *flags &= ~1;
}

void cost_view_anim_def_load_anim(GtkWidget* tbl) {
  cost_view_anim_def_tbl_t* p = g_object_get_data(G_OBJECT(tbl),"priv");
  GtkWidget *box,*w;
  int i;

  if(GTK_BIN(p->cmd_win)->child)
    gtk_object_destroy(GTK_OBJECT(GTK_BIN(p->cmd_win)->child));

  if(GTK_BIN(p->def_win)->child)
    gtk_object_destroy(GTK_OBJECT(GTK_BIN(p->def_win)->child));

  if(GTK_BIN(p->loop_win)->child)
    gtk_object_destroy(GTK_OBJECT(GTK_BIN(p->loop_win)->child));

  gtk_container_add(GTK_CONTAINER(p->cmd_win),
		    cost_view_anim_def_tbl_header(p->dec,p->step_size));
  gtk_widget_show_all(p->cmd_win);

  box = gtk_vbox_new(0,0);
  for(i = 0 ; i < 16 ; i++) {
    if(p->dec->anim->limb[i].start == 0xFFFF) continue;
    gtk_box_pack_start(GTK_BOX(box),
		       cost_view_anim_def_new(&p->dec->anim->limb[i],
					      &p->dec->pc[i],
					      (p->step_size+1)*
					      p->dec->cost->cmds_size,
					      20,p->step_size),
		       0,0,0);
  }
  gtk_container_add(GTK_CONTAINER(p->def_win),box);
  gtk_widget_show_all(p->def_win);

  box = gtk_vbox_new(0,0);
  for(i = 0 ; i < 16 ; i++) {
    if(p->dec->anim->limb[i].start == 0xFFFF) continue;
    w = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
				 p->dec->anim->limb[i].flags);
    g_signal_connect(G_OBJECT(w),"toggled",
		     G_CALLBACK(toogle_anim_loop_cb),
		     &p->dec->anim->limb[i].flags);
    gtk_widget_set_size_request(w,10,20);
    gtk_box_pack_start(GTK_BOX(box),w,0,0,0);
  }
  gtk_container_add(GTK_CONTAINER(p->loop_win),box);
  gtk_widget_show_all(p->loop_win);
}

GtkWidget* cost_view_anim_def_tbl_new(scc_cost_dec_t* dec,int step_size) {
  cost_view_anim_def_tbl_t* p = calloc(1,sizeof(cost_view_anim_def_tbl_t));
  GtkWidget *vbox,*hbox,*scr1,*scr2,*w;
  GtkObject *hadj;

  p->dec = dec;
  p->step_size = step_size;

  hbox = gtk_hbox_new(0,0);
  g_object_set_data(G_OBJECT(hbox),"priv",p);

  vbox = gtk_vbox_new(0,0);

  hadj = gtk_adjustment_new(0,0,step_size*100,
			    step_size/4,step_size,step_size);
  scr1 = gtk_scrolled_window_new(GTK_ADJUSTMENT(hadj),NULL);
  //scr1 = gtk_scrolled_window_new(NULL,NULL);

  p->cmd_win = gtk_viewport_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(scr1),p->cmd_win);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr1),
				 GTK_POLICY_NEVER,
				 GTK_POLICY_NEVER);

  gtk_box_pack_start(GTK_BOX(vbox),scr1,0,0,0);				 


  scr2 = gtk_scrolled_window_new(GTK_ADJUSTMENT(hadj),NULL);
  //scr2 = gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr2),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_NEVER);
  p->def_win = gtk_viewport_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(scr2),p->def_win);

  gtk_box_pack_start(GTK_BOX(vbox),scr2,1,1,0);

  gtk_box_pack_start(GTK_BOX(hbox),vbox,1,1,0);

  
  vbox = gtk_vbox_new(0,0);
  w = gtk_label_new("Loop");
  gtk_widget_set_size_request(w,20,24);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,0);


  scr1 = gtk_scrolled_window_new(NULL,
  				 gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scr2)));
  //scr1 = gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr1),
				 GTK_POLICY_NEVER,
				 GTK_POLICY_AUTOMATIC);
  p->loop_win = gtk_viewport_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(scr1),p->loop_win);
  gtk_box_pack_start(GTK_BOX(vbox),scr1,1,1,0);

  w = gtk_drawing_area_new();
  gtk_widget_set_size_request(w,20,19);
  gtk_box_pack_start(GTK_BOX(vbox),w,0,0,0);

  gtk_box_pack_start(GTK_BOX(hbox),vbox,0,0,0);

  gtk_widget_set_size_request(hbox,600,100);

  return hbox;
}



typedef struct scc_cost_view {
  scc_pal_t *pal; // the palette
  scc_cost_dec_t dec; // decoder
  //  scc_cost_t* cost; // loaded costume
  //  scc_cost_anim_t* anim; // loaded anim


  GtkWidget* win;

  GtkWidget* anim_combo;
  GtkListStore* anim_store;
  
  GtkWidget* limb_combo;
  GtkListStore* limb_store;

  GtkWidget* pic_list;
  GtkListStore* pic_store;

  GtkWidget* anim_img;
  GtkWidget* limb_img;

  GtkWidget* anim_def_tbl;


  uint8_t* buf;
  int buf_w,buf_h;

  GdkGC* img_gc;
  GdkRgbCmap *img_cmap;

  char play;
} scc_cost_view_t;

static void scc_cost_view_load_anim(scc_cost_view_t* cv,uint16_t aid) {
  GtkAdjustment* adj;
  int old_aid = (cv->dec.anim) ? cv->dec.anim->id : -1;

  if(!scc_cost_dec_load_anim(&cv->dec,aid)) {
    printf("Anim %d not found\n",aid);
    return;
  }

  cost_view_anim_def_load_anim(cv->anim_def_tbl);

  if(old_aid < 0 || old_aid != cv->dec.anim->id) {
    adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(cv->anim_img->parent->parent));
    gtk_adjustment_set_value(adj,(adj->upper-adj->page_size-adj->lower)/2+
			     adj->lower);
    adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(cv->anim_img->parent->parent));
    gtk_adjustment_set_value(adj,(adj->upper-adj->page_size-adj->lower)/2+
			     adj->lower);
  
  }

  if(cv->buf)
    memset(cv->buf,0,cv->buf_w*cv->buf_h);
  gtk_widget_queue_draw(cv->anim_img);

}

static void anim_selected_cb(GtkWidget *entry,scc_cost_view_t* cv) {
  int id;

  id = atoi(gtk_editable_get_chars(GTK_EDITABLE(entry),0,-1));
  if(id < 0) {
    printf("Invalid anim number\n");
    return;
  }

  scc_cost_view_load_anim(cv,id);
}

int play_timeout_cb(scc_cost_view_t* cv) {
  if(cv->play <= 0) {
    cv->play = -1;
    return FALSE;
  }
  
  scc_cost_dec_step(&cv->dec);  
  gtk_widget_queue_draw(cv->anim_img);
  gtk_widget_queue_draw(cv->anim_def_tbl);

  return TRUE;
}

static void anim_play_cb(GtkWidget *tbtn,scc_cost_view_t* cv) {
  if(!cv->dec.anim) return;

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tbtn))) {
    if(cv->play > 0) return;
    cv->play = 1;
    g_timeout_add(200,(GSourceFunc)play_timeout_cb,cv);
    
  } else
    cv->play = 0;
}

 

static void scc_cost_view_load_pic(scc_cost_view_t* cv,scc_cost_pic_t *pic) {

  GtkTreeIter iter;

  gtk_list_store_clear(cv->pic_store);

  while(pic) {
    gtk_list_store_append(cv->pic_store,&iter);
    gtk_list_store_set(cv->pic_store,&iter,
		       0,pic->id,
		       1,pic->width,
		       2,pic->height,
		       3,pic->rel_x,
		       4,pic->rel_y,
		       -1);
    pic = pic->next;
  }
  
}

static void limb_selected_cb(GtkWidget *entry,scc_cost_view_t* cv) {
  int id;

  id = atoi(gtk_editable_get_chars(GTK_EDITABLE(entry),0,-1));
  if(id < 0 || id > 15) {
    printf("Invalid limb number\n");
    return;
  }


  scc_cost_view_load_pic(cv,cv->dec.cost->limb_pic[id]);
  
}

static void limb_pic_selected_cb(GtkWidget* pic_list,GtkTreePath* path,
				 GtkTreeViewColumn* col,
				 scc_cost_view_t* cv) {
  gtk_widget_queue_draw(cv->limb_img);
}

GtkWidget* create_limb_select(scc_cost_view_t* cv) {
  GtkWidget* vbox;
  GtkWidget* hbox;
  GtkWidget* w, *scroll;
  GtkCellRenderer* render = gtk_cell_renderer_text_new();

  vbox = gtk_vbox_new(0,2);
  
  hbox = gtk_hbox_new(0,2);

  w = gtk_label_new("Limb");
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);

  cv->limb_store = gtk_list_store_new(2,G_TYPE_INT,G_TYPE_STRING);

  cv->limb_combo = gtk_combo_box_entry_new_with_model(GTK_TREE_MODEL(cv->limb_store),1);
  g_signal_connect(G_OBJECT(GTK_BIN(cv->limb_combo)->child),"activate",
		   G_CALLBACK(limb_selected_cb),cv);
  gtk_box_pack_start(GTK_BOX(hbox),cv->limb_combo,0,0,5);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);
  
  
  // The list
  scroll = gtk_scrolled_window_new(NULL,NULL);
  gtk_container_set_border_width(GTK_CONTAINER(scroll),5);
  cv->pic_store = gtk_list_store_new(7,G_TYPE_INT,
				      G_TYPE_INT,G_TYPE_INT,
				      G_TYPE_INT,G_TYPE_INT,
				      G_TYPE_INT,G_TYPE_INT);

  cv->pic_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(cv->pic_store));

  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Idx",render,
					      "text",0,NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Width",render,
					      "text",1,NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Height",render,
					      "text",2,NULL);

  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"X",render,
					      "text",3,NULL);  
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Y",render,
					      "text",4,NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Move X",render,
					      "text",5,NULL);
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(cv->pic_list),
					      -1,"Move Y",render,
					      "text",6,NULL);

  g_signal_connect(G_OBJECT(cv->pic_list),"row-activated",
		   G_CALLBACK(limb_pic_selected_cb),cv);

  gtk_container_add(GTK_CONTAINER(scroll),cv->pic_list);
  gtk_box_pack_start(GTK_BOX(vbox),scroll,1,1,5);

    // The last row of buttons
  hbox = gtk_hbox_new(0,2);

  w = gtk_button_new_with_label("Edit");
  gtk_box_pack_end(GTK_BOX(hbox),w,0,0,5);

  w = gtk_button_new_with_label("Remove");
  gtk_box_pack_end(GTK_BOX(hbox),w,0,0,5);

  w = gtk_button_new_with_label("Add");
  gtk_box_pack_end(GTK_BOX(hbox),w,0,0,5);
  
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);
  

  w = gtk_frame_new("Pictures");
  gtk_container_add(GTK_CONTAINER(w),vbox);

  return w;
}

static void expose_img_cb(GtkWidget *img,GdkEvent* ev,scc_cost_view_t* cv) {
  //int x1,x2,y1,y2;

  //if(!scc_cost_dec_bbox(&cv->dec,&x1,&y1,&x2,&y2)) return;
  //w = x2-x1;
  //h = y2-y1;
  
  if(!cv->buf || cv->buf_h/10 != img->allocation.height/10  || 
     cv->buf_w/10 < img->allocation.width/10) {
    cv->buf_h = (img->allocation.height/10)*10;
    cv->buf_w = (img->allocation.width/10)*10;
    cv->buf = realloc(cv->buf,cv->buf_h*cv->buf_w);
    
  }

  if(!cv->img_gc) cv->img_gc = gdk_gc_new(img->window);

  memset(cv->buf,0,cv->buf_h*cv->buf_w);
  scc_cost_dec_frame(&cv->dec,cv->buf,cv->buf_w/2,cv->buf_h/2,
		     cv->buf_w,cv->buf_h,cv->buf_w,NULL,0,255,255);

  //printf("Draw Image !!!\n");

  gdk_draw_indexed_image(img->window,cv->img_gc,
			 (img->allocation.width%10)/2,
			 (img->allocation.height%10)/2,
			 cv->buf_w,cv->buf_h,GDK_RGB_DITHER_NONE,
			 cv->buf,cv->buf_w,cv->img_cmap);
  
}

static void expose_limb_img_cb(GtkWidget *img,GdkEvent* ev,scc_cost_view_t* cv) {
  GtkTreeIter iter;
  GtkTreeSelection* psel = gtk_tree_view_get_selection(GTK_TREE_VIEW(cv->pic_list));
  int id,pid;
  scc_cost_pic_t* pic;

  if(!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(cv->limb_combo),&iter))
    return;
  gtk_tree_model_get(GTK_TREE_MODEL(cv->limb_store),&iter,0,&id,-1);

  if(!gtk_tree_selection_get_selected(psel,(GtkTreeModel**)&cv->pic_store,&iter))
    return;

  
  gtk_tree_model_get(GTK_TREE_MODEL(cv->pic_store),&iter,0,&pid,-1);
  
  pic = scc_cost_get_limb_pic(cv->dec.cost,id,pid,4);

  if(!pic) {
    printf("Failed to find the right picture!!!\n");
    return;
  } else {
    uint8_t buf[pic->width*pic->height];
    scc_cost_decode_pic(cv->dec.cost,pic,buf,pic->width,NULL,0,
			0,pic->width,0,pic->height,-1,255,255,0);

    gdk_draw_indexed_image(img->window,cv->img_gc,
			   (img->allocation.width%10)/2,
			   (img->allocation.height%10)/2,
			   pic->width,pic->height,GDK_RGB_DITHER_NONE,
			   buf,pic->width,cv->img_cmap);
  }

}

GtkWidget* create_anim_view(scc_cost_view_t* cv) {
  GtkWidget *pan,*vbox,*hbox;
  GtkWidget* w;

  pan = gtk_vpaned_new();

  vbox = gtk_vbox_new(0,2);
  hbox = gtk_hbox_new(0,2);

  w = gtk_label_new("Anim:");
  gtk_box_pack_start(GTK_BOX(hbox),w,0,0,5);
  cv->anim_store = gtk_list_store_new(2,G_TYPE_INT,G_TYPE_STRING);
  cv->anim_combo = gtk_combo_box_entry_new_with_model(GTK_TREE_MODEL(cv->anim_store),1);
  gtk_box_pack_start(GTK_BOX(hbox),cv->anim_combo,0,0,5);
  g_signal_connect(G_OBJECT(GTK_BIN(cv->anim_combo)->child),"activate",
		   G_CALLBACK(anim_selected_cb),cv);

  w = gtk_toggle_button_new_with_label("Play");
  gtk_box_pack_end(GTK_BOX(hbox),w,0,0,5);
  g_signal_connect(G_OBJECT(w),"toggled",
		   G_CALLBACK(anim_play_cb),cv);

  
  gtk_box_pack_start(GTK_BOX(vbox),hbox,0,0,5);

  cv->anim_def_tbl = cost_view_anim_def_tbl_new(&cv->dec,50);
 
  gtk_box_pack_start(GTK_BOX(vbox),cv->anim_def_tbl,1,1,5);

  gtk_paned_add1(GTK_PANED(pan),vbox);

  w = gtk_scrolled_window_new(NULL,NULL);

  cv->anim_img = gtk_drawing_area_new();
  gtk_widget_set_size_request(cv->anim_img,600,500);
  g_signal_connect(G_OBJECT(cv->anim_img),"expose-event",
		   G_CALLBACK(expose_img_cb),cv);

  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(w),cv->anim_img);

  gtk_paned_add2(GTK_PANED(pan),w);

  return pan;
}

static int quit_cb(GtkWidget *widget,GdkEvent *event,gpointer user_data) {
  gtk_main_quit();
  return 0;
}


scc_cost_view_t* create_win(void) {
  scc_cost_view_t* cv = calloc(1,sizeof(scc_cost_view_t));
  GtkWidget* hpan,*vpan;
  GtkWidget *w;

  cv->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(cv->win),"destroy-event",
  		   G_CALLBACK(quit_cb),NULL);
  g_signal_connect(G_OBJECT(cv->win),"delete-event",
  		   G_CALLBACK(quit_cb),NULL);
  gtk_container_set_border_width(GTK_CONTAINER(cv->win),5);

  hpan = gtk_hpaned_new();
  vpan = gtk_vpaned_new();

  w = create_limb_select(cv);
  gtk_paned_add1(GTK_PANED(vpan),w);
  
  w = gtk_scrolled_window_new(NULL,NULL);
  cv->limb_img = gtk_drawing_area_new();
  gtk_widget_set_size_request(cv->limb_img,200,200);
  g_signal_connect(G_OBJECT(cv->limb_img),"expose-event",
  		   G_CALLBACK(expose_limb_img_cb),cv);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(w),cv->limb_img);
  gtk_paned_add2(GTK_PANED(vpan),w);


  gtk_paned_add1(GTK_PANED(hpan),vpan);

  w = create_anim_view(cv);
  gtk_paned_add2(GTK_PANED(hpan),w);

  gtk_container_add(GTK_CONTAINER(cv->win),hpan);

  gtk_widget_set_size_request(cv->win,600,400);
  gtk_widget_show_all(cv->win);
  
  return cv;
}

void scc_cost_view_load_pal(scc_cost_view_t* cv,scc_pal_t* pal) {
  int i;

  if(cv->img_cmap) gdk_rgb_cmap_free(cv->img_cmap);

  cv->img_cmap = malloc(sizeof(GdkRgbCmap));

  cv->img_cmap->n_colors = 256;

  for(i = 0; i < 256 ; i++)
    cv->img_cmap->colors[i] = ((pal->r[i])<<16) | ((pal->g[i])<<8) | pal->b[i];

}

int scc_cost_view_load(scc_cost_view_t* cv,scc_pal_t* pal,scc_cost_t* cost) {
  scc_cost_anim_t* a;
  GtkTreeIter iter;
  char tmp[20];
  int l;

  scc_cost_dec_init(&cv->dec);

  scc_cost_view_load_pal(cv,pal);

  for(a = cost->anims ; a ; a = a->next) {
    gtk_list_store_append(cv->anim_store,&iter);
    sprintf(tmp,"%u",a->id);
    gtk_list_store_set(cv->anim_store,&iter,0,a->id,1,tmp,-1);
  }

  for(l = 0 ; l < 16 ; l++) {
    if(!cost->limb_pic[l]) continue;
    gtk_list_store_append(cv->limb_store,&iter);
    sprintf(tmp,"%u",l);
    gtk_list_store_set(cv->limb_store,&iter,0,l,1,tmp,-1);
  }

  cv->pal = pal;
  cv->dec.cost = cost;

  return 0;
}

static scc_pal_t* open_pals_file(char* path) {
  scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
  uint32_t type,len;
  scc_pal_t* pal;

  if(!fd) {
    printf("Failed to open %s.\n",path);
    return NULL;
  }

  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);

  if(type != MKID('P','A','L','S')) {
    printf("%s is not a pals file.\n",path);
    scc_fd_close(fd);
    return NULL;
  }

  pal = scc_parse_pals(fd,len-8);
  
  scc_fd_close(fd);
  return pal;
}

static scc_cost_t* open_cost_file(char * path) {
  scc_fd_t* fd = new_scc_fd(path,O_RDONLY,0);
  uint32_t type,len;
  scc_cost_t* cost;

  if(!fd) {
    printf("Failed to open %s.\n",path);
    return NULL;
  }

  type = scc_fd_r32(fd);
  len = scc_fd_r32be(fd);

  if(type != MKID('C','O','S','T')) {
    printf("%s is not a costume file.\n",path);
    scc_fd_close(fd);
    return NULL;
  }

  cost = scc_parse_cost(fd,len-8);
  
  scc_fd_close(fd);
  return cost;
}

static char* pals_path = NULL;
static char* bmp_path = NULL;

static scc_param_t costview_params[] = {
  { "pals", SCC_PARAM_STR, 0, 0, &pals_path },
  { "bmp", SCC_PARAM_STR, 0, 0, &bmp_path },
  { "help", SCC_PARAM_HELP, 0, 0, &costview_help },
  { NULL, 0, 0, 0, NULL }
};

int main(int argc,char** argv) {
  scc_cl_arg_t* files;
  scc_cost_view_t* cv;
  scc_cost_t* cost;
  scc_pal_t* pal;

  // make gtk pump his args
  gtk_init(&argc,&argv);

  files = scc_param_parse_argv(costview_params,argc-1,&argv[1]);
  if(!files) scc_print_help(&costview_help,1);

  cv = create_win();

  if(bmp_path) {
    int i;
    scc_img_t* img = scc_img_open(bmp_path);
    if(!img) {
      scc_log(LOG_ERR,"Failed to open %s.\n",bmp_path);
      return -1;
    }
    if (img->pal == NULL) {
      scc_log(LOG_ERR,"Image %s must have a palette.\n",bmp_path);
      scc_img_free(img);
      return -1;
    }
    pal = calloc(1,sizeof(scc_pal_t));
    for(i = 0 ; i < img->ncol && i < 256 ; i++) {
      pal->r[i] = img->pal[3*i];
      pal->g[i] = img->pal[3*i+1];
      pal->b[i] = img->pal[3*i+2];
    }
    scc_img_free(img);
  } else {
    if(!pals_path) {
      scc_log(LOG_WARN,"No palette given on the command line, using default.pals.\n");
      pals_path = "default.pals";
    }
    pal = open_pals_file(pals_path);
    if(!pal) return -1;
  }
  
  cost = open_cost_file(files->val);
  if(!cost) return -1;
   
  scc_cost_view_load(cv,pal,cost);

  gtk_main();
  return 0;
}
