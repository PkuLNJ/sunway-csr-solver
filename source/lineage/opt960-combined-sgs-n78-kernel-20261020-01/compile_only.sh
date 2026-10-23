#!/bin/bash
# ONE CPE TU only: no host compile, archive, link, main, case or runtime action.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0;test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-combined-sgs-n78-kernel-20261020-01
cd "$d";sha256sum -c staged-inputs.sha256
mkdir BUILD_ATTEMPTED
set +e
mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC \
 -I"$d" -c "$d/lnjCombinedSgs_CPE.c" -o "$d/combined.o" > "$d/COMPILE_CPE.log" 2>&1
rc=$?
set -e
printf 'COMPILE_CPE_EXIT_CODE=%s\n' "$rc"|tee "$d/COMPILE_CPE.EXIT_CODE"
if test "$rc" != 0;then tail -n 40 "$d/COMPILE_CPE.log";exit "$rc";fi
test -s "$d/combined.o"
nm -g "$d/combined.o" > "$d/symbols.txt"
grep -F lnjCombinedSgs_CPE "$d/symbols.txt"
if grep -E 'lnjRetainedPsiSgs_CPE|lnjBoundaryPsiSgs_CPE|lnjDeferredCacheSgs_CPE' "$d/symbols.txt";then
 echo EXTRA_OLD_KERNEL_SYMBOL_FORBIDDEN;exit 93
fi
readelf -SW "$d/combined.o" > "$d/sections.txt"
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' "$d/sections.txt"||true
sha256sum -c staged-inputs.sha256
sha256sum "$d/combined.o" > "$d/BUILD_OUTPUTS.sha256"
cat "$d/BUILD_OUTPUTS.sha256"
printf 'N78_ONE_CPE_TU_COMPILE_COMPLETE_NO_LINK_NO_RUNTIME\n' > "$d/BUILD_COMPLETE"
