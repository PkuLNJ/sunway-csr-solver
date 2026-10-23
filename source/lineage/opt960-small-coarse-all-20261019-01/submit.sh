#!/bin/bash
# Explicit one-shot12MPI phase probe only. -K is NOT a wait flag at this site.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail;test "$env_rc" = 0
z=$r/build/opt960-small-coarse-all-20261019-01
test -f "$z/BUILD_COMPLETE"
command -v timeout >/dev/null # monitor's scheduler probes must be bounded
test "$(tr -d '[:space:]' < "$z/COMPILE_EXIT_CODE")" = 0
test "$(tr -d '[:space:]' < "$z/LINK_EXIT_CODE")" = 0
(cd "$z";sha256sum -c SOURCES.sha256 && sha256sum -c BUILD_OUTPUTS.sha256)
q=$(bqueues -l q_swlidelei);case "$q" in *Open:Active*);;*)exit 90;;esac
for file in REQUEST.txt SUBMIT_WALL_EPOCH SUBMISSION.txt SUBMIT_RC JOB_ID;do test ! -e "$z/$file";done
mkdir "$z/submit-once.guard" "$z/jobControl"
printf '%s\n' "$q" > "$z/QUEUE_DETAILS.txt"
qload -w > "$z/RESOURCE_SNAPSHOT.txt"
export OMP_NUM_THREADS=1 FOAM_JOB_DIR=$z/jobControl QUEUE_NAME=q_swlidelei
unset LNJ_LEVEL_SGS_MODE LNJ_COLORED_SGS_MODE LNJ_TRUE_RESIDUAL LNJ_PROFILE_DIR LNJ_PROFILE_MODE LNJ_PRESSURE_RECYCLE LNJ_POSTSMOOTH_ADD_MODE LNJ_VCYCLE_RESET LNJ_SGS_NUMERIC_PACK
cd "$z"
binary_sha=$(sha256sum "$z/bin/tinyAll12" | cut -d' ' -f1)
sources_sha=$(sha256sum "$z/SOURCES.sha256" | cut -d' ' -f1)
(set -C;printf 'COMMAND=%s\nBINARY_SHA256=%s\nSOURCES_SHA256=%s\nUSER=swpku_of\nJOB_NAME=lnj-smallcoarse12\nQUEUE=q_swlidelei\nMPI=12\nNP=1\n' \
 "$z/bin/tinyAll12 -case $z -parallel" "$binary_sha" "$sources_sha" > "$z/REQUEST.txt")
(set -C;date +%s > "$z/SUBMIT_WALL_EPOCH")
set +e
reply=$(bsub -K -m 1 -b -p -health 0 -n 12 -np 1 -q q_swlidelei \
 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 00:02:00 \
 -J lnj-smallcoarse12 -o "$z/test.%J.out" "$z/bin/tinyAll12" -case "$z" -parallel 2>&1)
rc=$?;set -e
(set -C;printf '%s\n' "$reply" > "$z/SUBMISSION.txt")
(set -C;printf '%s\n' "$rc" > "$z/SUBMIT_RC")
printf '%s\n' "$reply"
test "$rc" = 0 || { echo NO_RETRY_SUBMISSION_FAILED;exit "$rc"; }
job=$(printf '%s\n' "$reply" | sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p')
[[ "$job" =~ ^[0-9]+$ ]] || { echo AMBIGUOUS_NO_RETRY_MAIN_MUST_INSPECT;exit 91; }
(set -C;printf '%s\n' "$job" > "$z/JOB_ID")
echo "START_MONITOR_IMMEDIATELY EXPECTED_JOB_ID=$job overall120seconds_from_submission_no_retry"
