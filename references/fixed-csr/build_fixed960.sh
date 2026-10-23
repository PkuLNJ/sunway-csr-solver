#!/usr/bin/env bash
# Rebuild an application entry against the checked, pre-existing Sunway libraries.
# Does NOT rebuild OpenFOAM libraries, submit jobs, or modify reference trees.
set -e
mode=${1:-}
action=${2:---check}
case "$mode" in host|cpe) ;; *) echo 'Usage: bash build_fixed960.sh host|cpe [--check|--build]'; exit 2;; esac
case "$action" in --check|--build) ;; *) exit 2;; esac
package=$(cd -- "$(dirname -- "$0")" && pwd -P)
server_root=/home/export/online1/mdt00/shisuan/swpku_of
if test "$mode" = host;then
 of=$server_root/xyc/baseline/OpenFOAM-7
else
 of=$server_root/xyc/test/of0404/OpenFOAM-7
fi
test -f "$of/etc/bashrc"
# The original environment is incompatible with nounset/errexit during sourcing.
set +e
source "$of/etc/bashrc" "WM_PROJECT_USER_DIR=$server_root/lnj/user"
env_rc=$?
set -euo pipefail
test "$env_rc" -eq 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
test "$WM_PROJECT_DIR" = "$of"
command -v mpicxx >/dev/null
command -v wmake >/dev/null
cd "$package"
sha256sum -c MANIFEST.sha256 >/dev/null
sha256sum -c "$mode/dependencies.sha256"
p=$of/platforms/$WM_OPTIONS/lib
# GNU-style -l resolution may choose a shared object before a static archive.
# Reject such ambiguity in either earlier OpenFOAM search directory.
while read -r expected archive;do
 base=${archive##*/}
 shared=${base%.a}.so
 selected=
 for search in "$p" "$p/mpi";do
  if test -e "$search/$shared" || test -L "$search/$shared";then
   printf 'REFUSE_AMBIGUOUS_LIBRARY=%s\n' "$search/$shared";exit 5
  fi
  if test -z "$selected" && test -s "$search/$base";then selected=$search/$base;fi
 done
 test -n "$selected"
 if test "$(readlink -f "$selected")" != "$(readlink -f "$archive")";then
  printf 'REFUSE_SHADOWED_ARCHIVE=%s expected=%s\n' "$selected" "$archive";exit 6
 fi
done < "$mode/dependencies.sha256"
test -s "$mode/source/solver/boussinesqFoam.C"
test -d "$mode/source/VoF"
printf 'CHECK_PASS mode=%s; prebuilt dependencies match delivery-date hashes\n' "$mode"
if test "$action" = --check;then exit 0;fi
work=$package/_build/$mode
test ! -e "$work" || { echo 'Output already exists; select a fresh extracted package. No overwrite performed.';exit 3; }
mkdir -p "$work/src" "$work/bin"
cp -a "$package/$mode/source/solver" "$work/src/solver"
cp -a "$package/$mode/source/VoF" "$work/src/VoF"
export FOAM_APPBIN=$work/bin FOAM_USER_APPBIN=$work/bin
cd "$work/src/solver"
set +e
wmake -s > "$work/wmake.log" 2>&1
wmake_rc=$?
set -e
entry=$work/src/solver/Make/$WM_OPTIONS/boussinesqFoam.o
# Historical Make/options cannot perform the complete final link. The object
# must be produced in this fresh directory; the explicit link below is decisive.
test -s "$entry" || { echo 'Entry compilation failed; inspect _build/<mode>/wmake.log';exit 4; }
printf 'WMAKE_EXIT=%s; fresh entry object present; explicit historical link follows\n' "$wmake_rc"
libs=(seaWave atmosphericModels immiscibleIncompressibleTwoPhaseMixture interfaceProperties twoPhaseProperties incompressibleTransportModels
 fvMotionSolvers fieldFunctionObjects incompressibleTurbulenceModels combustionModels turbulenceModels compressibleTurbulenceModels chemistryModel ODE forces
 fvOptions sampling twoPhaseMixture triSurface fileFormats surfMesh compressibleTransportModels fluidThermophysicalModels radiationModels solidThermo thermophysicalProperties
 reactionThermophysicalModels specie solidSpecie finiteVolume)
if test "$mode" = cpe;then libs+=(decompositionMethods);fi
libs+=(lagrangian conversion dynamicFvMesh topoChangerFvMesh dynamicMesh extrudeModel meshTools utilityFunctionObjects OpenFOAM OSspecific Pstream z m)
if test "$mode" = cpe;then libs+=(renumberMethods);fi
libs+=(dfUtils dfMatrices)
link_libs=()
for lib in "${libs[@]}";do link_libs+=("-l$lib");done
out=$work/bin/fixed960-$mode
mpicxx -mdynamic -o "$out" -Wl,--whole-archive "$entry" \
 -L"$p" -L"$p/mpi" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib" \
 -L"${FOAM_INST_DIR}/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi" \
 "${link_libs[@]}" \
 -L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi \
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread \
 > "$work/final-link.log" 2>&1
test -s "$out"
sha256sum "$out" | tee "$work/rebuilt-binary.sha256"
printf 'BUILD_LINK_PASS mode=%s output=%s\n' "$mode" "$out"
printf 'Not yet target-node startup, numerical, performance, or bitwise-identity verification.\n'
