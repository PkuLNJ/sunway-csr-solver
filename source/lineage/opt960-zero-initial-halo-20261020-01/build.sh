#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-zero-initial-halo-20261020-01
base=$r/build/opt960-small-coarse-fixed960-20261019-01
n23=$r/build/opt960-topology-match-integration-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
lib=$c/platforms/$WM_OPTIONS/lib
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
cd "$d";sha256sum -c staged-inputs.sha256;sha256sum -c reused-headers.sha256
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
test -f "$base/SMALL_COARSE_FIXED960_BUILD_COMPLETE"
check "$base/lib/libdfMatrices.a" 4d766e14df6dd779d6f6ff69a399b5a9553b6f9073d5770c53acc4528cc23817
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$lib/libOpenFOAM.a" 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$c/src/dfMatrices/csrMatrix/solvers/CSRGAMG/CSRGAMGSolver.H" 252a7f533d349118a3609ad6dea79b7947efe7fc371c30466f97822cd7fca643
check "$c/src/dfMatrices/csrMatrix/solvers/CSRGAMG/CSRGAMGSolver.C" 7f328e9ec4873238b7408a376586f5c9ca460bdc7450f8f4f5c5bba04ca540de
test "$(ar p "$base/lib/libdfMatrices.a" CSRGAMGSolver.o|sha256sum|cut -d' ' -f1)" = 6d1b75bac63797990e1b982083f15b1f1ad8daaa61991a4253968d607b8982d3
mkdir "$d/build-once.guard" "$d/bin" "$d/lib"
# Existing n27 TU switches including std17/profiling/FMAoff retained.
# Private workspace/new nonvirtual SGS headers FIRST; unchanged n23 dependencies next.
for unit in csrSymGaussSeidelSmoother CSRGAMGSolverSolve CSRGAMGPreconditioner CSRGAMGSolver;do
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 \
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
 -I"$d/src" -I"$n23/src" -I"$n18/src" -I"$n16/src" -I"$old/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
 -I/usr/sw/mpi/mpi_20220608_SEA/include -c "$d/src/$unit.C" -o "$d/$unit.o" \
 > "$d/$unit.compile.log" 2>&1 || { tail -n 30 "$d/$unit.compile.log";exit 91; }
done
printf '0\n' > COMPILE_EXIT_CODE
ar t "$base/lib/libdfMatrices.a" > MEMBERS.before
for unit in csrSymGaussSeidelSmoother CSRGAMGSolverSolve CSRGAMGPreconditioner CSRGAMGSolver;do
 test "$(grep -Fxc "$unit.o" MEMBERS.before)" = 1
done
cp -p "$base/lib/libdfMatrices.a" "$d/lib/libdfMatrices.a"
ar r "$d/lib/libdfMatrices.a" "$d/csrSymGaussSeidelSmoother.o" "$d/CSRGAMGSolverSolve.o" "$d/CSRGAMGPreconditioner.o" "$d/CSRGAMGSolver.o"
ar s "$d/lib/libdfMatrices.a"
ar t "$d/lib/libdfMatrices.a" > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
for unit in csrSymGaussSeidelSmoother CSRGAMGSolverSolve CSRGAMGPreconditioner CSRGAMGSolver;do
 test "$(ar p "$d/lib/libdfMatrices.a" "$unit.o"|sha256sum|cut -d' ' -f1)" = "$(sha256sum "$d/$unit.o"|cut -d' ' -f1)"
done
for member in CSRPCG.o tile.o numeric-pack.o topology-match.o;do
 a=$(ar p "$base/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 b=$(ar p "$d/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$member" >> PRESERVED_PCG_AND_CPE.sha256
done
# Copied archive+4 replacements, ordered member names checked, not all contents rehashed.
out=$d/bin/boussinesqFoam-zero-initial-halo
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" "$plugin" \
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
nm "$out" > final-symbols.txt
for name in lnjPermutationSgsTile_CPE lnjNumericPack_CPE lnjTopologyMatch_CPE;do
 grep -F "$name" final-symbols.txt
done
nm -C "$out" > final-demangled.txt
grep -F smoothFromZero final-demangled.txt
readelf -SW "$out" > final-sections.txt
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' final-sections.txt
sha256sum "$out" "$d/lib/libdfMatrices.a" "$entry" "$plugin" "$d/csrSymGaussSeidelSmoother.o" "$d/CSRGAMGSolverSolve.o" "$d/CSRGAMGPreconditioner.o" "$d/CSRGAMGSolver.o" > BUILD_OUTPUTS.sha256
date -Is > ZERO_INITIAL_HALO_BUILD_COMPLETE
echo ZERO_INITIAL_HALO_BUILD_COMPLETE_NOT_EXECUTED
