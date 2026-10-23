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
n19=$r/build/opt960-topology-plan-cache-20261019-01/integration
d=$r/build/opt960-tile-production-only-20261019-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
lib=$c/platforms/$WM_OPTIONS/lib
archive=$n19/lib/libdfMatrices.a
test "$(sha256sum "$archive" | cut -d' ' -f1)" = 6249e02b27f905fe7cb15e7b48b1f669dadc9c2fcbefb1696efb4bb3e1d330f5
test "$(sha256sum "$entry" | cut -d' ' -f1)" = e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
test "$(sha256sum "$lib/libOpenFOAM.a" | cut -d' ' -f1)" = 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
test "$(sha256sum "$lib/libseaWave.a" | cut -d' ' -f1)" = 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
sha256sum -c "$d/staged-inputs.sha256"
sha256sum -c "$d/reused-inputs.sha256"
test "$(ar p "$archive" tile.o | sha256sum | cut -d' ' -f1)" = 869d9ab91cd06bb60d68ad7b875bc6d8b35ebe995ea667926f4e9aab68df5087
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
# The removed research modes have NO references in the new smoother.
if nm -u "$d/csrSymGaussSeidelSmoother.o" | grep -E '(slave_)?(lnjLevelSgs_CPE|lnjPermutationSgs_CPE)$';then
  printf 'Unexpected dependency on removed research kernel\\n' >&2;exit 92
fi
for member in lnjLevelSgs_CPE.o lnjPermutationSgs_CPE.o;do
  test "$(ar t "$d/lib/libdfMatrices.a" | grep -cx "$member")" = 1
done
ar d "$d/lib/libdfMatrices.a" lnjLevelSgs_CPE.o lnjPermutationSgs_CPE.o
ar r "$d/lib/libdfMatrices.a" "$d/csrSymGaussSeidelSmoother.o"
ar s "$d/lib/libdfMatrices.a"
# A pinned archive copy is changed only by named ar operations.
# Check explicit removals and critical retained ABI/math members.
for member in lnjLevelSgs_CPE.o lnjPermutationSgs_CPE.o;do
  if ar t "$d/lib/libdfMatrices.a" | grep -Fx "$member" >/dev/null;then exit 92;fi
done
for member in CSRGAMGPreconditioner.o CSRGAMGSolverSolve.o tile.o;do
  test "$(ar p "$archive" "$member" | sha256sum | cut -d' ' -f1)" = "$(ar p "$d/lib/libdfMatrices.a" "$member" | sha256sum | cut -d' ' -f1)"
done

test "$(ar p "$d/lib/libdfMatrices.a" tile.o | sha256sum | cut -d' ' -f1)" = 869d9ab91cd06bb60d68ad7b875bc6d8b35ebe995ea667926f4e9aab68df5087
out=$d/bin/boussinesqFoam-tile-production-only
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
# Read-only artifact audit: no startup/solver execution here.
nm "$out" > "$d/final-symbols.txt"
if grep -E '(slave_)?(lnjLevelSgs_CPE|lnjPermutationSgs_CPE)$' "$d/final-symbols.txt";then exit 94;fi
grep -F lnjPermutationSgsTile_CPE "$d/final-symbols.txt"
readelf -SW "$out" > "$d/final-sections.txt"
readelf -lW "$out" > "$d/final-segments.txt"
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' "$d/final-sections.txt"
printf 'EXPECTED_LDM_IF_ONLY_TLS_REMOVED=0x14ae8 (84712 bytes; inspect actual sections, not proof of startup)\n'
date -Is > "$d/PRODUCTION_ONLY_BUILD_COMPLETE"
printf 'PRODUCTION_ONLY_NATURAL_TILE_CACHE_BUILD_COMPLETE no_solver_job_submitted=1\n'
