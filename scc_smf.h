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

/** @file scc_smf.h
 *  @brief A simple Standard MIDI File parser/writer.
 */

#define SMF_META_EVENT            0xFF
#define SMF_SYSEX_EVENT           0xF0
#define SMF_SYSEX_CONTINUATION    0xF7

/// A MIDI event.
typedef struct scc_smf_event {
    struct scc_smf_event* next;
    uint64_t time;          ///< Absolute time position.
    uint8_t  cmd;           ///< Event command.
    uint8_t  meta;          ///< Meta event code
    unsigned args_size;     ///< Size of the arguments
    unsigned char args[0];
} scc_smf_event_t;

/// A MIDI track.
typedef struct scc_smf_track {
    unsigned size;                ///< Size of the track when wrote in a file.
    scc_smf_event_t *events;      ///< Event list.
    scc_smf_event_t *last_event;  ///< Event list tail.
} scc_smf_track_t;

/// A MIDI file.
typedef struct scc_smf {
    unsigned size;             ///< Size of the whole file.
    unsigned type;             ///< MIDI file type.
    unsigned division;
    unsigned num_track;
    scc_smf_track_t* track;
} scc_smf_t;

/// Read a MIDI variable length integer.
unsigned scc_fd_read_smf_int(scc_fd_t* fd, unsigned max_size,
                             uint32_t* ret);

/// Write a MIDI variable length integer.
int scc_fd_write_smf_int(scc_fd_t* fd, unsigned v);

/// Parse a MIDI file from a scc_fd_t.
scc_smf_t* scc_smf_parse(scc_fd_t* fd);

/// Parse a MIDI file at the given path.
scc_smf_t* scc_smf_parse_file(char* file);

/// Free the memory used for the MIDI file.
void scc_smf_free(scc_smf_t* smf);

/// Find the size of an integer coded in variable length.
unsigned scc_smf_get_int_size(unsigned v);

/// Compute the size of a MIDI track.
int scc_smf_track_get_size(scc_smf_track_t* track);

/// Compute the size of a whole MIDI file.
int scc_smf_get_size(scc_smf_t* smf);

/// Remove a track.
int scc_smf_remove_track(scc_smf_t* smf, unsigned track);

/// Merge 2 tracks together.
int scc_smf_merge_track(scc_smf_t* smf, unsigned trackA, unsigned trackB);

/// Write a MIDI file to a fd.
int scc_smf_write(scc_smf_t* smf, scc_fd_t* fd);

/// Save to a MIDI file
int scc_smf_write_file(scc_smf_t* smf, char* path);

/// Print all the tracks and their events, for debug purpose.
void scc_smf_dump(scc_smf_t* smf);
