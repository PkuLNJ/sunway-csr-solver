#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
tile=$r/build/opt960-permutation-csr-tiles-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
d=$r/build/opt960-topology-plan-cache-20261019-01/integration
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
lib=$c/platforms/$WM_OPTIONS/lib
archive=$n18/lib/libdfMatrices.a
test "$(sha256sum "$archive" | cut -d' ' -f1)" = 7046d3c9dfc2170c5b0215ed4eff65a94abab092effe4d6ae3dcfdbfe74b5ca8
test "$(sha256sum "$entry" | cut -d' ' -f1)" = e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
test "$(sha256sum "$lib/libOpenFOAM.a" | cut -d' ' -f1)" = 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
test "$(sha256sum "$lib/libseaWave.a" | cut -d' ' -f1)" = 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
sha256sum -c "$d/staged-inputs.sha256"
mkdir "$d/build-once.guard" "$d/lib" "$d/bin"
mpicxx -mhost -msimd -ffp-contract=off -mieee -mftz -fpermissive -g -O3 \
  -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
  -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
  -I"$d/src" -I"$n18/src" -I"$n16/src" -I"$old/src" \
  -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
  -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
  -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
  -I/usr/sw/mpi/mpi_20220608_SEA/include \
  -c "$d/src/csrSymGaussSeidelSmoother.C" -o "$d/csrSymGaussSeidelSmoother.o" \
  > "$d/smoother.compile.log" 2>&1 || { tail -n 30 "$d/smoother.compile.log";exit 91; }
cp -p "$archive" "$d/lib/libdfMatrices.a"
test "$(ar t "$archive" | grep -cx csrSymGaussSeidelSmoother.o)" = 1
ar r "$d/lib/libdfMatrices.a" "$d/csrSymGaussSeidelSmoother.o"
ar s "$d/lib/libdfMatrices.a"
for member in CSRGAMGPreconditioner.o CSRGAMGSolverSolve.o lnjLevelSgs_CPE.o lnjPermutationSgs_CPE.o tile.o;do
  test "$(ar p "$archive" "$member" | sha256sum | cut -d' ' -f1)" = "$(ar p "$d/lib/libdfMatrices.a" "$member" | sha256sum | cut -d' ' -f1)"
done
out=$d/bin/boussinesqFoam-levelSGS-topology-cache
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" \
  -L"$d/lib" -L"$lib" -L"$lib/mpi" \
  -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
  -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
  -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
  -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
  -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
  -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
  -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
  -lmeshTools -lutilityFunctionObjects -lOpenFOAM -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
  -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi \
  -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread \
  > "$d/link.log" 2>&1 || { tail -n 30 "$d/link.log";exit 93; }
test -s "$out"
sha256sum "$out" "$d/lib/libdfMatrices.a"
date -Is > "$d/TOPOLOGY_CACHE_INTEGRATION_BUILD_COMPLETE"
printf 'TOPOLOGY_CACHE_FIVE_MODE_INTEGRATION_BUILD_COMPLETE no_solver_job_submitted=1\n'
