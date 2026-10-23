#!/bin/bash
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj
c=/home/export/online1/mdt00/shisuan/swpku_of/xyc/test/of0404/OpenFOAM-7
source "$c/etc/bashrc" "WM_PROJECT_USER_DIR=$r/user" >/dev/null
env_rc=$?
set -euo pipefail
test "$env_rc" = 0
test "$WM_OPTIONS" = linux64sw9gccDPInt64Opt
n16=$r/build/opt960-level-cpe-permutation-20261019-01
old=$r/build/opt960-level-sgs-20261019-01/integration
tile=$r/build/opt960-permutation-csr-tiles-20261019-01
n18=$r/build/opt960-permutation-tiles-integration-20261019-01
n19=$r/build/opt960-topology-plan-cache-20261019-01/integration
production=$r/build/opt960-tile-production-only-20261019-01
numeric=$r/build/opt960-cpe-numeric-pack-integration-20261019-01
match=$r/build/opt960-cpe-topology-match-20261019-01
d=$r/build/opt960-topology-match-integration-20261019-01
entry=$r/build/doc42-pressure-csr-of0404-aligned-init-20261012-01/src/boussinesqFoam/Make/$WM_OPTIONS/boussinesqFoam.o
lib=$c/platforms/$WM_OPTIONS/lib
test "$(sha256sum "$numeric/numeric-pack.o" | cut -d' ' -f1)" = 587da38e5374f2cc4faec2f522218ea9370338fecc3ae9dffe6fe99683564a45
test "$(sha256sum "$match/topology-match.o" | cut -d' ' -f1)" = 801e4134d1cefcac58454db30eeb1982c3658f6c08708c9dc62ef91ffa2400c0
test "$(sha256sum "$tile/tile.o" | cut -d' ' -f1)" = 869d9ab91cd06bb60d68ad7b875bc6d8b35ebe995ea667926f4e9aab68df5087
sha256sum -c "$d/staged-inputs.sha256"
mkdir "$d/fixture-build-once.guard"
mpicxx -mhost -msimd -mieee -mftz -ffp-contract=off -O3 -std=c++17 -fPIC \
 -I"$d/src" -I"$n16/src" -I"$old/src" -c "$d/tests/test_integration.cpp" -o "$d/test.o" \
 > "$d/test.compile.log" 2>&1 || { tail -n 30 "$d/test.compile.log";exit 91; }
mpicxx -mdynamic "$d/test.o" "$match/topology-match.o" "$numeric/numeric-pack.o" "$tile/tile.o" \
 -o "$d/test_integration" > "$d/test.link.log" 2>&1 || { tail -n 30 "$d/test.link.log";exit 92; }
sha256sum "$d/test_integration"
echo TOPOLOGY_MATCH_INTEGRATION_FIXTURE_BUILT_NOT_EXECUTED
