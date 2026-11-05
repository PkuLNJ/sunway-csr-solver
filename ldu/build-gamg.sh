#!/bin/bash
# Preserve the six GAMG objects used by historical job 8825043.
set -eo pipefail
here=$(cd "$(dirname "$0")" && pwd)
source "$here/../environment.sh" "$here/OpenFOAM-7"
g=$WM_PROJECT_DIR/src/OpenFOAM/matrices/lduMatrix/solvers/GAMG
p=$WM_PROJECT_DIR/src/OpenFOAM/matrices/lduMatrix/preconditioners/GAMGPreconditioner
out=$here/gamg-objects
mkdir -p "$out"
flags=(-mhost -msimd -mieee -mftz -fpermissive -g -O3 -std=c++14
       -Dlinux64 -DWM_ARCH_OPTION=64 -DWM_DP -DWM_LABEL_SIZE=64
       -DNoRepository -ftemplate-depth-100 -fPIC
       -I"$g" -I"$p" -I"$WM_PROJECT_DIR/src/OpenFOAM/lnInclude"
       -I"$WM_PROJECT_DIR/src/OSspecific/POSIX/lnInclude" -I"$MPI_ROOT/include")
sources=("$g/GAMGSolver.C" "$g/GAMGSolverAgglomerateMatrix.C"
         "$g/GAMGSolverInterpolate.C" "$g/GAMGSolverScale.C"
         "$g/GAMGSolverSolve.C" "$p/GAMGPreconditioner.C")
for src in "${sources[@]}"; do
    obj=$out/$(basename "$src" .C).o
    printf 'BUILD_GAMG %s\n' "$src"
    "$MPI_ROOT/bin/mpicxx" "${flags[@]}" -c "$src" -o "$obj"
    ar r "$FOAM_LIBBIN/libOpenFOAM.a" "$obj"
done
ar s "$FOAM_LIBBIN/libOpenFOAM.a"
