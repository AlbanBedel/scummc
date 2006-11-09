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
 * @file scc_fd.h
 * @ingroup utils
 * @brief Read/write file with XOR encryption
 */


typedef struct scc_fd {
  int fd;
  uint8_t enckey;
  char* filename;
} scc_fd_t;

scc_fd_t* new_scc_fd(char* path,int flags,uint8_t key);

int scc_fd_close(scc_fd_t* f);

int scc_fd_read(scc_fd_t* f,void *buf, size_t count);

uint8_t* scc_fd_load(scc_fd_t* f,size_t count);

int scc_fd_dump(scc_fd_t* f,char* path,int size);

off_t scc_fd_seek(scc_fd_t* f, off_t offset, int whence);

off_t scc_fd_pos(scc_fd_t* f);

uint8_t scc_fd_r8(scc_fd_t* f);

uint16_t scc_fd_r16le(scc_fd_t* f);
uint32_t scc_fd_r32le(scc_fd_t* f);

uint16_t scc_fd_r16be(scc_fd_t* f);
uint32_t scc_fd_r32be(scc_fd_t* f);

int scc_fd_write(scc_fd_t* f,void *buf, size_t count);

int scc_fd_w8(scc_fd_t*f,uint8_t a);

int scc_fd_w16le(scc_fd_t*f,uint16_t a);
int scc_fd_w32le(scc_fd_t*f,uint32_t a);

int scc_fd_w16be(scc_fd_t*f,uint16_t a);
int scc_fd_w32be(scc_fd_t*f,uint32_t a);

#ifdef IS_LITTLE_ENDIAN
#define scc_fd_r16(f) scc_fd_r16le(f)
#define scc_fd_r32(f) scc_fd_r32le(f)
#define scc_fd_w16(f,a) scc_fd_w16le(f,a)
#define scc_fd_w32(f,a) scc_fd_w32le(f,a)
#elif defined IS_BIG_ENDIAN
#define scc_fd_r16(f) scc_fd_r16be(f)
#define scc_fd_r32(f) scc_fd_r32be(f)
#define scc_fd_w16(f,a) scc_fd_w16be(f,a)
#define scc_fd_w32(f,a) scc_fd_w32be(f,a)
#else
#error "Endianness is not defined !!!"
#endif

#ifdef va_start
int scc_fd_vprintf(scc_fd_t*f,const char *fmt, va_list ap);
#endif

int scc_fd_printf(scc_fd_t*f,const char *fmt, ...);
