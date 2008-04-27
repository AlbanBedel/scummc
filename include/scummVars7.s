/*
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

int VAR_MOUSE_X                @ 1;
int VAR_MOUSE_Y                @ 2;
int VAR_VIRT_MOUSE_X           @ 3;
int VAR_VIRT_MOUSE_Y           @ 4;
int VAR_ROOM_WIDTH             @ 5;
int VAR_ROOM_HEIGHT            @ 6;
int VAR_CAMERA_POS_X           @ 7;
int VAR_CAMERA_POS_Y           @ 8;
int VAR_OVERRIDE               @ 9;
int VAR_ROOM                   @ 10;
int VAR_ROOM_RESOURCE          @ 11;
int VAR_TALK_ACTOR             @ 12;
int VAR_HAVE_MSG               @ 13;
int VAR_TIMER                  @ 14;
int VAR_TMR_4                  @ 15;
int VAR_TIMEDATE_YEAR          @ 16;
int VAR_TIMEDATE_MONTH         @ 17;
int VAR_TIMEDATE_DAY           @ 18;
int VAR_TIMEDATE_HOUR          @ 19;
int VAR_TIMEDATE_MINUTE        @ 20;
int VAR_TIMEDATE_SECOND        @ 21;
int VAR_LEFTBTN_DOWN           @ 22;
int VAR_RIGHTBTN_DOWN          @ 23;
int VAR_LEFTBTN_HOLD           @ 24;
int VAR_RIGHTBTN_HOLD          @ 25;
int VAR_MEMORY_PERFORMANCE     @ 26;
int VAR_VIDEO_PERFORMANCE      @ 27;

int VAR_GAME_LOADED            @ 29;

int VAR_V6_EMSSPACE            @ 32;
int VAR_VOICE_MODE             @ 33;
int VAR_RANDOM_NR              @ 34;
int VAR_NEW_ROOM               @ 35;
int VAR_WALKTO_OBJ             @ 36;
int VAR_NUM_GLOBAL_OBJS        @ 37;
int VAR_CAMERA_DEST_X          @ 38;
int VAR_CAMERA_DEST_Y          @ 39;
int VAR_CAMERA_FOLLOWED_ACTOR  @ 40;

int VAR_SCROLL_SCRIPT          @ 50;
int VAR_PRE_ENTRY_SCRIPT       @ 51;
int VAR_POST_ENTRY_SCRIPT      @ 52;
int VAR_PRE_EXIT_SCRIPT        @ 53;
int VAR_POST_EXIT_SCRIPT       @ 54;
int VAR_VERB_SCRIPT            @ 55;
int VAR_SENTENCE_SCRIPT        @ 56;
int VAR_INVENTORY_SCRIPT       @ 57;
int VAR_CUTSCENE_START_SCRIPT  @ 58;
int VAR_CUTSCENE_END_SCRIPT    @ 59;
int VAR_SAVELOAD_SCRIPT        @ 60;
int VAR_SAVELOAD_SCRIPT2       @ 61;

int VAR_CUTSCENEEXIT_KEY       @ 62;
int VAR_RESTART_KEY            @ 63;
int VAR_PAUSE_KEY              @ 64;
int VAR_MAINMENU_KEY           @ 65;
int VAR_VERSION_KEY            @ 66;
int VAR_TALKSTOP_KEY           @ 67;

int VAR_TIMER_NEXT             @ 97;
int VAR_TMR_1                  @ 98;
int VAR_TMR_2                  @ 99;
int VAR_TMR_3                  @ 100;

int VAR_CAMERA_MIN_X           @ 101;
int VAR_CAMERA_MAX_X           @ 102;
int VAR_CAMERA_MIN_Y           @ 103;
int VAR_CAMERA_MAX_Y           @ 104;
int VAR_CAMERA_THRESHOLD_X     @ 105;
int VAR_CAMERA_THRESHOLD_Y     @ 106;
int VAR_CAMERA_SPEED_X         @ 107;
int VAR_CAMERA_SPEED_Y         @ 108;
int VAR_CAMERA_ACCEL_X         @ 109;
int VAR_CAMERA_ACCEL_Y         @ 110;

int VAR_EGO                    @ 111;

int VAR_CURSORSTATE            @ 112;
int VAR_USERPUT                @ 113;
int VAR_DEFAULT_TALK_DELAY     @ 114;
int VAR_CHARINC                @ 115;
int VAR_DEBUGMODE              @ 116;
int VAR_FADE_DELAY             @ 117;
int VAR_KEYPRESS               @ 118;
// FT specific ?
//int VAR_CHARSET_MASK           @ 119;

int VAR_VIDEONAME              @ 123;

int VAR_STRING2DRAW            @ 130;
int VAR_CUSTOMSCALETABLE       @ 131;

int VAR_BLAST_ABOVE_TEXT       @ 133;

int VAR_MUSIC_BUNDLE_LOADED    @ 135;
int VAR_VOICE_BUNDLE_LOADED    @ 136;

int VAR_RETURN                 @ 254;

//
// Directions
//

#define WEST   0
#define EAST   1
#define SOUTH  2
#define NORTH  3


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

