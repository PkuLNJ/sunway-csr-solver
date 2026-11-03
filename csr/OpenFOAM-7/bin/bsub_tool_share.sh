#!/bin/bash

# check bin
#bin=$1
#if [ ! -b $bin ]; then
#	echo "Not exit: $bin"
#	exit 1
#fi

# run
#bsub -I -m 1 -b -p -q $QUEUE_NAME -host_stack 5000 -share_size 12000 -cgsp 64 -ro_size 128 $*
#bsub -I -m 1 -b -p -q $QUEUE_NAME  -cross -cross_size 30720 -swrunarg "-a 1 -o 2" -xmalloc -host_stack 1024  -share_size 4096 -cgsp 64 -ro_size 128 $*
#bsub -I -m 1 -b -p -q $QUEUE_NAME  -cross -cross_size 78848 -swrunarg "-a 1 -o 2" -xmalloc -host_stack 1024  -share_size 1536 -cgsp 64 -ro_size 128  -mpecg 6 $*
#bsub -I -m 1  -b -p -q $QUEUE_NAME  -cross -cross_size 84922 -swrunarg "-a 1 -o 2" -xmalloc -host_stack 512  -share_size 520 -cgsp 64 -ro_size 128  -mpecg 6 $*
bsub -I -m 1  -b -p -q $QUEUE_NAME  -cross -cross_size 89600 -swrunarg "-a 1 -o 2" -xmalloc -host_stack 128  -share_size 160 -cgsp 64 -ro_size 128 -priv_size 1  -mpecg 6 $*
