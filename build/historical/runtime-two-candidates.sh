#!/bin/bash
# One owned existing short-test case, serial runs; no initial mesh/field clone.
set -o pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
set +e;source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null;env_rc=$?
set -euo pipefail;test "$env_rc" = 0
x=$r/build/csr-next-small-20261023
runtime=$x/runtime
d=$r/tests/opt960-rhs-writeset-repair-n80-step1-20261020-01/candidate
ref=$r/tests/doc42-pressure-csr-private-960-50-20261012-01/cpe
lock=$d/.csr-next-small-20261023.lock
test "$(readlink -f "$d")" = "$d"
test "$d" != "$r/tests/opt960-n80-lidelei10-20261020-01/candidate"
action=${1:?claim/submit/poll/collect/compare}
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
safe_run(){ [[ "$1" =~ ^(base|alpha|div)[a-z0-9-]{0,25}$ ]]; }
case "$action" in
 claim)
  test "$(cat "$d/JOB_ID")" = 8671829
  grep -Fxq SCHEDULER_STATE=DONE "$d/capture/TERMINAL.txt"
  test ! -e "$d/.pool";test ! -e "$d/.csr-next-small-20261023.lock"
  rows=$(bjobs -a -w 8671829 2>/dev/null || true)
  printf '%s\n' "$rows"|awk '$1==8671829&&($2=="RUN"||$2=="PEND"){bad=1}END{exit bad}'
  check "$d/system/fvSolution" 7b08cb2403c2ac61cd047d49b1ec37c9f950fcc28fd32d526f01b4739c21b9a6
  check "$d/system/controlDict" 46db6420d9ee8e5e604ccf1357b04169ac0c00a71034eabc4cd60e2f6b89d706
  mkdir "$lock";printf 'csr_acceleration_review\n' > "$lock/owner"
  mkdir "$runtime" "$runtime/original-controls" "$runtime/old-output-8671829"
  cp -p "$d/system/controlDict" "$runtime/original-controls/controlDict"
  cp -p "$d/system/fvSolution" "$runtime/original-controls/fvSolution"
  cp -p "$d/system/fvSchemes" "$runtime/original-controls/fvSchemes"
  printf 'OWNED_IDLE\n' > "$lock/phase"
  # Preserve all old numeric outputs by rename. Source initial/constant untouched.
  for ((rank=0;rank<960;rank++));do
   src=$d/processor$rank/0.001
   if test -d "$src";then mkdir "$runtime/old-output-8671829/processor$rank";mv -- "$src" "$runtime/old-output-8671829/processor$rank/0.001";fi
  done
  printf 'CLAIMED_CASE=%s\nOWNER=csr_acceleration_review\nOLD_OUTPUT_RECOVERABLE=%s\n' "$d" "$runtime/old-output-8671829"
  exit 0;;
esac
test "$(cat "$lock/owner")" = csr_acceleration_review
case "$action" in
 acknowledge_failed_base)
  test "$(cat "$lock/run")" = base1b;test "$(cat "$lock/job-id")" = 8723338
  out=$runtime/base1b
  rows=$(bjobs -a -w 8723338)
  printf '%s\n' "$rows"|awk '$1==8723338{n++;if($2=="EXIT"&&$3=="swpku_of"&&$4=="lnj-small-base1b"&&$5=="q_share")g++}END{exit !(n==1&&g==1)}'
  bjobs -l 8723338 > "$out/STATUS_LONG.txt" 2>&1 || true
  LC_ALL=C grep -aFq 'Error: unknown CPE exception' "$out/stdout.8723338.out"
  ! LC_ALL=C grep -aEq 'OpenFOAM:|^Time = ' "$out/stdout.8723338.out"
  printf 'JOB_ID=8723338\nSCHEDULER_STATE=EXIT\nBEFORE_OPENFOAM_BANNER=1\nNUMERIC_BASELINE_INVALID=1\nSTOP_FAILED_OWN_RUN=1\n' > "$out/TERMINAL.txt"
  printf 'OWNED_IDLE\n' > "$lock/phase";printf 'FAILED_BASELINE_TERMINAL_CONFIRMED INPUTS_NOT_RESET\n';exit 0;;
 recover_failed)
  run=${2:?run};safe_run "$run";out=$runtime/$run
  test "$(cat "$lock/phase")" = SUBMIT_FAILED
  test "$(cat "$lock/run")" = "$run";test ! -e "$out/JOB_ID"
  test "$(cat "$out/SUBMIT_RC")" = 255
  grep -Fq "can't submit job on fs base, job not submitted" "$out/SUBMISSION.txt"
  printf 'OWNED_IDLE\n' > "$lock/phase"
  printf 'SUBMIT_FAILURE_CONFIRMED_NO_JOB RUN=%s\n' "$run";exit 0;;
 submit)
  run=${2:?run};arm=${3:?base/alpha/div};steps=${4:?1/10};queue=${5:?queue};reuse=${6:-normal};safe_run "$run"
  case "$arm" in base)bin=$r/build/opt960-rhs-writeset-repair-n80-20261020-01/bin/boussinesqFoam-rhs-writeset-n80;expected=be97dddf6c282ee839ed2dfa5948c7acc59c39d075de799c6dd2ef4df6aa4c82;;alpha|div)bin=$x/bin/boussinesqFoam-$arm;expected=$(awk -v p="$bin" '$2==p{print $1}' "$x/BUILD_OUTPUTS.sha256");;*)exit 78;;esac
  case "$steps" in 1)end=0.001;;10)end=0.01;;*)exit 78;;esac
  case "$queue" in q_swlidelei|q_share);;*)exit 78;;esac
  test "$(cat "$lock/phase")" = OWNED_IDLE
  test ! -e "$runtime/$run";mkdir "$runtime/$run" "$runtime/$run/jobControl"
  out=$runtime/$run
  printf 'PREPARING\n' > "$lock/phase";printf '%s\n' "$run" > "$lock/run"
  check "$bin" "$expected"
  check "$d/system/fvSolution" 7b08cb2403c2ac61cd047d49b1ec37c9f950fcc28fd32d526f01b4739c21b9a6
  cmp "$runtime/original-controls/fvSchemes" "$d/system/fvSchemes"
  if test "$reuse" = retry-before-main;then
   test "$arm" = base;test "$steps" = 1
   grep -Fxq SCHEDULER_STATE=EXIT "$runtime/base1b/TERMINAL.txt"
   grep -Fxq BEFORE_OPENFOAM_BANNER=1 "$runtime/base1b/TERMINAL.txt"
   cmp "$runtime/base1b/controlDict" "$d/system/controlDict"
  else
   test "$reuse" = normal
   # Only the two fields known to be written during createFields are refreshed.
   # Four disjoint rank ranges reuse the old prep contract, no case clone.
   reset_range(){
   local rank field
   for ((rank=$1;rank<=$2;rank++));do
   test -d "$d/processor$rank/0";test -d "$d/processor$rank/constant"
   test ! -e "$d/processor$rank/0.001";test ! -e "$d/processor$rank/0.01"
   for field in TT S;do
    test ! -L "$d/processor$rank/0/$field"
    if ! cmp -s "$ref/processor$rank/0/$field" "$d/processor$rank/0/$field";then
     cp -aL --reflink=auto --remove-destination "$ref/processor$rank/0/$field" "$d/processor$rank/0/$field"
    fi
   done
   done
   }
   reset_range 0 239 & p0=$!
   reset_range 240 479 & p1=$!
   reset_range 480 719 & p2=$!
   reset_range 720 959 & p3=$!
   rc=0;for pid in "$p0" "$p1" "$p2" "$p3";do wait "$pid" || rc=1;done;test "$rc" = 0
  fi
  sed -E "s/^endTime[[:space:]]+[^;]+;/endTime         $end;/;s/^writeInterval[[:space:]]+[^;]+;/writeInterval   $end;/;s/([[:space:]]+writeInterval[[:space:]]+)[^;]+;/\1$end;/" "$runtime/original-controls/controlDict" > "$d/system/controlDict"
  cp -p "$d/system/controlDict" "$out/controlDict"
  cp -p "$d/system/fvSolution" "$out/fvSolution"
  clean=$(qload -w|sed -r 's/\x1B\[[0-9;]*[[:alpha:]]//g'|tr -d '\r')
  idle=$(printf '%s\n' "$clean"|awk -v q="$queue" '$1=="QUEUE_NAME"{for(i=1;i<=NF;i++)if($i=="IDLEMPE")c=i}$1==q&&c{print $c;n++}END{if(n!=1)exit 1}')
  [[ "$idle" =~ ^[0-9]+$ ]]
  if test "$queue" = q_share;then test "$idle" -ge 960;fi
  detail=$(bqueues -l "$queue");case "$detail" in *Open:Active*);;*)exit 79;;esac
  name=lnj-small-$run
  for variable in ${!LNJ_@};do unset "$variable";done
  export OMP_NUM_THREADS=1 FOAM_JOB_DIR=$out/jobControl QUEUE_NAME=$queue
  export LNJ_LEVEL_SGS_MODE=tilepermute LNJ_SGS_TOPOLOGY_CACHE=1 LNJ_SGS_NUMERIC_PACK=cpe LNJ_SGS_TOPOLOGY_MATCH=cpe
  export LNJ_COARSE_CORRECTION=original LNJ_PCG_TRUE_RESIDUAL=1 LNJ_SGS_RETAIN_PSI=1 LNJ_SGS_BOUNDARY_MATERIALIZE=1 LNJ_SGS_BOUNDARY_RHS_RESET=1
  printf 'RUN=%s\nARM=%s\nBINARY=%s\nBINARY_SHA=%s\nCASE=%s\nSTEPS=%s\nEND=%s\nQUEUE=%s\nJOB_NAME=%s\nRANKS=960\nNP=6\nPRE_SUBMIT_IDLE=%s\nOWNER=csr_acceleration_review\n' "$run" "$arm" "$bin" "$expected" "$d" "$steps" "$end" "$queue" "$name" "$idle" > "$out/REQUEST.txt"
  sha256sum "$d/system/controlDict" "$d/system/fvSolution" "$d/system/fvSchemes" > "$out/INPUTS.sha256"
  date +%s > "$out/SUBMIT_EPOCH"
  printf 'SUBMITTING\n' > "$lock/phase";printf '%s\n' "$run" > "$lock/run"
  cd "$d"
  set +e
  reply=$(bsub -K -m 1 -b -p -health 0 -q "$queue" -n 960 -np 6 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 00:30:00 -J "$name" -o "$out/stdout.%J.out" "$bin" -case "$d" -parallel 2>&1)
  rc=$?;set -e;printf '%s\n' "$reply" > "$out/SUBMISSION.txt";printf '%s\n' "$rc" > "$out/SUBMIT_RC"
  test "$rc" = 0 || { printf 'SUBMIT_FAILED\n' > "$lock/phase";printf '%s\n' "$reply";exit "$rc"; }
  job=$(printf '%s\n' "$reply"|sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p');[[ "$job" =~ ^[1-9][0-9]*$ ]]
  printf '%s\n' "$job" > "$out/JOB_ID";printf '%s\n' "$job" > "$lock/job-id";printf 'RUNNING\n' > "$lock/phase"
  printf 'SUBMITTED RUN=%s JOB_ID=%s QUEUE=%s RANKS=960\n' "$run" "$job" "$queue"
  exit 0;;
 poll|collect)
  run=${2:?run};safe_run "$run";out=$runtime/$run
  job=$(cat "$out/JOB_ID");[[ "$job" =~ ^[1-9][0-9]*$ ]]
  name=$(sed -n 's/^JOB_NAME=//p' "$out/REQUEST.txt");queue=$(sed -n 's/^QUEUE=//p' "$out/REQUEST.txt")
  rows=$(bjobs -a -w "$job" 2>&1 || true)
  printf '%s\n' "$rows" > "$out/STATUS.txt"
  state=$(printf '%s\n' "$rows"|awk -v id="$job" '$1==id{print $2}')
  printf '%s\n' "$rows"|awk -v id="$job" -v n="$name" -v q="$queue" '$1==id{count++;if($3=="swpku_of"&&$4==n&&$5==q)good++}END{exit !(count==1&&good==1)}'
  printf 'RUN=%s JOB_ID=%s STATE=%s\n' "$run" "$job" "$state"
  if test -f "$out/stdout.$job.out";then tail -12 "$out/stdout.$job.out";fi
  if test "$state" = RUN && test ! -f "$out/FIRST_RUN_OBSERVED_EPOCH";then date +%s > "$out/FIRST_RUN_OBSERVED_EPOCH";fi
  age=0
  if test -f "$out/FIRST_RUN_OBSERVED_EPOCH";then age=$(( $(date +%s)-$(cat "$out/FIRST_RUN_OBSERVED_EPOCH") ));fi
  if test "$age" -ge 1800;then
   case "$state" in RUN)
    test "$(cat "$lock/run")" = "$run";test "$(cat "$lock/job-id")" = "$job"
    timeout 5 bkill "$job" > "$out/STOP_EXACT_OWN_JOB.txt" 2>&1
    printf 'RUNNING_WALL_FENCE_STOP_REQUESTED RUN=%s JOB_ID=%s RUN_OBSERVED_AGE=%s\n' "$run" "$job" "$age"
    exit 95;;esac
  fi
  if test "$action" = poll;then exit 0;fi
  test "$(cat "$lock/run")" = "$run";test "$(cat "$lock/job-id")" = "$job"
  case "$state" in DONE|EXIT);;*)exit 77;;esac
  bjobs -l "$job" > "$out/STATUS_LONG.txt" 2>&1 || true
  test "$state" = DONE
  log=$out/stdout.$job.out
  tr -d '\r' < "$log" > "$out/runtime.log"
  grep -Eq '^End[[:space:]]*$' "$out/runtime.log";grep -q job_finished "$out/runtime.log"
  ! grep -Eq 'FOAM FATAL|MPI_ABORT|Segmentation fault|Bus error|terminated by sig' "$out/runtime.log"
  steps=$(sed -n 's/^STEPS=//p' "$out/REQUEST.txt");end=$(sed -n 's/^END=//p' "$out/REQUEST.txt")
  test "$(grep -c '^Time = ' "$out/runtime.log")" = "$steps"
  test "$(grep -c '^LNJ_PCG_TRUE_RESIDUAL .*field=p_rgh .*true_pass=1' "$out/runtime.log")" = "$((18*steps))"
  test "$(grep '^LNJ_NATIVE_COARSE_TRUE ' "$out/runtime.log"|grep -c 'SOLVER=DICPCG .*FINITE=1 .*PASS=1 ')" = 31
  for ((rank=0;rank<960;rank++));do
   for field in U p_rgh TT S alpha.water p rhok;do test -s "$d/processor$rank/$end/$field";done
  done
  # Archive this run's outputs by rename; same input slot is now reusable.
  for ((rank=0;rank<960;rank++));do
   mkdir "$out/processor$rank"
   for folder in "$d/processor$rank"/*;do
    basename=${folder##*/}
    if [[ "$basename" =~ ^[0-9]+([.][0-9]+)?$ ]] && test "$basename" != 0;then mv -- "$folder" "$out/processor$rank/";fi
   done
  done
  printf 'JOB_ID=%s\nSCHEDULER_STATE=%s\nEND_PRESENT=1\nJOB_FINISHED_PRESENT=1\nPROGRAM_RC=SEE_STATUS_LONG_NOT_ASSUMED\n' "$job" "$state" > "$out/TERMINAL.txt"
  printf 'OWNED_IDLE\n' > "$lock/phase"
  printf 'COLLECTED RUN=%s JOB_ID=%s OUTPUT=%s\n' "$run" "$job" "$out"
  exit 0;;
 *)exit 78;;
esac
