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
d=$r/build/opt960-permutation-tiles-integration-20261019-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
lib=$c/platforms/$WM_OPTIONS/lib
archive=$n16/integration/lib/libdfMatrices.a
test "$(sha256sum "$archive" | cut -d' ' -f1)" = ace89594f92e3185386108930bf080edf85359b1273f76815570a6a914afd878
test "$(sha256sum "$n16/lnjPermutationSgs_CPE.o" | cut -d' ' -f1)" = 0eb08604b6e0befae0ad6bbe80bbb743844bae9e6407b6d9b99cd65f71baf69f
test "$(sha256sum "$lib/libOpenFOAM.a" | cut -d' ' -f1)" = 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
test "$(sha256sum "$lib/libseaWave.a" | cut -d' ' -f1)" = 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
test "$(sha256sum "$entry" | cut -d' ' -f1)" = e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
test "$(sha256sum "$tile/tile.o" | cut -d' ' -f1)" = 869d9ab91cd06bb60d68ad7b875bc6d8b35ebe995ea667926f4e9aab68df5087
sha256sum -c "$d/staged-inputs.sha256"
sha256sum -c "$d/reused-inputs.sha256"
sha256sum -c "$n16/integration/staged-inputs.sha256"
sha256sum -c "$n16/staged-inputs.sha256"
sha256sum -c "$old/staged-inputs.sha256"
test "$(grep -c '^PERMUTATION_CPE_FIXTURE ' "$n16/test.8662997.out")" = 12
grep -Fq 'PERMUTATION_CPE_TARGET_SYNTHETIC_PASS cases=12 NOT_CFD_SPEEDUP_EVIDENCE' "$n16/test.8662997.out"
mkdir "$d/build-once.guard" "$d/lib" "$d/bin"
mpicxx -mhost -msimd -ffp-contract=off -mieee -mftz -fpermissive -g -O3 \
  -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
  -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
  -I"$d/src" -I"$n16/src" -I"$old/src" \
  -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
  -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
  -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
  -I/usr/sw/mpi/mpi_20220608_SEA/include \
  -c "$d/src/csrSymGaussSeidelSmoother.C" -o "$d/csrSymGaussSeidelSmoother.o" \
  > "$d/smoother.compile.log" 2>&1 || { tail -n 30 "$d/smoother.compile.log";exit 91; }
cp -p "$archive" "$d/lib/libdfMatrices.a"
test "$(ar t "$d/lib/libdfMatrices.a" | grep -cx csrSymGaussSeidelSmoother.o)" = 1
test "$(ar t "$d/lib/libdfMatrices.a" | grep -cx lnjPermutationSgs_CPE.o)" = 1
if ar t "$d/lib/libdfMatrices.a" | grep -x tile.o >/dev/null;then exit 92;fi
ar r "$d/lib/libdfMatrices.a" "$d/csrSymGaussSeidelSmoother.o" "$tile/tile.o"
ar s "$d/lib/libdfMatrices.a"
# The pinned archive is copied and ar replaces only the named smoother.
# Verify critical ABI/math members, not an O(number-of-members) full archive scan.
for member in CSRGAMGPreconditioner.o CSRGAMGSolverSolve.o lnjLevelSgs_CPE.o lnjPermutationSgs_CPE.o;do
  test "$(ar p "$archive" "$member" | sha256sum | cut -d' ' -f1)" = "$(ar p "$d/lib/libdfMatrices.a" "$member" | sha256sum | cut -d' ' -f1)"
done
test "$(ar t "$d/lib/libdfMatrices.a" | grep -cx tile.o)" = 1
test "$(ar p "$d/lib/libdfMatrices.a" tile.o | sha256sum | cut -d' ' -f1)" = 869d9ab91cd06bb60d68ad7b875bc6d8b35ebe995ea667926f4e9aab68df5087
out=$d/bin/boussinesqFoam-levelSGS-tilepermute
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
date -Is > "$d/TILEPERMUTE_INTEGRATION_BUILD_COMPLETE"
printf 'TILEPERMUTE_FIVE_MODE_INTEGRATION_BUILD_COMPLETE no_solver_job_submitted=1\n'
