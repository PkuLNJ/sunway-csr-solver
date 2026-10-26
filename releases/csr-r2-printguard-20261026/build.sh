#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/csr-r2-miniter0-printguard-20261026-01
parent=$r/build/amr-scalar-cache-aligned-20261024-r2
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
fixed=$r/build/amr-sqr-wave-aligned-20261024-r1
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
lib=$c/platforms/$WM_OPTIONS/lib
cd "$w";mkdir BUILD_ONCE.guard src obj bin log
python make_guard.py
mpicxx -mhost -msimd -mieee -mftz -fpermissive -g -O3 -std=c++14 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC -I"$w/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" -I/usr/sw/mpi/mpi_20220608_SEA/include -c src/ProfilingZeroGuard.C -o obj/ProfilingZeroGuard.o > log/compile.log 2>&1
source link-only.sh
nm -C bin/boussinesqFoam-capture-wave-sqr-scalar-aligned | grep ' T Foam::CSRGAMGPreconditioner::print_time() const' > log/STRONG_GUARD_SYMBOL.txt
test -s log/STRONG_GUARD_SYMBOL.txt
sha256sum bin/boussinesqFoam-capture-wave-sqr-scalar-aligned | cut -d' ' -f1 > BINARY.sha256
sha256sum src/* obj/* bin/* > OUTPUTS.sha256
printf 'BUILD_AND_STRONG_SYMBOL_VERIFIED\n' > BUILD_COMPLETE
cat BUILD_COMPLETE
