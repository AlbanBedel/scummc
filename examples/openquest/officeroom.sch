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


room OfficeRoom {
    // for the intro
    bit didOfficeIntro;
    // for the plate
    bit hasSmelledPlate, hasPressedPlate, hasTalkedAboutPlate;

    object exitToSecretRoom;
    object ensignZobObj;
    object commanderZifObj;
    object carolObj;
    script setTalkedAboutPlate();
    script setTalkedAboutPlant();
}
