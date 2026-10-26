#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
source /home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/etc/bashrc >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/csr-lazy-up-geometry-final-audit-20261026-01
mkdir "$w";cd "$w";mkdir jobControl
left=$r/tests/csr-r2-miniter0-printguard-20261026-01
right=$r/tests/csr-lazy-up-geometry-min0-50-20261026-01
ready=0
for try in $(seq 1 540);do
 if test -f "$left/TERMINAL.txt" && test -f "$right/TERMINAL.txt";then ready=1;break;fi
 sleep 20
done
test "$ready" = 1 || { echo WAIT_LIMIT_NO_SUBMISSION;exit 3; }
for d in "$left" "$right";do grep -q '^SCHEDULER_STATE=DONE' "$d/TERMINAL.txt";grep -q '^STOP_REQUESTED=0' "$d/TERMINAL.txt";done
python - "$left" "$right" <<'PY'
import sys,re,json
traces=[];timings=[]
for d in sys.argv[1:]:
 j=open(d+'/JOB_ID').read().strip();t=open(d+'/solver.'+j+'.log').read().replace('\r','')
 assert re.search(r'^End\s*$',t,re.M) and 'job_finished' in t
 loops=[float(x) for x in re.findall(r'^Loop Time\s*=\s*(\S+)',t,re.M)]
 clocks=[float(x) for x in re.findall(r'ClockTime = (\S+) s',t)]
 times=[float(x) for x in re.findall(r'^Time = (\S+)',t,re.M)]
 assert len(loops)==50 and abs(times[-1]-.05)<1e-12
 timings.append({'job':j,'loop_2_49_mean':sum(loops[1:49])/48,'clock_2_49':clocks[48]-clocks[0]})
 traces.append(re.findall(r'Solving for (\S+), Initial residual = ([^,]+), Final residual = ([^,]+), No Iterations (\d+)',t))
assert traces[0] and traces[1]
open('TIMING_PAIR.json','w').write(json.dumps({'arms':timings,'loop_speedup':timings[0]['loop_2_49_mean']/timings[1]['loop_2_49_mean']-1,'clock_speedup':timings[0]['clock_2_49']/timings[1]['clock_2_49']-1},indent=2))
open('TRACE_PASS.json','w').write(json.dumps({'counts':[len(x) for x in traces],'exact':traces[0]==traces[1]}))
PY
export OMP_NUM_THREADS=1 FOAM_JOB_DIR=$w/jobControl
set +e
reply=$(bsub -K -m 1 -b -p -health 0 -q q_swlidelei -n 32 -host_stack 1024 -share_size 15000 -cgsp 64 -ro_size 128 -timelimit 00:20:00 -J lnj-cache-paircmp -o "$w/compare.%J.log" "$r/build/opt960-compare-mpi-20261019-01/compare_mpi" "$left" "$right" 0.05 960 2>&1)
rc=$?;set -e;printf '%s\n' "$reply" > SUBMISSION.txt;test "$rc" = 0
job=$(printf '%s\n' "$reply" | sed -n 's/.*Job <\([0-9][0-9]*\)>.*/\1/p');[[ "$job" =~ ^[1-9][0-9]*$ ]];echo "$job" > JOB_ID
printf 'FIELD_COMPARISON_SUBMITTED=%s\n' "$job"
