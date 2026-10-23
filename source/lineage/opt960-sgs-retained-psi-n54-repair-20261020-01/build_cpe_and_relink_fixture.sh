#!/bin/bash
# Implementation repair ONLY: one CPE compile, reuse actual fixture host.o.
# No host compilation, old overwrite, solver archive, case, run or submission.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0;test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-sgs-retained-psi-n54-repair-20261020-01
compiled=$r/build/opt960-sgs-retained-psi-n54-compile-20261020-01
fixture=$r/build/opt960-sgs-retained-psi-n54-fixture-20261020-01
host=$fixture/fixture.o
math=/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/lib/libm_gy.a
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
check "$host" 162ddf0bb84a54bc75ebb5cd2dc1a290f0f1fe071b2a59efb56078a3b877ace7
check "$compiled/obj/retained-psi.o" d87e639cc3286cdac4f2518f63f0fdc6a9162018b044f2cf7ba4a167bf5c52c3
check "$fixture/bin/n54_fixture" dda1c7a03e03628fe7850054acae4825df4e2bf12b131fe1cdcffa6029be9a79
check "$compiled/src/lnjRetainedPsiSgsParam.h" c77742f54009b64a50e4f6d6c674512294866762fe17d501d0701b7c7fb66840
check "$compiled/src/lnjLevelSgsParam.h" 9c26c8f995ee9d041c112754826bf2c8626188b4f991216ab6aa23739e11dfc1
check "$math" 3688c49464774be9e5b7b11162f79cc8a5c8a4e5f4f5706c719ff65e310c1f5d
cd "$d";sha256sum -c staged-inputs.sha256
check "$d/src/lnjRetainedPsiSgs_CPE.c" 28c524a2ca7a53ea45052b4c2b08f5bb15111019108bf06892510bc687962577
mkdir REPAIR_BUILD_ATTEMPTED obj bin
phase(){ local name=$1;shift;set +e;"$@" > "$d/$name.log" 2>&1;local status=$?;set -e
 printf '%s_EXIT_CODE=%s\n' "$name" "$status"|tee "$d/$name.EXIT_CODE"
 if test "$status" != 0;then tail -n 35 "$d/$name.log";exit "$status";fi; }
# Unchanged previously compiled n54 CPE flags and unchanged120-byte header.
phase COMPILE_CPE mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC \
 -I"$compiled/src" -c "$d/src/lnjRetainedPsiSgs_CPE.c" -o "$d/obj/retained-psi-repaired.o"
# Exact failed fixture host.o and same successful math closure; no CPE duplicate.
phase LINK mpicxx -mdynamic "$host" "$d/obj/retained-psi-repaired.o" \
 -Wl,--start-group "$math" -Wl,--end-group -lm -o "$d/bin/n54_fixture_repaired"
nm -u "$d/bin/n54_fixture_repaired" > "$d/undefined.txt"
if grep -E '(_sw7mc_|sw7mc_exception)' "$d/undefined.txt";then echo UNRESOLVED_VENDOR_MATH;exit 92;fi
nm -g "$d/bin/n54_fixture_repaired" > "$d/symbols.txt"
grep -F lnjRetainedPsiSgs_CPE "$d/symbols.txt"
if grep -E 'lnjPermutationSgs(Tile)?_CPE|lnjSgsHaloOverlap_CPE' "$d/symbols.txt";then echo EXTRA_KERNEL_FORBIDDEN;exit 93;fi
readelf -SW "$d/bin/n54_fixture_repaired" > "$d/sections.txt"
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' "$d/sections.txt"||true
sha256sum "$d/bin/n54_fixture_repaired" "$d/obj/retained-psi-repaired.o" "$host" > "$d/BUILD_OUTPUTS.sha256"
cat "$d/BUILD_OUTPUTS.sha256"
check "$host" 162ddf0bb84a54bc75ebb5cd2dc1a290f0f1fe071b2a59efb56078a3b877ace7
check "$compiled/obj/retained-psi.o" d87e639cc3286cdac4f2518f63f0fdc6a9162018b044f2cf7ba4a167bf5c52c3
check "$fixture/bin/n54_fixture" dda1c7a03e03628fe7850054acae4825df4e2bf12b131fe1cdcffa6029be9a79
printf 'N54_WAIT_PRECEDENCE_REPAIR_BUILT_ONLY_NOT_RUN\n' > "$d/BUILD_COMPLETE"
