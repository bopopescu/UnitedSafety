#!/bin/bash
if [ "$#" -ne 2 ]; then
	echo -e " \033[32m"  # Green
	echo "Usage: patches [version] [svn_number]"
	echo -e "\033[39m"  #default
	echo "        This will create the patches required for offsets from factory builds."
	exit 1
fi

rev=$2

echo -e " \033[32m"  # Green
echo "Creating patches to $1-$rev"
echo -e "\033[39m"  #default

cd /home/dave/RedStone/Software/create-release

./create-patch.pl ../releases/3.0.0/12424 ../releases/$1/$rev upload=1

