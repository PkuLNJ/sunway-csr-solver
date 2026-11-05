#!/bin/bash
set -eo pipefail
d=$(cd "$(dirname "$0")" && pwd)
b=$d/OpenFOAM-7
source "$d/../environment.sh" "$b"
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
cd "$d";mkdir BUILD_ONCE.guard objects lib bin logs
s=$d/source/applications/solvers/multiphase/boussinesqFoam
wave=$d/source/src/seaWave
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -O3 -std=c++14 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC)
incs=(-I"$s" -I"$d/source" -I"$wave/lnInclude" -I"$d/source/applications/solvers/multiphase/VoF" -I"$b/src/transportModels" -I"$b/src/finiteVolume/cfdTools" -I"$b/src/Pstream/mpi" -I/usr/sw/mpi/mpi_20220608_SEA/include)
for rel in transportModels/twoPhaseMixture transportModels/incompressible transportModels/interfaceProperties transportModels/immiscibleIncompressibleTwoPhaseMixture TurbulenceModels/turbulenceModels TurbulenceModels/incompressible finiteVolume dynamicFvMesh meshTools sampling thermophysicalModels/basic parallel/decompose/decompositionMethods fileFormats triSurface surfMesh dynamicMesh fvOptions OpenFOAM OSspecific/POSIX;do incs+=(-I"$b/src/$rel/lnInclude");done
mkdir -p "$wave/lnInclude"
find "$wave" -type f \( -name '*.H' -o -name '*.C' \) -exec ln -sfn '{}' "$wave/lnInclude/" \;
while read -r unit;do
 unit=${unit%$'\r'}
 case "$unit" in *.C)
 name=$(basename "$unit" .C)
 echo "COMPILE_WAVE=$unit"
 mpicxx "${flags[@]}" "${incs[@]}" -c "$wave/$unit" -o "objects/wave_$name.o" > "logs/wave_$name.log" 2>&1
 ;;esac
done < "$wave/Make/files"
ar rcs lib/libseaWave.a objects/wave_*.o
echo COMPILE_ENTRY
mpicxx "${flags[@]}" "${incs[@]}" -c "$s/boussinesqFoam.C" -o objects/boussinesqFoam.o > logs/entry.log 2>&1
names=(seaWave atmosphericModels immiscibleIncompressibleTwoPhaseMixture interfaceProperties twoPhaseProperties incompressibleTransportModels fvMotionSolvers fieldFunctionObjects incompressibleTurbulenceModels combustionModels turbulenceModels compressibleTurbulenceModels chemistryModel ODE forces fvOptions sampling twoPhaseMixture triSurface fileFormats surfMesh compressibleTransportModels fluidThermophysicalModels radiationModels solidThermo thermophysicalProperties reactionThermophysicalModels specie solidSpecie finiteVolume lagrangian conversion dynamicFvMesh topoChangerFvMesh dynamicMesh extrudeModel meshTools utilityFunctionObjects OpenFOAM OSspecific Pstream)
libs=()
for name in "${names[@]}";do
 lib=$b/platforms/$WM_OPTIONS/lib/lib$name.a
 test "$name" != OpenFOAM || lib=$b/platforms/$WM_OPTIONS/lib/libOpenFOAM.a
 test "$name" != seaWave || lib=$d/lib/libseaWave.a
 test "$name" != Pstream || lib=$b/platforms/$WM_OPTIONS/lib/mpi/libPstream.a
 test -s "$lib";libs+=("$lib")
done
math=/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/lib/libm_gy.a
printf '%s\n' "${libs[@]}" "$math" > LINK_INPUTS.txt
sha256sum "${libs[@]}" "$math" > LINK_INPUTS.sha256
mpicxx -mdynamic -o bin/boussinesqFoam-LDU -Wl,--whole-archive objects/boussinesqFoam.o "${libs[@]}" "$math" -lz -lm -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > logs/link.log 2>&1
sha256sum bin/* lib/* objects/* > OUTPUTS.sha256
nm bin/boussinesqFoam-LDU | grep -E 'athread_spawn|athread_join|CRTS_athread' > CPE_SYMBOLS.txt || true
test -s bin/boussinesqFoam-LDU
date -Is > BUILD_COMPLETE
