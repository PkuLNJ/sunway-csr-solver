#!/bin/bash
# Explicit build only. No solver launch, SSH, queue submission or input changes.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-coarse-rank-research-20261019-01
clean=$r/build/opt960-colored-sgs-20261019-01
parent=$clean/integrated/lib/libdfMatrices.a
lib=$c/platforms/$WM_OPTIONS/lib
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
cd "$d"
sha256sum -c STAGED_FILES.sha256
check_hash() { test "$(sha256sum "$1" | cut -d' ' -f1)" = "$2"; }
check_hash "$parent" c1b828e599d1b5af17f0f4732f991c12a8a5c2c033120960e3fbf760b165aeed
check_hash "$clean/solver-bin/boussinesqFoam-colored-sgs" 991983313d6130819481603def8f99e8f798f1df1dc57e684af3f8818d4d334a
check_hash "$lib/libdfMatrices.a" 03d2cd98850cc87c26780e03d40ec0a2c7d85a0565a29a84ce12e49ec4310809
check_hash "$lib/libOpenFOAM.a" 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
check_hash "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check_hash "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check_hash "$c/src/dfMatrices/csrMatrix/solvers/CSRGAMG/CSRGAMGSolver.C" 7f328e9ec4873238b7408a376586f5c9ca460bdc7450f8f4f5c5bba04ca540de
check_hash "$c/src/dfMatrices/csrMatrix/solvers/CSRGAMG/CSRGAMGSolverAgglomerateMatrix.C" 3ba6b0c94a5a84cfbfb5e0294fb6995d870883f38464b8ed7a3ac33893c90b71
grep -Eq '^[[:space:]]*directSolveCoarsest_\(false\)' "$c/src/dfMatrices/csrMatrix/solvers/CSRGAMG/CSRGAMGSolver.C"
mkdir "$d/build-once.guard" "$d/obj" "$d/lib" "$d/bin"
sha256sum "$parent" "$entry" "$lib/libdfMatrices.a" "$lib/libOpenFOAM.a" "$lib/libseaWave.a" > IMMUTABLE_DEPENDENCIES.sha256
printf '%s\n' 'BASE=clean991_faceSGS_workspace_c1b828' 'COARSE_SOLVE=n11_inner8_source_f98a21_logging_screen_not_accepted_baseline' 'ONLY_ADDED_ALGORITHM=lateSix_optional_runtime' > FOUNDATION.txt
for unit in CSRGAMGPreconditioner.o csrSymGaussSeidelSmoother.o lnjColoredSgs_CPE.o; do
  case "$unit" in
    CSRGAMGPreconditioner.o) expected=b18fb99a88419b5c2f2ad858705b19201e9493596b8377b98670eb706ae33c25;;
    csrSymGaussSeidelSmoother.o) expected=5ac3998d29beab871c68b9c3acc4855d12ee9e0fdae6e710ffe806b948472775;;
    lnjColoredSgs_CPE.o) expected=719da23013a7b1215baa94c641eb8b948e1858d186bdd94eed70b3eaf0c2ed2c;;
  esac
  observed=$(ar p "$parent" "$unit" | sha256sum | cut -d' ' -f1)
  test "$observed" = "$expected"
  printf '%s %s\n' "$unit" "$expected" >> PRESERVE_MEMBERS.txt
done
for unit in CSRGAMGSolverAgglomerateMatrix CSRGAMGSolverSolve lateSixCSRGAMGProcAgglomeration; do
  set +e
  mpicxx -mhost -msimd -ffp-contract=off -mieee -mftz -fpermissive -g -O3 \
    -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
    -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
    -I"$d/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
    -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
    -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
    -I/usr/sw/mpi/mpi_20220608_SEA/include \
    -c "$d/src/$unit.C" -o "$d/obj/$unit.o" > "$d/compile-$unit.log" 2>&1
  rc=$?
  set -e
  printf '%s\n' "$rc" > "$d/compile-$unit.EXIT_CODE"
  if test "$rc" != 0; then tail -n 40 "$d/compile-$unit.log"; exit "$rc"; fi
done
echo TARGET_COMPILE_ONLY_API_GATE_PASSED_NO_RUNTIME_TEST
cp -p "$parent" "$d/lib/libdfMatrices.a"
ar t "$parent" > ARCHIVE_MEMBERS.before
for unit in CSRGAMGSolverAgglomerateMatrix CSRGAMGSolverSolve; do
  test "$(grep -cx "$unit.o" ARCHIVE_MEMBERS.before)" = 1
  ar r "$d/lib/libdfMatrices.a" "$d/obj/$unit.o"
done
ar s "$d/lib/libdfMatrices.a"
ar t "$d/lib/libdfMatrices.a" > ARCHIVE_MEMBERS.after
cmp ARCHIVE_MEMBERS.before ARCHIVE_MEMBERS.after
while read -r unit expected; do
  test "$(ar p "$d/lib/libdfMatrices.a" "$unit" | sha256sum | cut -d' ' -f1)" = "$expected"
done < PRESERVE_MEMBERS.txt
out=$d/bin/boussinesqFoam-coarse8-lateSix-screen
set +e
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" \
  "$d/obj/lateSixCSRGAMGProcAgglomeration.o" \
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
  > "$d/link.log" 2>&1
rc=$?
set -e
printf '%s\n' "$rc" > LINK_EXIT_CODE
if test "$rc" != 0; then tail -n 40 link.log; exit "$rc"; fi
test -s "$out"
sha256sum "$out" "$d/lib/libdfMatrices.a" "$d/obj/lateSixCSRGAMGProcAgglomeration.o" > BUILD_OUTPUTS.sha256
sha256sum -c IMMUTABLE_DEPENDENCIES.sha256
date -Is > BUILD_COMPLETE
echo N14_BUILD_COMPLETE_RUNTIME_REGISTRATION_AND_MPI_AGGREGATION_NOT_YET_TESTED_NO_JOB_SUBMITTED
