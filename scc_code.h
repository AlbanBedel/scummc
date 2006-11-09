/* ScummC
 * Copyright (C) 2004-2006  Alban Bedel
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
 * @file scc_code.h
 * @ingroup scc
 * @brief SCUMM code generator
 */

/// @name Loop Stack
/// A little stack for the loops, this is needed
/// by the parser to check the branch instruction validity.
//@{

/// @brief       Add a loop entry to the stack.
/// @param type  Type of the corresponding instruction
/// @param sym   Name of the loop or NULL
void scc_loop_push(int type, char* sym);

/// @brief  Pop the current loop
/// @return The poped loop
scc_loop_t* scc_loop_pop(void);

/// @brief       Get the loop referenced by a branching instruction.
/// @param type  Type of the branching instruction
/// @param sym   Name of the loop or NULL
/// @return      The matching loop or NULL
scc_loop_t* scc_loop_get(int type,char* sym);

//@}

/// @name Code blocks
//@(

/// Create a new code block
scc_code_t* scc_code_new(int len);

/// Destroy a code block
void scc_code_free(scc_code_t* c);

/// Destroy a code block list
void scc_code_free_all(scc_code_t* c);

//@}

/// @name Scripts
//@{

/// @brief            Generate a script from a parse tree.
/// @param ns         Namespace to use
/// @param inst       The parse tree
/// @param return_op  The op code to use for return
/// @param close_scr  If true add a return at the end of the script.
scc_script_t* scc_script_new(scc_ns_t* ns, scc_instruct_t* inst,
                             uint8_t return_op,char close_scr);

/// Destroy a script
void scc_script_free(scc_script_t* scr);

/// Destroy a list of script
void scc_script_list_free(scc_script_t* scr);

//@}
