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
 * @file scc_func.h
 * @ingroup scc
 * @brief ScummC builtin functions
 */

// A define for the various print function families

#define PRINT(name,op) {                         \
    name "At", 0x##op##41, 0, 2, 0,      \
    { SCC_FA_VAL, SCC_FA_VAL}                    \
  },{                                            \
    name "Color", 0x##op##42, 0, 1, 0,   \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Clipped", 0x##op##43, 0, 1, 0, \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Center", 0x##op##45, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name "Left", 0x##op##47, 0, 0, 0,    \
    {}                                           \
  },{                                            \
    name "Overhead", 0x##op##48, 0, 0, 0,\
    {}                                           \
  },{                                            \
    name "Mumble", 0x##op##4A, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name , 0x##op##4B, 0, 1, 0,          \
    { SCC_FA_STR }                               \
  },{                                            \
    name "Begin", 0x##op##FE, 0, 0, 0,   \
    {}                                           \
  },{                                            \
    name "End", 0x##op##FF, 0, 0, 0,     \
    {} \
  }

#define PRINT2(name,op) {                        \
    name "At", 0x##op##41, 0, 2, 0,      \
    { SCC_FA_VAL, SCC_FA_VAL}                    \
  },{                                            \
    name "Color", 0x##op##42, 0, 1, 0,   \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Clipped", 0x##op##43, 0, 1, 0, \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Center", 0x##op##45, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name "Left", 0x##op##47, 0, 0, 0,    \
    {}                                           \
  },{                                            \
    name "Overhead", 0x##op##48, 0, 0, 0,\
    {}                                           \
  },{                                            \
    name "Mumble", 0x##op##4A, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name , 0x##op##4B, 0, 1, 0,          \
    { SCC_FA_STR }                               \
  },{                                            \
    name "Begin", 0x##op##FE, 0, 1, 0,   \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "End", 0x##op##FF, 0, 0, 0,     \
    {} \
  }

#define PRINT3(name,op) {                        \
    name "At", 0x##op##41, 0, 2, 0,      \
    { SCC_FA_VAL, SCC_FA_VAL}                    \
  },{                                            \
    name "Color", 0x##op##42, 0, 1, 0,   \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Clipped", 0x##op##43, 0, 1, 0, \
    { SCC_FA_VAL }                               \
  },{                                            \
    name "Center", 0x##op##45, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name "Left", 0x##op##47, 0, 0, 0,    \
    {}                                           \
  },{                                            \
    name "Overhead", 0x##op##48, 0, 0, 0,\
    {}                                           \
  },{                                            \
    name "Mumble", 0x##op##4A, 0, 0, 0,  \
    {}                                           \
  },{                                            \
    name , 0x##op##4B, 0, 1, 0,          \
    { SCC_FA_STR }                               \
  },{                                            \
    name "End", 0x##op##FF, 0, 0, 0,     \
    {} \
  }

static scc_func_t scc_func_v6_v7[] = {
  {
    "startScript", 0x5E, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_LIST }
  },{
    "startScript0", 0x5F, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    "startObject", 0x60, 0, 4, 0,
    // flags,     script,     entryp,     args
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_LIST }
  },{
    "drawObject", 0x61, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "drawObjectAt", 0x62, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
#if 0
    "stopObjectCodeA", 0x65, 0, 0, 0,
    {}
  },{
    "stopObjectCodeB", 0x66, 0, 0, 0,
    {}
  },{
#endif
    "endCutscene", 0x67, 0, 0, 0,
    {}
  },{
    "beginCutscene", 0x68, 0, 1, 0,
    { SCC_FA_LIST }
  },{
    "stopMusic", 0x69, 0, 0, 0,
    {}
  },{
    "freezeUnfreeze", 0x6A, 0, 1, 0,
    { SCC_FA_VAL }
  },{

    "cursorOn",0x6B90, 0, 0, 0,
    {}
  },{
    "cursorOff",0x6B91, 0, 0, 0,
    {}
  },{
    "userPutOn",0x6B92, 0, 0, 0,
    {}
  },{
    "userPutOff",0x6B93, 0, 0, 0,
    {}
  },{
    "softCursorOn", 0x6B94, 0, 0, 0,
    {}
  },{
    "softCursorOff", 0x6B95, 0, 0, 0,
    {}
  },{
    "softUserPutOn", 0x6B96, 0, 0, 0,
    {}
  },{
    "softUserPutOff", 0x6B97, 0, 0, 0,
    {}
  },{
    "setCursorImage", 0x6B99, 0, 2, 0, // z
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setCursorHotspot", 0x6B9A, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "initCharset", 0x6B9C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setCharsetColors", 0x6B9D, 0, 1, 0,
    { SCC_FA_LIST }
  },{
    "setCursorTransparency", 0x6BD6, 0, 1, 0,
    { SCC_FA_VAL }

  },{
    "breakScript", 0x6C, 0, 0, 0,
    {}
  },{
    "isObjectOfClass", 0x6D, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    "setObjectClass", 0x6E, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    "getObjectState", 0x6F, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "setObjectState", 0x70, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setObjectOwner", 0x71, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getObjectOwner", 0x72, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "startSound", 0x74, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "stopSound", 0x75, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "startMusic", 0x76, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "stopObjectScript", 0x77, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "cameraFollowActor", 0x79, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "startRoom", 0x7B, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "stopScript", 0x7C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "walkActorToObj", 0x7D, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "walkActorTo", 0x7E, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "putActorAt", 0x7F, 0, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "actorFace", 0x81, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "animateActor", 0x82, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "doSentence", 0x83, 0, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getRandomNumber", 0x87, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getRandomNumberRange", 0x88, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "isActorMoving",0x8A, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "isScriptRunning", 0x8B, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getActorRoom", 0x8C, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getObjectX", 0x8D, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getObjectY", 0x8E, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getObjectDir", 0x8F, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getActorWalkBox", 0x90, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getActorCostume", 0x91, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "findInventory", 0x92, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getInventoryCount", 0x93, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getVerbAt", 0x94, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{ // TODO: begin/endOverride

    // set the object name in the _newNames table.
    "setObjectName", 0x97, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_STR }
  },{
    "isSoundRunning", 0x98, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "setBoxFlags", 0x99, 0, 2, 0,
    { SCC_FA_LIST, SCC_FA_VAL }
  },{
    "createBoxMatrix", 0x9A, 0, 0, 0,
    {}
  },{

    "loadScript", 0x9B64, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "loadSound", 0x9B65, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "loadCostume", 0x9B66, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "loadRoom", 0x9B67, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "nukeScript", 0x9B68, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "nukeSound", 0x9B69, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "nukeCostume", 0x9B6A, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "nukeRoom", 0x9B6B, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "lockScript", 0x9B6C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "lockSound", 0x9B6D, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "lockCostume", 0x9B6E, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "lockRoom", 0x9B6F, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "unlockScript", 0x9B70, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "unlockSound", 0x9B71, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "unlockCostume", 0x9B72, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "unlockRoom", 0x9B73, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "loadCharset", 0x9B75, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "nukeCharset", 0x9B76, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "loadFlObject", 0x9B77, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }

  },{
    "setRoomScroll", 0x9CAC, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setScreen", 0x9CAE, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setRoomColor", 0x9CAF, 0, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setShakeOn", 0x9CB0, 0, 0, 0,
    {}
  },{
    "setShakeOff", 0x9CB1, 0, 0, 0,
    {}
  },{
    "setRoomIntensity", 0x9CB3, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "saveLoadThing", 0x9CB4, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "screenEffect", 0x9CB5, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setRoomRGBIntensity", 0x9CB6, 0, 5, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setRoomShadow", 0x9CB7, 0, 5, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "roomPalManipulate", 0x9CBA, 0, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setRoomCycleDelay", 0x9CBB, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setRoomPalette", 0x9CD5, 0, 1, 0,
    { SCC_FA_VAL }
  },{

    "setCurrentActor", 0x9DC5, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorCostume", 0x9D4C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorWalkSpeed", 0x9D4D, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setActorSounds", 0x9D4E, 0, 1, 0,
    { SCC_FA_LIST }
  },{
    "setActorWalkFrame", 0x9D4F, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorTalkFrame", 0x9D50, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setActorStandFrame", 0x9D51, 0, 1, 0,
    { SCC_FA_VAL }
  },{
#if 0
    // Dummy func in vm 0.6.1
    "actorSet", 0x9D52, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
#endif
    "initActor", 0x9D53, 0, 0, 0,
    {}
  },{
    "setActorElevation", 0x9D54, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorDefaultFrames", 0x9D55, 0, 0, 0,
    {}
  },{
    "setActorPalette", 0x9D56, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setActorTalkColor", 0x9D57, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorName", 0x9D58, 0, 1, 0,
    { SCC_FA_STR }
  },{
    "setActorInitFrame", 0x9D59, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorWidth", 0x9D5B, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorScale", 0x9D5C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "actorNeverZClip", 0x9D5D, 0, 0, 0,
    {}
  },{
    "setActorZClip", 0x9DE1, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorIgnoreBoxes", 0x9D5F, 0, 0, 0,
    {}
  },{
    "setActorFollowBoxes", 0x9D60, 0, 0, 0,
    {}
  },{
    "setActorAnimSpeed", 0x9D61, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorShadowMode", 0x9D62, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorTalkPos", 0x9D63, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setActorAnimVar", 0x9DC6, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setActorIgnoreTurnsOn", 0x9DD7, 0, 0, 0,
    {}
  },{
    "setActorIgnoreTurnsOff", 0x9DD8, 0, 0, 0,
    {}
  },{
    "initActorQuick", 0x9DD9, 0, 0, 0,
    {}
  },{
#if 0
    "actorDrawVirScr", 0x9DDA, 0, 0, 0,
    {}
  },{
#endif
    "setActorLayer", 0x9DE3, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorWalkScript", 0x9DE4, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setActorStanding", 0x9DE5, 0, 0, 0,
    {}
  },{
    "setActorDirection", 0x9DE6, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "actorTurnToDirection", 0x9DE7, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "actorFreeze", 0x9DE9, 0, 0, 0,
    {}
  },{
    "actorUnfreeze", 0x9DEA, 0, 0, 0,
    {}
  },{
    "setActorTalkScript", 0x9DEB, 0, 1, 0,
    { SCC_FA_VAL }
  },{

    "setCurrentVerb", 0x9EC4, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setVerbImage", 0x9E7C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setVerbName", 0x9E7D, 0, 1, 0,
    { SCC_FA_STR }
  },{
    "setVerbColor", 0x9E7E, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setVerbHiColor", 0x9E7F, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setVerbXY", 0x9E80, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setVerbOn", 0x9E81, 0, 0, 0,
    {}
  },{
    "setVerbOff", 0x9E82, 0, 0, 0,
    {}
  },{
    "killVerb", 0x9E83, 0, 0, 0,
    {}
  },{
    "initVerb", 0x9E84, 0, 0, 0,
    {}
  },{
    "setVerbDimColor", 0x9E85, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "verbDim", 0x9E86, 0, 0, 0,
    {}
  },{
    "setVerbKey", 0x9E87, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "verbCenter", 0x9E88, 0, 0, 0,
    {}
  },{
    "setVerbNameString", 0x9E89, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setVerbObject", 0x9E8B, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setVerbBackColor", 0x9E8C, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "redrawVerb", 0x9EFF, 0, 0, 0,
    {}
  },{

    "getActorAt", 0x9F, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getObjectAt", 0xA0, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    // This call allow to map the pseudo room index (idx > 0x7F) to
    // real room id. This call set all pseudo index in the list point
    // to VAL. idx < 0x80 are simply ignored.
    "setPseudoRooms", 0xA1, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    "getActorElevation", 0xA2, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getObjectVerbEntrypoint", 0xA3, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{

    "saveVerbs", 0xA58D, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "restoreVerbs", 0xA58E, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "deleteVerbs", 0xA58F, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{

    "drawBox", 0xA6, 0, 5, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL  }
  },{
    "getActorWidth", 0xA8, 1, 1, 0,
    { SCC_FA_VAL }
  },{

    "waitForActor", 0xA9A8, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_SELF_OFF }
  },{

    "waitForMessage", 0xA9A9, 0, 0, 0,
    {}
  },{
    "waitForCamera", 0xA9AA, 0, 0, 0,
    {}
  },{
    "waitForSentence", 0xA9AB, 0, 0, 0,
    {}
  },{
    "waitForAnimation", 0xA9E2, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_SELF_OFF }
  },{
    "waitForTurn", 0xA9E8, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_SELF_OFF }
  },{

    "getActorXScale", 0xAA, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getActorAnimCounter1", 0xAB, 1, 1, 0,
    { SCC_FA_VAL }
  },{

    // iMUSE
    "soundKludge", 0xAC, 0, 1, 0,
    { SCC_FA_LIST }
  },{
    "imSetMasterVolume", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x0006) }
  },{
    "imGetMasterVolume", 0xAC, 0, 0, 1,
    { SCC_FA_OPC(0x0007) }
  },{
    "imStartSound", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x0008) }
  },{
    "imStopSound", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x0009) }
  },{
    "imStopAllSounds", 0xAC, 0, 0, 1,
    { SCC_FA_OPC(0x000A) }
  },{
    "imGetSoundStatus", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x000D) }
  },{
    "imSetVolchan", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0010) }
  },{
    "imSetChannelVolume", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0011) }
  },{
    "imSetVolchanEntry", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0012) }
  },{
    "imClearTrigger", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0013) }
  },{
    "imPlayerGetParam", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0100) }
  },{
    "imPlayerSetPriority", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0101) }
  },{
    "imPlayerSetVolume", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0102) }
  },{
    "imPlayerSetPan", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0103) }
  },{
    "imPlayerSetTranspose", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0104) }
  },{
    "imPlayerSetDetune", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0105) }
  },{
    "imPlayerSetSpeed", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0106) }
  },{
    "imPlayerJump", 0xAC, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0107) }
  },{
    "imPlayerScan", 0xAC, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0108) }
  },{
    "imPlayerSetLoop", 0xAC, 0, 6, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0109) }
  },{
    "imPlayerClearLoop", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x010A) }
  },{
    "imPlayerSetOnOff", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x010B) }
  },{
    "imPlayerSetHook", 0xAC, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x010C) }
  },{
    "imPlayerFade", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x010D) }
  },{
    "imQueueTrigger", 0xAC, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x010E) }
  },{
    "imQueueCommand", 0xAC, 0, 7, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, 
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x010F) }
  },{
    "imClearQueue", 0xAC, 0, 0, 1,
    { SCC_FA_OPC(0x0110) }
  },{
    "imPlayerGetParam", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0113) }
  },{
    "imPlayerSetPartVolume", 0xAC, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0116) }
  },{
    "imQueryQueue", 0xAC, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x0117) }
  },{


    "isAnyOf", 0xAD, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{

    "restartGame", 0xAE9E, 0, 0, 0,
    {}
  },{
    "pauseGame", 0xAE9F, 0, 0, 0,
    {}
  },{
    "shutdown", 0xAEA0, 0, 0, 0,
    {}
  },{

    "isActorInBox", 0xAF, 1, 1, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "delay", 0xB0, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "delaySeconds", 0xB1, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "delayMinutes", 0xB2, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "stopSentence", 0xB3,0, 0, 0,
    {}
  },
  
  PRINT("print",B4),
  PRINT("cursorPrint",B5),
  PRINT("dbgPrint",B6),
  PRINT("sysPrint",B7),
  PRINT2("actorPrint",B8),
  PRINT3("egoPrint",B8),
  
  {
    "egoPrintBegin" , 0xB9FE, 0, 0, 0,
    {}
  },{
    "actorSay", 0xBA, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_STR }
  },{
    "egoSay", 0xBB, 0, 1, 0,
    { SCC_FA_STR }
  },{
    
    // dim array
    "dimInt", 0xBCC7, 0, 2, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL }
  },{
    "dimBit", 0xBCC8, 0, 2, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL }
  },{
    "dimNibble", 0xBCC9, 0, 2, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL }
  },{
    "dimByte", 0xBCCA, 0, 2, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL }
  },{
    "dimChar", 0xBCCB, 0, 2, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL }
  },{
    "undim", 0xBCCC, 0, 1, 0,
    { SCC_FA_ARRAY }
  },{

    // this one still need special care i think
    "startObject2", 0xBE, 0, 3, 0,
    // script,    entryp,     args
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_LIST }
  },{

    "startScript2", 0xBF, 0, 2, 0,
    // script,    args
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    
    // dim2 array
    "dimInt2", 0xC0C7, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "dimBit2", 0xC0C8, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "dimNibble2", 0xC0C9, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "dimByte2", 0xC0CA, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "dimChar2", 0xC0CB, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{

    "trace", 0xC1, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_STR }
  },{
    
    "abs", 0xC4, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getDistObjObj", 0xC5, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getDistObjPt", 0xC6, 1, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getDistPtPt", 0xC7, 1, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{

    // kernelGet
    "getPixel", 0xC8, 1, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x71) }
  },{
    "getSpecialBox", 0xC8, 1, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x73) }
  },{
    "isInBox", 0xC8, 1, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x74) }
  },{
    "getColor", 0xC8, 1, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0xCE) }
  },{
    "getRoomObjectX", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xCF) }
  },{
    "getRoomObjectY", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD0) }
  },{
    "getRoomObjectWidth", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD1) }
  },{
    "getRoomObjectHeight", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD2) }
  },{
    "getKeyState", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD3) }
  },{
    "getActorFrame", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD4) }
  },{
    "getVerbX", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD5) }
  },{
    "getVerbY", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD6) }
  },{
    "getBoxFlags", 0xC8, 1, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD7) }
  },{

    "breakXTimes", 0xCA, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "pickOneOf", 0xCB, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_LIST }
  },{
    "pickOneOfDefault", 0xCC, 1, 3, 0,
    { SCC_FA_VAL, SCC_FA_LIST, SCC_FA_VAL }
  },{
    "stampObject", 0xCD, 0, 4, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "getDateTime", 0xD0, 0, 0, 0,
    {}
  },{
    "stopTalking", 0xD1,0, 0, 0,
    {}
  },{
    "getActorAnimVar", 0xD2, 1, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "shuffle", 0xD4, 0, 3, 0,
    { SCC_FA_ARRAY, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "jumpToScript", 0xD5, 0, 3, 0,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_LIST }
  },{
    "isRoomScriptRunning", 0xD8, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getRoomObjects", 0xDD, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    // this opcode check ground area in minigame "Asteroid Lander" in the dig
    "unknownE1", 0xE1, 1, 1, 0,
    { SCC_FA_VAL }
  },{
#if 0
    // very strange func. First it look at the ptr, if the value there
    // is 0 it then allocate an array at the same address and pick a
    // random value out of it.
    // If the value pointed to by the ptr is not 0, it's interpreted
    // as an array address. The first value in this array is read as
    // a string ressourse num. and finnaly some strange operation is
    // done with the string and the array.
    // need more investigation
    "pickVarRandom", 0xE3, 1, 2, 0,
    { SCC_FA_PTR, SCC_FA_LIST }
  },{
    // use an alternative box set, we can't generate such thing atm anyway.
    "setBoxSet", 0xE4, 0, 1, 0,
    { SCC_FA_VAL }
  },{
#endif
    "getActorLayer", 0xEC, 1, 1, 0,
    { SCC_FA_VAL }
  },{
    "getObjectNewDir", 0xED, 1, 1, 0,
    { SCC_FA_VAL }

  },{
    NULL, 0, 0, 0, 0, {}
  }
};

static scc_func_t scc_func_v6_only[] = {
  {
    "panCameraTo", 0x78, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "setCameraAt", 0x7A, 0, 1, 0,
    { SCC_FA_VAL }
  },{
    "putActorAtObject", 0x80, 0, 3, 0, // z
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{
    "pickupObject", 0x84, 0, 2, 0,  // z
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "startRoomWithEgo", 0x85, 0, 4, 0, // z
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{

    // kernelSet
    "grabCursor", 0xC9, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x04) }
  },{
    "fadeOut", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x05) }
  },{
    "fadeIn", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x06) }
  },{
#if 0
    "startManiac", 0xC9, 0, 0, 1,
    { SCC_FA_OPC(0x08) }
  },{
#endif
    "killAllScriptsExceptCurrent", 0xC9, 0, 0, 1,
    { SCC_FA_OPC(0x09) }
  },{
    "nukeFlObjects", 0xC9, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x68) }
  },{
    "setActorScale", 0xC9, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x6B) }
  },{
    "setShadowPalette", 0xC9, 0, 5, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_OPC(0x6C) }
  },{
    "clearCharsetMask", 0xC9, 0, 0, 1,
    { SCC_FA_OPC(0x6E) }
  },{
    "setActorShadowMode", 0xC9, 0, 3, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x6F) }
  },{
    "shiftShadowPalette", 0xC9, 0, 7, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x70) }
  },{
    "enqueueObject", 0xC9, 0, 8, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x77) }
  },{
    "swapColors", 0xC9, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x78) }
  },{
    "copyColor", 0xC9, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x7B) }
  },{
    "saveSound", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x7C) }
  },{

    NULL, 0, 0, 0, 0, {}
  }
};


static scc_func_t scc_func_v7_only[] = {
  {
    "panCameraTo", 0x78, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "setCameraAt", 0x7A, 0, 2, 0,
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "putActorAtObject", 0x80, 0, 2, 0, // z
    { SCC_FA_VAL, SCC_FA_VAL }
  },{
    "pickupObject", 0x84, 0, 1, 0,  // z
    { SCC_FA_VAL }
  },{
    "startRoomWithEgo", 0x85, 0, 3, 0, // z
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL }
  },{

    // kernelSet
    "grabCursor", 0xC9, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x04) }
  },{
    "smushPlay", 0xC9, 0, 0, 1,
    { SCC_FA_OPC(0x06) },
  },{
    "setCursorFromImage", 0xC9, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0C) }
  },{
    "remapActorPalette", 0xC9, 0, 4, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0D) }
  },{
    "remapActorPalette2", 0xC9, 0, 5, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x0E) }
  },{
    "setSmushFrameRate", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x0F) }
  },{
    "enqueueText", 0xC9, 0, 4, 1, // VAR_STRING2DRAW
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x10) }
  },{
    "enqueueText2", 0xC9, 0, 4, 1, // VAR_STRING2DRAW
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x11) }
  },{
    "setActorScale", 0xC9, 0, 2, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x6B) }
  },{
    "setShadowPalette", 0xC9, 0, 6, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x6C) }
  },{
    "setShadowPalette2", 0xC9, 0, 6, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_OPC(0x6D) }
  },{
    // 0x72 is a stub in scummvm 0.10.0
    "-freezeScripts2", 0xC9, 0, 0, 1,
    { SCC_FA_OPC(0x75) }
  },{
    "enqueueObject", 0xC9, 0, 8, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x76) }
  },{
    "enqueueObject2", 0xC9, 0, 8, 1,
    { SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL,
      SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_VAL, SCC_FA_OPC(0x77) }
  },{
    "saveSound", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0x7C) }
  },{
    "enableSubtitles", 0xC9, 0, 1, 1,
    { SCC_FA_VAL, SCC_FA_OPC(0xD7) }
  },{

    NULL, 0, 0, 0, 0, {}
  }
};

static scc_func_t* scc_func_v6[] = {
    scc_func_v6_v7,
    scc_func_v6_only,
    NULL
};

static scc_func_t* scc_func_v7[] = {
    scc_func_v6_v7,
    scc_func_v7_only,
    NULL
};

