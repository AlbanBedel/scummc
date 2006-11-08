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

/** @file scc_smf.c
 *  @brief A simple Standard MIDI File parser/writer.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "scc_util.h"
#include "scc_fd.h"
#include "scc_smf.h"


unsigned scc_fd_read_smf_int(scc_fd_t* fd, unsigned max_size,
                             uint32_t* ret) {
  unsigned v = 0x80, val = 0, n = 0;
  while((v & 0x80) && n < max_size) {
    v = scc_fd_r8(fd);
    val <<= 7;
    val |= v&0x7F;
    n++;
  }
  if(v & 0x80) return 0;
  *ret = val;
  return n;
}

int scc_fd_write_smf_int(scc_fd_t* fd, unsigned v) {
  int i;
  if(!v) return scc_fd_w8(fd,v);
  // Find the start
  for(i = 4 ; i >= 0 ; i--)
    if(v & (0x7F << (i*7))) break;
  for( ; i > 0 ; i--)
    scc_fd_w8(fd,0x80 | (v >> (i*7)));
  return scc_fd_w8(fd,v & 0x7F);
}

static int scc_smf_parse_track(scc_fd_t* fd, scc_smf_track_t* track,
                               unsigned size) {
  scc_smf_event_t* ev = NULL;
  unsigned pos = 0;
  uint64_t time = 0;
  
  while(pos < size) {
    uint32_t delta, arg_size;
    uint8_t cmd, meta = 0, runaway = 0;
    int n = scc_fd_read_smf_int(fd,size-pos,&delta);
    if(!n || pos + n + 2 > size) return 0;
    cmd = scc_fd_r8(fd);
    pos += n+1;
    // Meta and sysex events
    switch(cmd) {
    case SMF_META_EVENT:
      if(pos + 2 > size) {
        scc_log(LOG_ERR,"Invalid Meta Event size.\n");
        return 0;
      }
      meta = scc_fd_r8(fd), pos++;
    case SMF_SYSEX_EVENT:
    case SMF_SYSEX_CONTINUATION:
      // Read the event length
      if(!(n = scc_fd_read_smf_int(fd,size-pos,&arg_size))) {
        scc_log(LOG_ERR,"Failed to read SysEx length.\n");
        return 0;
      }
      pos += n;
      break;
    default:
      if(cmd & 0x80) { // Normal event
        if((cmd & 0xF0) == 0xC0 ||
           (cmd & 0xF0) == 0xD0)
          arg_size = 1;
        else
          arg_size = 2;
      } else { // Runaway event
        if(!track->last_event) {
          scc_log(LOG_ERR,"Runaway event without a preceding event.\n");
          return 0;
        }
        arg_size = track->last_event->args_size-1;
        runaway = 1;
        //vent = track->last_event->event;
      }
    }

    if(pos + arg_size > size) {
      scc_log(LOG_ERR,"Invalid argument size.\n");
      return 0;
    }

    ev = malloc(sizeof(scc_smf_event_t) + runaway + arg_size);
    ev->next = NULL;
    ev->time = time + delta;
    if(runaway) {
      ev->cmd = track->last_event->cmd;
      ev->args[0] = cmd;
    } else
      ev->cmd = cmd;
    ev->meta = meta;
    ev->args_size = runaway + arg_size;
    if(arg_size > 0 && scc_fd_read(fd,&ev->args[runaway],arg_size) != arg_size) {
      scc_log(LOG_ERR,"Failed to read arguments.\n");
      free(ev);
      return 0;
    }
    pos += arg_size;
    SCC_LIST_ADD(track->events,track->last_event,ev);
    time += delta;
  }
  
  // Compute the size ourself in case the input data
  // is not encoded in the shortest possible way.
  track->size = scc_smf_track_get_size(track);

  return 1;
}

scc_smf_t* scc_smf_parse(scc_fd_t* fd) {
  scc_smf_t* smf;
  uint32_t chunk, size;

  while(1) {
      chunk = scc_fd_r32(fd);
      // Skip scumm chunk headers
      if(chunk == MKID('A','D','L',' ') ||
         chunk == MKID('R','O','L',' ') ||
         chunk == MKID('G','M','D',' ')) {
          scc_fd_r32(fd);
          continue;
      }
      if(chunk == MKID('M','D','h','d') ||
         chunk == MKID('M','D','p','g')) {
          size = scc_fd_r32be(fd);
          scc_fd_seek(fd,size,SEEK_CUR);
          continue;
      }

      if(chunk != MKID('M','T','h','d') ||
         scc_fd_r32be(fd) != 6) {
          scc_log(LOG_ERR,"%s doesn't seems to be a valid MIDI file.\n",fd->filename);
          return 0;
      }
      break;
  }
  
  smf = calloc(1,sizeof(scc_smf_t));
  smf->size = 8 + 6;
  smf->type = scc_fd_r16be(fd);
  scc_fd_r16be(fd); //smf->num_track = scc_fd_r16be(fd);
  smf->division = scc_fd_r16be(fd);

  // Read the tracks
  while(1) {
    chunk = scc_fd_r32(fd);
    size = scc_fd_r32be(fd);
  
    // EOF
    if(!chunk || !size) break;

    // Not a MIDI track: skip it
    if(chunk != MKID('M','T','r','k')) {
      scc_log(LOG_WARN,"Skiping unknow midi chunk: %c%c%c%c\n",UNMKID(chunk));
      scc_fd_seek(fd,SEEK_CUR,size);
      continue;
    }
    smf->track = realloc(smf->track,(smf->num_track+1)*sizeof(scc_smf_track_t));
    memset(&smf->track[smf->num_track],0,sizeof(scc_smf_track_t));
    if(!scc_smf_parse_track(fd,&smf->track[smf->num_track],size)) {
      scc_log(LOG_ERR,"Failed to parse MIDI track %d\n",smf->num_track);
      scc_smf_free(smf);
      return NULL;
    }
    smf->size += smf->track[smf->num_track].size;
    smf->num_track++;
  }

  return smf;
}

scc_smf_t* scc_smf_parse_file(char* file) {
  scc_fd_t* fd = new_scc_fd(file,O_RDONLY,0);
  scc_smf_t* smf;
  if(!fd) {
    scc_log(LOG_ERR,"Failed to open %s: %s\n",file,strerror(errno));
    return NULL;
  }
  smf = scc_smf_parse(fd);
  scc_fd_close(fd);
  return smf;
}

void scc_smf_free(scc_smf_t* smf) {
  if(smf->track) {
    int i;
    for(i = 0 ; i < smf->num_track ; i++) {
      scc_smf_event_t* ev;
      SCC_LIST_FREE(smf->track[i].events,ev);
    }
    free(smf->track);
  }
  free(smf);
}


unsigned scc_smf_get_int_size(unsigned v) {
  int r = sizeof(v)*8/7;
  if(!v) return 1;
  while(!(v & (0x7F<<(7*r))))
    r--;
  return r+1;
}

int scc_smf_track_get_size(scc_smf_track_t* track) {
  unsigned size = 8; // Header
  scc_smf_event_t* ev, *last = NULL;

  for(ev = track->events ; ev ; last = ev, ev = ev->next) {
    // Delta
    size += scc_smf_get_int_size(last ? ev->time - last->time : 0);
    // Events
    switch(ev->cmd) {
    case SMF_META_EVENT:
      size++; // Meta event type
    case SMF_SYSEX_EVENT:
    case SMF_SYSEX_CONTINUATION:
      size++; // No runaway
      size += scc_smf_get_int_size(ev->args_size);
      break;
    default:
      // Event (if it is not a runaway)
      if(!last || last->cmd != ev->cmd)
        size++;
    }
    // Arguments
    size += ev->args_size;
  }

  return size;
}

int scc_smf_get_size(scc_smf_t* smf) {
  unsigned size = 8+6; // Header
  unsigned i;

  for(i = 0 ; i < smf->num_track ; i++)
    size += scc_smf_track_get_size(&smf->track[i]);

  return size;
}

int scc_smf_track_write(scc_smf_track_t* track, scc_fd_t* fd) {
  scc_smf_event_t* ev, *last = NULL;
  // Write the header
  scc_fd_w32(fd,MKID('M','T','r','k'));
  scc_fd_w32be(fd,track->size-8);
  for(ev = track->events ; ev ; last = ev, ev = ev->next) {
    scc_fd_write_smf_int(fd,last ? ev->time - last->time : 0);
    switch(ev->cmd) {
    case SMF_META_EVENT:
    case SMF_SYSEX_EVENT:
    case SMF_SYSEX_CONTINUATION:
      scc_fd_w8(fd,ev->cmd);
      if(ev->cmd == SMF_META_EVENT)
        scc_fd_w8(fd,ev->meta);
      scc_fd_write_smf_int(fd,ev->args_size);
      break;
    default:
      if(!last || last->cmd != ev->cmd)
        scc_fd_w8(fd,ev->cmd);
            
    }
    if(ev->args_size > 0)
      scc_fd_write(fd,ev->args,ev->args_size);
  }
  return 1;
}

int scc_smf_write(scc_smf_t* smf, scc_fd_t* fd) {
  int i;
  // Write the header
  scc_fd_w32(fd,MKID('M','T','h','d'));
  scc_fd_w32be(fd,6);
  scc_fd_w16be(fd,smf->type);
  scc_fd_w16be(fd,smf->num_track);
  scc_fd_w16be(fd,smf->division);
  // Write the tracks
  for(i = 0 ; i < smf->num_track ; i++)
    scc_smf_track_write(&smf->track[i],fd);
  return 1;
}

int scc_smf_write_file(scc_smf_t* smf, char* path) {
  scc_fd_t* fd = new_scc_fd(path,O_WRONLY|O_CREAT|O_TRUNC,0);

  if(!fd) {
    printf("Failed to open %s for writing.\n",path);
    return 0;
  }
  
  if(!scc_smf_write(smf,fd)) {
    printf("Failed to write MIDI file %s.\n",path);
    scc_fd_close(fd);
    return 0;
  }
  
  scc_fd_close(fd);
  return 1;
}


int scc_smf_remove_track(scc_smf_t* smf, unsigned track) {
  scc_smf_event_t* ev;

  if(track >= smf->num_track) return 0;

  SCC_LIST_FREE(smf->track[track].events,ev);
  smf->size -= smf->track[track].size;

  if(track < smf->num_track-1)
    memmove(&smf->track[track],&smf->track[track+1],
            (smf->num_track-track-1)*sizeof(scc_smf_track_t));

  smf->num_track--;

  return 1;
}

int scc_smf_merge_track(scc_smf_t* smf, unsigned trackA, unsigned trackB) {
  scc_smf_event_t *evA,*evB = NULL,*evBnext = NULL,*evEnd = NULL;
  
  if(trackA == trackB ||
     trackA >= smf->num_track ||
     trackB >= smf->num_track)
    return 0;
  
  if(smf->track[trackA].last_event->cmd == SMF_META_EVENT &&
     smf->track[trackA].last_event->meta == 0x2F) {
    evEnd = smf->track[trackA].last_event;
    for(evA = smf->track[trackA].events ; evA->next ; evB = evA, evA = evA->next);
    evB->next = NULL;
    smf->track[trackA].last_event = evB;
  }
  if(smf->track[trackB].last_event->cmd == SMF_META_EVENT &&
     smf->track[trackB].last_event->meta == 0x2F) {
    for(evA = smf->track[trackB].events ; evA->next ; evB = evA, evA = evA->next);
    if(evB) {
      evB->next = NULL;
      smf->track[trackB].last_event = evB;
    } else
      smf->track[trackB].last_event = smf->track[trackB].events = NULL;
    if(!evEnd) evEnd = evA->next;
    else free(evA);
  }
    
  evA = smf->track[trackA].events;
  for(evB = smf->track[trackB].events ; evB ; evB = evBnext) {
    evBnext = evB->next;
    if(!evA) {
      evB->next = NULL;
      smf->track[trackA].last_event = smf->track[trackA].events = evB;
    } else {
      while(evA->next && evA->next->time <= evB->time)
        evA = evA->next;
      evB->next = evA->next;
      evA->next = evB;
      if(!evB->next)
        smf->track[trackA].last_event = evB;
    }
  }
  
  if(evEnd) {
    while(evA->next)
      evA = evA->next;
    evA->next = evEnd;
    smf->track[trackA].last_event = evEnd;
  }

  smf->track[trackA].size = scc_smf_track_get_size(&smf->track[trackA]);
  smf->track[trackB].events = smf->track[trackB].last_event = NULL;

  return scc_smf_remove_track(smf,trackB);
}

void scc_smf_dump(scc_smf_t* smf) {
  int i,j;
  scc_smf_event_t* ev;
  
  scc_log(LOG_MSG,"Total size: %d\n",smf->size);
  scc_log(LOG_MSG,"Type: %d\n",smf->type);
  scc_log(LOG_MSG,"Division: %d\n",smf->division);
  
  for(i = 0 ; i < smf->num_track ; i++) {
    scc_log(LOG_MSG,"Track %d (%d):\n",i,smf->track[i].size);
    for(ev = smf->track[i].events ; ev ; ev = ev->next) {
      scc_log(LOG_MSG,"%6" PRIu64 "  : %02X    - ",ev->time, ev->cmd);
      switch(ev->cmd & 0xF0) {
      case 0x80:
        scc_log(LOG_MSG,"Note off (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0x90:
        scc_log(LOG_MSG,"Note on (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0xA0:
        scc_log(LOG_MSG,"Aftertouch (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0xB0:
        if(ev->args[0] > 0x77) {
          scc_log(LOG_MSG,"Chan mode (%02X)",ev->cmd & 0x0F);
          switch(ev->args[0]) {
          case 0x79:
            scc_log(LOG_MSG," [Reset All Controllers]");
            break;
          case 0x7A:
            scc_log(LOG_MSG," [Local Control]");
            break;
          case 0x7B:
            scc_log(LOG_MSG," [All Notes Off]");
            break;
          case 0x7C:
            scc_log(LOG_MSG," [Omni Mode Off]");
            break;
          case 0x7D:
            scc_log(LOG_MSG," [Omni Mode On]");
            break;
          case 0x7E:
            scc_log(LOG_MSG," [Mono Mode On]");
            break;
          case 0x7F:
            scc_log(LOG_MSG," [Poly Mode On]");
            break;
          }
        } else {
          scc_log(LOG_MSG,"Control change (chan: %d)",ev->cmd & 0x0F);
          switch(ev->args[0]) {
          case 0x01:
            scc_log(LOG_MSG," [Mod Wheel]");
            break;
          case 0x02:
            scc_log(LOG_MSG," [Breath ctrl]");
            break;
          case 0x04:
            scc_log(LOG_MSG," [Foot ctrl]");
            break;
          case 0x05:
            scc_log(LOG_MSG," [Portamento Time]");
            break;
          case 0x06:
            scc_log(LOG_MSG," [Data Entry MSB]");
            break;
          case 0x07:
            scc_log(LOG_MSG," [Volume]");
            break;
          case 0x08:
            scc_log(LOG_MSG," [Balance]");
            break;
          case 0x0A:
            scc_log(LOG_MSG," [Pan]");
            break;
          case 0x0B:
            scc_log(LOG_MSG," [Expression ctrl]");
            break;
          case 0x10:
            scc_log(LOG_MSG," [General Purpose 1]");
            break;
          case 0x11:
            scc_log(LOG_MSG," [General Purpose 2]");
            break;
          case 0x12:
            scc_log(LOG_MSG," [General Purpose 3]");
            break;
          case 0x13:
            scc_log(LOG_MSG," [General Purpose 4]");
            break;
          case 0x40:
            scc_log(LOG_MSG," [Sustain]");
            break;
          case 0x41:
            scc_log(LOG_MSG," [Portamento]");
            break;
          case 0x42:
            scc_log(LOG_MSG," [Sustenuto]");
            break;
          case 0x43:
            scc_log(LOG_MSG," [Soft Pedal]");
            break;
          case 0x45:
            scc_log(LOG_MSG," [Hold 2]");
            break;
          case 0x50:
            scc_log(LOG_MSG," [General Purpose 5]");
            break;
          case 0x51:
            scc_log(LOG_MSG," [Temp Change (General Purpose 6)]");
            break;
          case 0x52:
            scc_log(LOG_MSG," [General Purpose 7]");
            break;
          case 0x53:
            scc_log(LOG_MSG," [General Purpose 8]");
            break;
          case 0x5B:
            scc_log(LOG_MSG," [Ext Effects Depth]");
            break;
          case 0x5C:
            scc_log(LOG_MSG," [Tremelo Depth]");
            break;
          case 0x5D:
            scc_log(LOG_MSG," [Chorus Depth]");
            break;
          case 0x5E:
            scc_log(LOG_MSG," [Detune Depth (Celeste Depth)]");
            break;
          case 0x5F:
            scc_log(LOG_MSG," [Phaser Depth]");
            break;
          case 0x60:
            scc_log(LOG_MSG," [Data Increment]");
            break;
          case 0x61:
            scc_log(LOG_MSG," [Data Decrement]");
            break;
          case 0x62:
            scc_log(LOG_MSG," [Non-Registered Param LSB]");
            break;
          case 0x63:
            scc_log(LOG_MSG," [Non-Registered Param MSB]");
            break;
          case 0x64:
            scc_log(LOG_MSG," [Registered Param LSB]");
            break;
          case 0x65:
            scc_log(LOG_MSG," [Registered Param MSB]");
            break;
          }
          if(ev->args[0] >= 0x20 &&
             ev->args[0] <  0x40)
            scc_log(LOG_MSG," [Set MSB]");
        }
        break;
      case 0xC0:
        scc_log(LOG_MSG,"Program Change (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0xD0:
        scc_log(LOG_MSG,"Chan. Pressure (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0xE0:
        scc_log(LOG_MSG,"Pitch Wheel (chan: %d)",ev->cmd & 0x0F);
        break;
      case 0xF0:
        switch(ev->cmd & 0x0F) {
        case 0x00:
        case 0x07:
          scc_log(LOG_MSG,"SysEx");
          switch(ev->args[0]) {
          case 0x41:
              scc_log(LOG_MSG," - Roland");
              break;
          case 0x7C:
              scc_log(LOG_MSG," - YM2612");
              break;
          case 0x7D:
              scc_log(LOG_MSG," - iMUSE - ");
              switch(ev->args[1]) {
              case 0x00:
                  scc_log(LOG_MSG,"Allocate new part");
                  break;
              case 0x01:
                  scc_log(LOG_MSG,"Shut down a part");
                  break;
              case 0x02:
                  scc_log(LOG_MSG,"Start of song");
                  break;
              case 0x10:
                  scc_log(LOG_MSG,"Adlib instrument definition (Part)");
                  break;
              case 0x11:
                  scc_log(LOG_MSG,"Adlib instrument definition (Global)");
                  break;
              case 0x21:
                  scc_log(LOG_MSG,"Parameter adjust");
                  break;
              case 0x30:
                  scc_log(LOG_MSG,"Hook - jump");
                  break;
              case 0x31:
                  scc_log(LOG_MSG,"Hook - global transpose");
                  break;
              case 0x32:
                  scc_log(LOG_MSG,"Hook - part on/off");
                  break;
              case 0x33:
                  scc_log(LOG_MSG,"Hook - set volume");
                  break;
              case 0x34:
                  scc_log(LOG_MSG,"Hook - set program");
                  break;
              case 0x35:
                  scc_log(LOG_MSG,"Hook - set transpose");
                  break;
              case 0x40:
                  scc_log(LOG_MSG,"Marker");
                  break;
              case 0x50:
                  scc_log(LOG_MSG,"Set loop");
                  break;
              case 0x51:
                  scc_log(LOG_MSG,"Clear loop");
                  break;
              case 0x60:
                  scc_log(LOG_MSG,"Set instrument");
                  break;
              }
          }
          break;
        case 0x08:
          scc_log(LOG_MSG,"Timing clock");
          break;
        case 0x09:
          scc_log(LOG_MSG,"Reserved");
          break;
        case 0x0A:
          scc_log(LOG_MSG,"Start");
          break;
        case 0x0B:
          scc_log(LOG_MSG,"Continue");
          break;
        case 0x0C:
          scc_log(LOG_MSG,"Stop");
          break;
        case 0x0D:
          scc_log(LOG_MSG,"Reserved");
          break;
        case 0x0E:
          scc_log(LOG_MSG,"Active Sensing");
          break;
        case 0x0F:
          scc_log(LOG_MSG,"\b\b\b\b\b%02X - Meta event ",ev->meta);
          switch(ev->meta) {
          case 0x00:
            scc_log(LOG_MSG,"(Seq Num)");
            break;
          case 0x01:
            scc_log(LOG_MSG,"(Text)");
            break;
          case 0x02:
            scc_log(LOG_MSG,"(Copyright)");
            break;
          case 0x03:
            scc_log(LOG_MSG,"(Seq name)");
            break;
          case 0x04:
            scc_log(LOG_MSG,"(Instrument name)");
            break;
          case 0x05:
            scc_log(LOG_MSG,"(Lyric)");
            break;
          case 0x06:
            scc_log(LOG_MSG,"(Marker)");
            break;
          case 0x07:
            scc_log(LOG_MSG,"(Cue point)");
            break;
          case 0x20:
            scc_log(LOG_MSG,"(Chan prefix)");
            break;
          case 0x21:
            scc_log(LOG_MSG,"(Port prefix)");
            break;
          case 0x2F:
            scc_log(LOG_MSG,"(End of track)");
            break;
          case 0x51:
            scc_log(LOG_MSG,"(Set tempo)");
            break;
          case 0x54:
            scc_log(LOG_MSG,"(SMPTE Offset)");
            break;
          case 0x58:
            scc_log(LOG_MSG,"(Time sig)");
            break;
          case 0x59:
            scc_log(LOG_MSG,"(Key sig)");
            break;
          case 0x7F:
            scc_log(LOG_MSG,"(Sequencer-Specific event)");
            break;
          default:
            scc_log(LOG_MSG,"(%02X)",ev->meta);
          }
        default:
          // Fallback
          ; // kill gcc warning
        }
        break;
      default:
        scc_log(LOG_MSG,"%02X (%d)",ev->cmd,ev->args_size);
      }
      for(j = 0 ; j < ev->args_size ; j++) {
        if(j % 16 == 0) scc_log(LOG_MSG,"\n         ");
        scc_log(LOG_MSG," %02X",ev->args[j]);
      }
      scc_log(LOG_MSG,"\n");
    }
    scc_log(LOG_MSG,"\n");
  }

}
