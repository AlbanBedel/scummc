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

#define SCVM_ARRAY_BIT    0
#define SCVM_ARRAY_NIBBLE 1
#define SCVM_ARRAY_BYTE   2
#define SCVM_ARRAY_WORD   3


typedef struct scvm_array {
  unsigned type; // var type
  unsigned size; // num elements
  unsigned line_size; // for 2 dimension arrays
  union {
    uint16_t *word;
    uint8_t *byte;
  } data;
} scvm_array_t;


typedef struct scvm_color {
  uint8_t r,g,b;
} PACKED_ATTRIB scvm_color_t;

typedef scvm_color_t scvm_palette_t[256];

typedef struct scvm_image {
  uint8_t* data;
  uint8_t** zplane;  
} scvm_image_t;

typedef struct scvm_object_pdata {
  unsigned klass;
  unsigned owner;
  unsigned state;
  char* name;
} scvm_object_pdata_t;

#define SCVM_OBJ_OBIM 1
#define SCVM_OBJ_OBCD 2

typedef struct scvm_object scvm_object_t;

struct scvm_object {
  unsigned id,loaded;
  unsigned num_image;
  unsigned num_zplane;
  unsigned width,height;
  int x,y;
  unsigned num_hotspot;
  int* hotspot;
  scvm_image_t* image;
  unsigned flags;
  scvm_object_t* parent;
  unsigned actor_dir;
  unsigned* verb_entries;
  scvm_script_t* script;
  // presistent object data
  scvm_object_pdata_t* pdata;
};

typedef struct scvm_cycle {
  unsigned id;
  unsigned delay;
  unsigned flags;
  unsigned start,end;
  unsigned counter;
} scvm_cycle_t;

typedef struct scvm_room {
  unsigned id;
  // graphics
  unsigned width,height;  
  unsigned num_zplane;
  scvm_image_t image;
  unsigned num_palette;
  scvm_palette_t* palette;
  scvm_color_t* current_palette;
  unsigned num_cycle;
  scvm_cycle_t* cycle;
  unsigned trans;
  
  // objects
  unsigned num_object;
  scvm_object_t** object;
  
  // scripts
  scvm_script_t* entry;
  scvm_script_t* exit;
  unsigned num_script;
  scvm_script_t** script;
} scvm_room_t;

typedef struct scvm_actor {
  unsigned id;
  char* name;
  scc_cost_t* costume;
  scvm_room_t* room;
  int x,y;
  unsigned width,height;
  unsigned direction;
  unsigned flags;
  
  unsigned anim_speed;
  unsigned scale_x,scale_y;
  unsigned layer;
  
  unsigned walk_speed_x,walk_speed_y;
  unsigned walk_script;
  unsigned elevation;

  unsigned init_frame;
  unsigned walk_frame;
  unsigned talk_start_frame, talk_end_frame;
  unsigned stand_frame;
  
  
  unsigned talk_color;
  int talk_x,talk_y;
  unsigned talk_script;
} scvm_actor_t;

#define SCVM_VIEW_SHAKE 1

#define SCVM_VIEW_PALETTE_CHANGED (1<<16)

typedef struct scvm_view {
  // room position on the screen
  int room_start,room_end;
  int scroll_left,scroll_right;
  int camera_x;
  int screen_width, screen_height;
  int effect;
  unsigned flags;
  // palette used with the current room
  scvm_palette_t palette;
} scvm_view_t;

int scvm_view_draw(scvm_t* vm, scvm_view_t* view,
                   uint8_t* buffer, int stride,
                   unsigned width, unsigned height);
void scvm_view_scale_palette(scvm_view_t* view,scvm_color_t* palette,
                             unsigned red, unsigned green, unsigned blue,
                             unsigned start, unsigned end);

// variables used for communication between the scripts
// and the VM.
typedef struct scvm_vars {
  // 000
  int keypress;
  int ego;
  int camera_pos_x;
  int have_msg;
  int room;
  int override;
  int machine_speed;
  int me;
  int num_actor;
  int sound_mode;
  // 010
  int current_drive;
  int timer1;
  int timer2;
  int timer3;
  int music_timer;
  int actor_range_min;
  int actor_range_max;
  int camera_min_x;
  int camera_max_x;
  int timer_next;
  // 020
  int virtual_mouse_x;
  int virtual_mouse_y;
  int room_resource;
  int last_sound;
  int cutscene_exit_key;
  int talk_actor;
  int camera_fast_x;
  int camera_script;
  int pre_entry_script;
  int post_entry_script;
  // 030
  int pre_exit_script;
  int post_exit_script;
  int verb_script;
  int sentence_script;
  int inventory_script;
  int cutscene_start_script;
  int cutscene_end_script;
  int charinc;
  int walk_to_object;
  int debug_mode;
  // 040
  int heap_space;
  int room_width;
  int restart_key;
  int pause_key;
  int mouse_x;
  int mouse_y;
  int timer;
  int timer4;
  int soundcard;
  int videomode;
  // 050
  int mainmenu_key;
  int fixed_disk;
  int cursor_state;
  int userput;
  int room_height;
  int unknown1;
  int sound_result;
  int talk_stop_key;
  int unknown2;
  int fade_delay;
  // 060
  int no_subtitles;
  int gui_entry_script;
  int gui_exit_script;
  int unknown3;
  int sound_param[3];
  int mouse_present;
  int memory_performance;
  int video_performance;
  // 070
  int room_flag;
  int game_loaded;
  int new_room;
  int unknown4;
  int left_button_hold;
  int right_button_hold;
  int ems_space;
  int unknown5[13];
  // 090
  int game_disk_msg;
  int open_failed_msg;
  int read_error_msg;
  int pause_msg;
  int restart_msg;
  int quit_msg;
  int save_button;
  int load_button;
  int play_button;
  int cancel_button;
  // 100
  int quit_button;
  int ok_button;
  int save_disk_msg;
  int enter_name_msg;
  int not_saved_msg;
  int not_loaded_msg;
  int save_msg;
  int load_msg;
  int save_menu_title;
  int load_menu_title;
  // 110
  int gui_colors;
  int debug_password;
  int unknown6[5];
  int main_menu_title;
  int random_num;
  int timedate_year;
  // 120
  int unknown7[2];
  int game_version;
  int charset_mask;
  int unknown8;
  int timedate_hour;
  int timedate_minute;
  int unknown9;
  int timedate_day;
  int timedate_month;
  // 130
  //int padding[126];
} PACKED_ATTRIB scvm_vars_t;

// Don't change the first 4 as they match the
// v6 resouces opcodes.
#define SCVM_RES_SCRIPT  0
#define SCVM_RES_SOUND   1
#define SCVM_RES_COSTUME 2
#define SCVM_RES_ROOM    3
#define SCVM_RES_CHARSET 4
#define SCVM_RES_OBJECT  5
#define SCVM_RES_MAX     6

typedef int (*scvm_get_var_f)(struct scvm* vm,unsigned addr);
typedef int (*scvm_set_var_f)(struct scvm* vm,unsigned addr, int val);

// VM states

#define SCVM_BEGIN_CYCLE      10
#define SCVM_RUNNING          20

#define SCVM_START_SCRIPT     100

#define SCVM_OPEN_ROOM        200
#define SCVM_RUN_PRE_EXIT     210
#define SCVM_RUN_EXCD         220
#define SCVM_RUN_POST_EXIT    230
#define SCVM_SETUP_ROOM       240
#define SCVM_RUN_PRE_ENTRY    250
#define SCVM_RUN_ENCD         260
#define SCVM_RUN_POST_ENTRY   270
#define SCVM_OPENED_ROOM      280

struct scvm {
  // variables
  unsigned num_var;
  int *var_mem;
  scvm_vars_t* var;
  scvm_get_var_f get_var[0x100];
  scvm_set_var_f set_var[0x100];
  // bit variables
  unsigned num_bitvar;
  uint8_t *bitvar;
  // arrays
  unsigned num_array;
  scvm_array_t *array;
  
  // ressources
  char *path;
  char *basename;
  unsigned num_file;
  uint8_t file_key;
  scc_fd_t** file;
  scvm_res_type_t res[SCVM_RES_MAX];

  // object's persistant data
  unsigned num_object, num_local_object;
  scvm_object_pdata_t* object_pdata;
  
  // actors
  unsigned num_actor;
  scvm_actor_t* actor;
  scvm_actor_t* current_actor;
  
  // current room
  scvm_room_t* room;
  unsigned next_room;
  // view
  scvm_view_t* view;
  
  // threads
  unsigned state;
  unsigned num_thread;
  unsigned cycle;
  scvm_thread_t* current_thread;
  scvm_thread_t* next_thread;
  scvm_thread_t *thread;
  scvm_op_t* optable;
  scvm_op_t* suboptable;
  unsigned time;

  // stack
  unsigned stack_size;
  unsigned stack_ptr;
  int *stack;
  
  // system callback
  unsigned (*get_time)(scvm_t* vm);
  int (*random)(scvm_t* vm,int min,int max);
};

#define SCVM_ERR_SCRIPT_BOUND    -1
#define SCVM_ERR_NO_OP           -2
#define SCVM_ERR_STACK_OVERFLOW  -3
#define SCVM_ERR_STACK_UNDERFLOW -4
#define SCVM_ERR_BAD_ADDR        -5
#define SCVM_ERR_ARRAY_BOUND     -6
#define SCVM_ERR_ARRAY_TYPE      -7
#define SCVM_ERR_JUMP_BOUND      -8
#define SCVM_ERR_OUT_OF_ARRAY    -9
#define SCVM_ERR_STRING_BOUND    -10
#define SCVM_ERR_BAD_RESOURCE    -11
#define SCVM_ERR_BAD_THREAD      -12
#define SCVM_ERR_BAD_STATE       -13
#define SCVM_ERR_BAD_ACTOR       -14
#define SCVM_ERR_BAD_COSTUME     -15
#define SCVM_ERR_OVERRIDE_OVERFLOW  -16
#define SCVM_ERR_OVERRIDE_UNDERFLOW -17
#define SCVM_ERR_BAD_OBJECT         -18
