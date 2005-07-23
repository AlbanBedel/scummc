/* ScummC
 * Copyright (C) 2004-2005  Alban Bedel
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

#define SCC_PARAM_FLAG 1
#define SCC_PARAM_INT  2
#define SCC_PARAM_STR  3
#define SCC_PARAM_DBL  4
#define SCC_PARAM_STR_LIST 5

#define SCC_PARAM_TYPE_NO_ARG 1

#define SCC_PARAM_UNKNOWN      -1
#define SCC_PARAM_NEED_ARG     -2
#define SCC_PARAM_INVALID      -3
#define SCC_PARAM_OUT_OF_RANGE -4

typedef struct scc_param_st {
  char* name;
  int type;
  int min,max;
  void* ptr;
} scc_param_t;

typedef struct scc_cl_arg_st scc_cl_arg_t;
struct scc_cl_arg_st {
  scc_cl_arg_t* next;
  char* val;
};

int scc_param_parse(scc_param_t* params,char* k,char* v);

scc_cl_arg_t* scc_param_parse_argv(scc_param_t* params,int argc,char** argv);
