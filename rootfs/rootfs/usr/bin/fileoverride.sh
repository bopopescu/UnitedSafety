#!/bin/sh
# copy file to destination, if file exists in folder, rename it to <file name>.bak
# usage: fileoverride.sh filenamewithpath destfile md5sum

tag='fileoverride'

if [ "$2" = "" ]; then
	logger -t "$tag" "destination not exists!!!"
	exit 1;
fi

if [ "$3" = "" ]; then
	logger -t "$tag" "md5sum not exists!!!"
	exit 1;
fi

if [ ! -f $1 ]; then
	logger -t "$tag" "File '"$1"' not found!!!";
	exit 1;
fi

md5File1=`md5sum $1 | awk 'BEGIN{}{print $1}'`;
if [ $md5File1 != $3 ]; then
	exit 2;
fi

dt=$2
if [ -f $dt ]; then

	md5File2=`md5sum $dt | awk 'BEGIN{}{print $1}'`;

	cp $dt $dt.bak

	if [ $md5File1 == $md5File2 ]; then
		exit 0;
	else
	   cp $dt $dt.bak
	fi
fi

logger -t "$tag" "Start coping file '"`basename $1`"'...";
cp $1 $dt
sync
exit 0
