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
 * @file scvm_res.h
 * @ingroup scvm
 * @brief SCVM ressources management
 */

typedef struct scvm scvm_t;

#define SCVM_RES_LOCKED 1

typedef struct scvm_res {
  // datafile / room where the resource is
  unsigned file, room;
  // absolute position in the data file
  unsigned long offset;
  // locked
  unsigned flags;
  void* data;
} scvm_res_t;

typedef void* (*scvm_res_load_f)(scvm_t* vm, scc_fd_t* fd, unsigned num);
typedef void (*scvm_res_nuke_f)(void* data);

typedef struct scvm_res_type {
  // debug stuff
  const char* name;
  
  scvm_res_load_f load;
  scvm_res_nuke_f nuke;  
  unsigned num;
  scvm_res_t *idx;
} scvm_res_type_t;


scc_fd_t* scvm_open_file(scvm_t* vm,unsigned num);
void scvm_close_file(scvm_t* vm,unsigned num);

void scvm_res_init(scvm_res_type_t* res, char* name, unsigned num,
                   scvm_res_load_f load, scvm_res_nuke_f nuke);
int scvm_res_load_index(scvm_t* vm, scvm_res_type_t* res,scc_fd_t* fd,
                        uint32_t ref_type);

int scvm_lock_res(scvm_t* vm, unsigned type, unsigned num);

int scvm_unlock_res(scvm_t* vm, unsigned type, unsigned num);

void* scvm_load_res(scvm_t* vm, unsigned type, unsigned num);

void* scvm_load_script(scvm_t* vm,scc_fd_t* fd, unsigned num);

void* scvm_load_room(scvm_t* vm,scc_fd_t* fd, unsigned num);

void* scvm_load_costume(scvm_t* vm,scc_fd_t* fd, unsigned num);

void * scvm_load_charset(scvm_t* vm,scc_fd_t* fd, unsigned num);
