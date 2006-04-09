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
} __attribute__ ((__packed__)) scvm_color_t;

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


typedef struct scvm_room {
  unsigned id;
  // graphics
  unsigned width,height;  
  unsigned num_zplane;
  scvm_image_t image;
  unsigned num_palette;
  scvm_palette_t* palette;
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
  unsigned room;
  int x,y;
  unsigned direction;
  unsigned flags;
} scvm_actor_t;

#define SCVM_VIEW_SHAKE 1

typedef struct scvm_view {
  // room position on the screen
  int room_start,room_end;
  int scroll_left,scroll_right;
  int effect;
  unsigned flags;
  // palette used with the current room
  scvm_palette_t palette;
} scvm_view_t;

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

#define SCVM_START_SCRIPT 1
#define SCVM_RUNNING      2

#define SCVM_OPEN_ROOM    3
#define SCVM_RUN_EXCD     4
#define SCVM_SETUP_ROOM   5
#define SCVM_RUN_ENCD     6
#define SCVM_OPENED_ROOM  7

struct scvm {
  // variables
  unsigned num_var;
  int *var;
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

  // stack
  unsigned stack_size;
  unsigned stack_ptr;
  int *stack;
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
