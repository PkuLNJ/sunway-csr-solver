#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/csr-reset-trim-20261026-01
parent=$r/build/amr-scalar-cache-aligned-20261024-r2
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
fixed=$r/build/amr-sqr-wave-aligned-20261024-r1
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
lib=$c/platforms/$WM_OPTIONS/lib
cd "$w";mkdir BUILD_ONCE.guard obj bin log lib
sha256sum src/* > SOURCES.sha256
ar p "$d/lib/libdfMatrices.a" CSRGAMGPreconditioner.o | sha256sum > ORIGINAL_PCG.sha256
grep -q '^115da75922ad7e1ecd07c865550852f9c387fdfefccda899d223b015325e4005 ' ORIGINAL_PCG.sha256
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 -std=c++17 -D_PROFILING_ -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC -I"$w/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" -I/usr/sw/mpi/mpi_20220608_SEA/include -c src/CSRGAMGPreconditioner.C -o obj/CSRGAMGPreconditioner.o > log/compile.log 2>&1
cp -p "$d/lib/libdfMatrices.a" lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.before
ar r lib/libdfMatrices.a obj/CSRGAMGPreconditioner.o
ar s lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
python make_link.py
source link-only.sh
sha256sum bin/* lib/* obj/* > OUTPUTS.sha256
printf 'RESET_TRIM_PRIVATE_BUILD_COMPLETE_NOT_RUNTIME_VERIFIED\n' > BUILD_COMPLETE
cat BUILD_COMPLETE
