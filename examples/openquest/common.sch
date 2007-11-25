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

bit verbsOn,cursorOn,cursorLoaded;
int sntcVerb,sntcObjA,sntcObjB;
int* invObj;


// The sentence line
verb SntcLine;

// The inventory slots
verb invSlot0 @ 100, invSlot1 @ 101, invSlot2 @ 102, invSlot3 @ 103,
    invSlot4 @ 104, invSlot5 @ 105, invSlot6 @ 106, invSlot7 @ 107;

// The inventory arrows
verb invUp, invDown;
int invOffset;
#define MAX_INVENTORY_DISPLAY    8

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


// define actors
actor ensignZob;
#define ZOB_COLOR     231
#define ZOB_DIM_COLOR 232
actor commanderZif;
#define ZIF_COLOR     243

actor carol;
#define CAROL_COLOR   249

// The room we start in
room Road;

// The secret room
room SecretRoom;

room ResRoom {

    // our standard charset
    chset chtest;
    // dialog charset
    chset dialogCharset;

    script inputHandler(int area,int cmd, int btn);
    script keyboardHandler(int key);
    script inventoryHandler(int obj);
    //script showVerbs(int show);
    script showCursor();
    script hideCursor();
    script mouseWatch();


}
