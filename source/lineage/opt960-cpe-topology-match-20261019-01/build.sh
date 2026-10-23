#!/bin/bash
# Build-only; no MPI run or scheduler submission.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-cpe-topology-match-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
sha256sum -c "$d/staged-inputs.sha256"
sha256sum -c "$d/reused-inputs.sha256"
mkdir "$d/build-once.guard" "$d/bin"
mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC -fstack-usage \
 -I"$d/src" -c "$d/src/lnjTopologyMatch_CPE.c" -o "$d/topology-match.o" \
 > "$d/kernel.compile.log" 2>&1 || { tail -n 25 "$d/kernel.compile.log";exit 91; }
for arm in fixture benchmark;do
 source=$d/tests/test_match.cpp
 if test "$arm" = benchmark;then source=$d/benchmark_match.cpp;fi
 mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -O3 -std=c++17 -fPIC \
  -I"$d/src" -I"$old/src" -c "$source" -o "$d/$arm.o" \
  > "$d/$arm.compile.log" 2>&1 || { tail -n 25 "$d/$arm.compile.log";exit 92; }
 mpicxx -mdynamic "$d/$arm.o" "$d/topology-match.o" -o "$d/bin/$arm" \
  > "$d/$arm.link.log" 2>&1 || { tail -n 25 "$d/$arm.link.log";exit 93; }
done
readelf -SW "$d/topology-match.o" > "$d/kernel.sections.txt"
sha256sum "$d/topology-match.o" "$d/bin/fixture" "$d/bin/benchmark"
date -Is > "$d/TARGET_BUILD_COMPLETE"
echo TOPOLOGY_MATCH_BUILD_COMPLETE_NOT_EXECUTED
