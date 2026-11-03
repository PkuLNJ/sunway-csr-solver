#!/bin/sh
# run path
casePath=`pwd`
if [ $# -gt 0 ]; then
	casePath=$1
fi	
cd $casePath || exit 1    # Run from this directory

# inti log and status
logFile=log
statusFile='status' #status:init 0, running 1, error 2, cancel 3, complete 4
if [ ! -e $logFile ]; then echo "Not running"; exit 1; fi
statusNo=`cat $statusFile`
if [ $statusNo != 1 ]; then echo "Not running"; exit 1; fi

#kill bjob
jobID=`awk -F"[<>]" '/^Job/{jobid=$2};END{print jobid}' $logFile`
echo "bkill $jobID"
bkill $jobID

#kill PID
PID=`awk -F"[<>]" '/PID/ {print $(NF-1)}' $logFile`
if [ `ps -ef | grep case_start.sh | grep $PID | wc -l` -gt 0 ];then
	echo "kill -9 $PID"
	kill -9 $PID
fi

#write log
echo "Canceled" >> $logFile
#update status
echo 3 > $statusFile
#------------------------------------------------------------------------------
