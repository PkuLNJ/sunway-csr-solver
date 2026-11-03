#!/bin/bash

# check bin
#bin=$1
#if [ ! -b $bin ]; then
#	echo "Not exit: $bin"
#	exit 1
#fi

# run
#bsub -I -m 1 -b -p -q $QUEUE_NAME -host_stack 5000 -share_size 12000 -cgsp 64 -ro_size 128 $*
bsub -I -m 1 -b -p -q $QUEUE_NAME -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 $*
#bsub -I -m 1 -b -p -q $QUEUE_NAME  -cross -cross_size 30720 -swrunarg "-a 1 -o 2" -xmalloc -host_stack 1024  -share_size 4096 -cgsp 64 -ro_size 128 $*
