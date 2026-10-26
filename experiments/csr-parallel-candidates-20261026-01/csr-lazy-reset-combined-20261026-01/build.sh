#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/csr-lazy-reset-combined-20261026-01
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
fixed=$r/build/amr-sqr-wave-aligned-20261024-r1
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
appref=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
parent=$r/build/amr-scalar-cache-aligned-20261024-r2
n38=$r/build/opt960-zero-initial-halo-20261020-01-repair-01
cd "$w";mkdir BUILD_ONCE.guard obj bin log lib
u=$r/build/csr-lazy-u-combined-20261026-01
lazy=$r/build/csr-reset-trim-20261026-01
test -f "$u/BUILD_COMPLETE";test -f "$lazy/BUILD_COMPLETE"
sha256sum "$u/obj/boussinesqFoam.o" "$u/obj/ConstructProfile.o" "$lazy/obj/CSRGAMGPreconditioner.o" "$u/lib/libdfMatrices.a" > REUSED_INPUTS.sha256
cp -p "$u/obj/boussinesqFoam.o" "$u/obj/ConstructProfile.o" "$lazy/obj/CSRGAMGPreconditioner.o" obj/
cp -p "$u/lib/libdfMatrices.a" lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.before
ar r lib/libdfMatrices.a obj/CSRGAMGPreconditioner.o
ar s lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
lib=$c/platforms/$WM_OPTIONS/lib
python make_link.py
source link-only.sh
sha256sum bin/* obj/* lib/* > OUTPUTS.sha256
sha256sum -c REUSED_INPUTS.sha256 > REUSED_UNCHANGED.txt
echo COMBINED_LINK_COMPLETE > BUILD_COMPLETE
cat BUILD_COMPLETE
