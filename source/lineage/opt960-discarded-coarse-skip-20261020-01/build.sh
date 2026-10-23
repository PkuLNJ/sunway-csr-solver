#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-discarded-coarse-skip-20261020-01
base=$r/build/opt960-zero-initial-halo-20261020-01-repair-01
n23=$r/build/opt960-topology-match-integration-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
lib=$c/platforms/$WM_OPTIONS/lib
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
cd "$d"
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
test -f "$base/ZERO_INITIAL_HALO_REPAIR01_BUILD_COMPLETE"
check "$base/lib/libdfMatrices.a" 2d8baff535b2035782ea1c459e9628d2156828a86908309fe0c4c6c5d56c26d8
check "$base/bin/boussinesqFoam-zero-initial-halo" 15e5c5420fd9f4141d80dd88723a4f2055bb3e6ffbe9626a63a505790ee0f1bd
check "$base/CSRGAMGSolverSolve.o" cd0f9994932e8c5d33bc52b7482b7f122cfac8451f1b0a9579dff50fda2f1f62
check "$base/CSRGAMGSolver.o" 1b2d0a5869ead79b4ad573669a97135abd46f6b16d01e387241b9817b1927568
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$lib/libOpenFOAM.a" 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check "$d/src/CSRGAMGSolverSolve.C" 74c5ac58a26b2c6f2fdacb678978cec6a59b40e20a28382ed4151a09ed3f73f0
check "$base/src/CSRGAMGSolver.H" 252a7f533d349118a3609ad6dea79b7947efe7fc371c30466f97822cd7fca643
mkdir build-once.guard bin lib
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3 \
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 \
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC \
 -I"$base/src" -I"$n23/src" -I"$n18/src" -I"$n16/src" -I"$old/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude" \
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude" \
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude" \
 -I/usr/sw/mpi/mpi_20220608_SEA/include -c "$d/src/CSRGAMGSolverSolve.C" -o "$d/CSRGAMGSolverSolve.o" \
 > "$d/CSRGAMGSolverSolve.compile.log" 2>&1 || { tail -n 30 "$d/CSRGAMGSolverSolve.compile.log";exit 91; }
printf '0\n' > COMPILE_EXIT_CODE
ar t "$base/lib/libdfMatrices.a" > MEMBERS.before
test "$(grep -Fxc CSRGAMGSolverSolve.o MEMBERS.before)" = 1
cp -p "$base/lib/libdfMatrices.a" "$d/lib/libdfMatrices.a"
ar r "$d/lib/libdfMatrices.a" "$d/CSRGAMGSolverSolve.o"
ar s "$d/lib/libdfMatrices.a"
ar t "$d/lib/libdfMatrices.a" > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
test "$(ar p "$d/lib/libdfMatrices.a" CSRGAMGSolverSolve.o|sha256sum|cut -d' ' -f1)" = "$(sha256sum CSRGAMGSolverSolve.o|cut -d' ' -f1)"
for member in CSRGAMGSolver.o csrSymGaussSeidelSmoother.o CSRGAMGPreconditioner.o CSRPCG.o tile.o numeric-pack.o topology-match.o;do
 a=$(ar p "$base/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 b=$(ar p "$d/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$member" >> PRESERVED_KEY_MEMBERS.sha256
done
out=$d/bin/boussinesqFoam-discarded-coarse-skip
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
sha256sum "$out" "$d/lib/libdfMatrices.a" "$d/CSRGAMGSolverSolve.o" > BUILD_OUTPUTS.sha256
date -Is > DISCARDED_COARSE_SKIP_BUILD_COMPLETE
echo DISCARDED_COARSE_SKIP_BUILD_COMPLETE_NOT_EXECUTED
