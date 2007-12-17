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
 * @file scvm_dbg.c
 * @ingroup scvm
 * @brief Debugger interface
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "scc_fd.h"
#include "scc_util.h"
#include "scc_param.h"
#include "scc_cost.h"
#include "scc_box.h"
#include "scvm_res.h"
#include "scvm_thread.h"
#include "scvm.h"

typedef struct scvm_debug_cmd {
  char* name, *full_name;
  int (*action)(scvm_t* vm, char* arg);
  void (*usage)(char* args);
  struct scvm_debug_cmd* next;
} scvm_debug_cmd_t;

static int dbg_interrupt = 0;
static scvm_debug_cmd_t debug_cmd[];

static void dbg_interrupt_handler(int sig) {
  printf("Kill debug\n");
  dbg_interrupt = 1;
}

static int check_interrupt(scvm_t* vm) {
  return dbg_interrupt;
}

static scvm_debug_cmd_t* scvm_find_debug_cmd(scvm_t* vm, char* line,
                                             scvm_debug_cmd_t* cmdtable,
                                             char** next_args, int* found) {
  char* cmd, *args, *cmd_end_pos, cmd_end;
  int i,len;
  if(next_args) *next_args = NULL;
  if(found) *found = 0;
  if(!line)
    return NULL;
  for(cmd = line ; isspace(cmd[0]) ; cmd++);
  if(!cmd[0])
    return NULL;
  for(args = cmd+1 ; args[0] && !isspace(args[0]) ; args++);
  cmd_end = args[0];
  cmd_end_pos = args;
  if(args[0]) {
    args[0] = 0;
    for(args++ ; isspace(args[0]) ; args++);
  }
  if(!args[0])
    args = NULL;
  len = strlen(cmd);
  for(i = 0 ; cmdtable[i].name ; i++)
    if(strncmp(cmd,cmdtable[i].name,len) == 0) {
      if(cmdtable[i].next) {
        scvm_debug_cmd_t* next_cmd;
        cmd_end_pos[0] = cmd_end;
        if(!args) {
          if(found && cmdtable[i].action) *found = 1;
          return cmdtable+i;
        }
        next_cmd = scvm_find_debug_cmd(vm,args,cmdtable[i].next,next_args,found);
        if(next_cmd && (!found || *found)) return next_cmd;
        // not found
        if(next_args) *next_args = args;
        return cmdtable+i;
      }
      if(next_args) *next_args = args;
      if(found) *found = 1;
      return cmdtable+i;
    }
  cmd_end_pos[0] = cmd_end;
  return NULL;
}

/////////////// breakpoint ///////////////////////////

static void cmd_breakpoint_usage(char* args) {
  printf("Usage: breakpoint [room] script [pos]\n");
}


static int cmd_breakpoint(scvm_t* vm, char* args) {
  int arg[3], num, id;
  int room = 0, script = 0, pos = 0;
  if(!args) {
    cmd_breakpoint_usage(NULL);
    return 0;
  }
  num = sscanf(args,"%d %d %d",arg,arg+1,arg+2);
  if(num == 3)
    room = arg[0], script = arg[1], pos = arg[2];
  else if(num == 2)
    script = arg[0], pos = arg[1];
  else if(num == 1)
    script = arg[0];
  else {
    cmd_breakpoint_usage(NULL);
    return 0;
  }
  id = scvm_add_breakpoint(vm,room,script,pos);
  if(id < 0) {
    printf("Failed to create breakpoint.\n");
    return 0;
  }
  if(room)
    printf("Added breakpoint %d in room %d, script %d at 0x%X\n",
           id,room,script,pos);
  else
    printf("Added breakpoint %d in script %d at 0x%X\n",
           id,script,pos);
  return 1;
}

/////////////// help ///////////////////////////

static void cmd_help_usage(char* args) {
  printf("Usage: help [ CMD ... ]\n");
}

static int cmd_help(scvm_t* vm, char* args) {
  scvm_debug_cmd_t* cmdtable = debug_cmd, *cmd;
  char* next_args;
  int i;
  if(args && (cmd = scvm_find_debug_cmd(vm,args,debug_cmd,&next_args,NULL))) {
    if(cmd->next) {
      // print subcmd list
      printf("Usage: %s SUBCMD ...\n"
             "where SUBCMD is one of:\n",cmd->full_name);
      cmdtable = cmd->next;
    } else if(cmd->usage) {
      cmd->usage(next_args);
      return 1;
    } else {
      printf("Command is missing a help message.\n");
      return 0;
    }
  } else {
    if(args)
      printf("Unknown command: %s\n",args);
    printf("Commands:\n");
  }

  for(i = 0 ; cmdtable[i].name ; i++)
    printf("  %-20s -\n",cmdtable[i].name);
  return 1;
}

/////////////// quit ///////////////////////////

static void cmd_quit_usage(char* args) {
  printf("Usage: quit\n");
}

static int cmd_quit(scvm_t* vm, char* args) {
  // kill the vm ?
  return -1;
}

///////////// show actor ///////////////////////

static void cmd_show_actor_usage(char* args) {
  printf("Usage: show actor [ actor-addr ]\n");
}

static int cmd_show_actor(scvm_t* vm, char* args) {
  int a;
  scvm_actor_t* actor;
  if(args) {
    if(sscanf(args,"%d",&a) != 1) {
      cmd_show_actor_usage(NULL);
      return 0;
    }
    if(a <= 0 || a >= vm->num_actor) {
      printf("Invalid actor address: %d\n",a);
      return 0;
    }
    actor = &vm->actor[a];
  } else if(!vm->current_actor) {
    printf("Current actor is not set.\n");
    return 0;
  } else
    actor = vm->current_actor;

  printf("Actor %d:\n",actor->id);
  printf("  Name               : %s\n",actor->name);
  if(actor->costdec.cost)
    printf("  Costume            : %d\n",actor->costdec.cost->id);
  if(actor->costdec.anim)
    printf("  Animation          : %d\n",actor->costdec.anim_id);
  printf("  Room               : %d\n",actor->room);
  printf("  Position           : %dx%d\n",actor->x,actor->y);
  printf("  Size               : %dx%d\n",actor->width,actor->height);
  printf("  Direction          : %d\n",actor->direction);
  printf("  Scaling            : %dx%d\n",actor->scale_x,actor->scale_y);
  printf("  Layer              : %d\n",actor->layer);
  printf("  Elevation          : %d\n",actor->elevation);
  printf("\n");
  // flags
  printf("  Anim speed         : %d\n",actor->anim_speed);
  printf("  Anim cycle         : %d\n",actor->anim_cycle);
  printf("  Walk speed         : %dx%d\n",actor->walk_speed_x,
         actor->walk_speed_y);
  printf("  Walk script        : %d\n",actor->walk_script);
  printf("\n");
  printf("  Init frame         : %d\n",actor->init_frame);
  printf("  Walk frame         : %d\n",actor->walk_frame);
  printf("  Talk start frame   : %d\n",actor->talk_start_frame);
  printf("  Talk end frame     : %d\n",actor->talk_end_frame);
  printf("  Stand frame        : %d\n",actor->stand_frame);
  printf("\n");
  printf("  Talk color         : %d\n",actor->talk_color);
  printf("  Talk position      : %dx%d\n",actor->talk_x,actor->talk_y);
  printf("  Talk script        : %d\n",actor->talk_script);
  return 1;
}

///////////// show array ///////////////////////

static void cmd_show_array_usage(char* args) {
  printf("Usage: show array array-addr\n");
}

static int cmd_show_array(scvm_t* vm, char* args) {
  int i, a, val;
  if(!args || sscanf(args,"%d",&a) != 1) {
    cmd_show_array_usage(NULL);
    return 0;
  }
  if(a <= 0 || a >= vm->num_array ||
     vm->array[a].size == 0) {
    printf("Invalid array address: %d\n",a);
    return 0;
  }

  printf("Array %d = { ",a);
  for(i = 0 ; i < vm->array[a].size ; i++)
    if(!scvm_read_array(vm,a,i,0,&val))
      printf("%s%d",i > 0 ? ", " : "",val);
  printf(" }\n");
  return 1;
}

///////////// show avar ///////////////////////

static void cmd_show_avar_usage(char* args) {
  printf("Usage: show avar var-addr\n");
}

static int cmd_show_avar(scvm_t* vm, char* args) {
  int i, var, a, val;
  if(sscanf(args,"local %d",&var) == 1) {
    var &= 0x3FFF;
    var |= 0x4000;
  } else if(sscanf(args,"%d",&var) != 1) {
    cmd_show_avar_usage(NULL);
    return 0;
  }
  if(scvm_thread_read_var(vm,vm->current_thread,var,&a)) {
    printf("Invalid address: %d\n",var);
    return 0;
  }
  if(a <= 0 || a >= vm->num_array ||
     vm->array[a].size == 0) {
    printf("Invalid array address: %d\n",a);
    return 0;
  }

  printf("Array %d = { ",a);
  for(i = 0 ; i < vm->array[a].size ; i++)
    if(!scvm_read_array(vm,a,i,0,&val))
      printf("%s%d",i > 0 ? ", " : "",val);
  printf(" }\n");
  return 1;
}

///////////// show costume ////////////////////
static void cmd_show_costume_usage(char* args) {
  printf("Usage: show costume costume-addr\n");
}

static int cmd_show_costume(scvm_t* vm, char* args) {
  int id;
  scc_cost_t* cost;
  scc_cost_anim_t* ani;
  if(!args || sscanf(args,"%d",&id) != 1) {
    cmd_show_costume_usage(NULL);
    return 0;
  }
  if(!(cost = scvm_load_res(vm,SCVM_RES_COSTUME,id)))
    return 0;
  printf("Costume %d:\n",cost->id);
  printf("  Format              : 0x%X\n",cost->format);
  printf("  Colors              : %d\n",cost->pal_size);
  printf("  Anims               :");
  for(ani = cost->anims ; ani ; ani = ani->next)
    printf(" %d",ani->id);
  printf("\n");
  return 1;
}

///////////// show costume ////////////////////
static void cmd_show_object_usage(char* args) {
  printf("Usage: show object object-addr\n");
}

static int cmd_show_object(scvm_t* vm, char* args) {
  int id,n = 0;
  scvm_object_t* obj;
  if(!args || sscanf(args,"%d",&id) != 1) {
    cmd_show_object_usage(NULL);
    return 0;
  }
  if(!(obj = scvm_load_res(vm,SCVM_RES_OBJECT,id)))
    return 0;

  printf("Object %d:\n",obj->id);
  printf("  Name                : %s\n",obj->pdata->name);
  printf("  Class mask          : 0x%X\n",obj->pdata->klass);
  printf("  Owner               : %d\n",obj->pdata->owner);
  printf("  State               : %d\n",obj->pdata->state);
  printf("  Position            : %dx%d\n",obj->x,obj->y);
  printf("  Size                : %dx%d\n",obj->width,obj->height);
  printf("  Images              : %d\n",obj->num_image);
  printf("  Z planes            : %d\n",obj->num_zplane);
  printf("  Hotspots            :");
  for(n = 0 ; n < obj->num_hotspot ; n++)
    printf(" %dx%d",obj->hotspot[n<<1],obj->hotspot[(n<<1)+1]);
  printf("\n");
  if(obj->parent)
    printf("  Parent              : %d\n",obj->parent->id);
  printf("  Direction           : %d\n",obj->actor_dir);
  printf("  Verbs               :");
  for(n = 0 ; n < obj->num_verb_entries ; n++)
    printf(" %d",obj->verb_entries[n<<1]);
  printf("\n");
  return 1;
}

///////////// show room ///////////////////////

static void cmd_show_room_usage(char* args) {
  printf("Usage: show room [ room-addr ]\n");
}

static int cmd_show_room(scvm_t* vm, char* args) {
  int id;
  scvm_room_t* room;
  if(args) {
    if(sscanf(args,"%d",&id) != 1) {
      cmd_show_avar_usage(NULL);
      return 0;
    }
    if(!(room = scvm_load_res(vm,SCVM_RES_ROOM,id)))
      return 0;
  } else if(!vm->room) {
    printf("No current room.\n");
    return 0;
  } else
    room = vm->room;

  printf("Room %d:\n",room->id);
  printf("  Size                : %dx%d\n",room->width,room->height);
  printf("  Z planes            : %d\n",room->num_zplane);
  printf("  Palettes            : %d\n",room->num_palette);
  printf("  Cycles              : %d\n",room->num_cycle);
  printf("  Transparent color   : %d\n",room->trans);
  printf("  Entry script        : %s\n",room->entry ? "yes" : "no");
  printf("  Exit script         : %s\n",room->exit ? "yes" : "no");
  printf("  Scripts             : %d\n",room->num_script);
  printf("  Objects             :");
  for(id = 0 ; id < room->num_object ; id++)
    if(room->object[id])
      printf(" %d",room->object[id]->id);
  printf("\n");
  return 1;
}


/////////////// show thread ///////////////////////////

static void show_thread(scvm_thread_t* t) {
  printf("Thread %d\n",t->id);
  printf("  State               : %s\n",scvm_thread_state_name(t->state));
  if(t->parent)
    printf("  Parent thread       : %d\n",t->parent->id);
  if(!t->state)
    return;
  if(t->next_state)
    printf("  Next VM state       : %s\n",scvm_state_name(t->next_state));
  if(t->flags) {
    char flags[1024];
    scvm_thread_flags_name(t->flags,flags,sizeof(flags));
    printf("  Flags               : %s\n",flags);
  }
  printf("  Cycle               : %d\n",t->cycle);
  if(t->delay)
    printf("  Delay               : %d ms\n",t->delay);
  if(t->script && (t->script->id & 0x0FFF0000) == 0x0ECD0000)
    printf("  Script              : %s\n",
           t->script->id == 0x0ECD0000 ? "entry" : "exit");
  else
    printf("  Script              : %d\n",t->script ? t->script->id : -1);
  printf("  Code pointer        : %d - 0x%X\n",t->code_ptr,t->code_ptr);
  printf("  OP start            : %d - 0x%X\n",t->op_start,t->code_ptr);
}

static void cmd_show_thread_usage(char* args) {
  printf("Usage: show thread [ id ... ]\n");
}

static int cmd_show_thread(scvm_t* vm, char* args) {
  int t;
  if(!args) {
    for(t = 0 ; t < vm->num_thread ; t++)
      if(vm->thread[t].state > SCVM_THREAD_STOPPED)
        show_thread(&vm->thread[t]);
    return 1;
  }
  while(1) {
    char* end = args;
    while(isspace(args[0])) args++;
    if(!args[0]) break;
    t = strtol(args,&end,0);
    if(end == args)
      return 0;
    show_thread(&vm->thread[t]);
    args = end;
  }
  return 1;
}

/////////////// show var ///////////////////////////

static void cmd_show_var_usage(char* args) {
  printf("Usage: show var [ local | bit ] var-addr\n");
}

static int cmd_show_var(scvm_t* vm, char* args) {
  int var,val;
  if(!args) {
    cmd_show_var_usage(NULL);
    return 0;
  }
  if(sscanf(args,"local %d",&var) == 1) {
    var &= 0x3FFF;
    var |= 0x4000;
  } else if(sscanf(args,"bit %d",&var) == 1) {
    var &= 0x7FFF;
    var |= 0x8000;
  } else if(sscanf(args,"%d",&var) != 1) {
    cmd_show_var_usage(NULL);
    return 0;
  }
  if(scvm_thread_read_var(vm,vm->current_thread,var,&val)) {
    printf("Invalid address: %d\n",var);
    return 0;
  }
  return 1;
}

/////////////// show vm ///////////////////////////

static void cmd_show_vm_usage(char* args) {
  printf("Usage: show vm\n");
}

static int cmd_show_vm(scvm_t* vm, char* args) {
  printf("VM:\n");
  printf("  Data path           : %s\n",vm->path);
  printf("  Basename            : %s\n",vm->basename);
  printf("  File key            : 0x%02x\n",vm->file_key);
  printf("  Data files          : %d\n",vm->num_file);
  printf("\n");
  printf("  Global variables    : %d\n",vm->num_var);
  printf("  Bit variables       : %d\n",vm->num_bitvar);
  printf("  Arrays              : %d\n",vm->num_array);
  printf("  Stack               : %d/%d\n",vm->stack_ptr,vm->stack_size);
  printf("\n");
  printf("  State               : %s\n",scvm_state_name(vm->state));
  printf("  Cycle               : %d\n",vm->cycle);
  if(vm->room)
    printf("  Room                : %d\n",vm->room->id);
  if(vm->current_thread)
    printf("  Thread              : %d\n",vm->current_thread->id);
  if(vm->current_actor)
    printf("  Actor               : %d\n",vm->current_actor->id);
  return 1;
}

/////////////// show xxx ///////////////////////////

#define SHOW_CMD(name) \
  { #name, "show" #name, cmd_show_##name, cmd_show_##name##_usage }

static scvm_debug_cmd_t show_cmd[] = {
  SHOW_CMD(actor),
  SHOW_CMD(array),
  SHOW_CMD(avar),
  SHOW_CMD(costume),
  SHOW_CMD(object),
  SHOW_CMD(room),
  SHOW_CMD(thread),
  SHOW_CMD(var),
  SHOW_CMD(vm),
  { NULL }
};


/////////////// thread ///////////////////////////

static void cmd_thread_usage(char* args) {
  printf("Usage: thread [ id ]\n");
}

static int cmd_thread(scvm_t* vm, char* args) {
  if(!args) {
    if(!vm->current_thread) {
      printf("No thread is currently scheduled.\n");
      return 0;
    }
    printf("Current thread is %d.\n",vm->current_thread->id);
    return 1;
  }
  // TODO: switch thread
  return 0;
}


/////////////// run ///////////////////////////

static void cmd_run_usage(char* args) {
  printf("Usage: run [ boot-param ]\n");
}

static int cmd_run(scvm_t* vm, char* args) {
  sig_t oldsig;
  // todo: reset the vm
  dbg_interrupt = 0;
  oldsig = signal(SIGINT,dbg_interrupt_handler);
  scvm_run(vm);
  signal(SIGINT,oldsig);
  return 1;
}

////////////////// root commands /////////////////////

#define DBG_CMD(name) \
  { #name, #name, cmd_##name, cmd_##name##_usage }
#define DBG_SUBCMD(name) \
  { #name, #name, NULL, NULL, name##_cmd }

static scvm_debug_cmd_t debug_cmd[] = {
  DBG_CMD(breakpoint),
  DBG_CMD(help),
  DBG_CMD(quit),
  DBG_CMD(run),
  DBG_SUBCMD(show),
  DBG_CMD(thread),
  { NULL }
};

#ifndef HAVE_READLINE
char* readline(const char* prompt) {
    char* line = malloc(512);
    printf("%s",prompt);
    if(!fgets(line,512,stdin)) {
        free(line);
        return NULL;
    }
    return line;
}

void add_history(const char* line) {
}
#endif

int scvm_debugger(scvm_t* vm) {
  char* line = NULL, *ptr, *args;
  scvm_debug_cmd_t* cmd;
  int r = 1, found;

  if(!vm->dbg)
    vm->dbg = calloc(1,sizeof(scvm_debug_t));

  vm->dbg->check_interrupt = check_interrupt;

  do {
    if(line) free(line);
    if(!(line = readline("(scvm) ")))
      continue;
    for(ptr = line ; isspace(ptr[0]) ; ptr++);
    if(!ptr[0]) continue;
    add_history(ptr);
    cmd = scvm_find_debug_cmd(vm,ptr,debug_cmd,&args,&found);
    if(!found) {
      if(cmd) {
        if(args)
          printf("Unknown command: %s\n",ptr);
        else
          printf("Incomplete command: %s\n",ptr);
      }
      cmd_help(vm,ptr);
      r = 0;
      continue;
    }
    r = cmd->action(vm,args);
  } while(r >= 0);
  return r;
}
