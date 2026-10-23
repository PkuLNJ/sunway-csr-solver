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
production=$r/build/opt960-tile-production-only-20261019-01
numeric=$r/build/opt960-cpe-numeric-pack-integration-20261019-01
match=$r/build/opt960-cpe-topology-match-20261019-01
n23=$r/build/opt960-topology-match-integration-20261019-01
d=$r/build/opt960-small-coarse-fixed960-20261019-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
lib=$c/platforms/$WM_OPTIONS/lib
archive=$n23/lib/libdfMatrices.a
test "$(sha256sum "$archive" | cut -d' ' -f1)" = f7e302871b7fbb3032e20de8171dd5ec6dc0ec8dd67e5e29b3dc8d5626570a95
test "$(ar p "$archive" CSRPCG.o | sha256sum | cut -d' ' -f1)" = e6c993e5385340ece3ff33b6b366b99bd21adbf57f5ec4ec4db1f33fb47c870a
test "$(sha256sum "$c/src/dfMatrices/csrMatrix/solvers/CSRPCG/CSRPCG.C" | cut -d' ' -f1)" = 01621fc4a45c65b37749c6c9d03c523df1d45a607d74b40d4508006a8ace04fd
test "$(sha256sum "$c/src/dfMatrices/csrMatrix/solvers/CSRPCG/CSRPCG.H" | cut -d' ' -f1)" = 0248ec03cfd0ef7a2efb6def292753c69a938d509bd43b8046277ded2397aa1b
test "$(sha256sum "$entry" | cut -d' ' -f1)" = e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
test "$(sha256sum "$lib/libOpenFOAM.a" | cut -d' ' -f1)" = 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
test "$(sha256sum "$lib/libseaWave.a" | cut -d' ' -f1)" = 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
sha256sum -c "$d/staged-inputs.sha256"
mkdir "$d/build-once.guard" "$d/lib" "$d/bin"
# Explicitly matched contraction flags; this is NOT proof the original mode
# matches the historical object. Its fourteen-field check against68815 is REQUIRED.
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
test "$(sha256sum "$plugin" | cut -d' ' -f1)" = 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
for unit in CSRGAMGSolverSolve CSRGAMGSolverAgglomerateMatrix CSRGAMGAgglomerateLduAddressing CsrPrimitiveMeshBridge CSRPCG;do
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 \
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
 -I"$d/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
 -I/usr/sw/mpi/mpi_20220608_SEA/include -c "$d/src/$unit.C" -o "$d/$unit.o" \
 > "$d/$unit.compile.log" 2>&1 || { tail -n 30 "$d/$unit.compile.log";exit 91; }
done
cp -p "$archive" "$d/lib/libdfMatrices.a"
ar t "$archive" > "$d/MEMBERS.before"
for unit in CSRGAMGSolverSolve CSRGAMGSolverAgglomerateMatrix CSRGAMGAgglomerateLduAddressing CSRPCG;do
 test "$(grep -Fxc "$unit.o" "$d/MEMBERS.before")" = 1
done
test "$(grep -Fxc CsrPrimitiveMeshBridge.o "$d/MEMBERS.before" || true)" = 0
test "$(grep -Ec 'lateSix|smallCoarseAll' "$d/MEMBERS.before" || true)" = 0
ar r "$d/lib/libdfMatrices.a" "$d/CSRGAMGSolverSolve.o" "$d/CSRGAMGSolverAgglomerateMatrix.o" "$d/CSRGAMGAgglomerateLduAddressing.o" "$d/CSRPCG.o" "$d/CsrPrimitiveMeshBridge.o"
ar s "$d/lib/libdfMatrices.a"
for member in CSRGAMGPreconditioner.o CSRGAMGSolver.o csrSymGaussSeidelSmoother.o tile.o numeric-pack.o topology-match.o;do
 test "$(ar p "$archive" "$member" | sha256sum | cut -d' ' -f1)" = "$(ar p "$d/lib/libdfMatrices.a" "$member" | sha256sum | cut -d' ' -f1)"
done
out=$d/bin/boussinesqFoam-small-coarse-fixed960
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
sha256sum "$out" "$d/lib/libdfMatrices.a"
# Pinned n23 copy + explicit4 replacements and1bridge addition; avoid rescanning every
# archive member. Named retained-member checks above cover other candidates.
ar t "$d/lib/libdfMatrices.a" > "$d/MEMBERS.after"
grep -Fxv CsrPrimitiveMeshBridge.o "$d/MEMBERS.after" > "$d/MEMBERS.after.originals"
cmp "$d/MEMBERS.before" "$d/MEMBERS.after.originals"
# Read-only artifact audit: no startup/solver execution here.
nm "$out" > "$d/final-symbols.txt"
nm -C "$out" > "$d/final-demangled-symbols.txt"
grep -Fq smallCoarseAllCSRGAMGProcAgglomeration "$d/final-demangled-symbols.txt"
if grep -Fq lateSixCSRGAMGProcAgglomeration "$d/final-demangled-symbols.txt";then exit 95;fi
if grep -E '(slave_)?(lnjLevelSgs_CPE|lnjPermutationSgs_CPE)$' "$d/final-symbols.txt";then exit 94;fi
grep -F lnjPermutationSgsTile_CPE "$d/final-symbols.txt"
grep -F lnjNumericPack_CPE "$d/final-symbols.txt"
grep -F lnjTopologyMatch_CPE "$d/final-symbols.txt"
readelf -SW "$out" > "$d/final-sections.txt"
readelf -lW "$out" > "$d/final-segments.txt"
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' "$d/final-sections.txt"
printf 'BASE_PRODUCTION_LDM=0x14ae8 (84712 bytes); no new slave kernel; verify unchanged sections\n'
date -Is > "$d/SMALL_COARSE_FIXED960_BUILD_COMPLETE"
printf 'SMALL_COARSE_FIXED960_BUILD_COMPLETE not_executed=1\n'
