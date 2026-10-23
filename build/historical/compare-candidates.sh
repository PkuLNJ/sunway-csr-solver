#!/bin/bash
set -o pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
set +e;source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null;env_rc=$?
set -euo pipefail;test "$env_rc" = 0
x=$r/build/csr-next-small-20261023
runtime=$x/runtime
bin=$r/build/opt960-compare-mpi-20261019-01/compare_mpi
test "$(sha256sum "$bin"|cut -d' ' -f1)" = 11f61297d873103e7f8bdfe30654817c0dd6a006328caa59106c16c538763f37
action=${1:?submit/poll}
label=${2:?comparison label};[[ "$label" =~ ^[a-z0-9-]{1,30}$ ]]
out=$runtime/compare-$label
if test "$action" = submit;then
 left=${3:?left};right=${4:?right};time=${5:?time};queue=${6:?queue}
 [[ "$left" =~ ^(base|alpha|div)[a-z0-9-]{0,25}$ ]];[[ "$right" =~ ^(base|alpha|div)[a-z0-9-]{0,25}$ ]]
 case "$time" in 0.001|0.01);;*)exit 79;;esac
 case "$queue" in q_swlidelei|q_share);;*)exit 79;;esac
 for run in "$left" "$right";do
  grep -Fxq SCHEDULER_STATE=DONE "$runtime/$run/TERMINAL.txt"
  grep -Fxq END_PRESENT=1 "$runtime/$run/TERMINAL.txt"
  test "$(sed -n 's/^END=//p' "$runtime/$run/REQUEST.txt")" = "$time"
 done
 clean=$(qload -w|sed -r 's/\x1B\[[0-9;]*[[:alpha:]]//g'|tr -d '\r')
 idle=$(printf '%s\n' "$clean"|awk -v q="$queue" '$1=="QUEUE_NAME"{for(i=1;i<=NF;i++)if($i=="IDLEMPE")c=i}$1==q&&c{print $c}')
 [[ "$idle" =~ ^[0-9]+$ ]];test "$idle" -ge 64
 mkdir "$out" "$out/jobControl"
 name=lnj-smcmp-$label
 export OMP_NUM_THREADS=1 FOAM_JOB_DIR=$out/jobControl QUEUE_NAME=$queue
 printf 'LEFT=%s\nRIGHT=%s\nTIME=%s\nQUEUE=%s\nJOB_NAME=%s\nRANKS=64\nCASE_RANKS=960\nFIELD_GATE_THRESHOLD=1e-12\n' "$left" "$right" "$time" "$queue" "$name" > "$out/REQUEST.txt"
 date +%s > "$out/SUBMIT_EPOCH"
 cd "$out"
 set +e
 reply=$(bsub -K -m 1 -b -p -health 0 -q "$queue" -n 64 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 00:15:00 -J "$name" -o "$out/compare.%J.out" "$bin" "$runtime/$left" "$runtime/$right" "$time" 960 2>&1)
 rc=$?;set -e;printf '%s\n' "$reply" > "$out/SUBMISSION.txt";printf '%s\n' "$rc" > "$out/SUBMIT_RC";test "$rc" = 0
 job=$(printf '%s\n' "$reply"|sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p');[[ "$job" =~ ^[1-9][0-9]*$ ]]
 printf '%s\n' "$job" > "$out/JOB_ID"
 printf 'COMPARE_SUBMITTED LABEL=%s JOB_ID=%s LEFT=%s RIGHT=%s\n' "$label" "$job" "$left" "$right"
 exit 0
fi
test "$action" = poll
job=$(cat "$out/JOB_ID");[[ "$job" =~ ^[1-9][0-9]*$ ]]
name=$(sed -n 's/^JOB_NAME=//p' "$out/REQUEST.txt");queue=$(sed -n 's/^QUEUE=//p' "$out/REQUEST.txt")
rows=$(bjobs -a -w "$job" 2>&1 || true);printf '%s\n' "$rows" > "$out/STATUS.txt"
printf '%s\n' "$rows"|awk -v id="$job" -v n="$name" -v q="$queue" '$1==id{count++;if($3=="swpku_of"&&$4==n&&$5==q)good++}END{exit !(count==1&&good==1)}'
state=$(printf '%s\n' "$rows"|awk -v id="$job" '$1==id{print $2}')
printf 'COMPARE_LABEL=%s JOB_ID=%s STATE=%s\n' "$label" "$job" "$state"
case "$state" in DONE|EXIT)
 bjobs -l "$job" > "$out/STATUS_LONG.txt" 2>&1 || true
 test "$state" = DONE
 tr -d '\r' < "$out/compare.$job.out" > "$out/compare.log"
 grep -Fq 'COMPARISON_COMPLETE (metrics only; no automatic scientific acceptance threshold)' "$out/compare.log"
 grep -q job_finished "$out/compare.log"
 test "$(grep -c '^FIELD=' "$out/compare.log")" = 14
 ! grep -Eq 'COMPARISON_ERROR|MPI_ABORT|Segmentation fault|Bus error|terminated by sig' "$out/compare.log"
 printf 'JOB_ID=%s\nSCHEDULER_STATE=%s\nCOMPARISON_COMPLETE=1\nNUMERIC_GATE=PENDING_LOCAL_1e-12\n' "$job" "$state" > "$out/TERMINAL.txt"
 cat "$out/compare.log";;
 RUN|PEND)
  if test "$state" = RUN && test ! -f "$out/FIRST_RUN_OBSERVED_EPOCH";then date +%s > "$out/FIRST_RUN_OBSERVED_EPOCH";fi
  age=0
  if test -f "$out/FIRST_RUN_OBSERVED_EPOCH";then age=$(( $(date +%s)-$(cat "$out/FIRST_RUN_OBSERVED_EPOCH") ));fi
  if test "$state" = RUN && test "$age" -ge 900;then timeout 5 bkill "$job" > "$out/STOP_EXACT_OWN_JOB.txt" 2>&1;exit 95;fi;;
 *)exit 80;;
esac
