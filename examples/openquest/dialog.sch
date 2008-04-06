/* ScummC
 * Copyright (C) 2007  Alban Bedel, Gerrit Karius
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


// Dialog string list
char *dialogList;
int numDialog,numActiveDialog,dialogOffset,selectedSentence;
int dialogColor, dialogHiColor;
verb dialogVerb0 @ 110, dialogVerb1 @ 111, dialogVerb2 @ 112;
verb dialogVerb3 @ 113, dialogVerb4 @ 114;
verb dialogUp, dialogDown;

#define MAX_DIALOG_LINES    5
#define MAX_DIALOG_SENTENCE 16



room Dialog {

    // Reset the dialog list
    script dialogClear(int kill);
    // Add an entry to the dialog list
    script dialogAdd(char* str);
    // Remove an entry, if kill is not zero undim the string too
    script dialogRemove(int idx, int kill);
    // Start a dialog
    script dialogStart(int color, int hiColor);
    // Hide the dialog verbs
    script dialogHide();
    // End a dialog
    script dialogEnd();

}
