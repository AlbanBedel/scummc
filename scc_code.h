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

scc_code_t* scc_code_new(int len);

void scc_code_free(scc_code_t* c);

void scc_code_free_all(scc_code_t* c);

scc_script_t* scc_script_new(scc_ns_t* ns, scc_instruct_t* inst);

void scc_script_free(scc_script_t* scr);

void scc_script_list_free(scc_script_t* scr);
