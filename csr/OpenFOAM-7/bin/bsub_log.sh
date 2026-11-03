#!/bin/sh

function log_analyze_async() {
	head_line="null"
	jobid=
	last_line=
	count=0
	while true
	do
		if [ -s $1 ]; then
			# head
			if [ "$head_line" == "null" ]; then
				head_line=`head -n 1 $1`
				jobid=`echo $head_line | awk -F"[<>]" '/^Job/{jobid=$2};END{print jobid}'`
				echo $head_line
				sleep 1
				continue
			fi

			# log analyze
			tail_line=`tail -n 1 $1`	
			case $tail_line in
				# run error
				*"SWCH Error"* | "["*"] \#"*"?"* | ": <swch"*)
					echo $tail_line
					# bkill 
					echo "bkill $jobid due to error"
					bkill $jobid
					exit 0
					;;
				# bsub queue
				"Job submit failed"* | "Job submit failed, Queue is closed"* | "Job submit failed, Access/permission denied"* | "job submit failed, ret = -19, reason: No enough compute nodes"*)
					exit 0
					;;
				# bsub end
				"Job"*"has been finished"*)
					exit 0
					;;
				# bsub exception
				*"bsub"*"已杀死"*)
					exit 0
					;;
				*)
					if [ "$last_line" == "$tail_line" ]; then
						#echo $count
						# check bjobs
						if [ $jobid ]; then
							bjobs_ret=`bjobs $jobid`
							if [ "$bjobs_ret" == "Invalid job id specified" ];then
								echo "job has been finished"
								exit 0
							fi
						fi
						let count++
						if [ $count -gt 20 ]; then exit 0;fi
						sleep 2	
					else
						#echo $count
						last_line=$tail_line
						count=0
					fi
					;;
			esac
		fi
		sleep 1
	done
}

function log_analyze_end() {
	tail_line=`tail -n 1 $1`	
	case $tail_line in
		# queue
		*"Job submit failed"*)
			exit 1
			;;
		# job error
		*"has been finished, exit reason: bkill"* | *"has been finished, exit reason: kill by signal"* | *"has been finished, exit reason: tasks exit with code: 1"*)
			tail -n 3 $1 | head -n 1
			tail -n 1 $1
			exit 1
			;;
		# bsub exception
		*"已杀死"*"bsub"* | *"bsub_tool.sh"*)
			tail -n 1 $1
			exit 1
			;;
		# normal end
		*)
			tail -n 3 $1 | head -n 1
			tail -n 1 $1
			;;
	esac
}
