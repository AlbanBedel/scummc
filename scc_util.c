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

// This file should contain generic stuff that can be helpfull anywhere.
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h> 

#ifdef IS_MINGW
#include <windows.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "scc_fd.h"

#include "scc_util.h"

int scc_log_level = 2;

void scc_log(int lvl,char* fmt, ...) {
    va_list ap;
    if(lvl > scc_log_level) return;
    va_start(ap, fmt);
    vfprintf(stderr,fmt,ap);
    va_end(ap);
}

// work around incomplete libc's
#ifndef HAVE_ASPRINTF

int vasprintf(char **strp, const char *fmt, va_list ap) {
    /* Guess we need no more than 100 bytes. */
    int n, size = 100;
    char *p;
    if ((p = malloc (size)) == NULL)
        return -1;
    while (1) {
        /* Try to print in the allocated space. */
        n = vsnprintf (p, size, fmt, ap);
        /* If that worked, return the string. */
        if (n > -1 && n < size) {
            strp[0] = p;
            return n;
        }
        /* Else try again with more space. */
        if (n > -1)    /* glibc 2.1 */
            size = n+1; /* precisely what is needed */
        else           /* glibc 2.0 */
            size *= 2;  /* twice the old size */
        if ((p = realloc (p, size)) == NULL)
            return -1;
    }
}

int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vasprintf(strp,fmt,ap);
    va_end(ap);
    return n;
}        

#endif

// this should probably go into some common util stuff
// or in scc_fd dunno
scc_data_t* scc_data_load(char* path) {
  scc_data_t* data;
  scc_fd_t* fd;
  int len;

  fd = new_scc_fd(path,O_RDONLY,0);
  if(!fd) {
    scc_log(LOG_ERR,"Failed to open %s.\n",path);
    return NULL;
  }
  // get file size
  scc_fd_seek(fd,0,SEEK_END);
  len = scc_fd_pos(fd);
  scc_fd_seek(fd,0,SEEK_SET);

  data = malloc(sizeof(scc_data_t) + len);
  data->size = len;

  if(scc_fd_read(fd,data->data,len) != len) {
    scc_log(LOG_ERR,"Failed to load %s\n",path);
    free(data);
    scc_fd_close(fd);
    return NULL;
  }

  scc_fd_close(fd);
  return data;
}

//
// Windows glob implementation from MPlayer.
//
#ifdef IS_MINGW

int glob(const char *pattern, int flags,
          int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
{
    HANDLE searchhndl;
    WIN32_FIND_DATA found_file;

    if(errfunc)
        scc_log(LOG_ERR,"glob():ERROR: Sorry errfunc not supported "
                "by this implementation\n");
    if(flags)
        scc_log(LOG_ERR,"glob():ERROR:Sorry no flags supported "
                "by this globimplementation\n");

    //scc_log(LOG_DBG,"PATTERN \"%s\"\n",pattern);

    pglob->gl_pathc = 0;
    searchhndl = FindFirstFile( pattern,&found_file);

    if(searchhndl == INVALID_HANDLE_VALUE) {
        if(GetLastError() == ERROR_FILE_NOT_FOUND) {
            pglob->gl_pathc = 0;
            //scc_log(LOG_DBG,"could not find a file matching your search criteria\n");
            return 1;
        } else {
            //scc_log(LOG_DBG,"glob():ERROR:FindFirstFile: %i\n",GetLastError());
            return 1;
        }
    }

    pglob->gl_pathv = malloc(sizeof(char*));
    pglob->gl_pathv[0] = strdup(found_file.cFileName);
    pglob->gl_pathc++;

    while(1) {
        if(!FindNextFile(searchhndl,&found_file)) {
            if(GetLastError()==ERROR_NO_MORE_FILES) {
                //scc_log(LOG_DBG,"glob(): no more files found\n");
                break;
            } else {
                //scc_log(LOG_DBG,"glob():ERROR:FindNextFile:%i\n",GetLastError());
                return 1;
            }
        } else {
            //scc_log(LOG_DBG,"glob: found file %s\n",found_file.cFileName);
            pglob->gl_pathc++;
            pglob->gl_pathv = realloc(pglob->gl_pathv,pglob->gl_pathc * sizeof(char*));
            pglob->gl_pathv[pglob->gl_pathc-1] = strdup(found_file.cFileName);
        }
    }
    FindClose(searchhndl);
    return 0;
}

void globfree(glob_t *pglob) {
    int i;
    for(i=0; i <pglob->gl_pathc ;i++)
        free(pglob->gl_pathv[i]);
    free(pglob->gl_pathv);
}

#endif // IS_MINGW
