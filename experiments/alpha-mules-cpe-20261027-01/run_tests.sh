#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
source /home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/etc/bashrc >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/alpha-mules-cpe-20261027-01
cd "$w";mkdir RUN_ONCE.guard
trap 'rc=$?;echo "$rc" > "$w/RUN_EXIT_CODE"' EXIT
run_arm(){
 local arm=$1 a d job
 a=$r/build/alpha-mules-cpe-20261027-01-$arm
 d=$r/tests/alpha-mules-cpe-20261027-01-$arm
 bash "$a/prepare.sh"
 bash "$a/submit.sh"
 job=$(cat "$d/JOB_ID");echo "$arm $job $d" >> "$w/JOBS.txt"
 bash "$a/monitor.sh" "$job"
 python "$w/validate_dispatch.py" "$d" "$arm" > "$d/DISPATCH_VALIDATED.txt"
}
compare(){
 local label=$1 left=$2 right=$3 at=$4 out reply job state
 out=$w/compare-$label;mkdir "$out" "$out/jobControl"
 export FOAM_JOB_DIR=$out/jobControl OMP_NUM_THREADS=1
 reply=$(bsub -K -m 1 -b -p -health 0 -q q_swlidelei -n 32 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 00:20:00 -J lnj-sgs-cmp-$label -o "$out/compare.%J.log" "$r/build/opt960-compare-mpi-20261019-01/compare_mpi" "$left" "$right" "$at" 960 2>&1)
 printf '%s\n' "$reply" > "$out/SUBMISSION.txt"
 job=$(printf '%s\n' "$reply"|sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p');[[ "$job" =~ ^[1-9][0-9]*$ ]];echo "$job" > "$out/JOB_ID"
 echo "compare-$label $job $out" >> "$w/JOBS.txt"
 for try in $(seq 1 720);do
  bjobs "$job" > "$out/STATUS.txt" 2>&1 || true
  state=$(awk -v id="$job" '$1==id{print $2}' "$out/STATUS.txt")
  if test "$state" = EXIT;then cp "$out/STATUS.txt" "$out/TERMINAL.txt";return 4;fi
  if test "$state" = DONE;then
   cp "$out/STATUS.txt" "$out/TERMINAL.txt"
   for flush in $(seq 1 12);do grep -q job_finished "$out/compare.$job.log" && break;sleep 5;done
   python "$w/check_compare.py" "$out/compare.$job.log" > "$out/NUMERIC_GATE.json"
   return $?
  fi
  sleep 20
 done
 echo COMPARISON_WAIT_LIMIT_NO_RETRY;return 5
}
test "$(cat "$w/BUILD_EXIT_CODE")" = 0
test -s "$w/BUILD_COMPLETE"
run_arm off-step1
run_arm on-step1
compare step1 "$r/tests/alpha-mules-cpe-20261027-01-off-step1" "$r/tests/alpha-mules-cpe-20261027-01-on-step1" 0.001
echo STRICT_STEP1_GATE_PASSED > "$w/STEP1_PASS"
run_arm on-50
compare final "$r/tests/csr-b1-sgs-combined-20261026-01" "$r/tests/alpha-mules-cpe-20261027-01-on-50" 0.05
echo FIFTY_STEPS_AND_STRICT_FINAL_PASS > "$w/COMPLETE"
