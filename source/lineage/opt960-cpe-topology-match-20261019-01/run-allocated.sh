#!/bin/bash
# Only execute INSIDE an explicitly allocated 1MPE/64CPE job.
set -euo pipefail
r=/home/export/online1/mdt00/shisuan/swpku_of/lnj/build
d=$r/opt960-cpe-topology-match-20261019-01
g=$r/opt960-level-sgs-20261019-01/graph-benchmark/graphs
case "${1-}" in
 fixture)exec "$d/bin/fixture";;
 benchmark)exec "$d/bin/benchmark" "$g/processor0.edges" "$g/processor479.edges" "$g/processor959.edges";;
 *)echo 'Use fixture or benchmark inside 1MPE allocation' >&2;exit 2;;
esac
