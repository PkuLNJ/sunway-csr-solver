#!/bin/bash
# Compile/link only. Preserve original n14 archive/plugin/entry and old tiny6.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0;test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-coarse-rank-research-20261019-01
z=$r/build/opt960-coarse-csr-bridge12-20261019-01
lib=$c/platforms/$WM_OPTIONS/lib
test -f "$d/BUILD_COMPLETE"
cmp "$z/N14_BUILD_OUTPUTS_PINNED.sha256" "$d/BUILD_OUTPUTS.sha256"
test "$(sha256sum "$d/obj/lateSixCSRGAMGProcAgglomeration.o" | cut -d' ' -f1)" = 75e0993d605935314216a11bfb4e0f56185b4b4c004eb3311fe79e77eb80f6de
(cd "$z";sha256sum -c SOURCES.sha256)
mkdir "$z/build-once.guard" "$z/obj" "$z/bin" "$z/log" "$z/lib"
cd "$z"
flags=(-mhost -msimd -ffp-contract=off -mieee -mftz -fpermissive -g -O2
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -DNoRepository
 -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC
 -I"$z" -I"$d/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude"
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude"
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude"
 -I/usr/sw/mpi/mpi_20220608_SEA/include)
for unit in CsrPrimitiveMeshBridge CSRGAMGAgglomerateLduAddressing tiny12;do
 set +e
 mpicxx "${flags[@]}" -c "$z/$unit.C" -o "$z/obj/$unit.o" > "$z/log/$unit.compile.log" 2>&1
 rc=$?;set -e
 printf '%s\n' "$rc" > "$z/$unit.COMPILE_EXIT_CODE"
 if test "$rc" != 0;then tail -n 45 "$z/log/$unit.compile.log";exit "$rc";fi
done
printf '0\n' > "$z/COMPILE_EXIT_CODE"
ar t "$d/lib/libdfMatrices.a" > "$z/MEMBERS.before"
test "$(grep -Fxc CSRGAMGAgglomerateLduAddressing.o "$z/MEMBERS.before")" = 1
test "$(grep -Fxc CsrPrimitiveMeshBridge.o "$z/MEMBERS.before" || true)" = 0
ar p "$d/lib/libdfMatrices.a" CSRGAMGAgglomerateLduAddressing.o | sha256sum > "$z/OLD_ADDRESSING_MEMBER.sha256"
# Copy immutable existing archive, then ONLY replace addressing/add private helper.
cp -p "$d/lib/libdfMatrices.a" "$z/lib/libdfMatrices.a"
ar r "$z/lib/libdfMatrices.a" "$z/obj/CSRGAMGAgglomerateLduAddressing.o" "$z/obj/CsrPrimitiveMeshBridge.o"
ar s "$z/lib/libdfMatrices.a"
ar t "$z/lib/libdfMatrices.a" > "$z/MEMBERS.after"
test "$(grep -Fxc CsrPrimitiveMeshBridge.o "$z/MEMBERS.after")" = 1
grep -Fvx CsrPrimitiveMeshBridge.o "$z/MEMBERS.after" > "$z/MEMBERS.after.originals"
cmp "$z/MEMBERS.before" "$z/MEMBERS.after.originals"
for unit in CSRGAMGSolver.o CSRGAMGSolverSolve.o CSRGAMGSolverAgglomerateMatrix.o CSRGAMGPreconditioner.o;do
 a=$(ar p "$d/lib/libdfMatrices.a" "$unit" | sha256sum | cut -d' ' -f1)
 b=$(ar p "$z/lib/libdfMatrices.a" "$unit" | sha256sum | cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$unit" >> "$z/PRESERVED_CORE_MEMBERS.sha256"
done
printf '0\n' > "$z/ARCHIVE_EDIT_EXIT_CODE"
out=$z/bin/tiny12
set +e
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$z/obj/tiny12.o" \
 "$d/obj/lateSixCSRGAMGProcAgglomeration.o" \
 -L"$z/lib" -L"$lib" -L"$lib/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects -lOpenFOAM -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi \
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > "$z/log/link.log" 2>&1
rc=$?;set -e;printf '%s\n' "$rc" > "$z/LINK_EXIT_CODE"
if test "$rc" != 0;then tail -n 45 "$z/log/link.log";exit "$rc";fi
cmp "$z/N14_BUILD_OUTPUTS_PINNED.sha256" "$d/BUILD_OUTPUTS.sha256"
sha256sum "$out" "$z/lib/libdfMatrices.a" "$d/obj/lateSixCSRGAMGProcAgglomeration.o" > "$z/BUILD_OUTPUTS.sha256"
date -Is > "$z/BUILD_COMPLETE"
echo CSR_BRIDGE12_BUILD_COMPLETE_NO_JOB_SUBMITTED
