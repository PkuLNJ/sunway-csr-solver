#!/bin/bash
set -o pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
set +e;source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null;env_rc=$?
set -euo pipefail;test "$env_rc" = 0;test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
x=$r/build/csr-next-small-20261023
old=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src
n80=$r/build/opt960-rhs-writeset-repair-n80-20261020-01
native=$r/build/opt960-native-coarse-dic-20261019-01-target-01
plugin=$r/build/opt960-small-coarse-all-20261019-01/obj/smallCoarseAllCSRGAMGProcAgglomeration.o
lib=$c/platforms/$WM_OPTIONS/lib
check(){ test "$(sha256sum "$1"|cut -d' ' -f1)" = "$2"; }
check "$old/boussinesqFoam/boussinesqFoam.C" e5ac2a4206cadf54992ec2557d9f598dd4aa2f9fa87326a5702f1842f9916575
check "$old/boussinesqFoam/pEqn.H" 37ce91e3afc536e0b22cbda2ea88ff76f116ba0b79220e66cef6d3b4608a38b1
check "$old/VoF/alphaEqn.H" e826e065803a12b7faab3c5f0e1dbf0603ed10d996debbea131c7ae482353c3d
check "$n80/bin/boussinesqFoam-rhs-writeset-n80" be97dddf6c282ee839ed2dfa5948c7acc59c39d075de799c6dd2ef4df6aa4c82
check "$n80/lib/libdfMatrices.a" d44d3d586cf32a38c7d084be07c794797d06c63096afe6da5c5d1f1cdf6a4b9d
check "$native/lib/libOpenFOAM.a" 63beb30e8414c3cd0d032d6960d42e11de9d152209403ad996c2004268a38a76
check "$plugin" 816523d2d338e101d9140515f573b044bfddd5321e8a352df94986042258bc7c
check "$lib/libseaWave.a" 5a21a758e6cc8881eee724be960ec6bf348cabe3bbdf1cabb6cbe6bb93938428
check "$x/alphaEqn.lazy-snapshot.H" 26812ba767520e372288a8ce0949607876af270aef3ec633ff495039e943aae0
check "$x/pEqn.hoisted-div.H" 1b362fd8e34eb017025ce2883300367ad992b9e6ab20605d59b5232a577b03f6
cd "$x";test -d build-once.guard;test ! -e BUILD_COMPLETE;mkdir build-r2.guard
test "$(cat log/alpha-compile.rc)" != 0
mv log/alpha-compile.log log/alpha-compile-r1.log
mv log/alpha-compile.rc log/alpha-compile-r1.rc
printf 'UNMODIFIED_REFERENCE=%s\nN80_ARCHIVE_REUSED_NO_COPY=1\nNATIVE_ARCHIVE_REUSED_NO_COPY=1\nONLY_ENTRY_COMPILE=2\n' "$n80/bin/boussinesqFoam-rhs-writeset-n80" > BUILD_CONTRACT.txt
# Same native wmake flags: no new standard/profile/FMA/precision option.
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64 -Wall -Wextra -Wold-style-cast -Wnon-virtual-dtor -Wno-unused-parameter -Wno-invalid-offsetof -Wno-attributes -O3 -DNoRepository -ftemplate-depth-100 -fPIC)
incdirs=(transportModels/twoPhaseMixture transportModels transportModels/incompressible transportModels/interfaceProperties TurbulenceModels/turbulenceModels TurbulenceModels/incompressible transportModels/immiscibleIncompressibleTwoPhaseMixture finiteVolume parallel/decompose/decompositionMethods mesh/snappyHexMesh fileFormats triSurface dynamicFvMesh meshTools dynamicMesh renumber/renumberMethods renumber/zoltanRenumber dfRenumber dfSchedule dfUtils dfMatrices dfGenMatrix waves OpenFOAM OSspecific/POSIX)
incs=(-I/usr/sw/mpi/mpi_20220608_SEA/include)
for relative in "${incdirs[@]}";do
 if test "$relative" = transportModels;then incs+=(-I"$c/src/$relative");else incs+=(-I"$c/src/$relative/lnInclude");fi
done
for arm in alpha div;do
 mkdir -p "$x/$arm/src/boussinesqFoam" "$x/$arm/src/VoF"
 find -L "$old/boussinesqFoam" -maxdepth 1 -type f -exec cp -p {} "$x/$arm/src/boussinesqFoam/" \;
 find -L "$old/VoF" -maxdepth 1 -type f -exec cp -p {} "$x/$arm/src/VoF/" \;
 if test "$arm" = alpha;then cp -p "$x/alphaEqn.lazy-snapshot.H" "$x/$arm/src/VoF/alphaEqn.H";else cp -p "$x/pEqn.hoisted-div.H" "$x/$arm/src/boussinesqFoam/pEqn.H";fi
 app=$x/$arm/src/boussinesqFoam
 printf 'COMPILE_ENTRY arm=%s time=%s\n' "$arm" "$(date -Is)"
 printf '%q ' mpicxx "${flags[@]}" -I"$app" -I"$x/$arm/src/VoF" "${incs[@]}" -c "$app/boussinesqFoam.C" -o "$x/$arm/boussinesqFoam.o" > "$x/log/$arm-command.txt"
 set +e
 mpicxx "${flags[@]}" -I"$app" -I"$x/$arm/src/VoF" "${incs[@]}" -c "$app/boussinesqFoam.C" -o "$x/$arm/boussinesqFoam.o" > "$x/log/$arm-compile.log" 2>&1
 rc=$?;set -e;printf '%s\n' "$rc" > "$x/log/$arm-compile.rc"
 if test "$rc" != 0;then tail -60 "$x/log/$arm-compile.log";exit "$rc";fi
 printf 'LINK_ENTRY arm=%s time=%s\n' "$arm" "$(date -Is)"
 out=$x/bin/boussinesqFoam-$arm
 set +e
 mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$x/$arm/boussinesqFoam.o" "$plugin" \
 -L"$n80/lib" -L"$lib" -L"$lib/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 -lseaWave -latmosphericModels -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -ldecompositionMethods -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects "$native/lib/libOpenFOAM.a" -lOSspecific -lPstream -lz -lm -lrenumberMethods -ldfUtils -ldfMatrices \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi \
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread > "$x/log/$arm-link.log" 2>&1
 rc=$?;set -e;printf '%s\n' "$rc" > "$x/log/$arm-link.rc"
 if test "$rc" != 0;then tail -60 "$x/log/$arm-link.log";exit "$rc";fi
 readelf -SW "$out" > "$x/log/$arm-sections.txt"
 ldmhex=$(awk '{for(i=1;i<=NF;i++)if($i==".ldm")print $(i+4)}' "$x/log/$arm-sections.txt")
 tlshex=$(awk '{for(i=1;i<=NF;i++)if($i==".slave_tls")print $(i+4)}' "$x/log/$arm-sections.txt")
 test "$((16#$ldmhex))" = 85544;test "$((16#$tlshex))" = 40
 sha256sum "$out" "$x/$arm/boussinesqFoam.o" >> "$x/BUILD_OUTPUTS.sha256"
done
printf 'TWO_PRIVATE_ENTRY_BUILDS_COMPLETE_NO_RUNTIME\n' > "$x/BUILD_COMPLETE"
cat "$x/BUILD_OUTPUTS.sha256"
