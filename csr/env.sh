#!/bin/bash
# Usage: source ./env.sh host  (or cpe). No compilation is performed here.
case "${1:-}" in host|cpe) export SOLVER_BACKEND="$1";; *) echo 'Usage: source env.sh host|cpe' >&2; return 2;; esac
export SOLVER_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
export MPI_ROOT=/usr/sw/mpi/mpi_20220608_SEA
export PATH="$SOLVER_ROOT/build/$SOLVER_BACKEND/bin:$MPI_ROOT/bin:/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/bin:/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr_sw/bin:/usr/sw/bin:/usr/bin:/bin"
export LD_LIBRARY_PATH="$SOLVER_ROOT/build/$SOLVER_BACKEND/lib:/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/lib:/usr/sw/swgcc/swgcc710-tools-SEA-1307/usr/lib_for_gcc/lib:$MPI_ROOT/lib/single_dynamic:/usr/sw/lib"
export WM_PROJECT=OpenFOAM WM_PROJECT_VERSION=7 WM_OPTIONS=linux64sw9gccDPInt64Opt
export WM_PRECISION_OPTION=DP WM_LABEL_SIZE=64
export WM_PROJECT_DIR="$SOLVER_ROOT/OpenFOAM-7"
export FOAM_ETC="$WM_PROJECT_DIR/etc"
export FOAM_LIBBIN="$SOLVER_ROOT/build/$SOLVER_BACKEND/lib"
export FOAM_APPBIN="$SOLVER_ROOT/build/$SOLVER_BACKEND/bin"
# Both binaries follow the same algorithm and admission branches. The compile
# macro chooses the executor; these legacy option names do not select hardware.
for name in ${!LNJ_@};do unset "$name";done
export MPI_BUFFER_SIZE=20000000
export OMP_NUM_THREADS=1
export LNJ_SGS_RETAIN_PSI=1 LNJ_SGS_BOUNDARY_MATERIALIZE=1 LNJ_SGS_BOUNDARY_RHS_RESET=1
export LNJ_U_RECONSTRUCT_CACHE=1 LNJ_LAZY_GAMG=1 LNJ_P_RECONSTRUCT_CACHE=1 LNJ_MULES_CPE=1
export LNJ_LEVEL_SGS_MODE=tilepermute LNJ_SGS_TOPOLOGY_CACHE=1 LNJ_SGS_NUMERIC_PACK=cpe LNJ_SGS_TOPOLOGY_MATCH=cpe
