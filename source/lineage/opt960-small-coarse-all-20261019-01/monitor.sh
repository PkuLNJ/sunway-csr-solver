#!/bin/bash
# Overall budget120s INCLUDING pending; stop only this verified exact job by~105s.
# Never submit/restart anything. Direct binary's programRC is NOT separately captured.
set -euo pipefail
command -v timeout >/dev/null
z=/home/export/online1/mdt00/shisuan/swpku_of/lnj/build/opt960-small-coarse-all-20261019-01
job=$(cat "$z/JOB_ID");[[ "$job" =~ ^[0-9]+$ ]]
[[ "${EXPECTED_JOB_ID:-}" =~ ^[0-9]+$ ]] || { echo MAIN_MUST_EXPLICITLY_SET_EXPECTED_JOB_ID;exit 88; }
test "$EXPECTED_JOB_ID" = "$job" || { echo EXPECTED_ID_MISMATCH_NO_ACTION;exit 88; }
test "$(tr -d '[:space:]' < "$z/SUBMIT_RC")" = 0
receipt_id=$(tr -d '\r' < "$z/SUBMISSION.txt" | sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p')
test "$receipt_id" = "$job" || { echo RECEIPT_ID_MISMATCH_NO_ACTION;exit 88; }
grep -Fxq "COMMAND=$z/bin/tinyAll12 -case $z -parallel" "$z/REQUEST.txt"
for line in USER=swpku_of JOB_NAME=lnj-smallcoarse12 QUEUE=q_swlidelei MPI=12 NP=1;do grep -Fxq "$line" "$z/REQUEST.txt";done
binary_sha=$(sed -n 's/^BINARY_SHA256=//p' "$z/REQUEST.txt")
sources_sha=$(sed -n 's/^SOURCES_SHA256=//p' "$z/REQUEST.txt")
[[ "$binary_sha" =~ ^[0-9a-f]{64}$ && "$sources_sha" =~ ^[0-9a-f]{64}$ ]]
test "$(sha256sum "$z/bin/tinyAll12" | cut -d' ' -f1)" = "$binary_sha"
test "$(sha256sum "$z/SOURCES.sha256" | cut -d' ' -f1)" = "$sources_sha"
(cd "$z";sha256sum -c SOURCES.sha256)
started=$(cat "$z/SUBMIT_WALL_EPOCH");[[ "$started" =~ ^[0-9]+$ ]]
end=$((started+120));stop_at=$((started+105))
test "$(date +%s)" -lt "$end" || { echo STALE_CHAIN_BUDGET_EXHAUSTED_MAIN_CHECK_THIS_ID_NO_ACTION;exit 89; }
mkdir "$z/observe-once.guard" "$z/capture"
out=$z/test.$job.out
stop_requested=0
while test "$(date +%s)" -lt "$end";do
 now=$(date +%s)
 remaining=$((end-now));probe=3;if test "$remaining" -lt 3;then probe=$remaining;fi
 timeout "$probe" bjobs -a -w "$job" > "$z/capture/latest-status.raw" 2>&1 || true
 tr -d '\r' < "$z/capture/latest-status.raw" > "$z/capture/latest-status.txt"
 state=$(awk -v id="$job" '$1==id{print $2}' "$z/capture/latest-status.txt")
 if test -n "$state";then
  awk -v id="$job" '$1==id{n++;if($3=="swpku_of" && $4=="lnj-smallcoarse12" && $5=="q_swlidelei")good++}END{exit !(n==1 && good==1)}' "$z/capture/latest-status.txt" \
    || { echo CURRENT_ROW_OWNERSHIP_MISMATCH_NO_KILL;exit 93; }
 fi
 if test -f "$out";then
  cp -p "$out" "$z/capture/stdout.raw"
  tr -d '\r' < "$out" > "$z/capture/stdout.normalized.log"
  grep -E 'SMALLCOARSE12_(BEFORE_ARGLIST|LOCAL_PHASE|LOCAL_MODE|FAIL|ALL_PASS|STRUCTURE_PASS|SOLVE_PASS)' "$z/capture/stdout.normalized.log" > "$z/capture/phases.txt" || true
 fi
 case "$state" in DONE|EXIT)
  cp -p "$z/capture/latest-status.txt" "$z/capture/TERMINAL_STATUS.txt"
  printf 'JOB_ID=%s\nSCHEDULER_STATE=%s\nPROGRAM_RC=NOT_SEPARATELY_CAPTURED\nSTOP_REQUESTED=%s\n' "$job" "$state" "$stop_requested" > "$z/capture/TERMINAL.txt"
  if test "$state" = DONE && test "$stop_requested" = 0;then
   log=$z/capture/stdout.normalized.log
   test -f "$log"
   for mode in none smallCoarseAll smallCoarseFallback;do
    grep -Fq "SMALLCOARSE12_STRUCTURE_PASS MODE=$mode" "$log"
    grep -Fq "SMALLCOARSE12_SOLVE_PASS MODE=$mode" "$log"
   done
   for mode in none smallCoarseAll smallCoarseFallback;do
    grep -Fq "SMALLCOARSE12_REAL_PRECON_PASS MODE=$mode" "$log"
   done
   grep -Eq 'LNJ_SMALL_COARSE_ADMISSION .*ACCEPT=1 REASON=admitted.*INNER_MAXITER=64' "$log"
   grep -Fq 'LNJ_SMALL_COARSE_GATHER CHILD_RANKS=1 PARENT_RANKS=12' "$log"
   grep -Eq 'LNJ_GLOBAL_COARSE OUTER_FIELD=tinyPressure .*COARSE_COMM_SIZE=1 REQUESTED_MAXITER=64 EFFECTIVE_MAXITER=64 COARSE_FALLBACK=0 .* MINITER=0 MAXITER=64' "$log"
   grep -Eq 'LNJ_SMALL_COARSE_ADMISSION .*BYTECAP=1 .*ACCEPT=0 REASON=bytecap_exceeded' "$log"
   grep -Eq 'LNJ_GLOBAL_COARSE OUTER_FIELD=tinyPressureFallback .*COARSE_COMM_SIZE=12 REQUESTED_MAXITER=64 EFFECTIVE_MAXITER=8 COARSE_FALLBACK=1 FALLBACK_REASON=smallCoarseAll_not_serial .* MINITER=0 MAXITER=8' "$log"
   grep -Fq SMALLCOARSE12_ALL_PASS "$log"
   grep -Eq '^End[[:space:]]*$' "$log"
   grep -q job_finished "$log"
   if grep -Eq 'FOAM FATAL|MPI_ABORT|SMALLCOARSE12_FAIL|Segmentation fault|Bus error|terminated by sig' "$log";then exit 92;fi
   echo SMALL_COARSE12_TARGET_INTEGRATION_MARKERS_PASS_NOT_CFD_OR_SPEEDUP;exit 0
  fi
  echo SMALL_COARSE12_TERMINAL_PARTIAL_OR_BOUNDED_STOP_INSPECT_PER_RANK_PHASES;exit 1;;
 esac
 if test "$now" -ge "$stop_at" && test "$stop_requested" = 0;then
  # Immutable request+receipt+main expectedID above; current supported -a -w row
  # must match exactID/user/full unique name/queue. No unsupported -l query.
  case "$state" in RUN|PEND)
   set +e
   timeout 3 bkill "$job" > "$z/capture/BKILL_THIS_JOB_ONLY.txt" 2>&1
   kill_rc=$?
   set -e
   if test "$kill_rc" != 0;then
    printf 'JOB_ID=%s\nSTOP_FAILED_OR_TIMEOUT=%s\nMAIN_MUST_CHECK_THIS_EXACT_JOB\nNO_RETRY=1\n' "$job" "$kill_rc" > "$z/capture/ATTENTION_REQUIRED"
    cat "$z/capture/ATTENTION_REQUIRED";exit 95
   fi
   stop_requested=1;;
  *) echo CANNOT_VERIFY_OWN_JOB_DO_NOT_KILL_OTHER_TASKS > "$z/capture/ATTENTION_REQUIRED";exit 93;;esac
 fi
 remaining=$((end-$(date +%s)))
 if test "$remaining" -gt 0;then nap=2;if test "$remaining" -lt 2;then nap=$remaining;fi;sleep "$nap";fi
done
printf 'JOB_ID=%s\nOBSERVATION_BUDGET_EXHAUSTED=120_SECONDS\nSTOP_REQUESTED=%s\nSTATE_UNCONFIRMED=1\nNO_RETRY=1\n' "$job" "$stop_requested" > "$z/capture/ATTENTION_REQUIRED"
cat "$z/capture/ATTENTION_REQUIRED"
exit 94
