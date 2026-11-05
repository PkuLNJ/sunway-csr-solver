#!/bin/bash
set -eo pipefail
here=$(cd "$(dirname "$0")" && pwd)
variant=${1:?Use ldu or csr}
case "$variant" in ldu|csr) ;; *) exit 2;; esac
source "$here/environment.sh" "$here/$variant/OpenFOAM-7"
export WM_NCOMPPROCS=${JOBS:-4}
mkdir -p "$FOAM_LIBBIN/mpi" "$FOAM_EXT_LIBBIN" "$here/logs/$variant"
# Build the dependency scanner with the login-node compiler from its source.
make -C "$WM_DIR/src" WM_CC=gcc WM_CXX=g++ CC=g++ cc=gcc AR=ar RANLIB=ranlib
while IFS= read -r files; do
  module=${files%/Make/files}
  "$WM_DIR/wmakeLnInclude" "$module"
done < <(find "$WM_PROJECT_DIR/src" -path '*/Make/files' -type f)
while read -r module; do
  test -n "$module" || continue
  if test "$variant" = ldu; then
    case "$module" in src/dfUtils|src/dfMatrices) continue;; esac
  fi
  echo "BUILD_MODULE=$module"
  "$WM_DIR/wmake" -j "$WM_NCOMPPROCS" lib "$WM_PROJECT_DIR/$module"
done < "$here/modules.txt"
if test "$variant" = ldu; then bash "$here/ldu/build-gamg.sh"; fi
echo LIBRARIES_BUILT
