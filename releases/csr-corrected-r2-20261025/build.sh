#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
rc=$?;set -euo pipefail;test "$rc" = 0
w=$r/build/amr-scalar-cache-aligned-20261024-r2
root=$r/build/amr-firstdiv-20261023-r1
d=$root/csr
fixed=$r/build/amr-sqr-wave-aligned-20261024-r1
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
appref=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
cd "$w";mkdir BUILD_ONCE.guard;mkdir obj bin log
sha256sum -c STAGED.sha256 > log/STAGED_VERIFIED.txt
test "$(sha256sum "$fixed/obj/SqrVectorFieldVerified.o"|cut -d' ' -f1)" = 7c205a91419cff7061f3c7fc5d7a810e91ae568934802f2f1fadb3148f588949
test -e "$fixed/FINAL_SQR_CODE_VERIFIED.json"
sha256sum -c "$fixed/OUTPUTS.sha256" > log/PARENT_OUTPUTS_VERIFIED.txt
cp -p "$fixed/OUTPUTS.sha256" PARENT_OUTPUTS.sha256
cp -p "$fixed/FINAL_SQR_CODE_VERIFIED.json" PARENT_SQR_CODE_VERIFIED.json
sha256sum "$root/MainExit.o" "$plugin" "$d/obj/boussinesqFoam.o" "$d/obj/AmrCapture.o" "$d/obj/CaptureWriter.o" "$fixed/obj/SqrVectorFieldVerified.o" "$fixed/lib/libseaWave.a" "$d/lib/"*.a > REUSED_INPUTS.sha256
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -O3 -std=c++14 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository -ftemplate-depth-100 -fPIC)
incs=(-I"$w/src" -I"$d/src" -I"$root/adapter" -I"$root/io" -I"$appref" -I"$appref/../VoF" -I"$c/applications/solvers/multiphase/VoF" -I"$c/src/transportModels" -I"$c/src/finiteVolume/cfdTools" -I"$c/src/Pstream/mpi" -I/usr/sw/mpi/mpi_20220608_SEA/include)
for rel in transportModels/twoPhaseMixture transportModels/incompressible transportModels/interfaceProperties TurbulenceModels/turbulenceModels TurbulenceModels/incompressible transportModels/immiscibleIncompressibleTwoPhaseMixture finiteVolume parallel/decompose/decompositionMethods mesh/snappyHexMesh fileFormats triSurface dynamicFvMesh meshTools dynamicMesh renumber/renumberMethods renumber/zoltanRenumber dfRenumber dfSchedule dfUtils dfMatrices dfGenMatrix waves OpenFOAM OSspecific/POSIX;do incs+=(-I"$c/src/$rel/lnInclude");done
printf '%q ' mpicxx "${flags[@]}" "${incs[@]}" -c "$w/src/boussinesqFoam.C" -o "$w/obj/boussinesqFoam.o" > log/compile.command
mpicxx "${flags[@]}" "${incs[@]}" -c "$w/src/boussinesqFoam.C" -o "$w/obj/boussinesqFoam.o" > log/compile.log 2>&1
printf 'ENTRY_COMPILE_RC=0\n' > COMPILE_EXIT_CODE
lib=$c/platforms/$WM_OPTIONS/lib
mpicxx -mdynamic -o "$w/bin/boussinesqFoam-capture-wave-sqr-scalar-aligned" -Wl,--trace -Wl,-Map,"$w/log/link.map" -Wl,--wrap=main -Wl,--wrap=MPI_Init_thread -Wl,--wrap=MPI_Init -Wl,--whole-archive "$root/MainExit.o" "$fixed/obj/SqrVectorFieldVerified.o" "$w/obj/boussinesqFoam.o" "$d/obj/AmrCapture.o" "$d/obj/CaptureWriter.o" "$plugin" \
 -L"$fixed/lib" -L"$d/lib" -L"$lib" -L"$lib/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects "$d/lib/libOpenFOAM.a" -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > "$w/log/link.log" 2>&1
printf 'ENTRY_LINK_RC=0\n' > LINK_EXIT_CODE
grep -F "$fixed/lib/libseaWave.a" log/link.log
grep -F "$fixed/obj/SqrVectorFieldVerified.o" log/link.log
sha256sum bin/boussinesqFoam-capture-wave-sqr-scalar-aligned obj/boussinesqFoam.o > OUTPUTS.sha256
python verify_code.py
sha256sum -c REUSED_INPUTS.sha256 > log/REUSED_INPUTS_STILL_UNCHANGED.txt
printf 'PRIVATE_R2_COMPILE_LINK_AND_SQR_CODE_VERIFIED_NO_FLOW_SUBMITTED\n' > BUILD_COMPLETE
cat BUILD_COMPLETE
