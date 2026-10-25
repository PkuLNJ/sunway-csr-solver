#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
w=$r/build/amr-wave-aligned-20261024-r1
mkdir "$w";mkdir "$w/lib" "$w/obj" "$w/bin" "$w/log"
native=/home/export/online1/mdt00/shisuan/swpku_of/xyc/baseline/OpenFOAM-7/platforms/$WM_OPTIONS/lib/libseaWave.a
lib=$c/platforms/$WM_OPTIONS/lib
sha256sum "$native" "$lib/libseaWave.a" "$d/bin/boussinesqFoam-capture" > "$w/INPUTS.sha256"
ar p "$native" seaWaveModel.o > "$w/obj/seaWaveModel.o"
test "$(sha256sum "$w/obj/seaWaveModel.o" | cut -d' ' -f1)" = dc174f381e2e07f250e7a4370d39a39719b2499016b01b11044c741f4111cfe5
cp -p "$lib/libseaWave.a" "$w/lib/libseaWave.a"
ar r "$w/lib/libseaWave.a" "$w/obj/seaWaveModel.o";ar s "$w/lib/libseaWave.a"
while read -r member;do
 before=$(ar p "$lib/libseaWave.a" "$member" | sha256sum | cut -d' ' -f1)
 after=$(ar p "$w/lib/libseaWave.a" "$member" | sha256sum | cut -d' ' -f1)
 printf '%s %s %s\n' "$member" "$before" "$after" >> "$w/MEMBERS.txt"
 if test "$member" != seaWaveModel.o;then test "$before" = "$after";fi
done < <(ar t "$lib/libseaWave.a")
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
mpicxx -mdynamic -o "$w/bin/boussinesqFoam-capture-wave-aligned" -Wl,--trace -Wl,--wrap=main -Wl,--wrap=MPI_Init_thread -Wl,--wrap=MPI_Init -Wl,--whole-archive "$root/MainExit.o" "$d/obj/boussinesqFoam.o" "$d/obj/AmrCapture.o" "$d/obj/CaptureWriter.o" "$plugin" \
 -L"$w/lib" -L"$d/lib" -L"$lib" -L"$lib/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects "$d/lib/libOpenFOAM.a" -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > "$w/log/link.log" 2>&1
grep -F "$w/lib/libseaWave.a" "$w/log/link.log"
sha256sum "$w/bin/boussinesqFoam-capture-wave-aligned" "$w/lib/libseaWave.a" > "$w/OUTPUTS.sha256"
printf 'ONLY_CHANGED_MEMBER=libseaWave.a(seaWaveModel.o)\nNO_SOLVER_RECOMPILE=1\n' > "$w/CHANGE.txt"
echo WAVE_ALIGNED_LINK_COMPLETE
