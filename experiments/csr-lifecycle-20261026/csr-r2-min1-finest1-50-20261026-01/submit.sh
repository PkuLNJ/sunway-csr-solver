#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
source /home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/etc/bashrc >/dev/null
envrc=$?;set -euo pipefail;test "$envrc" = 0
w=$r/build/csr-r2-min1-finest1-50-20261026-01
d=$r/tests/csr-r2-min1-finest1-50-20261026-01
b=$r/build/amr-scalar-cache-aligned-20261024-r2
bin=$b/bin/boussinesqFoam-capture-wave-sqr-scalar-aligned
test -s "$d/INPUTS_READY";test ! -e "$d/processor0/0.001";test ! -e "$d/processor0/0.05"
test "$(sha256sum "$bin"|cut -d' ' -f1)" = 3c522ea5cfdb54d47b6bce2fa315fac38974e005a74839bf419314a335b1dd95
(cd "$d";sha256sum -c CONFIG.sha256)
cmp "$w/INPUT-controlDict50" "$d/system/controlDict"
qload q_swlidelei -w | sed -r 's/\x1B\[[0-9;]*[[:alpha:]]//g' | tr -d '\r' | awk '
 $1=="QUEUE_NAME"{for(i=1;i<=NF;i++){if($i=="IDLE")n=i;if($i=="IDLEMPE")m=i}next}
 $1=="q_swlidelei"&&n&&m{print "QUEUE=q_swlidelei IDLE_NODES="$n" IDLE_MPE="$m;found=1}
 END{if(!found)exit 3}' | tee "$d/RESOURCE_SUBMIT.txt"
detail=$(bqueues -l q_swlidelei);case "$detail" in *Open:Active*);;*)exit 4;;esac
python - "$r/tests/csr-r2-min1-finest1-step1-20261026-01" <<'PY'
import sys,re,json,os,math
p=sys.argv[1];term=open(p+'/TERMINAL.txt').read();assert 'SCHEDULER_STATE=DONE' in term and 'STOP_REQUESTED=0' in term
result=json.load(open(p+'/TIMING_RESULT.json'));assert not result['missing_final_fields']
s=open(p+'/solver.8753193.log').read().replace('\r','');assert re.search(r'^End\s*$',s,re.M)
rows=re.findall(r'GAMGPCG:  Solving for p_rgh, Initial residual = ([^,]+), Final residual = ([^,]+), No Iterations (\d+)',s);assert len(rows)==18
for a,b,n in rows:
 a=float(a);b=float(b);assert not(math.isnan(b) or math.isinf(b));assert b<=max(1e-7,.05*a)*1.000001
assert float(rows[-1][1])<=1e-7
assert not os.path.exists(p+'/ATTENTION_REQUIRED')
print('SINGLE_STEP_CONVERGENCE_AND_OUTPUT_GATE_PASSED; full field equivalence not asserted')
PY
cd "$d";mkdir SUBMIT_ONCE.guard
for variable in ${!LNJ_@};do unset "$variable";done
export OMP_NUM_THREADS=1 QUEUE_NAME=q_swlidelei FOAM_JOB_DIR=$d/jobControl
export LNJ_SGS_RETAIN_PSI=1 LNJ_SGS_BOUNDARY_MATERIALIZE=1 LNJ_SGS_BOUNDARY_RHS_RESET=1
# Exact previously verified r2 runtime modes; crucially no LNJ_AMR_CAPTURE_DIR.
test -z "${LNJ_AMR_CAPTURE_DIR:-}"
env | LC_ALL=C sort | grep -E '^(LNJ_[^=]*|OMP_NUM_THREADS|QUEUE_NAME|FOAM_JOB_DIR)=' > ENVIRONMENT.txt
printf 'BINARY=%s\nBINARY_SHA=3c522ea5cfdb54d47b6bce2fa315fac38974e005a74839bf419314a335b1dd95\nCASE=%s\nQUEUE=q_swlidelei\nMPI=960\nNODES=160\nSTEPS=50\nRUN_WALL_LIMIT_SECONDS=7200\n' "$bin" "$d" > REQUEST.txt
date +%s > SUBMIT_EPOCH
set +e
reply=$(bsub -K -m 1 -b -p -health 0 -q q_swlidelei -n 960 -np 6 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 02:00:00 -J lnj-csr-min1-finest1 -o "$d/solver.%J.log" "$bin" -case "$d" -parallel 2>&1)
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
