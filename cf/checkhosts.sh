#!/bin/sh
#
#  ScummC checkhosts.sh
#  Copyright (C) 2006  Alban Bedel
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

[ -z "$HOST_LIST" ] && \
 HOST_LIST="x86-linux1 x86-linux2 x86-freebsd1 x86-openbsd1 x86-netbsd1 x86-solaris1 amd64-linux1 amd64-linux2 usf-cf-alpha-linux-1 ppc-osx1 ppc-osx2 ppc-osx3 openpower-linux1 sparc-solaris1 sparc-solaris2"

if type tput > /dev/null 2>&1 ; then
  RED="`(tput bold ; tput setf 4) 2> /dev/null`"
  GREEN="`(tput bold ; tput setf 2) 2> /dev/null`"
  SGR0="`tput sgr0 2> /dev/null`"
fi

if [ -z "$SRCDIR" ] ; then
   echo "\$SRCDIR must be defined."
   exit 0
fi

cd $SRCDIR

for host in $HOST_LIST ; do
  {
    ping -c 1 $host > /dev/null 2>&1
    echo $? > cf/$host.status
  } &
done

wait

for host in $HOST_LIST ; do
  r=`cat cf/$host.status`
  printf "%-24s: " $host
  [ "$r"  = "0" ] && echo "${GREEN}up${SGR0}" || echo "${RED}down${SGR0}"
done
