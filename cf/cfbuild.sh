#!/bin/sh
#
#  ScummC cfbuild.sh
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

if [ -z "$SRCDIR" ] ; then
   echo "\$SRCDIR must be defined."
   exit 0
fi

cd $SRCDIR/cf.build.logs

[ -z "$HOST_LIST" ] && \
    HOST_LIST="amd64-linux2 usf-cf-alpha-linux-1 x86-freebsd1 x86-openbsd1 x86-netbsd1 ppc-osx3 sparc-solaris2 x86-solaris1 openpower-linux1"

for host in $HOST_LIST ; do
    echo "Lauching 'make $@' on $host"
    { 
	echo > $host.log
	echo "Lauching 'make $@' on $host (`date`)" >> $host.log
	echo >> $host.log
	ssh -n $host ". .bash_profile ; cd $SRCDIR ; ./cf/dobuild.sh $@" 2>> $host.log >&2
	r=$?
	echo $r > $host.status
	if [ $r -ne 0 ] ; then
	    echo "Build failed ($r)" >> $host.log
	    echo >> $host.log
	    cat $host.config >> $host.log 2>&1
	    cat ~/$SRCDIR/build.$host*/*/build.log >> $host.log 2> /dev/null
	    echo "$host: FAILED"
	    tail $host.log
	    echo
	else
	    echo "$host: OK"
	    echo "Build successful" >> $host.log
	    echo >> $host.log
	    cat $host.config >> $host.log 2>&1
	    cat ~/$SRCDIR/build.$host*/*/build.log >> $host.log 2> /dev/null
	fi
    } &
done

echo
echo "Waiting for the builds to finish."
echo
wait
echo

err=0
for host in $HOST_LIST ; do
    r=`cat $host.status`
    [ "$r" = "0" ] || err=$(($err+1))
done
rm -f *.status
exit $err
