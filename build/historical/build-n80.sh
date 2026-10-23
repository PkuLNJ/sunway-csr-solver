#!/bin/bash
# One host translation unit only; original n78 class ABI/kernel stay unchanged.
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?;set -euo pipefail;test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
d=$r/build/opt960-rhs-writeset-repair-n80-20261020-01
base=$r/build/opt960-combined-sgs-n78-production-20261020-01
kernel=$r/build/opt960-combined-sgs-n78-kernel-20261020-01
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
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
check "$base/lib/libdfMatrices.a" 92671508a15c70e9a310edbc4e856d127e28a6c60174beed10a80398915d04c8
check "$base/src/csrSymGaussSeidelSmoother.C" 8f58c737c9c60083bad3a12a589aa9b65afd2ab38d9aa81dda02c1bfc23bc392
check "$base/src/csrSymGaussSeidelSmoother.H" 43a50c33471d1b8ccd37e4fd025c81b52d8b4193cd24ecac63eef71667ce8e61
check "$entry" e5edd01a92a82710d82322693faf2d102311b8267b77d567c3470699807abcab
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$native/lib/libOpenFOAM.a" 63beb30e8414c3cd0d032d6960d42e11de9d152209403ad996c2004268a38a76
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
grep -Fxq MAIN_ONE_PREDICATE_WRITESET_REPAIR_REVIEWED=1 MAIN_APPROVAL.txt
mkdir BUILD_ONCE.guard obj lib bin log
host_flags=(-mhost -msimd -mieee -mftz -ffp-contract=off -fpermissive -g -O3
 -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64
 -DNoRepository -D_PROFILING_ -std=c++17 -ftemplate-depth-100 -fPIC)
includes=(-I"$base/src" -I"$kernel" -I"$n38/src" -I"$n23/src" -I"$n18/src"
 -I"$n16/src" -I"$old/src" -I"$c/src/dfMatrices/lnInclude" -I"$c/src/dfUtils/lnInclude"
 -I"$c/src/finiteVolume/lnInclude" -I"$c/src/meshTools/lnInclude"
 -I"$c/src/OpenFOAM/lnInclude" -I"$c/src/OSspecific/POSIX/lnInclude"
 -I/usr/sw/mpi/mpi_20220608_SEA/include)
set +e
mpicxx "${host_flags[@]}" "${includes[@]}" -c src/csrSymGaussSeidelSmoother.C -o obj/csrSymGaussSeidelSmoother.o > log/COMPILE.log 2>&1
rc=$?;set -e;printf 'COMPILE_EXIT_CODE=%s\n' "$rc" > COMPILE.EXIT_CODE
if test "$rc" != 0;then tail -n 40 log/COMPILE.log;exit "$rc";fi
ar t "$base/lib/libdfMatrices.a" > MEMBERS.before
test "$(grep -Fxc csrSymGaussSeidelSmoother.o MEMBERS.before)" = 1
cp -p "$base/lib/libdfMatrices.a" lib/libdfMatrices.a
ar r lib/libdfMatrices.a obj/csrSymGaussSeidelSmoother.o
ar s lib/libdfMatrices.a
ar t lib/libdfMatrices.a > MEMBERS.after;cmp MEMBERS.before MEMBERS.after
test "$(ar p lib/libdfMatrices.a csrSymGaussSeidelSmoother.o|sha256sum|cut -d' ' -f1)" = "$(sha256sum obj/csrSymGaussSeidelSmoother.o|cut -d' ' -f1)"
for member in CSRGAMGSolverSolve.o csrMatrixSmoother.o CSRPCG.o CSRGAMGSolver.o CSRGAMGPreconditioner.o CSRGAMGSolverScale.o numeric-pack.o topology-match.o tile.o;do
 test "$(grep -Fxc "$member" MEMBERS.before)" = 1
 a=$(ar p "$base/lib/libdfMatrices.a" "$member"|sha256sum|cut -d' ' -f1)
 b=$(ar p lib/libdfMatrices.a "$member"|sha256sum|cut -d' ' -f1)
 test "$a" = "$b";printf '%s  %s\n' "$b" "$member" >> PRESERVED_KEY_MEMBERS.sha256
done
test "$(ar p lib/libdfMatrices.a tile.o|sha256sum|cut -d' ' -f1)" = 5c991ab61547fabb7bf85adc5b0d6704e704756ed9882a988a577a26571bef3d
out=$d/bin/boussinesqFoam-rhs-writeset-n80
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
nm "$out" > log/symbols.txt
for symbol in lnjCombinedSgs_CPE lnjNumericPack_CPE lnjTopologyMatch_CPE;do
 awk -v s="$symbol" '$NF~(s"$")&&$(NF-1)!="U"{n++}END{exit n!=1}' log/symbols.txt
done
readelf -SW "$out" > log/sections.txt
ldmhex=$(awk '{for(i=1;i<=NF;i++)if($i==".ldm")print $(i+4)}' log/sections.txt)
tlshex=$(awk '{for(i=1;i<=NF;i++)if($i==".slave_tls")print $(i+4)}' log/sections.txt)
test "$((16#$ldmhex))" = 85544;test "$((16#$tlshex))" = 40
sha256sum "$out" lib/libdfMatrices.a obj/csrSymGaussSeidelSmoother.o > BUILD_OUTPUTS.sha256
printf 'N80_ONE_HOST_TU_BUILD_COMPLETE_NO_RUNTIME\n' > BUILD_COMPLETE
