#!/bin/sh

#============================================================================
# - Optimize for size
# - Strip symbols to reduce size
#============================================================================
cat Makefile|sed 's/^CFLAGS = -g -O2/CFLAGS = -Os -s/' > Makefile.srd
mv Makefile.srd Makefile
if [ "0" != "$?" ];then
	exit 1
fi
exit 0
