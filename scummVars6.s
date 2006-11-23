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

class ClassNeverClip           @ 20;
class ClassAlwaysClip          @ 21;
class ClassIgnoreBoxes         @ 22;

class ClassYFlip               @ 29;
class ClassXFlip               @ 30;
class ClassPlayer              @ 31;
class ClassUntouchable         @ 32;

int VAR_KEYPRESS               @ 0;
int VAR_EGO                    @ 1;
int VAR_CAMERA_POS_X           @ 2;
int VAR_HAVE_MSG               @ 3;
int VAR_ROOM                   @ 4;
int VAR_OVERRIDE               @ 5;
int VAR_MACHINE_SPEED          @ 6;
int VAR_ME                     @ 7;
int VAR_NUM_ACTOR              @ 8;
int VAR_V6_SOUNDMODE           @ 9;
int VAR_CURRENTDRIVE           @ 10;
int VAR_TMR_1                  @ 11;
int VAR_TMR_2                  @ 12;
int VAR_TMR_3                  @ 13;
int VAR_MUSIC_TIMER            @ 14;
int VAR_ACTOR_RANGE_MIN        @ 15;
int VAR_ACTOR_RANGE_MAX        @ 16;
int VAR_CAMERA_MIN_X           @ 17;
int VAR_CAMERA_MAX_X           @ 18;
int VAR_TIMER_NEXT             @ 19;
int VAR_VIRT_MOUSE_X           @ 20;
int VAR_VIRT_MOUSE_Y           @ 21;
int VAR_ROOM_RESOURCE          @ 22;
int VAR_LAST_SOUND             @ 23;
int VAR_CUTSCENEEXIT_KEY       @ 24;
int VAR_TALK_ACTOR             @ 25;
int VAR_CAMERA_FAST_X          @ 26;
int VAR_CAMERA_SCRIPT          @ 27;
int VAR_PRE_ENTRY_SCRIPT       @ 28;
int VAR_POST_ENTRY_SCRIPT      @ 29;
int VAR_PRE_EXIT_SCRIPT        @ 30;
int VAR_POST_EXIT_SCRIPT       @ 31;
int VAR_VERB_SCRIPT            @ 32;
int VAR_SENTENCE_SCRIPT        @ 33;
int VAR_INVENTORY_SCRIPT       @ 34;
int VAR_CUTSCENE_START_SCRIPT  @ 35;
int VAR_CUTSCENE_END_SCRIPT    @ 36;
int VAR_CHARINC                @ 37;
int VAR_WALKTO_OBJ             @ 38;
int VAR_DEBUGMODE              @ 39;
int VAR_HEAPSPACE              @ 40;
int VAR_ROOM_WIDTH             @ 41;
int VAR_RESTART_KEY            @ 42;
int VAR_PAUSE_KEY              @ 43;
int VAR_MOUSE_X                @ 44;
int VAR_MOUSE_Y                @ 45;
int VAR_TIMER                  @ 46;
int VAR_TMR_4                  @ 47;
int VAR_SOUNDCARD              @ 48;
int VAR_VIDEOMODE              @ 49;
int VAR_MAINMENU_KEY           @ 50;
int VAR_FIXEDDISK              @ 51;
int VAR_CURSORSTATE            @ 52;
int VAR_USERPUT                @ 53;
int VAR_ROOM_HEIGHT            @ 54;

int VAR_SOUNDRESULT            @ 56;
int VAR_TALKSTOP_KEY           @ 57;

int VAR_FADE_DELAY             @ 59;
int VAR_NOSUBTITLES            @ 60;
int VAR_GUI_ENTRY_SCRIPT       @ 61;
int VAR_GUI_EXIT_SCRIPT        @ 62;

int VAR_SOUNDPARAM             @ 64;
int VAR_SOUNDPARAM2            @ 65;
int VAR_SOUNDPARAM3            @ 66;
int VAR_MOUSEPRESENT           @ 67;
int VAR_MEMORY_PERFORMANCE     @ 68;
int VAR_VIDEO_PERFORMANCE      @ 69;     // Zak256 Note: Cashcard for Zak
int VAR_ROOM_FLAG              @ 70;     // Zak256 Note: Cashcard for Annie
int VAR_GAME_LOADED            @ 71;   // Zak256 Note: Cashcard for Melissa
int VAR_NEW_ROOM               @ 72;      // Zak256 Note: Cashcard for Leslie

int VAR_LEFTBTN_HOLD           @ 74;
int VAR_RIGHTBTN_HOLD          @ 75;
int VAR_V6_EMSSPACE            @ 76;

// "Inserez la disquette %c et cliquez sur la souris."
char* VAR_GAME_DISK_MSG        @ 90;
// "Impossible d'ouvrir %s, (%c%d) Cliquez sur la souris."
char* VAR_OPEN_FAILED_MSG      @ 91;
// "Erreur de lecture disque %c, (%c%d) Cliquez sur la souris."
char* VAR_READ_ERROR_MSG       @ 92;
// "Pause. Appuyez sur ESPACE pour continuer."
char* VAR_PAUSE_MSG            @ 93;
// "Vous voulez vraiment recommencer?  (O/N)o"
char* VAR_RESTART_MSG          @ 94;
// "Vous voulez vraiment quitter le jeu?  (O/N)O"
char* VAR_QUIT_MSG             @ 95;
// "Sauver"
char* VAR_SAVE_BTN             @ 96;
// "Charger"
char* VAR_LOAD_BTN             @ 97;
// "Jouer"
char* VAR_PLAY_BTN             @ 98;
// "Annuler"
char* VAR_CANCEL_BTN           @ 99;
// "Quitter"
char* VAR_QUIT_BTN             @ 100;
// "OK"
char* VAR_OK_BTN               @ 101;
// "Inserez votre disquette de sauvegardes"
char* VAR_SAVE_DISK_MSG        @ 102;
// "Vous devez entrer un nom"
char* VAR_ENTER_NAME_MSG       @ 103;
// "Partie NON sauvegardee (disque plein?)"
char* VAR_NOT_SAVED_MSG        @ 104;
// "Partie NON chargee"
char* VAR_NOT_LOADED_MSG       @ 105;
// "Sauvegarde de '%s'"
char* VAR_SAVE_MSG             @ 106;
// "Chargement de '%s'"
char* VAR_LOAD_MSG             @ 107;
// "Entrez le nom de la SAUVEGARDE"
char* VAR_SAVE_MENU_TITLE      @ 108;
// "Entrez le nom de la partie a CHARGER"
char* VAR_LOAD_MENU_TITLE      @ 109;
// Array with color index for various parts of the GUI
int* VAR_GUI_COLORS            @ 110;
// Array with the debug password
char* VAR_DEBUG_PASSWORD       @ 111;

// "Que desirez-vous?"
char* VAR_MAIN_MENU_TITLE      @ 117;
int VAR_RANDOM_NR              @ 118;
int VAR_TIMEDATE_YEAR          @ 119;

int VAR_GAME_VERSION           @ 122;
// dunno what it do, vars.cpp say it's SnM specific
int VAR_CHARSET_MASK           @ 123;

int VAR_TIMEDATE_HOUR          @ 125;
int VAR_TIMEDATE_MINUTE        @ 126;

int VAR_TIMEDATE_DAY           @ 128;
int VAR_TIMEDATE_MONTH         @ 129;

//
// iMUSE constants
//

// Commands
#define IM_SET_MASTER_VOLUME     0x0006
#define IM_GET_MASTER_VOLUME     0x0007
#define IM_START_SOUND           0x0008
#define IM_STOP_SOUND            0x0009
#define IM_STOP_ALL_SOUNDS       0x000B
#define IM_PLAYER_SET            0x000C
#define IM_GET_SOUND_STATUS      0x000D
#define IM_FADE_SOUND            0x000E
#define IM_MAYBE_HOOK            0x000F
#define IM_SET_VOLCHAN           0x0010
#define IM_SET_CHANNEL_VOLUME    0x0011
#define IM_SET_VOLCHAN_ENTRY     0x0012
#define IM_CLEAR_TRIGGER         0x0013
#define IM_PLAYER_GET_PARAM      0x0100
#define IM_PLAYER_SET_PRIORITY   0x0101
#define IM_PLAYER_SET_VOLUME     0x0102
#define IM_PLAYER_SET_PAN        0x0103
#define IM_PLAYER_SET_TRANSPOSE  0x0104
#define IM_PLAYER_SET_DETUNE     0x0105
#define IM_PLAYER_SET_SPEED      0x0106
#define IM_PLAYER_JUMP           0x0107
#define IM_PLAYER_SCAN           0x0108
#define IM_PLAYER_SET_LOOP       0x0109
#define IM_PLAYER_CLEAR_LOOP     0x010A
#define IM_PLAYER_SET_ON_OFF     0x010B
#define IM_PLAYER_SET_HOOK       0x010C
#define IM_PLAYER_FADE           0x010D
#define IM_QUEUE_TRIGGER         0x010E
#define IM_QUEUE_COMMAND         0x010F
#define IM_LIVE_MIDI_ON          0x0111
#define IM_LIVE_MIDI_OFF         0x0112
#define IM_PLAYER_GET_PARAM      0x0113
#define IM_PLAYER_SET_HOOK       0x0114
//#define IM_PLAYER_SET_VOLUME     0x0116
#define IM_QUERY_QUEUE           0x0117

// Player parameters

#define IM_PRIORITY              0x00
#define IM_VOLUME                0x01
#define IM_PAN                   0x02
#define IM_TRANSPOSE             0x03
#define IM_DETUNE                0x04
#define IM_SPEED                 0x05
#define IM_TRACK_INDEX           0x06
#define IM_BEAT_INDEX            0x07
#define IM_TICK_INDEX            0x08
#define IM_LOOP_COUNTER          0x09
#define IM_LOOP_TO_BEAT          0x0A
#define IM_LOOP_TO_TICK          0x0B
#define IM_LOOP_FROM_BEAT        0x0C
#define IM_LOOP_FROM_TICK        0x0D
#define IM_PART_ON               0x0E
#define IM_PART_VOL              0x0F
#define IM_PART_INSTRUMENT       0x10
#define IM_PART_TRANSPOSE        0x11
#define IM_JUMP_HOOK             0x12
#define IM_TRANSPOSE_HOOK        0x13
#define IM_PART_ON_OFF_HOOK      0x14
#define IM_PART_VOLUME_HOOK      0x15
#define IM_PART_PROGRAM_HOOK     0x16
#define IM_PART_TRANSPOSE_HOOK   0x17

// Hook type

#define IM_HOOK_JUMP             0x00
#define IM_HOOK_TRANSPOSE        0x01
#define IM_HOOK_PART_ON_OFF      0x02
#define IM_HOOK_PART_VOLUME      0x03
#define IM_HOOK_PART_PROGRAM     0x04
#define IM_HOOK_PART_TRANSPOSE   0x05

