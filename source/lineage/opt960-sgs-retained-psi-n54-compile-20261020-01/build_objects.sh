#!/bin/bash
# ONLY two object compilations. No archive copy/change, link, main or case.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0;test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-sgs-retained-psi-n54-compile-20261020-01
base=$r/build/opt960-discarded-coarse-skip-20261020-01
n23=$r/build/opt960-topology-match-integration-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
cd "$d";sha256sum -c staged-inputs.sha256;sha256sum -c reused-headers.sha256
test -f "$base/DISCARDED_COARSE_SKIP_BUILD_COMPLETE"
test "$(sha256sum "$base/lib/libdfMatrices.a"|cut -d' ' -f1)" = d8c6f460734bacc3ac7bbc81585e9ba5490750a0908b2c527116de919e6f3771
mkdir COMPILE_ATTEMPTED obj
phase(){
 local name=$1;shift
 set +e;"$@" > "$d/$name.log" 2>&1;local result=$?;set -e
 printf '%s_EXIT_CODE=%s\n' "$name" "$result"|tee "$d/$name.EXIT_CODE"
 if test "$result" != 0;then tail -n 35 "$d/$name.log";exit "$result";fi
}
# Exact n38/n39 host flags, not a new language standard. Original dfMatrices
# Make/options also explicitly has std=c++17 and finiteVolume/lnInclude.
# processorFvPatchField.H is resolved via that actual finiteVolume lnInclude,
# whose header/source pins are in reused-headers.sha256 (no invented SDK path).
phase COMPILE_SGS mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 \
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
 -I"$d/src" -I"$n23/src" -I"$n18/src" -I"$n16/src" -I"$old/src" \
 -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
 -I/usr/sw/mpi/mpi_20220608_SEA/include \
 -c "$d/src/csrSymGaussSeidelSmoother.C" -o "$d/obj/csrSymGaussSeidelSmoother.o"
# Exact previously validated tile CPE flags. Only this candidate compiled.
phase COMPILE_CPE mpicc -mslave -msimd -mieee -mftz -ffp-contract=off -O3 -std=gnu11 -fPIC \
 -I"$d/src" -c "$d/src/lnjRetainedPsiSgs_CPE.c" -o "$d/obj/retained-psi.o"
phase NM_SGS nm -u "$d/obj/csrSymGaussSeidelSmoother.o"
phase NM_CPE nm -g "$d/obj/retained-psi.o"
grep -F lnjRetainedPsiSgs_CPE "$d/NM_SGS.log"
grep -F lnjRetainedPsiSgs_CPE "$d/NM_CPE.log"
if grep -F lnjPermutationSgsTile_CPE "$d/NM_SGS.log";then echo UNEXPECTED_OLD_CPE_REFERENCE;exit 92;fi
readelf -SW "$d/obj/retained-psi.o" > "$d/CPE_SECTIONS.txt"
sha256sum "$d/obj/csrSymGaussSeidelSmoother.o" "$d/obj/retained-psi.o" > "$d/OBJECTS.sha256"
cat "$d/OBJECTS.sha256"
printf 'N54_TWO_OBJECTS_COMPILED_ONLY_NO_ARCHIVE_LINK_OR_RUNTIME=1\n' > "$d/OBJECTS_COMPLETE"
