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

/** @file midi.c
 *  @brief A MIDI file hacking tool.
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
#include "scc_fd.h"
#include "scc_util.h"

#include "scc_param.h"
#include "scc_smf.h"


static int strip_track = -1;
static int set_type = -1;
static int dump = 0;
static int* merge = NULL;

static scc_param_t scc_parse_params[] = {
  { "strip-track", SCC_PARAM_INT, 0, 255, &strip_track },
  { "set-type", SCC_PARAM_INT, 0, 2, &set_type },
  { "merge-track", SCC_PARAM_INT_LIST, 0, 255, &merge },
  { "dump", SCC_PARAM_FLAG, 0, 1, &dump },
  { NULL, 0, 0, 0, NULL }
};

static void usage(char* prog) {
  scc_log(LOG_MSG,"Usage: %s [-strip-track n] [-set-type t] input.mid output.mid\n",prog);
  exit(-1);
}

int main(int argc,char** argv) {
  scc_cl_arg_t* files;
  scc_smf_t* smf;
  
  if(argc < 3) usage(argv[0]);
  
  files = scc_param_parse_argv(scc_parse_params,argc-1,&argv[1]);

  if(!files) usage(argv[0]);

  smf = scc_smf_parse_file(files->val);
  if(!smf) return 1;
  
  if(dump) {
    scc_smf_dump(smf);
    return 0;
  }

  if(merge && merge[0] > 1) {
    int t;
    for(t = 2 ; t < merge[0]+1 ; t++)
      scc_smf_merge_track(smf,merge[1],merge[t]);
  }
  
  if(strip_track >= 0) {
    if(strip_track >= smf->num_track) {
      scc_log(LOG_ERR,"Track %d doesn't exist, there are only %d tracks.\n",
              strip_track,smf->num_track);
      return 1;
    }
    scc_smf_remove_track(smf,strip_track);
  }

  if(set_type >= 0)
    smf->type = set_type;

  return !scc_smf_write_file(smf,files->next->val);
}
