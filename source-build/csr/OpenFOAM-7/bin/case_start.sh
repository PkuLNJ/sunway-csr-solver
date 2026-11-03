#!/bin/sh
# run path
casePath=`pwd`
if [ $# -gt 0 ]; then
	casePath=$1
fi	
cd $casePath || exit 1    # Run from this directory

# init log and status
logFile=log
statusFile='status' #status:init 0, running 1, error 2, cancel 3, complete 4
echo "`pwd`" > $logFile
echo "PID <$$>" >> $logFile
echo 0 > $statusFile

# check system
if [ ! -d 'system' ]; then
	echo "Error: no system file!" >> $logFile
	echo 2 > $statusFile
	exit 1
fi
# modify writeFormat
sed -i 's/writeFormat     binary;/writeFormat     ascii;/' system/controlDict

# make Allrun
pvbatch $WM_PROJECT_DIR/paraview/Create_SH.py $casePath >> $logFile
if [ ! -f 'Allrun' ]; then
	echo "Error: no Allrun file!" >> $logFile
	echo 2 > $statusFile
	exit 1
fi

# Allrun
rm -rf log.* 
echo 1 > $statusFile
# modify Allmesh
if [ -f 'Allmesh' ]; then
	sed -i 's/^cd \${/#cd \${/' ./Allmesh
fi
# modify Allrun
sed -i 's/^cd \${/#cd \${/' ./Allrun
sed -i '/-force/!s/runApplication decomposePar/runApplication decomposePar -force/' ./Allrun
sh ./Allrun >> $logFile
#check allrun ret
if [ $? -ne 0 ];then
	echo "Error: Allrun error!" >> $logFile
	echo 2 > $statusFile
	exit 1
fi
echo "Calculate completed." >> $logFile

# save pic
pvbatch $WM_PROJECT_DIR/paraview/Save_Image.py $casePath >> $logFile
echo "Save pictures completed." >> $logFile
echo 4 > $statusFile

#------------------------------------------------------------------------------
