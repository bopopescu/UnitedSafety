#!/bin/sh

des='/dev/ttyAM0'
R='\x1b[2;41;30m'
I='\x1b[1;46;36m'
J='\x1b[1;7;46;36m'
F='\x1b[0m'

printf "\x1bc" > $des
printf "$F\n" > $des
printf "\n" > $des
printf "\n" > $des
i=0
printf "\x1b[s" > $des

while true
do
	printf "\x1b[u" > $des
	printf "$R                 REDSTONE USB TEST                   $F\n" > $des
	printf "$I=====================================================$F\n" > $des
	printf "$I===                  GOOD-BYE!!!                  ===$F\n" > $des
	printf "$I=====================================================$F\n" > $des
	printf "$R                 REDSTONE USB TEST                   $F\n" > $des
	
	if [ "$i" -gt 3 ];then
		break
	fi

	usleep 500000
	
	printf "\x1b[u" > $des
	printf "$R                 REDSTONE USB TEST                   $F\n" > $des
	printf "$J=====================================================$F\n" > $des
	printf "$J===                  GOOD-BYE!!!                  ===$F\n" > $des
	printf "$J=====================================================$F\n" > $des
	printf "$R                 REDSTONE USB TEST                   $F\n" > $des

	usleep 500000
	
	i=`expr $i + 1`
done

sleep 1
printf "\x1bc" > $des
exit 0
