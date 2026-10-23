#!/bin/bash
# Repair only existing tile.o; retain SGS and every other host member. Link ONLY, no runtime.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-sgs-retained-psi-n54-repaired-link-20261020-01
compiled=$r/build/opt960-sgs-retained-psi-n54-compile-20261020-01
base=$r/build/opt960-sgs-retained-psi-n54-link-20261020-01
repaired=$r/build/opt960-sgs-retained-psi-n54-repair-20261020-01
n39=$r/build/opt960-discarded-coarse-skip-20261020-01
n38=$r/build/opt960-zero-initial-halo-20261020-01-repair-01
native=$r/build/opt960-native-coarse-dic-20261019-01-target-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
lib=$c/platforms/$WM_OPTIONS/lib
cd "$d";sha256sum -c SOURCES.sha256
# Actual n54 object pins supplied by main after compile session39773 exit0.
# Final linked binary/archive pins remain unknown until actual link/read.
sgs_sha=f7091b7f29503ddfd72040ac3506413ba100889e3df68c3834ddea0307f2a158
cpe_sha=7690cd4052a7608d40f6942d725eaedbc3b8ad77e7b7b00a780de9a759ee4afd
pcg_sha=e155377088f35fcae3e344d8ab0e9b34809479fc652dc4e98422b6decde6582d
check(){ printf '%s  %s\n' "$2" "$1"|sha256sum -c -; }
test -f "$compiled/OBJECTS_COMPLETE"
grep -Fxq COMPILE_SGS_EXIT_CODE=0 "$compiled/COMPILE_SGS.EXIT_CODE"
grep -Fxq COMPILE_CPE_EXIT_CODE=0 "$compiled/COMPILE_CPE.EXIT_CODE"
test -f "$n39/DISCARDED_COARSE_SKIP_BUILD_COMPLETE"
test -f "$base/N54_RETAINED_PSI_LINK_COMPLETE"
grep -Fxq N54_WAIT_PRECEDENCE_REPAIR_BUILT_ONLY_NOT_RUN "$repaired/BUILD_COMPLETE"
grep -Fxq COMPILE_CPE_EXIT_CODE=0 "$repaired/COMPILE_CPE.EXIT_CODE"
grep -Fxq LINK_EXIT_CODE=0 "$repaired/LINK.EXIT_CODE"
check "$base/lib/libdfMatrices.a" 8f0f589626eec767e8f58b252236a6ffbaf88a94c50ab9a085b89ed5d291fd30
test -f "$native/NATIVE_COARSE_DIC_BUILD_COMPLETE"
check "$compiled/src/csrSymGaussSeidelSmoother.C" a4817911c1ac72614886f768445e0b1a62ab4277cc086b10734d0da51e2c858a
check "$repaired/src/lnjRetainedPsiSgs_CPE.c" 28c524a2ca7a53ea45052b4c2b08f5bb15111019108bf06892510bc687962577
check "$compiled/obj/csrSymGaussSeidelSmoother.o" "$sgs_sha"
check "$repaired/obj/retained-psi-repaired.o" "$cpe_sha"
check "$n39/lib/libdfMatrices.a" d8c6f460734bacc3ac7bbc81585e9ba5490750a0908b2c527116de919e6f3771
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$n38/src/CSRGAMGSolver.H" 252a7f533d349118a3609ad6dea79b7947efe7fc371c30466f97822cd7fca643
check "$lib/libOpenFOAM.a" 83f65d705d3642a43889824068e7f4259618522eeac64c6b5899855a9802e56a
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check "$native/lib/libOpenFOAM.a" 63beb30e8414c3cd0d032d6960d42e11de9d152209403ad996c2004268a38a76
check "$native/obj/GAMGSolverSolve.o" 3bd3278472cde337c5d1741298ff9eba67a3bc31a94913b226c466b0c6edfdbb
check "$native/src/GAMGSolverSolve.C" c6638f86cb466cdf0b3fbc6f6e161f479117f9b7690bc7d6ebeeab2d08ce086f
cmp "$native/MEMBERS.before" "$native/MEMBERS.after"
test "$(ar p "$native/lib/libOpenFOAM.a" GAMGSolverSolve.o|sha256sum|cut -d' ' -f1)" = 3bd3278472cde337c5d1741298ff9eba67a3bc31a94913b226c466b0c6edfdbb
test "$(ar p "$n39/lib/libdfMatrices.a" CSRPCG.o|sha256sum|cut -d' ' -f1)" = "$pcg_sha"
mkdir link-once.guard lib bin log obj
ar t "$base/lib/libdfMatrices.a" > MEMBERS.before
for member in csrSymGaussSeidelSmoother.o tile.o;do
 test "$(grep -Fxc "$member" MEMBERS.before)" = 1
done
test "$(ar p "$base/lib/libdfMatrices.a" tile.o|sha256sum|cut -d' ' -f1)" = d87e639cc3286cdac4f2518f63f0fdc6a9162018b044f2cf7ba4a167bf5c52c3
test "$(ar p "$base/lib/libdfMatrices.a" csrSymGaussSeidelSmoother.o|sha256sum|cut -d' ' -f1)" = "$sgs_sha"
# Verify existing current-symbol ownership, not the obsolete pre-n54 symbol.
nm -A -g "$base/lib/libdfMatrices.a" > log/base-global-symbols.txt
if grep -Fq lnjPermutationSgsTile_CPE log/base-global-symbols.txt;then echo OLD_TILE_IN_BASE;exit 94;fi
awk '$NF ~ /lnjRetainedPsiSgs_CPE$/ && $(NF-1)!="U" {if($1 !~ /:tile.o:/)exit 1;n++} END{if(n!=1)exit 1}' log/base-global-symbols.txt
awk '$NF ~ /lnjRetainedPsiSgs_CPE$/ && $(NF-1)=="U" {if($1 !~ /:csrSymGaussSeidelSmoother.o:/)exit 1;n++} END{if(n!=1)exit 1}' log/base-global-symbols.txt
# Rename only a private copy of the hardware-validated repair to existing member.
cp -p "$repaired/obj/retained-psi-repaired.o" obj/tile.o
check obj/tile.o "$cpe_sha"
cp -p "$base/lib/libdfMatrices.a" "$d/lib/libdfMatrices.a"
ar r "$d/lib/libdfMatrices.a" "$d/obj/tile.o"
ar s "$d/lib/libdfMatrices.a"
ar t "$d/lib/libdfMatrices.a" > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
test "$(ar p "$d/lib/libdfMatrices.a" tile.o|sha256sum|cut -d' ' -f1)" = "$cpe_sha"
test "$(ar p "$d/lib/libdfMatrices.a" csrSymGaussSeidelSmoother.o|sha256sum|cut -d' ' -f1)" = "$sgs_sha"
if grep -Exq '(retained-psi|retained-psi-repaired)[.]o' MEMBERS.after;then echo DUPLICATE_CPE_MEMBER;exit 94;fi
nm -A -g "$d/lib/libdfMatrices.a" > log/candidate-global-symbols.txt
if grep -Fq lnjPermutationSgsTile_CPE log/candidate-global-symbols.txt;then echo OLD_TILE_REMAINS;exit 94;fi
awk '$NF ~ /lnjRetainedPsiSgs_CPE$/ && $(NF-1)!="U" {if($1 !~ /:tile.o:/)exit 1;n++} END{if(n!=1)exit 1}' log/candidate-global-symbols.txt
awk '$NF ~ /lnjRetainedPsiSgs_CPE$/ && $(NF-1)=="U" {if($1 !~ /:csrSymGaussSeidelSmoother.o:/)exit 1;n++} END{if(n!=1)exit 1}' log/candidate-global-symbols.txt
# Named critical members only. No all-member ar-p/hash traversal.
for member in csrSymGaussSeidelSmoother.o CSRPCG.o CSRGAMGSolver.o CSRGAMGSolverSolve.o CSRGAMGPreconditioner.o CSRGAMGSolverScale.o numeric-pack.o topology-match.o;do
 test "$(grep -Fxc "$member" MEMBERS.before)" = 1
 a=$(ar p "$base/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 b=$(ar p "$d/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$member" >> PRESERVED_KEY_MEMBERS.sha256
done
test "$(ar p "$d/lib/libdfMatrices.a" CSRPCG.o|sha256sum|cut -d' ' -f1)" = "$pcg_sha"
out=$d/bin/boussinesqFoam-sgs-retained-psi-n54-repaired
set +e
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" "$plugin" \
 -L"$d/lib" -L"$lib" -L"$lib/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects "$native/lib/libOpenFOAM.a" -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi \
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread \
 > "$d/link.log" 2>&1
rc=$?;set -e;printf '%s\n' "$rc" > "$d/LINK_EXIT_CODE"
if test "$rc" != 0;then tail -n 40 "$d/link.log";exit "$rc";fi
test -s "$out"
nm -C "$out" > log/final-demangled.txt
if grep -Fq lnjExactPressure log/final-demangled.txt;then echo UNEXPECTED_N29_ENTRY_HELPER;exit 94;fi
grep -Fq smoothFromZero log/final-demangled.txt
nm "$out" > log/final-symbols.txt
if grep -Fq lnjPermutationSgsTile_CPE log/final-symbols.txt;then echo OLD_TILE_LINKED;exit 94;fi
for symbol in lnjRetainedPsiSgs_CPE lnjNumericPack_CPE lnjTopologyMatch_CPE;do
 awk -v s="$symbol" '$NF ~ (s"$")&&$(NF-1)!="U"{n++}END{exit n!=1}' log/final-symbols.txt
done
nm -u "$out" > log/final-undefined.txt
if grep -E '(_sw7mc_|sw7mc_exception)' log/final-undefined.txt;then echo UNRESOLVED_VENDOR_MATH;exit 94;fi
readelf -SW "$out" > log/final-sections.txt
ldm=$(awk '{for(i=1;i<=NF;i++)if($i==".ldm")print $(i+4)}' log/final-sections.txt)
tls=$(awk '{for(i=1;i<=NF;i++)if($i==".slave_tls")print $(i+4)}' log/final-sections.txt)
# Same120-byte ABI/TLS source arrays; record actual linked sections, no invented cap.
test -n "$ldm";test -n "$tls"
printf 'LDM_HEX=%s\nSLAVE_TLS_HEX=%s\nREQUIRES_MAIN_SECTION_REVIEW=1\n' "$ldm" "$tls" > SECTIONS_REVIEW_REQUIRED.txt
readelf -SW "$repaired/obj/retained-psi-repaired.o" > log/candidate-cpe-sections.txt
check "$n39/lib/libdfMatrices.a" d8c6f460734bacc3ac7bbc81585e9ba5490750a0908b2c527116de919e6f3771
check "$base/lib/libdfMatrices.a" 8f0f589626eec767e8f58b252236a6ffbaf88a94c50ab9a085b89ed5d291fd30
sha256sum "$out" "$d/lib/libdfMatrices.a" "$compiled/obj/csrSymGaussSeidelSmoother.o" "$repaired/obj/retained-psi-repaired.o" "$entry" "$plugin" "$native/lib/libOpenFOAM.a" > BUILD_OUTPUTS.sha256
printf 'SGS_OBJECT_SHA=%s\nCPE_OBJECT_SHA=%s\nORIGINAL_N39_CSRPCG_SHA=%s\nNEW_BINARY_SHA=SEE_BUILD_OUTPUTS\n' "$sgs_sha" "$cpe_sha" "$pcg_sha" > ACTUAL_OBJECT_PINS.txt
date -Is > N54_REPAIRED_RETAINED_PSI_LINK_COMPLETE
echo N54_REPAIRED_RETAINED_PSI_LINK_COMPLETE_REQUIRES_SECTION_MPI_STARTUP_FIELD_GATES
