/* ScummC
 * Copyright (C) 2007  Alban Bedel
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

/// @defgroup scc_target ScummC target definitons
/**
 * @file scc_target.c
 * @ingroup scc_target
 * @brief ScummC target definitions
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "scc_util.h"
#include "scc_parse.h"

#include "scc_func.h"

static int scc_addr_max_v6[] = {
    0,                       // unused
    0x3FFF,                  // VAR
    0xFF,                    // ROOM
    0xC7,                    // SCR
    0xFFFF,                  // COST
    0xFFFF,                  // SOUND
    0xFFFF,                  // CHSET
    0xFF,                    // LSCR
    0xFE,                    // VERB (0xFF is default)
    0xFFFF,                  // OBJ
    0x0F,                    // STATE
    0x400F,                  // LVAR
    0xFFFF,                  // BVAR
    0xFFFF,                  // PALS
    0x0F,                    // CYCL
    0x0F,                    // ACTOR
    0xFF,                    // BOX
    SCC_MAX_CLASS,           // CLASS
    0x7FFFFFFF               // VOICE
};

static int scc_addr_min_v6[] = {
    0,                       // nothing
    0x100,                   // VAR (don't use the engine vars)
    1,                       // ROOM
    1,                       // SCR
    1,                       // COST
    1,                       // SOUND
    1,                       // CHSET
    200,                     // LSCR
    1,                       // VERB
    17,                      // OBJ
    1,                       // STATE
    0x4000,                  // LVAR
    0x8000,                  // BVAR
    1,                       // PAL
    1,                       // CYCL
    1,                       // ACTOR
    0,                       // BOX
    1,                       // CLASS
    0                        // VOICE
};


static int scc_addr_max_v7[] = {
    0,                       // unused
    0x3FFF,                  // VAR
    0xFF,                    // ROOM
    0x7CF,                   // SCR
    0xFFFF,                  // COST
    0xFFFF,                  // SOUND
    0xFFFF,                  // CHSET
    0xFFFF,                  // LSCR
    0xFE,                    // VERB (0xFF is default)
    0xFFFF,                  // OBJ
    0xFF,                    // STATE
    0x400F,                  // LVAR
    0xFFFF,                  // BVAR
    0xFFFF,                  // PALS
    0x0F,                    // CYCL
    0x1D,                    // ACTOR
    0xFF,                    // BOX
    SCC_MAX_CLASS,           // CLASS
    0x7FFFFFFF               // VOICE
};

static int scc_addr_min_v7[] = {
    0,                       // nothing
    0x100,                     // VAR (don't use the engine vars)
    1,                       // ROOM
    1,                       // SCR
    1,                       // COST
    1,                       // SOUND
    1,                       // CHSET
    2000,                    // LSCR
    1,                       // VERB
    32,                      // OBJ
    1,                       // STATE
    0x4000,                  // LVAR
    0x8000,                  // BVAR
    1,                       // PAL
    1,                       // CYCL
    1,                       // ACTOR
    0,                       // BOX
    1,                       // CLASS
    0                        // VOICE
};

static scc_target_t target_list[] = {
    { 6, scc_func_v6, scc_addr_max_v6, scc_addr_min_v6, 200 },
    { 7, scc_func_v7, scc_addr_max_v7, scc_addr_min_v7, 2000 },
    { -1 }
};

scc_target_t* scc_get_target(int version) {
    int i;
    for(i = 0 ; target_list[i].version >= 0 ; i++)
        if(target_list[i].version == version)
            return target_list+i;
    return NULL;
}
