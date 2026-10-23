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
test "$(readlink -f "$d")" = "$d"
test -s "$d/snapshot/applications/solvers/multiphase/boussinesqFoam/boussinesqFoam.C"
mkdir "$d/build-once.guard"
mkdir "$d/objects" "$d/lib" "$d/bin" "$d/compile-logs"
date -Is > "$d/BUILD_STARTED"
g=$d/snapshot/src/OpenFOAM/matrices/lduMatrix/solvers/GAMG
s=$d/snapshot/applications/solvers/multiphase/boussinesqFoam
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -std=c++14 -ftemplate-depth-100 -fPIC -O3 -I"$b/src/OpenFOAM/lnInclude" -I"$b/src/OSspecific/POSIX/lnInclude" -I/usr/sw/mpi/mpi_20220608_SEA/include)
printf 'UNMODIFIED_X86_SOURCE_SNAPSHOT=%s\n' "$d/source.tar.gz"
printf 'PLATFORM_ADAPTATION=SW64 MPE compiler and label64 platform headers; baseline POSIX directory compatibility\n'
printf 'NATIVE_GAMG=Recompile all GAMGSolver translation units with unchanged x86 header, no added profiling\n'
cp -p "$b/platforms/$WM_OPTIONS/lib/libOpenFOAM.a" "$d/lib/libOpenFOAM.a"
pids=(); objects=()
for f in "$g"/*.C; do
 name=${f##*/}; name=${name%.C}
 case "$name" in GAMGSolverSolve_opt) continue;; esac
 test "$(ar t "$d/lib/libOpenFOAM.a" | grep -cx "$name.o")" = 1
 obj=$d/objects/$name.o
 printf 'COMPILE_NATIVE_GAMG=%s AT=%s\n' "$name" "$(date -Is)"
 mpicxx "${flags[@]}" -I"$g" -c "$f" -o "$obj" > "$d/compile-logs/$name.log" 2>&1 &
 pids+=("$!"); objects+=("$obj")
 if test "${#pids[@]}" = 4; then
  failed=0; for pid in "${pids[@]}"; do wait "$pid" || failed=1; done
  test "$failed" = 0 || { grep -En -m5 'error:|fatal error:' "$d/compile-logs/"*.log || true; exit 91; }
  pids=()
 fi
done
failed=0; for pid in "${pids[@]}"; do wait "$pid" || failed=1; done
test "$failed" = 0 || { grep -En -m5 'error:|fatal error:' "$d/compile-logs/"*.log || true; exit 91; }
test "${#objects[@]}" -ge 6
for obj in "${objects[@]}"; do test -s "$obj"; done
ar r "$d/lib/libOpenFOAM.a" "${objects[@]}"
ar s "$d/lib/libOpenFOAM.a"
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
