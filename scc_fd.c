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


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "scc_fd.h"

scc_fd_t* new_scc_fd(char* path,int flags,uint8_t key) {
  int fd;
  scc_fd_t* scc_fd;

  if(flags & O_CREAT)
    fd = open(path,flags,0644);
  else
    fd = open(path,flags);
  if(fd < 0) return NULL;
  scc_fd = malloc(sizeof(scc_fd_t));
  scc_fd->fd = fd;
  scc_fd->enckey = key;
  scc_fd->filename = strdup(path);

  return scc_fd;
}

int scc_fd_close(scc_fd_t* f) {
  int r = close(f->fd);
  free(f->filename);
  free(f);
  return r;
}

int scc_fd_read(scc_fd_t* f,void *buf, size_t count) {
  if(count <= 0) return 0;
  count = read(f->fd,buf,count);
  if(count <= 0) return count;
  else {
    uint8_t* ptr = ((uint8_t*)buf) + count;
    do {
      ptr--;
      *ptr ^= f->enckey;
    } while(ptr != buf);
  }
  return count;
}

uint8_t* scc_fd_load(scc_fd_t* f,size_t count) {
  uint8_t* ret = malloc(count);
  int r,pos = 0;
  
  while(pos < count) {
    r = scc_fd_read(f,ret+pos,count - pos);
    if(r <= 0) {
      free(ret);
      return NULL;
    }
    pos += r;
  }

  return ret;
}

int scc_fd_dump(scc_fd_t* f,char* path,int size) {
  char buf[2048];
  int r=0,w,dfd = open(path,O_CREAT|O_WRONLY,0644);

  if(dfd < 0) {
    printf("Failed to open %s: %s\n",path,strerror(errno));
    return 0;
  }

  while(size > 0) {
    r = scc_fd_read(f,buf,size > 2048 ? 2048 : size);
    if(r < 0) {
      printf("Read error: %s\n",strerror(errno));
      close(dfd);
      return 0;
    }
    size -= r;
    while(r > 0) {
      w = write(dfd,buf,r);
      if(w < 0) {
	printf("Write error: %s\n",strerror(errno));
	close(dfd);
	return 0;
      }
      if(w < r) memmove(buf,buf+w,r-w);
      r -= w;
    }
  }
  close(dfd);
  return 1;
}

off_t scc_fd_seek(scc_fd_t* f, off_t offset, int whence) {
  return lseek(f->fd,offset,whence);
}

off_t scc_fd_pos(scc_fd_t* f) {
  return lseek(f->fd,0,SEEK_CUR);
}

uint8_t scc_fd_r8(scc_fd_t* f) {
  uint8_t r = 0;
  scc_fd_read(f,&r,1);
  return r;
}

uint16_t scc_fd_r16le(scc_fd_t* f) {
  uint16_t a = scc_fd_r8(f);
  uint16_t b = scc_fd_r8(f);
  return a | (b << 8);
}

uint32_t scc_fd_r32le(scc_fd_t* f) {
  uint32_t a = scc_fd_r16le(f);
  uint32_t b = scc_fd_r16le(f);
  return (b << 16) | a;
}

uint16_t scc_fd_r16be(scc_fd_t* f) {
  uint16_t b = scc_fd_r8(f);
  uint16_t a = scc_fd_r8(f);
  return a | (b << 8);
}

uint32_t scc_fd_r32be(scc_fd_t* f) {
  uint32_t b = scc_fd_r16be(f);
  uint32_t a = scc_fd_r16be(f);
  return (b << 16) | a;
}

int scc_fd_write(scc_fd_t* f,void *buf, size_t count) {
  int w;
  uint8_t* ptr = buf;

  for(w = 0 ; w < count ; w++)
    ptr[w] ^= f->enckey;

  w = write(f->fd,buf,count);

  for(w = 0 ; w < count ; w++)
    ptr[w] ^= f->enckey;

  return w;
}

int scc_fd_w8(scc_fd_t*f,uint8_t a) {
  return scc_fd_write(f,&a,sizeof(a));
}

int scc_fd_w16le(scc_fd_t*f,uint16_t a) {
  scc_fd_w8(f,a & 0xFF);
  return scc_fd_w8(f,(a >> 8) & 0xFF);
}

int scc_fd_w32le(scc_fd_t*f,uint32_t a) {
  scc_fd_w16le(f,a & 0xFFFF);
  return scc_fd_w16le(f,(a >> 16) & 0xFFFF);
}

int scc_fd_w16be(scc_fd_t*f,uint16_t a) {
  scc_fd_w8(f,(a >> 8) & 0xFF);
  return scc_fd_w8(f,a & 0xFF);
}

int scc_fd_w32be(scc_fd_t*f,uint32_t a) {
  scc_fd_w16be(f,(a >> 16) & 0xFFFF);
  return scc_fd_w16be(f,a & 0xFFFF);
}

