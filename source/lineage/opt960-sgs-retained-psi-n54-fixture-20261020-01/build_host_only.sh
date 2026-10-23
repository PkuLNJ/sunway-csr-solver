#!/bin/bash
# Host one TU + previously compiled candidate CPE. No CPE rebuild, no execution.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
d=$r/build/opt960-sgs-retained-psi-n54-fixture-20261020-01
compiled=$r/build/opt960-sgs-retained-psi-n54-compile-20261020-01
old=$r/build/opt960-level-sgs-20261019-01/integration
cpe=$compiled/obj/retained-psi.o
math=/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/lib/libm_gy.a
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
check "$cpe" d87e639cc3286cdac4f2518f63f0fdc6a9162018b044f2cf7ba4a167bf5c52c3
check "$compiled/src/lnjRetainedPsiSgsHost.H" 489ce9bcdfd1f4d6f709fdd28a049b3eed8a63af75a8345a3f9286a6cbb70564
check "$compiled/src/lnjRetainedPsiSgsParam.h" c77742f54009b64a50e4f6d6c674512294866762fe17d501d0701b7c7fb66840
check "$math" 3688c49464774be9e5b7b11162f79cc8a5c8a4e5f4f5706c719ff65e310c1f5d
check "$old/src/lnjLevelPacked.H" cb469733aeb160187e07baaac7efaf0a3f5499f7229eb2474bd16b54fe300b6e
check "$old/src/lnjLevelSgs.H" 9359024bfe5a41407be4a59a7cf3f681ae5fcead704e88516025813c34916767
cd "$d";sha256sum -c staged-inputs.sha256;mkdir HOST_BUILD_ATTEMPTED bin
phase(){ local name=$1;shift;set +e;"$@" > "$d/$name.log" 2>&1;local rc=$?;set -e
 printf '%s_EXIT_CODE=%s\n' "$name" "$rc"|tee "$d/$name.EXIT_CODE"
 if test "$rc" != 0;then tail -n 30 "$d/$name.log";exit "$rc";fi; }
# Actual successful n45 tiny flags; no Foam libraries or alternate kernels.
phase COMPILE_HOST mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -O3 -std=c++17 -fPIC \
 -I"$compiled/src" -I"$old/src" -c "$d/fixture.cpp" -o "$d/fixture.o"
# Explicit static vendor math closure prevents the already observed unresolved
# _sw7mc_log/sw7mc_exception from minimal dynamic links. No copied old kernel.
phase LINK mpicxx -mdynamic "$d/fixture.o" "$cpe" \
 -Wl,--start-group "$math" -Wl,--end-group -lm -o "$d/bin/n54_fixture"
nm -u "$d/bin/n54_fixture" > "$d/undefined.txt"
if grep -E '(_sw7mc_|sw7mc_exception)' "$d/undefined.txt";then echo UNRESOLVED_PLATFORM_MATH;exit 92;fi
nm -g "$d/bin/n54_fixture" > "$d/symbols.txt"
grep -F lnjRetainedPsiSgs_CPE "$d/symbols.txt"
if grep -E 'lnjPermutationSgs(Tile)?_CPE|lnjSgsHaloOverlap_CPE' "$d/symbols.txt";then echo UNEXPECTED_EXTRA_KERNEL;exit 93;fi
readelf -SW "$d/bin/n54_fixture" > "$d/sections.txt"
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' "$d/sections.txt"||true
sha256sum "$d/bin/n54_fixture" "$d/fixture.o" "$cpe" > "$d/BUILD_OUTPUTS.sha256"
cat "$d/BUILD_OUTPUTS.sha256"
printf 'N54_FIXTURE_HOST_ONLY_BUILD_COMPLETE_NOT_RUN\n' > "$d/BUILD_COMPLETE"
