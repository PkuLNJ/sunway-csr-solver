#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/csr-construction-profile-20261026-01
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
fixed=$r/build/amr-sqr-wave-aligned-20261024-r1
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
appref=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
parent=$r/build/amr-scalar-cache-aligned-20261024-r2
n38=$r/build/opt960-zero-initial-halo-20261020-01-repair-01
cd "$w";mkdir BUILD_ONCE.guard obj bin log lib
sha256sum src/* > SOURCES.sha256
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -O3 -std=c++14 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC)
incs=(-I"$w/src" -I"$d/src" -I"$root/adapter" -I"$root/io" -I"$appref" -I"$appref/../VoF" -I"$c/applications/solvers/multiphase/VoF" -I"$c/src/transportModels" -I"$c/src/finiteVolume/cfdTools" -I"$c/src/Pstream/mpi" -I/usr/sw/mpi/mpi_20220608_SEA/include)
for rel in transportModels/twoPhaseMixture transportModels/incompressible transportModels/interfaceProperties TurbulenceModels/turbulenceModels TurbulenceModels/incompressible transportModels/immiscibleIncompressibleTwoPhaseMixture finiteVolume parallel/decompose/decompositionMethods mesh/snappyHexMesh fileFormats triSurface dynamicFvMesh meshTools dynamicMesh renumber/renumberMethods renumber/zoltanRenumber dfRenumber dfSchedule dfUtils dfMatrices dfGenMatrix waves OpenFOAM OSspecific/POSIX;do incs+=(-I"$c/src/$rel/lnInclude");done

mpicxx "${flags[@]}" "${incs[@]}" -c src/boussinesqFoam.C -o obj/boussinesqFoam.o > log/entry.compile.log 2>&1
mpicxx "${flags[@]}" "${incs[@]}" -c src/ConstructProfile.C -o obj/ConstructProfile.o > log/timer.compile.log 2>&1
for unit in CSRGAMGSolver CSRGAMGSolverAgglomerateMatrix;do
 mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 -std=c++17 -D_PROFILING_ -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC -I"$w/src" -I"$n38/src" "${incs[@]}" -c "src/$unit.C" -o "obj/$unit.o" > "log/$unit.compile.log" 2>&1
done
cp -p "$d/lib/libdfMatrices.a" lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.before
ar r lib/libdfMatrices.a obj/CSRGAMGSolver.o obj/CSRGAMGSolverAgglomerateMatrix.o
ar s lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
lib=$c/platforms/$WM_OPTIONS/lib
python make_link.py
source link-only.sh
sha256sum bin/* lib/* obj/* > OUTPUTS.sha256
printf 'CONSTRUCTION_PROFILE_BUILD_COMPLETE\n' > BUILD_COMPLETE
cat BUILD_COMPLETE
