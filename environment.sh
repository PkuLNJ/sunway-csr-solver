#!/bin/bash
# Source this file with the selected full source tree as argument.
export WM_PROJECT=OpenFOAM WM_PROJECT_VERSION=7
export WM_PROJECT_DIR=$(cd "$1" && pwd)
export WM_DIR=$WM_PROJECT_DIR/wmake WM_ARCH=linux64 WM_ARCH_OPTION=64
export WM_COMPILER=sw9gcc WM_COMPILE_OPTION=Opt WM_PRECISION_OPTION=DP WM_LABEL_SIZE=64
export WM_OPTIONS=linux64sw9gccDPInt64Opt WM_MPLIB=SWMPI WM_LINK_LANGUAGE=c++ WM_OSTYPE=POSIX
export WM_CC=gcc WM_CXX=g++ WM_PROJECT_USER_DIR=$WM_PROJECT_DIR/user
export FOAM_LIBBIN=$WM_PROJECT_DIR/platforms/$WM_OPTIONS/lib
export FOAM_APPBIN=$WM_PROJECT_DIR/platforms/$WM_OPTIONS/bin
export WM_THIRD_PARTY_DIR=$WM_PROJECT_DIR/external
export WM_PROJECT_INST_DIR=$(dirname "$WM_PROJECT_DIR")
export FOAM_EXT_LIBBIN=$WM_THIRD_PARTY_DIR/lib
export FOAM_MPI=mpi MPI_ROOT=${MPI_ROOT:-/usr/sw/mpi/mpi_20220608_SEA}
export PATH=$WM_PROJECT_DIR/bin:$WM_DIR:$MPI_ROOT/bin:$PATH
unset WM_SCHEDULER

