#!/bin/bash

fname=$1

if [ '' = "$fname" ];then
	echo "Usage: $0 <path to source file>"
	exit 1
fi

if [ ! -f "$fname" ];then
	echo "ERROR: \"$fname\" is not a regular file"
	exit 1
fi

dir=`echo -n "$1"|sed 's/[^\/]\+$//'`

if [ -e "${dir}.svn" ];then
	version=`svn info "$dir"`
else
	version=`cd "$dir" && git-svn info`
fi

if [ 0 != "$?" ];then
	echo "ERROR: Could not get SVN version information"
	exit 1
fi

version=`echo "$version"|grep "Last Changed Rev:"|sed 's/.*: //'`

cat "$fname"|grep "\".*\"[^\"]\+\\\$\\\$\\\$REDSTONE_SVN_VERSION\\\$\\\$\\\$" > /dev/null

if [ 0 != "$?" ];then
	echo "ERROR: No REDSTONE_SVN_VERSION tag in \"$fname\""
	exit 1
fi


errval=0
cat "$fname"|sed "s/\(.*\)\".*\"\([^\"]\+\\\$\\\$\\\$REDSTONE_SVN_VERSION\\\$\\\$\\\$\)/\\1\"$version\"\\2/" > "/tmp/$$-version"

if [ 0 != "$?" ];then
	errval=1
	echo "ERROR: Failed while trying to replace REDSTONE_SVN_VERSION tag"

else

	if [ -x "$fname" ];then
		chmod 755 "/tmp/$$-version"
	fi

	mv "/tmp/$$-version" "$fname"

	if [ 0 != "$?" ];then
		errval=1
		echo "ERROR: Failed trying to update versioned file \"$fname\" from \"/tmp/$$-version\""
	else
		echo "File \"$fname\" marked with version $version"
	fi
fi

rm -f "/tmp/$$-version"

exit $errval;
