# Explicit options under review. No Allwmake entry point.
# Numerical options are identical for host and CPE-enabled host code.
CXX = $(MPI_ROOT)/bin/mpicxx
CC = $(MPI_ROOT)/bin/mpicc
HOST_NUMERICS := -mhost -O3 -msimd -mieee -mftz
CXX_LANGUAGE := -std=c++14
CSR_LANGUAGE := -std=c++17
# Existing CSR objects use these options; keep them in both backends.
CSR_NUMERICS := -ffp-contract=off
CSR_PROFILING := -D_PROFILING_
FOAM_ABI := -DWM_DP -DWM_LABEL_SIZE=64 -Dlinux64 -DWM_ARCH_OPTION=64
FOAM_TEMPLATES := -DNoRepository -ftemplate-depth-100
# Retained until clean compilation demonstrates whether they are removable.
COMPATIBILITY := -fpermissive -fPIC
# Debug information is explicitly separate, not a floating point mode.
DEBUG_INFO := -g
# Backend identity is the ONLY intended difference in host translation units.
HOST_BACKEND := -DLNJ_BACKEND_HOST=1
CPE_BACKEND := -DLNJ_BACKEND_CPE=1
# Verified from the MULES build recipe; other kernels require their own audit.
MULES_CPE_FLAGS := -mslave -O3 -msimd -mieee -mftz -ffp-contract=off -std=gnu11 -fPIC
