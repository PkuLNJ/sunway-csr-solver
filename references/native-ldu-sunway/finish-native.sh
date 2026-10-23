#!/bin/bash
export LC_ALL=C
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
b=/home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7
d=$r/build/x86-ldu-native-20261016-01
source "$b/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
test -d "$d/build-once.guard" && test ! -e "$d/BUILD_COMPLETE"
mkdir "$d/finish-once.guard"
s=$d/snapshot/applications/solvers/multiphase/boussinesqFoam
objects=()
for name in GAMGSolver GAMGSolverAgglomerateMatrix GAMGSolverInterpolate GAMGSolverScale GAMGSolverSolve; do
 test -s "$d/objects/$name.o"
 test -f "$d/compile-logs/$name.log"
 if grep -En 'error:|fatal error:' "$d/compile-logs/$name.log"; then exit 91; fi
 objects+=("$d/objects/$name.o")
done
printf 'REUSING_SUCCESSFUL_NATIVE_GAMG_OBJECTS=5; no duplicate compilation\n'
ar r "$d/lib/libOpenFOAM.a" "${objects[@]}"
ar s "$d/lib/libOpenFOAM.a"
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -std=c++14 -ftemplate-depth-100 -fPIC -O3 -I"$b/src/OpenFOAM/lnInclude" -I"$b/src/OSspecific/POSIX/lnInclude" -I/usr/sw/mpi/mpi_20220608_SEA/include)
incs=(-I"$s" -I"$d/snapshot/applications/solvers/multiphase/VoF" -I"$b/src/transportModels")
for rel in transportModels/twoPhaseMixture transportModels/incompressible transportModels/interfaceProperties transportModels/immiscibleIncompressibleTwoPhaseMixture TurbulenceModels/turbulenceModels TurbulenceModels/incompressible finiteVolume dynamicFvMesh meshTools sampling thermophysicalModels/basic parallel/decompose/decompositionMethods fileFormats triSurface surfMesh dynamicMesh; do incs+=(-I"$b/src/$rel/lnInclude"); done
incs+=(-I"$b/src/finiteVolume/cfdTools" -I"$b/src/Pstream/mpi")
printf 'COMPILE_UNMODIFIED_X86_ENTRY=%s\n' "$(date -Is)"
mpicxx "${flags[@]}" "${incs[@]}" -c "$s/boussinesqFoam.C" -o "$d/objects/boussinesqFoam.o" > "$d/compile-logs/entry.log" 2>&1 || { grep -En -m8 'error:|fatal error:' "$d/compile-logs/entry.log" || true; exit 92; }
names=(seaWave atmosphericModels immiscibleIncompressibleTwoPhaseMixture interfaceProperties twoPhaseProperties incompressibleTransportModels fvMotionSolvers fieldFunctionObjects incompressibleTurbulenceModels combustionModels turbulenceModels compressibleTurbulenceModels chemistryModel ODE forces fvOptions sampling twoPhaseMixture triSurface fileFormats surfMesh compressibleTransportModels fluidThermophysicalModels radiationModels solidThermo thermophysicalProperties reactionThermophysicalModels specie solidSpecie finiteVolume lagrangian conversion dynamicFvMesh topoChangerFvMesh dynamicMesh extrudeModel meshTools utilityFunctionObjects OpenFOAM OSspecific Pstream)
libs=()
for name in "${names[@]}"; do
 lib=$b/platforms/$WM_OPTIONS/lib/lib$name.a
 test "$name" != OpenFOAM || lib=$d/lib/libOpenFOAM.a
 test "$name" != Pstream || lib=$b/platforms/$WM_OPTIONS/lib/mpi/libPstream.a
 test -s "$lib"
 libs+=("$lib")
 printf 'LINK_INPUT=%s\n' "$lib"
done
printf 'LINK_NATIVE_MPE_ONLY=%s\n' "$(date -Is)"
mpicxx -mdynamic -o "$d/bin/boussinesqFoam-x86LDU-MPE" -Wl,--whole-archive "$d/objects/boussinesqFoam.o" "${libs[@]}" -Wl,--no-whole-archive -lz -lm -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > "$d/compile-logs/link.log" 2>&1 || { grep -En -m12 'undefined reference|cannot find|error:' "$d/compile-logs/link.log" || true; exit 93; }
test -s "$d/bin/boussinesqFoam-x86LDU-MPE"
printf '\nCPE_SYMBOL_AUDIT\n'
if nm "$d/bin/boussinesqFoam-x86LDU-MPE" | grep -E 'athread_spawn|athread_join|CRTS_athread'; then echo UNEXPECTED_CPE_SYMBOLS; exit 94; fi
sha256sum "$s"/*.C "$s"/*.H "$d/bin/boussinesqFoam-x86LDU-MPE" "$d/lib/libOpenFOAM.a" > "$d/source-and-binary.sha256"
date -Is > "$d/BUILD_COMPLETE"
printf 'X86_LDU_MPE_BUILD_COMPLETE=1 AT=%s\n' "$(date -Is)"
