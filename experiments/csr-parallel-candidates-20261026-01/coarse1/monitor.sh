#!/bin/bash
set -euo pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
w=$r/build/csr-parallel-coarse1-min0-50-20261026-01
d=$r/tests/csr-parallel-coarse1-min0-50-20261026-01
bin=$r/build/csr-lazy-u-combined-20261026-01/bin/boussinesqFoam-capture-wave-sqr-scalar-aligned
job=${1:?explicit own job id required}
[[ "$job" =~ ^[1-9][0-9]*$ ]];test "$(cat "$d/JOB_ID")" = "$job"
test "$(cat "$d/SUBMISSION_EXIT_CODE")" = 0
grep -Fxq "BINARY=$bin" "$d/REQUEST.txt";grep -Fxq "CASE=$d" "$d/REQUEST.txt"
cd "$d";mkdir MONITOR_ATTEMPT2_ONCE.guard
run_started=0;queue_end=$(($(date +%s)+86400));stopped=0
own_job(){
 timeout 10 bjobs -l "$job" > OWN_JOB_IDENTITY.txt 2>&1 || return 1
 grep -Fq "Job<$job>" OWN_JOB_IDENTITY.txt && grep -Fq "$bin" OWN_JOB_IDENTITY.txt && grep -Fq "$d" OWN_JOB_IDENTITY.txt
}
while test "$(date +%s)" -lt "$queue_end";do
 timeout 10 bjobs "$job" > STATUS_LATEST.txt 2>&1 || true
 state=$(awk -v id="$job" '$1==id{print $2}' STATUS_LATEST.txt)
 if test "$state" = RUN && test "$run_started" = 0;then run_started=$(date +%s);printf '%s\n' "$run_started" > FIRST_RUNNING_EPOCH;fi
 case "$state" in DONE|EXIT)
  cp -p STATUS_LATEST.txt TERMINAL_STATUS.txt
  timeout 10 bjobs -l "$job" > TERMINAL_DETAIL.txt 2>&1 || true
  printf 'JOB_ID=%s\nSCHEDULER_STATE=%s\nSTOP_REQUESTED=%s\n' "$job" "$state" "$stopped" > TERMINAL.txt
  test "$state" = DONE && test "$stopped" = 0 || exit 2
  landed=0
  for attempt in $(seq 1 20);do
   if grep -Eq '^End[[:space:]]*$' "solver.$job.log" && grep -q job_finished "solver.$job.log";then landed=1;break;fi
   sleep 3
  done
  test "$landed" = 1 || { echo DONE_LOG_NOT_YET_COMPLETE > ATTENTION_REQUIRED;exit 3; }
  python "$w/summarize.py" "$d" > TIMING_SUMMARY.txt 2>&1 || { echo SUMMARY_FAILED_REVIEW_EXISTING_LOG > ATTENTION_REQUIRED;exit 4; }
  echo FIFTY_STEP_RUN_AND_TIMING_COMPLETE;exit 0;;
 esac
 reason=
 if test -f "solver.$job.log" && python "$w/fatal_check.py" "solver.$job.log" > FATAL_MATCH.txt;then reason=FATAL_OR_NONFINITE_LOG;fi
 if test "$run_started" -gt 0 && test "$(date +%s)" -ge "$((run_started+7200))";then reason=OWN_RUN_WALL_LIMIT;fi
 if test -n "$reason" && test "$stopped" = 0;then
  printf '%s\n' "$reason" > ATTENTION_REQUIRED
  if own_job;then bkill "$job" > OWN_JOB_STOP.txt 2>&1;stopped=1;else echo IDENTITY_UNCONFIRMED_NO_KILL >> ATTENTION_REQUIRED;exit 5;fi
 fi
 sleep 30
done
echo QUEUE_OBSERVATION_LIMIT_NO_RESUBMISSION > ATTENTION_REQUIRED;exit 6
