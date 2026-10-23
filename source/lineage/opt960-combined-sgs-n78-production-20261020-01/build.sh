#!/bin/bash
# LOCAL-REVIEW DRAFT. Three host TUs + reuse actual compiled CPE; no runtime.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-combined-sgs-n78-production-20261020-01
base=$r/build/opt960-sgs-retained-psi-n54-repaired-link-20261020-01
kernel=$r/build/opt960-combined-sgs-n78-kernel-20261020-01
n39=$r/build/opt960-discarded-coarse-skip-20261020-01
n38=$r/build/opt960-zero-initial-halo-20261020-01-repair-01
n23=$r/build/opt960-topology-match-integration-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
native=$r/build/opt960-native-coarse-dic-20261019-01-target-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
lib=$c/platforms/$WM_OPTIONS/lib
cd "$d";sha256sum -c SOURCES.sha256
grep -Fxq MAIN_SOURCE_AND_THREE_HEADER_OWNERS_REVIEWED=1 MAIN_APPROVAL.txt
grep -Fxq CPE_OBJECT_5C991_TARGET_REVIEWED=1 MAIN_APPROVAL.txt
check(){ printf '%s  %s\n' "$2" "$1"|sha256sum -c -; }
check "$base/lib/libdfMatrices.a" b995b3ea9356180c150228b3b158ba8a2df1e808cd7588c454a166c5634e2be4
check "$kernel/combined.o" 5c991ab61547fabb7bf85adc5b0d6704e704756ed9882a988a577a26571bef3d
check "$kernel/lnjCombinedSgsParam.h" 8c8684c03bdacc185393bf0113ae257701d3d1d3fd865892bc1a682d5b9db88c
check "$n38/src/CSRGAMGSolver.H" 252a7f533d349118a3609ad6dea79b7947efe7fc371c30466f97822cd7fca643
check "$n39/src/CSRGAMGSolverSolve.C" 74c5ac58a26b2c6f2fdacb678978cec6a59b40e20a28382ed4151a09ed3f73f0
check "$c/src/dfMatrices/csrMatrix/csrMatrix/csrMatrixSmoother.C" 65d09bbb083ff425f9c7378f1bf7b24b787895f35a1cbc148bf49161557f2ae5
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$native/lib/libOpenFOAM.a" 63beb30e8414c3cd0d032d6960d42e11de9d152209403ad996c2004268a38a76
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
mkdir BUILD_ONCE.guard obj lib bin log
phase(){
 local name=$1;shift;set +e;"$@" > "$d/log/$name.log" 2>&1
 local rc=$?;set -e;printf '%s_EXIT_CODE=%s\n' "$name" "$rc"|tee "$d/$name.EXIT_CODE"
 if test "$rc" != 0;then tail -n 40 "$d/log/$name.log";exit "$rc";fi
}
# Same n54/n39 host compile flags. Private header first in ALL three TUs.
host_flags=(-mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC)
includes=(-I"$d/src" -I"$kernel" -I"$n38/src" -I"$n23/src" -I"$n18/src"
 -I"$n16/src" -I"$old/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude"
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude"
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude"
 -I/usr/sw/mpi/mpi_20220608_SEA/include)
for unit in csrSymGaussSeidelSmoother CSRGAMGSolverSolve csrMatrixSmoother;do
 phase "COMPILE_$unit" mpicxx "${host_flags[@]}" "${includes[@]}" -c "$d/src/$unit.C" -o "$d/obj/$unit.o"
done
# REUSE already successful actual CPE. Never compile or link old tile alongside it.
cp -p "$kernel/combined.o" obj/tile.o
check obj/tile.o 5c991ab61547fabb7bf85adc5b0d6704e704756ed9882a988a577a26571bef3d
ar t "$base/lib/libdfMatrices.a" > MEMBERS.before
for member in csrSymGaussSeidelSmoother.o CSRGAMGSolverSolve.o csrMatrixSmoother.o tile.o;do
 test "$(grep -Fxc "$member" MEMBERS.before)" = 1
done
test "$(ar p "$base/lib/libdfMatrices.a" tile.o|sha256sum|cut -d' ' -f1)" = 7690cd4052a7608d40f6942d725eaedbc3b8ad77e7b7b00a780de9a759ee4afd
test "$(ar p "$base/lib/libdfMatrices.a" csrSymGaussSeidelSmoother.o|sha256sum|cut -d' ' -f1)" = f7091b7f29503ddfd72040ac3506413ba100889e3df68c3834ddea0307f2a158
nm -A -g "$base/lib/libdfMatrices.a" > log/base-symbols.txt
awk '$NF~/lnjRetainedPsiSgs_CPE$/&&$(NF-1)!="U"{if($1!~/:tile.o:/)exit 1;n++}END{if(n!=1)exit 1}' log/base-symbols.txt
awk '$NF~/lnjRetainedPsiSgs_CPE$/&&$(NF-1)=="U"{if($1!~/:csrSymGaussSeidelSmoother.o:/)exit 1;n++}END{if(n!=1)exit 1}' log/base-symbols.txt
cp -p "$base/lib/libdfMatrices.a" "$d/lib/libdfMatrices.a"
phase AR_REPLACE ar r "$d/lib/libdfMatrices.a" "$d/obj/csrSymGaussSeidelSmoother.o" "$d/obj/CSRGAMGSolverSolve.o" "$d/obj/csrMatrixSmoother.o" "$d/obj/tile.o"
phase AR_INDEX ar s "$d/lib/libdfMatrices.a"
ar t "$d/lib/libdfMatrices.a" > MEMBERS.after
cmp MEMBERS.before MEMBERS.after
for member in csrSymGaussSeidelSmoother.o CSRGAMGSolverSolve.o csrMatrixSmoother.o tile.o;do
 test "$(ar p "$d/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)" = "$(sha256sum "$d/obj/$member"|cut -d' ' -f1)"
done
# Check named critical members, NOT claim exhaustive all-member hash proof.
for member in CSRPCG.o CSRGAMGSolver.o CSRGAMGPreconditioner.o CSRGAMGSolverScale.o numeric-pack.o topology-match.o;do
 test "$(grep -Fxc "$member" MEMBERS.before)" = 1
 a=$(ar p "$base/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 b=$(ar p "$d/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$member" >> PRESERVED_KEY_MEMBERS.sha256
done
test "$(ar p "$d/lib/libdfMatrices.a" CSRPCG.o|sha256sum|cut -d' ' -f1)" = e155377088f35fcae3e344d8ab0e9b34809479fc652dc4e98422b6decde6582d
nm -A -g "$d/lib/libdfMatrices.a" > log/candidate-symbols.txt
if grep -E 'lnj(RetainedPsiSgs|BoundaryPsiSgs|DeferredCacheSgs|PermutationSgsTile)_CPE' log/candidate-symbols.txt;then echo OLD_TILE_SYMBOL_REMAINS;exit 94;fi
awk '$NF~/lnjCombinedSgs_CPE$/&&$(NF-1)!="U"{if($1!~/:tile.o:/)exit 1;n++}END{if(n!=1)exit 1}' log/candidate-symbols.txt
awk '$NF~/lnjCombinedSgs_CPE$/&&$(NF-1)=="U"{if($1!~/:csrSymGaussSeidelSmoother.o:/)exit 1;n++}END{if(n!=1)exit 1}' log/candidate-symbols.txt
out=$d/bin/boussinesqFoam-combined-sgs-n78
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
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > log/LINK.log 2>&1
rc=$?;set -e;printf 'LINK_EXIT_CODE=%s\n' "$rc" > LINK.EXIT_CODE
if test "$rc" != 0;then tail -n 40 log/LINK.log;exit "$rc";fi
nm "$out" > log/final-symbols.txt
nm -C "$out" > log/final-demangled.txt
if grep -E 'lnj(RetainedPsiSgs|BoundaryPsiSgs|DeferredCacheSgs|PermutationSgsTile)_CPE' log/final-symbols.txt;then exit 94;fi
if grep -Fq lnjExactPressure log/final-demangled.txt;then exit 94;fi
grep -Fq smoothFromZero log/final-demangled.txt
for symbol in lnjCombinedSgs_CPE lnjNumericPack_CPE lnjTopologyMatch_CPE;do
 awk -v s="$symbol" '$NF~(s"$")&&$(NF-1)!="U"{n++}END{exit n!=1}' log/final-symbols.txt
done
nm -u "$out" > log/final-undefined.txt
if grep -E '(_sw7mc_|sw7mc_exception)' log/final-undefined.txt;then exit 94;fi
readelf -SW "$out" > log/final-sections.txt
readelf -SW obj/tile.o > log/combined-object-sections.txt
printf 'NEW_CPE_LDM_ABI_REQUIRES_MAIN_REVIEW=1\nNO_OLD_84712_TLS40_ASSERT=1\n' > SECTIONS_REVIEW_REQUIRED.txt
sha256sum "$out" "$d/lib/libdfMatrices.a" obj/*.o "$entry" "$plugin" "$native/lib/libOpenFOAM.a" > BUILD_OUTPUTS.sha256
check "$base/lib/libdfMatrices.a" b995b3ea9356180c150228b3b158ba8a2df1e808cd7588c454a166c5634e2be4
echo N78_COMBINED_BUILD_COMPLETE_REQUIRES_SECTION_HARDWARE_HELP_AND_FIELDS_REVIEW > BUILD_COMPLETE

