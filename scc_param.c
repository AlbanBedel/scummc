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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stdint.h>

#include "scc_util.h"
#include "scc_param.h"


typedef struct scc_param_parser_st {
  int type;
  int flags;
  int (*parse)(scc_param_t* p,char* val);
} scc_param_parser_t;

static int scc_parse_flag(scc_param_t* param,char* val) {
  int* p = param->ptr;

  p[0] = param->max;

  return 0;
}

static int scc_parse_int(scc_param_t* param,char* val) {
  int* p = param->ptr;
  char* end = NULL;
  int n;
  
  n = strtol(val,&end,0);

  if(!end || end[0] != '\0') return SCC_PARAM_INVALID;
  if(n < param->min || n > param->max) return SCC_PARAM_OUT_OF_RANGE;

  p[0] = n;

  return 1;
}

static int scc_parse_double(scc_param_t* param,char* val) {
  double* p = param->ptr;
  char* end = NULL;
  double n;
  
  n = strtod(val,&end);

  if(!end || end[0] != '\0') return SCC_PARAM_INVALID;
  if(n < param->min || n > param->max) return SCC_PARAM_OUT_OF_RANGE;

  p[0] = n;

  return 1;
}

static int scc_parse_str(scc_param_t* param,char* val) {
  char** p = param->ptr;

  if(val[0] == '\0') return SCC_PARAM_INVALID;

  if(p[0]) free(p[0]);
  p[0] = strdup(val);

  return 1;
}

scc_param_parser_t scc_param_parser[] = {
  { SCC_PARAM_FLAG, SCC_PARAM_TYPE_NO_ARG, scc_parse_flag },
  { SCC_PARAM_INT, 0, scc_parse_int },
  { SCC_PARAM_DBL, 0, scc_parse_double },
  { SCC_PARAM_STR, 0, scc_parse_str },
  { 0, 0, NULL }
};

static scc_param_parser_t* scc_param_get_parser(int type) {
  int i;

  for(i = 0 ; scc_param_parser[i].type > 0 ; i++) {
    if(scc_param_parser[i].type == type) return &scc_param_parser[i];
  }
  return NULL;
}

int scc_param_parse(scc_param_t* params,char* k,char* v) {
  scc_param_parser_t* parser;
  int i;

  for(i = 0 ; params[i].name ; i++) {
    if(!strcmp(params[i].name,k)) break;
  }

  if(!params[i].name) return SCC_PARAM_UNKNOWN;

  parser = scc_param_get_parser(params[i].type);
  if(!params[i].name) return SCC_PARAM_UNKNOWN;

  if(!((parser->flags & SCC_PARAM_TYPE_NO_ARG) || v))
    return SCC_PARAM_NEED_ARG;

  return parser->parse(&params[i],v);
}

scc_cl_arg_t* scc_param_parse_argv(scc_param_t* params,int argc,char** argv) {
  int n = 0,r,get_args = 1;
  scc_cl_arg_t* list = NULL, *last = NULL, *arg;
  
  while(n < argc) {
    if(get_args && argv[n][0] == '-') {
      // end of param list
      if(!strcmp(argv[n],"--")) {
	get_args = 0;
	n++;
	continue;
      }

      // that should be ok as argv must be NULL terminated
      r = scc_param_parse(params,&argv[n][1],argv[n+1]);
      if(r < 0) {
	switch(r) {
	case SCC_PARAM_UNKNOWN:
	  fprintf(stderr,"Unknown parameter: %s\n",argv[n]);
	  break;
	case SCC_PARAM_NEED_ARG:
	  fprintf(stderr,"Parameter %s need argument.\n",argv[n]);
	  break;
	case SCC_PARAM_INVALID:
	  fprintf(stderr,"Argument of %s is invalid: %s.\n",argv[n],argv[n+1]);
	  break;
	case SCC_PARAM_OUT_OF_RANGE:
	  fprintf(stderr,"Argument of %s is out of range.\n",argv[n]);
	  break;
	default:
	  fprintf(stderr,"Error while parsing parameter %s.\n",argv[n]);
	  break;
	}
	// free list
	return NULL;
      }
      n += 1+r;
      continue;
    }

    arg = calloc(1,sizeof(scc_cl_arg_t));
    arg->val = strdup(argv[n]);
    SCC_LIST_ADD(list,last,arg);
    n++;
  }

  return list;
}
