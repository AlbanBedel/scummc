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
 * @file scc_util.c
 * @ingroup utils
 * @brief Common stuff and portabilty helpers
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
// Windows glob implementation from SoX.
//
#ifdef IS_MINGW
typedef struct file_entry
{
    char name[MAX_PATH];
    struct file_entry *next;
} file_entry;

static int
insert(
    const char* path,
    const char* name,
    file_entry** phead)
{
    int len;
    file_entry* cur = malloc(sizeof(file_entry));
    if (!cur)
    {
        return ENOMEM;
    }

    len = _snprintf(cur->name, MAX_PATH, "%s%s", path, name);
    cur->name[MAX_PATH - 1] = 0;
    cur->next = *phead;
    *phead = cur;

    return len < 0 || len >= MAX_PATH ? ENAMETOOLONG : 0;
}

static int
entry_comparer(
    const void* pv1,
    const void* pv2)
{
	const file_entry* const * pe1 = pv1;
	const file_entry* const * pe2 = pv2;
    return _stricmp((*pe1)->name, (*pe2)->name);
}

int
glob(
    const char *pattern,
    int flags,
    int (*unused)(const char *epath, int eerrno),
    glob_t *pglob)
{
    char path[MAX_PATH];
    file_entry *head = NULL;
    int err = 0;
    size_t len;
    unsigned entries = 0;
    WIN32_FIND_DATAA finddata;
    HANDLE hfindfile = FindFirstFileA(pattern, &finddata);

    if (!pattern || flags != (flags & GLOB_FLAGS) || unused || !pglob)
    {
        errno = EINVAL;
        return EINVAL;
    }

    path[MAX_PATH - 1] = 0;
    strncpy(path, pattern, MAX_PATH);
    if (path[MAX_PATH - 1] != 0)
    {
        errno = ENAMETOOLONG;
        return ENAMETOOLONG;
    }

    len = strlen(path);
    while (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\')
        len--;
    path[len] = 0;

    if (hfindfile == INVALID_HANDLE_VALUE)
    {
        if (flags & GLOB_NOCHECK)
        {
            err = insert("", pattern, &head);
            entries++;
        }
    }
    else
    {
        do
        {
            err = insert(path, finddata.cFileName, &head);
            entries++;
        } while (!err && FindNextFileA(hfindfile, &finddata));

        FindClose(hfindfile);
    }

    if (err == 0)
    {
        pglob->gl_pathv = malloc((entries + 1) * sizeof(char*));
        if (pglob->gl_pathv)
        {
            pglob->gl_pathc = entries;
            pglob->gl_pathv[entries] = NULL;
            for (; head; head = head->next, entries--)
                pglob->gl_pathv[entries - 1] = (char*)head;
            qsort(pglob->gl_pathv, pglob->gl_pathc, sizeof(char*), entry_comparer);
        }
        else
        {
            pglob->gl_pathc = 0;
            err = ENOMEM;
        }
    }
    else if (pglob)
    {
        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }

    if (err)
    {
        file_entry *cur;
        while (head)
        {
            cur = head;
            head = head->next;
            free(cur);
        }

        errno = err;
    }

    return err;
}

void
globfree(
    glob_t* pglob)
{
    if (pglob)
    {
        char** cur;
        for (cur = pglob->gl_pathv; *cur; cur++)
        {
            free(*cur);
        }

        pglob->gl_pathc = 0;
        pglob->gl_pathv = NULL;
    }
}

#endif // IS_MINGW
