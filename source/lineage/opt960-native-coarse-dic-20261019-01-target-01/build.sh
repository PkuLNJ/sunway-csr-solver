#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-native-coarse-dic-20261019-01-target-01
n27=$r/build/opt960-small-coarse-fixed960-20261019-01
lib=$c/platforms/$WM_OPTIONS/lib
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
cd "$d"
sha256sum -c staged-inputs.sha256
test -f "$n27/SMALL_COARSE_FIXED960_BUILD_COMPLETE"
check() { test "$(sha256sum "$1" | cut -d' ' -f1)" = "$2"; }
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$n27/lib/libdfMatrices.a" 4d766e14df6dd779d6f6ff69a399b5a9553b6f9073d5770c53acc4528cc23817
check "$lib/libOpenFOAM.a" 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$c/src/OpenFOAM/matrices/lduMatrix/solvers/GAMG/GAMGSolverSolve.C" ad0a2baff87c8d56235b317a937c34293f7f2b9689e27e9d085c9dfbfef34e24
check "$c/src/OpenFOAM/Make/options" ed953f9190379b45c021e476626b81ef9fa5ce6afa70f6e1cc8396a69f08430e
check "$c/wmake/rules/linux64swgcc/c++" cf98db04ebd9a98687f211205d8eab09c9db8e0b09595dd0c7d209c1e83f3f83
check "$c/wmake/rules/linux64swgcc/c++Opt" 9ba0e6290b6ec56f7463531e9a4680efdf05ef70f6cd633142501ef2550e6e91
check "$c/wmake/rules/linux64swgcc/general" 0d931a95c951991e668aa601599bf16497fb1fe071fc92b610eab4907d39b884
check "$c/wmake/rules/General/general" 6d2f3294562dbdcaf9a63e14b23dbc36686993e71fa8d8f5999321a2d2bf2e88
mkdir "$d/build-once.guard" "$d/lib" "$d/bin" "$d/obj"
# Captured actual wmake switches, no added std/FMA/profiling macro.
flags=(-mhost -msimd -mieee -mftz -fpermissive -g
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64
 -Wall -Wextra -Wold-style-cast -Wnon-virtual-dtor -Wno-unused-parameter
 -Wno-invalid-offsetof -Wno-attributes -O3 -DNoRepository -ftemplate-depth-100
 -I"$d/obj" -I"$d" -I"$c/src/OpenFOAM/lnInclude"
 -I"$c/src/OSspecific/POSIX/lnInclude" -I/usr/sw/mpi/mpi_20220608_SEA/include -fPIC)
printf '%q ' mpicxx "${flags[@]}" -c "$d/src/GAMGSolverSolve.C" -o "$d/obj/GAMGSolverSolve.o" > compile-command.txt
printf '\n' >> compile-command.txt
set +e
mpicxx "${flags[@]}" -c "$d/src/GAMGSolverSolve.C" -o "$d/obj/GAMGSolverSolve.o" > compile.log 2>&1
rc=$?;set -e;printf '%s\n' "$rc" > COMPILE_EXIT_CODE
if test "$rc" != 0;then tail -n 40 compile.log;exit "$rc";fi
ar t "$lib/libOpenFOAM.a" > MEMBERS.before
test "$(grep -Fxc GAMGSolverSolve.o MEMBERS.before)" = 1
ar p "$lib/libOpenFOAM.a" GAMGSolverSolve.o | sha256sum > ORIGINAL_SOLVE_MEMBER.sha256
cp -p "$lib/libOpenFOAM.a" "$d/lib/libOpenFOAM.a"
ar r "$d/lib/libOpenFOAM.a" "$d/obj/GAMGSolverSolve.o"
ar s "$d/lib/libOpenFOAM.a"
ar t "$d/lib/libOpenFOAM.a" > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
test "$(ar p "$d/lib/libOpenFOAM.a" GAMGSolverSolve.o | sha256sum | cut -d' ' -f1)" = "$(sha256sum "$d/obj/GAMGSolverSolve.o" | cut -d' ' -f1)"
# Ordered names and replaced member checked, not every other member rehashed.
out=$d/bin/boussinesqFoam-native-coarse-dic
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" "$plugin" \
  -L"$d/lib" -L"$n27/lib" -L"$lib" -L"$lib/mpi" \
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
readelf -SW "$out" > final-sections.txt
grep -E '[.]ldm[[:space:]]|[.]slave_tls[[:space:]]' final-sections.txt
sha256sum "$out" "$d/lib/libOpenFOAM.a" "$d/obj/GAMGSolverSolve.o" "$entry" "$n27/lib/libdfMatrices.a" "$plugin" > BUILD_OUTPUTS.sha256
date -Is > NATIVE_COARSE_DIC_BUILD_COMPLETE
echo NATIVE_COARSE_DIC_BUILD_COMPLETE_NO_EXECUTION
