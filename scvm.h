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

/// @defgroup scvm SCVM
/**
 * @file scvm.h
 * @ingroup scvm
 * @brief A primitive SCUMM VM implementation.
 */

struct scc_charmap_st;

#define SCVM_ARRAY_BIT    0
#define SCVM_ARRAY_NIBBLE 1
#define SCVM_ARRAY_BYTE   2
#define SCVM_ARRAY_WORD   3

#define SCVM_MAX_ZPLANE  16

typedef struct scvm_array {
  unsigned type; // var type
  unsigned size; // num elements
  unsigned line_size; // for 2 dimension arrays
  union {
    uint16_t *word;
    uint8_t *byte;
  } data;
} scvm_array_t;

/// Number of colors in a palette
#define SCVM_PALETTE_SIZE 256

typedef struct scvm_color {
  uint8_t r,g,b;
} scvm_color_t;

typedef scvm_color_t scvm_palette_t[SCVM_PALETTE_SIZE];

typedef struct scvm_image {
  uint8_t* data;
  uint8_t** zplane;  
  unsigned have_trans;
} scvm_image_t;

#define SCVM_CHAR_MAX_ARGS              2

#define SCVM_CHAR_ESCAPE                0xFF

#define SCVM_CHAR_NEW_LINE              0x01
#define SCVM_CHAR_KEEP                  0x02
#define SCVM_CHAR_WAIT                  0x03
#define SCVM_CHAR_INT_VAR               0x04
#define SCVM_CHAR_VERB                  0x05
#define SCVM_CHAR_NAME                  0x06
#define SCVM_CHAR_STRING_VAR            0x07
#define SCVM_CHAR_ACTOR_ANIM            0x09
#define SCVM_CHAR_VOICE                 0x0A
#define SCVM_CHAR_COLOR                 0x0B
#define SCVM_CHAR_CHARSET               0x0E

#define SCVM_STRING_CENTER                 1

typedef struct scvm_string_dc_st {
    struct scc_charmap_st*   chset;
    int8_t                   pal[15];
    unsigned                 flags;
    int                      pen_x;
    int                      pen_y;
} scvm_string_dc_t;

int scvm_getc(unsigned char* str, unsigned* c, int* arg);

int scvm_strlen(unsigned char* str);

int scvm_init_string_dc(scvm_t* vm,scvm_string_dc_t* dc,unsigned chset_no);

void scvm_string_dc_copy(scvm_string_dc_t* to, const scvm_string_dc_t* from);

int scvm_string_get_size(scvm_t* vm, scvm_string_dc_t* dc,
                         unsigned char* str,
                         unsigned* p_width, unsigned* p_height);

int scvm_string_draw(scvm_t* vm, scvm_string_dc_t* dc,
                     unsigned char* str,
                     uint8_t* dst, int dst_stride,
                     int dx, int dy, int clip_w, int clip_h);

int scvm_string_expand(scvm_t* vm, unsigned char* str,
                       unsigned char* dst, unsigned size);

typedef struct scvm_object_pdata {
  unsigned klass;
  unsigned owner;
  unsigned state;
  unsigned char* name;
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
  unsigned num_verb_entries;
  scvm_script_t* script;
  // presistent object data
  scvm_object_pdata_t* pdata;
};

scvm_object_t* scvm_get_object(scvm_t* vm, unsigned id);
int scvm_get_object_name(scvm_t* vm, unsigned id, unsigned char** name);
int scvm_get_object_position(scvm_t* vm, unsigned id, int* x, int* y);
scvm_object_t* scvm_get_object_at(scvm_t* vm, int x, int y);
int scvm_get_object_verb_entry_point(scvm_t* vm, unsigned id,
                                     unsigned verb, unsigned *entry);
int scvm_start_object_script(scvm_t* vm, unsigned id,
                             unsigned verb, unsigned flags,
                             unsigned* args);

// Verb mode
#define SCVM_VERB_HIDE     0
#define SCVM_VERB_SHOW     1
#define SCVM_VERB_DIM      2

// Verb flags
#define SCVM_VERB_CENTER        (1<<0)
#define SCVM_VERB_HAS_IMG       (1<<16)
#define SCVM_VERB_HAS_NAME_IMG  (1<<17)

typedef struct scvm_verb {
  unsigned id;
  unsigned save_id;
  unsigned char* name;
  unsigned width, height;
  int      x,y;
  unsigned color, back_color;
  unsigned hi_color, dim_color;
  unsigned charset;
  unsigned key;
  unsigned mode;
  unsigned flags;
  scvm_image_t img;
} scvm_verb_t;

scvm_verb_t* scvm_verb_init(scvm_verb_t* v, unsigned id, unsigned charset);

scvm_verb_t* scvm_get_verb(scvm_t* vm, unsigned id, unsigned save_id);

scvm_verb_t* scvm_new_verb(scvm_t* vm, unsigned id);

void scvm_kill_verb(scvm_t* vm, unsigned id, unsigned save_id);

int scvm_set_verb_image(scvm_t* vm, unsigned id,
                        unsigned room_id, unsigned obj_id);

scvm_image_t* scvm_get_verb_image(scvm_t* vm, scvm_verb_t* vrb);

void scvm_save_verb(scvm_t* vm, unsigned id, unsigned save_id);

void scvm_restore_verb(scvm_t* vm, unsigned id, unsigned save_id);

scvm_verb_t* scvm_get_verb_at(scvm_t* vm, unsigned x, unsigned y);

typedef struct scvm_sentence {
  unsigned verb;
  unsigned object_a;
  unsigned object_b;
} scvm_sentence_t;

#define SCVM_MAX_SENTENCE 6

int scvm_do_sentence(scvm_t* vm, unsigned vrb, unsigned obj_a,
                     unsigned obj_b);

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
  uint8_t* zplane[SCVM_MAX_ZPLANE];
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

  // boxes
  unsigned num_box;
  scc_box_t* box;
  uint8_t* boxm;
} scvm_room_t;

#define SCVM_ACTOR_IGNORE_BOXES        1

#define SCVM_ACTOR_WALKING_STOPPED     0
#define SCVM_ACTOR_WALKING_INIT        1
#define SCVM_ACTOR_WALKING_FIND_DST    2
#define SCVM_ACTOR_WALKING_TO_DST      3


typedef struct scvm_actor {
  unsigned id;
  unsigned char* name;
  scc_cost_dec_t costdec;
  //scvm_room_t* room;
  int room;
  int x,y;
  unsigned box;
  unsigned width,height;
  unsigned direction;
  unsigned flags;
  
  // the anim step every anim_speed cycles
  unsigned anim_speed, anim_cycle;
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

  unsigned walking;
  int walk_dx, walk_dy, walk_err;
  int walk_to_x, walk_to_y, walk_to_dir, walk_to_box;
  int dstX, dstY, dst_box;
} scvm_actor_t;

void scvm_actor_init(scvm_actor_t* a);
void scvm_actor_set_default_frames(scvm_actor_t* a);
void scvm_actor_put_at(scvm_actor_t* a, int x, int y, unsigned room);
void scvm_actor_walk_to(scvm_actor_t* a, int x, int y, int dir);
void scvm_actor_set_costume(scvm_actor_t* a, scc_cost_t* cost);
void scvm_actor_animate(scvm_actor_t* a, int anim);
void scvm_actor_step_anim(scvm_actor_t* a);
int scvm_put_actor_at(scvm_t* vm, unsigned aid, int x, int y, unsigned rid);
void scvm_put_actors(scvm_t* vm);
void scvm_step_actors(scvm_t* vm);

#define SCVM_VIEW_SHAKE 1
#define SCVM_VIEW_PAN   2

#define SCVM_VIEW_PALETTE_CHANGED (1<<16)

typedef struct scvm_view {
  // room position on the screen
  int room_start,room_end;
  int scroll_left,scroll_right;
  int camera_x, camera_dst_x;
  int screen_width, screen_height;
  unsigned effect;
  unsigned follow;
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
int scvm_abs_position_to_virtual(scvm_t* vm, int* dx, int* dy);
void scvm_pan_camera_to(scvm_t* vm, int x);
int scvm_set_camera_at(scvm_t* vm, int x);
int scvm_move_camera(scvm_t* vm);

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
  int timer_total;
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
  int input_mode;
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


typedef struct scvm_breakpoint {
  unsigned id;
  unsigned room_id;
  unsigned script_id;
  unsigned pos;
} scvm_breakpoint_t;

typedef struct scvm_debug {
  unsigned num_breakpoint;
  scvm_breakpoint_t* breakpoint;

  int (*check_interrupt)(scvm_t* vm);

} scvm_debug_t;

struct scvm_backend_priv;

typedef struct scvm_backend {
    // start the backend
    int (*init)(struct scvm_backend* be);
    void (*uninit)(struct scvm_backend* be);
    // system callback
    unsigned (*get_time)(struct scvm_backend_priv* be);
    int (*random)(struct scvm_backend_priv* be,int min,int max);
    void (*sleep)(struct scvm_backend_priv* be, unsigned time);
    // rendering
    int (*init_video)(struct scvm_backend_priv* be, unsigned width,
                      unsigned height, unsigned bpp);
    void (*update_palette)(struct scvm_backend_priv* be, scvm_color_t* pal);
    void (*draw)(struct scvm_backend_priv* be, scvm_t* vm, scvm_view_t* view);
    void (*flip)(struct scvm_backend_priv* be);
    void (*uninit_video)(struct scvm_backend_priv* be);
    // input
    void (*check_events)(struct scvm_backend_priv* be, scvm_t* vm);

    struct scvm_backend_priv* priv;
} scvm_backend_t;

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

#define SCVM_UNINITED         0
#define SCVM_BOOT             5
#define SCVM_PAUSE            8

#define SCVM_BEGIN_CYCLE      10
#define SCVM_RUNNING          20
#define SCVM_FINISHED_SCRIPTS 30
#define SCVM_FINISH_CYCLE     40

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

#define SCVM_CAMERA_FOLLOW_ACTOR                             300
#define SCVM_CAMERA_START_FOLLOWING_ACTOR                    310
#define SCVM_CAMERA_START_FOLLOWING_ACTOR_IN_ROOM            320

#define SCVM_RUN_INVENTORY                                   400

#define SCVM_CHECK_SENTENCE                                  450
#define SCVM_REMOVE_SENTENCE                                 455
#define SCVM_CHECKED_SENTENCE                                460

#define SCVM_CHECK_INPUT                                     480
#define SCVM_CHECKED_INPUT                                   490
#define SCVM_MOVE_CAMERA                                     500
#define SCVM_WALK_ACTORS                                     510

#define SCVM_RESTART                                        9999
#define SCVM_QUIT                                          10000

// Input zone
#define SCVM_INPUT_VERB   1
#define SCVM_INPUT_ROOM   2
#define SCVM_INPUT_KEY    4

struct scvm {
  // Debuging data
  scvm_debug_t* dbg;

  int boot_param;

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
  // verb
  unsigned num_verb;
  scvm_verb_t *verb;
  scvm_verb_t *current_verb;
  unsigned current_verb_id;
  // sentences
  unsigned num_sentence;
  scvm_sentence_t sentence[SCVM_MAX_SENTENCE];

  // charset
  unsigned num_charset;
  struct scc_charmap_st* current_charset;

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
  unsigned next_room_state;
  // view
  scvm_view_t* view;
  
  // threads
  unsigned state;
  unsigned pause_state;
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

  // input
  int keypress, btnpress;
  int key_state[256/8];
  int btn_state;

  scvm_backend_t* backend;
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
#define SCVM_ERR_NO_ROOM            -19
#define SCVM_ERR_BAD_PALETTE        -20
#define SCVM_ERR_UNINITED_VM        -21
#define SCVM_ERR_VIDEO_MODE         -22
#define SCVM_ERR_BAD_VERB           -23
#define SCVM_ERR_BAD_COLOR          -24
#define SCVM_ERR_TOO_MANY_VERB      -25
#define SCVM_ERR_SENTENCE_OVERFLOW  -26



#define SCVM_NOT_ERR_BASE           (10000)
#define SCVM_NOT_ERR(n)             (-SCVM_NOT_ERR_BASE-(n))
#define SCVM_IS_ERR(n)              ((n) < 0 && (n) > SCVM_NOT_ERR(0))
#define SCVM_IS_NOT_ERR(n)          ((n) >= 0 || (n) <= SCVM_NOT_ERR(0))

#define SCVM_ERR_INTERRUPTED        SCVM_NOT_ERR(0)
#define SCVM_ERR_BREAKPOINT         SCVM_NOT_ERR(1)
#define SCVM_ERR_QUIT               SCVM_NOT_ERR(2)


const char* scvm_state_name(unsigned state);

int scvm_add_breakpoint(scvm_t* vm, unsigned room_id,
                        unsigned script_id, unsigned pos);

int scvm_run_once(scvm_t* vm);

int scvm_run(scvm_t* vm);

unsigned scvm_get_time(scvm_t* vm);

int scvm_random(scvm_t* vm, int min, int max);

int scvm_init_video(scvm_t* vm, unsigned w, unsigned h, unsigned bpp);

void scvm_update_palette(scvm_t* vm, scvm_color_t* pal);

void scvm_draw(scvm_t* vm, scvm_view_t* view);

void scvm_sleep(scvm_t* vm, unsigned delay);

void scvm_flip(scvm_t* vm);

void scvm_uninit_video(scvm_t* vm);

int scvm_debugger(scvm_t* vm);

unsigned scvm_pause(scvm_t* vm);

