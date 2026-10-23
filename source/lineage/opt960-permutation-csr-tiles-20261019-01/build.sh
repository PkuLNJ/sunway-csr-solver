#!/bin/bash
# Build-only recipe. Execution requires a separate allocated 1-MPE target job.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-permutation-csr-tiles-20261019-01
sha256sum -c "$d/staged-inputs.sha256"
mkdir "$d/target-build-once.guard" "$d/bin"
mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC \
  -I"$d/src" -c "$d/baseline/lnjPermutationSgs_CPE.c" -o "$d/original.o" \
  > "$d/compile-original.log" 2>&1 || { tail -n 25 "$d/compile-original.log";exit 91; }
mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC \
  -I"$d/src" -c "$d/src/lnjPermutationSgsTile_CPE.c" -o "$d/tile.o" \
  > "$d/compile-tile.log" 2>&1 || { tail -n 25 "$d/compile-tile.log";exit 92; }
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -O3 -std=c++17 -fPIC \
  -I"$d/src" -c "$d/tests/test_permutation_tiles.cpp" -o "$d/test.o" \
  > "$d/compile-host.log" 2>&1 || { tail -n 25 "$d/compile-host.log";exit 93; }
mpicxx -mdynamic "$d/test.o" "$d/original.o" "$d/tile.o" \
  -o "$d/bin/test_permutation_tiles" > "$d/link.log" 2>&1 || { tail -n 25 "$d/link.log";exit 94; }
sha256sum "$d/bin/test_permutation_tiles" "$d/original.o" "$d/tile.o"
date -Is > "$d/TARGET_BUILD_COMPLETE"
printf 'PERMUTATION_TILE_BUILD_COMPLETE (not executed)\n'
