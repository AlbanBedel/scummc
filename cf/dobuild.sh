#!/bin/sh
#
#  ScummC dobuild.sh
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

# Get the hostname unless it was given.
[ -z "$HOST" ] && HOST=`uname -n | cut -d . -f 1`

# Add our own host specific binary directory if it's not already set.
# This is needed for the boxes with missing or too old make, bison, flex.
if [ -z "$BINDIR" ] ; then
    export BINDIR=$HOME/$HOST/bin 
    export PATH=$BINDIR:$PATH
fi

# Try to get the config (needed to know which make to run).
cfg=`sed 's,^ *#.*,,; s,?=,=,' config.$HOST.mak 2> /dev/null`
# No config available, run configure.
if [ $? -ne 0 ] ; then
    echo "Failed to parse config.$HOST.mak. Try to configure."
    ./configure --debug > cf.build.logs/$HOST.config
    if [ $? -ne 0 ] ; then
	echo "Configure failed."
	exit 1
    fi
    # Try to read the config again, quit if that fail.
    cfg=`sed 's,^ *#.*,,; s,?=,=,' config.$HOST.mak 2> /dev/null`
    if [ $? -ne 0 ] ; then
	echo "Failed to parse config.$HOST.mak after configure."
	exit 1
    fi
fi
# Load the config.
eval $cfg

# Quit if $MAKE is not defined.
if [ -z "$MAKE" ] ; then
    echo "Failed to find which make we must use."
    exit 1
fi


# Disable the slow output.
SHOW_WARNINGS=no "$MAKE" $@ 2> /dev/null >&2


