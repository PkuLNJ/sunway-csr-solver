#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
w=$r/build/amr-sqr-wave-aligned-20261024-r1
m=$r/build/top-math-isolate-20261024-01/strong-r1
wave=$r/build/amr-wave-aligned-20261024-r1
test "$(cat "$m/MATH_JOB_EXIT_CODE")" = 0
test "$(cat "$m/native-core.EXIT_CODE")" = 0
test "$(cat "$m/csr-core.EXIT_CODE")" = 2
test "$(cat "$m/csr-fixed.EXIT_CODE")" = 0
grep -q 'MATH_TEST_COMPLETE checks=447678 failures=0' "$m/csr-fixed.runtime.log"
test "$(sha256sum "$m/SqrVectorFieldVerified.o" | cut -d' ' -f1)" = 7c205a91419cff7061f3c7fc5d7a810e91ae568934802f2f1fadb3148f588949
mkdir "$w";mkdir "$w/lib" "$w/obj" "$w/bin" "$w/log" "$w/src"
cp -p "$m/SqrVectorFieldVerified.o" "$w/obj/"
cp -p "$m/SqrVectorFieldVerified.C" "$w/src/"
cp -p "$m/FIX_FLAGS.txt" "$m/FIX_STRONG_SYMBOLS.txt" "$m/OUTPUTS.sha256" "$w/"
cp -p "$wave/lib/libseaWave.a" "$w/lib/"
cmp "$w/lib/libseaWave.a" "$wave/lib/libseaWave.a"
sha256sum "$w/obj/SqrVectorFieldVerified.o" "$w/lib/libseaWave.a" "$d/bin/boussinesqFoam-capture" > "$w/INPUTS.sha256"
lib=$c/platforms/$WM_OPTIONS/lib
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
mpicxx -mdynamic -o "$w/bin/boussinesqFoam-capture-wave-sqr-aligned" -Wl,--trace -Wl,-Map,"$w/log/link.map" -Wl,--wrap=main -Wl,--wrap=MPI_Init_thread -Wl,--wrap=MPI_Init -Wl,--whole-archive "$root/MainExit.o" "$w/obj/SqrVectorFieldVerified.o" "$d/obj/boussinesqFoam.o" "$d/obj/AmrCapture.o" "$d/obj/CaptureWriter.o" "$plugin" \
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
grep -F "$w/obj/SqrVectorFieldVerified.o" "$w/log/link.log"
sha256sum "$w/bin/boussinesqFoam-capture-wave-sqr-aligned" "$w/lib/libseaWave.a" "$w/obj/SqrVectorFieldVerified.o" > "$w/OUTPUTS.sha256"
printf 'CHANGED_VS_WAVE_ALIGNED=one strong Field sqr override\nREUSED_MATH_VERIFIED_OBJECT=1\nNO_SOLVER_RECOMPILE=1\n' > "$w/CHANGE.txt"
echo SQR_WAVE_ALIGNED_LINK_COMPLETE
