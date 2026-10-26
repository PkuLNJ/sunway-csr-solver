#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
source /home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/etc/bashrc >/dev/null
envrc=$?;set -euo pipefail;test "$envrc" = 0
w=$r/build/csr-parallel-coarse1-min0-50-20261026-01
d=$r/tests/csr-parallel-coarse1-min0-50-20261026-01
b=$r/build/csr-lazy-u-combined-20261026-01
bin=$b/bin/boussinesqFoam-capture-wave-sqr-scalar-aligned
test -s "$d/INPUTS_READY";test ! -e "$d/processor0/0.001";test ! -e "$d/processor0/0.05"
test -s "$b/BUILD_COMPLETE"
expected=$(awk '/bin\//{print $1;exit}' "$b/OUTPUTS.sha256")
test "$(sha256sum "$bin"|cut -d' ' -f1)" = "$expected"
(cd "$d";sha256sum -c CONFIG.sha256)
cmp "$w/INPUT-controlDict50" "$d/system/controlDict"
qload q_swlidelei -w > "$d/RESOURCE_SUBMIT.txt" || true
grep -q q_swlidelei "$d/RESOURCE_SUBMIT.txt"
detail=$(bqueues -l q_swlidelei);case "$detail" in *Open:Active*);;*)exit 4;;esac
cd "$d";mkdir SUBMIT_ONCE.guard
for variable in ${!LNJ_@};do unset "$variable";done
export OMP_NUM_THREADS=1 QUEUE_NAME=q_swlidelei FOAM_JOB_DIR=$d/jobControl
export LNJ_SGS_RETAIN_PSI=1 LNJ_SGS_BOUNDARY_MATERIALIZE=1 LNJ_SGS_BOUNDARY_RHS_RESET=1
export LNJ_U_RECONSTRUCT_CACHE=1 LNJ_LAZY_GAMG=1 
# Exact previously verified r2 runtime modes; crucially no LNJ_AMR_CAPTURE_DIR.
test -z "${LNJ_AMR_CAPTURE_DIR:-}"
env | LC_ALL=C sort | grep -E '^(LNJ_[^=]*|OMP_NUM_THREADS|QUEUE_NAME|FOAM_JOB_DIR)=' > ENVIRONMENT.txt
printf 'BINARY=%s\nBINARY_SHA=%s\nCASE=%s\nQUEUE=q_swlidelei\nMPI=960\nNODES=160\nSTEPS=50\nRUN_WALL_LIMIT_SECONDS=7200\n' "$bin" "$expected" "$d" > REQUEST.txt
date +%s > SUBMIT_EPOCH
set +e
reply=$(bsub -K -m 1 -b -p -health 0 -q q_swlidelei -n 960 -np 6 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 02:00:00 -J lnj-50-coarse1 -o "$d/solver.%J.log" "$bin" -case "$d" -parallel 2>&1)
submitrc=$?
set -e
printf '%s\n' "$reply" | tee SUBMISSION.txt
printf '%s\n' "$submitrc" > SUBMISSION_EXIT_CODE
test "$submitrc" = 0
job=$(printf '%s\n' "$reply"|sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p')
[[ "$job" =~ ^[1-9][0-9]*$ ]]
printf '%s\n' "$job" > JOB_ID
bjobs "$job" 2>&1 || true
echo CSR_R2_50_STEP_SUBMITTED=$job
