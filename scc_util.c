
// This file should contain generic stuff that can be helpfull anywhere.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"

#include "scc_util.h"

// this should probably go into some common util stuff
// or in scc_fd dunno
scc_data_t* scc_data_load(char* path) {
  scc_data_t* data;
  scc_fd_t* fd;
  int len;

  fd = new_scc_fd(path,O_RDONLY,0);
  if(!fd) {
    printf("Failed to open %s.\n",path);
    return NULL;
  }
  // get file size
  scc_fd_seek(fd,0,SEEK_END);
  len = scc_fd_pos(fd);
  scc_fd_seek(fd,0,SEEK_SET);

  data = malloc(sizeof(scc_data_t) + len);
  data->size = len;

  if(scc_fd_read(fd,data->data,len) != len) {
    printf("Failed to load %s\n",path);
    free(data);
    scc_fd_close(fd);
    return NULL;
  }

  scc_fd_close(fd);
  return data;
}
