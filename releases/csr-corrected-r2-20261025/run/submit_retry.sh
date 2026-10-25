#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
source /home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/etc/bashrc >/dev/null
envrc=$?;set -euo pipefail;test "$envrc" = 0
w=$r/build/csr-r2-960-50-20261025-01
d=$r/tests/csr-r2-960-50-20261025-01
bin=$r/build/amr-scalar-cache-aligned-20261024-r2/bin/boussinesqFoam-capture-wave-sqr-scalar-aligned
test "$(cat "$d/JOB_ID")" = 8749862
oldstate=$(bjobs 8749862 2>&1);printf '%s\n' "$oldstate"
printf '%s\n' "$oldstate" | awk '$1==8749862&&$2=="EXIT"{ok=1}END{exit !ok}'
test ! -e "$d/processor0/0.001";test ! -e "$d/processor0/0.05"
if grep -Eq '^Time[[:space:]]*=' "$d/solver.8749862.log";then echo STARTED_TIMESTEP_REQUIRES_MANUAL_INPUT_REVIEW;exit 88;fi
test "$(sha256sum "$bin"|cut -d' ' -f1)" = 3c522ea5cfdb54d47b6bce2fa315fac38974e005a74839bf419314a335b1dd95
(cd "$d";sha256sum -c CONFIG.sha256)
(cd "$w";sha256sum -c RETRY_STAGED.sha256)
qload q_swlidelei -w | sed -r 's/\x1B\[[0-9;]*[[:alpha:]]//g' | tr -d '\r' | awk '
 $1=="QUEUE_NAME"{for(i=1;i<=NF;i++){if($i=="IDLE")n=i;if($i=="IDLEMPE")m=i}next}
 $1=="q_swlidelei"&&n&&m{print "QUEUE=q_swlidelei IDLE_NODES="$n" IDLE_MPE="$m;found=1}
 END{if(!found)exit 3}' > "$w/RESOURCE_RETRY.txt"
detail=$(bqueues -l q_swlidelei);case "$detail" in *Open:Active*);;*)exit 4;;esac
cd "$d";mkdir MANUAL_RETRY_ONCE.guard;mkdir -p attempts;mkdir attempts/8749862
for name in JOB_ID REQUEST.txt SUBMISSION.txt SUBMISSION_EXIT_CODE SUBMIT_EPOCH ENVIRONMENT.txt RESOURCE_SUBMIT.txt STATUS_LATEST.txt TERMINAL_STATUS.txt TERMINAL_DETAIL.txt TERMINAL.txt MONITOR.log MONITOR_PID FIRST_RUNNING_EPOCH ATTENTION_REQUIRED OWN_JOB_STOP.txt OWN_JOB_IDENTITY.txt;do
 if test -f "$name";then mv -- "$name" "attempts/8749862/$name";fi
done
cp -p solver.8749862.log attempts/8749862/
printf 'MANUAL_RETRY_AFTER_MONITOR_FALSE_POSITIVE_NORMAL_SIGFPE_BANNER\nSOLVER_AND_NUMERICAL_CONFIG_UNCHANGED\nNO_TIMESTEP_BEGAN_IN_PREVIOUS_JOB\n' > attempts/8749862/CAUSE.txt
for variable in ${!LNJ_@};do unset "$variable";done
export OMP_NUM_THREADS=1 QUEUE_NAME=q_swlidelei FOAM_JOB_DIR=$d/jobControl
export LNJ_SGS_RETAIN_PSI=1 LNJ_SGS_BOUNDARY_MATERIALIZE=1 LNJ_SGS_BOUNDARY_RHS_RESET=1
test -z "${LNJ_AMR_CAPTURE_DIR:-}"
env | LC_ALL=C sort | grep -E '^(LNJ_[^=]*|OMP_NUM_THREADS|QUEUE_NAME|FOAM_JOB_DIR)=' > ENVIRONMENT.txt
printf 'BINARY=%s\nBINARY_SHA=3c522ea5cfdb54d47b6bce2fa315fac38974e005a74839bf419314a335b1dd95\nCASE=%s\nQUEUE=q_swlidelei\nMPI=960\nNODES=160\nSTEPS=50\nRUN_WALL_LIMIT_SECONDS=7200\nMANUAL_ATTEMPT=2\n' "$bin" "$d" > REQUEST.txt
date +%s > SUBMIT_EPOCH
set +e
reply=$(bsub -K -m 1 -b -p -health 0 -q q_swlidelei -n 960 -np 6 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 02:00:00 -J lnj-csr-r2-50r2-1025 -o "$d/solver.%J.log" "$bin" -case "$d" -parallel 2>&1)
submitrc=$?
set -e
printf '%s\n' "$reply" | tee SUBMISSION.txt;printf '%s\n' "$submitrc" > SUBMISSION_EXIT_CODE;test "$submitrc" = 0
job=$(printf '%s\n' "$reply"|sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p');[[ "$job" =~ ^[1-9][0-9]*$ ]]
printf '%s\n' "$job" > JOB_ID
bjobs "$job" 2>&1 || true
echo CSR_R2_50_MANUAL_ATTEMPT2_SUBMITTED=$job
