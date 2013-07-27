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

// The action verbs
verb Give,  PickUp, Use;
verb Open,  LookAt, Push;
verb Close, TalkTo, Pull;

verb WalkTo, WalkToXY;
// The sentence line
verb SntcLine;
// The inventory slots
verb invSlot0 @ 100, invSlot1 @ 101, invSlot2 @ 102, invSlot3 @ 103,
    invSlot4 @ 104, invSlot5 @ 105, invSlot6 @ 106, invSlot7 @ 107;
// The inventory arrows
verb invUp, invDown;
// The verb colors
#define VERB_COLOR       6
#define VERB_HI_COLOR   11
#define VERB_DIM_COLOR   7

// Object callbacks
verb Icon,Preposition,SetBoxes;

// Object class
class Openable,Pickable;

// Allow the objects to insert a word (like "to" or "with") between the
// verb and object in the sentence.
char *sntcPrepo;

// List of the objects used to handle action on actors
int  *actorObject;

// Dialog string list
char *dialogList;
int numDialog,numActiveDialog,dialogOffset,selectedSentence;
int dialogColor, dialogHiColor;
verb dialogVerb0 @ 110, dialogVerb1 @ 111, dialogVerb2 @ 112;
verb dialogVerb3 @ 113, dialogVerb4 @ 114;
verb dialogUp, dialogDown;

#define MAX_DIALOG_LINES    5
#define MAX_DIALOG_SENTENCE 16

// define an actor for our hero
actor hero;
#define BEASTY_COLOR      7
#define BEASTY_DIM_COLOR 26

room ResRoom {
    script showVerbs(int show);
    script showCursor();
    script hideCursor();
    
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

    // Inventory objects
    object axe;
}
