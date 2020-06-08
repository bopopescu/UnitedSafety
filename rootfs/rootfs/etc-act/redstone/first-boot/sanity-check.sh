#!/bin/sh

version=`cat /version | head -n 1 | awk '{print $0}'`
dt=`date '+%d/%m/%Y %H:%M:%S'`

prevDir=`ls -la /tmp/logprev | awk  'BEGIN {FS="."}{print $2}'`
thePrevDir=/mnt/nvram/log/logdir.$prevDir

if [ -e $thePrevDir ]; then
  factory="$thePrevDir/starting-factory-restore"
  if [ -e $factory ]; then
	send-email.sh "Trulink $version perform factory restore at $dt" /mnt/nvram/rom/sn.txt 
	exit 0;
  fi
  send-email.sh "Trulink $version update at $dt" /mnt/nvram/rom/sn.txt 
fi	
