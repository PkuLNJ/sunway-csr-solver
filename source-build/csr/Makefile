# Explicit incremental build. Every compiler command is printed by make.
# Work in progress: complete wiring present; executable validation pending.
.DEFAULT_GOAL := help
.DELETE_ON_ERROR:
include build-config/compiler-options.mk

BACKEND ?= cpe
ifeq ($(BACKEND),host)
BACKEND_FLAGS := $(HOST_BACKEND)
else ifeq ($(BACKEND),cpe)
BACKEND_FLAGS := $(CPE_BACKEND)
else
$(error BACKEND must be host or cpe)
endif

# SDK is a pinned, self-contained dependency directory, not an experiment tree.
SDK ?= $(CURDIR)/OpenFOAM-7
MPI_ROOT ?= /usr/sw/mpi/mpi_20220608_SEA
BUILD := build/$(BACKEND)
CPPFLAGS := $(FOAM_ABI) $(FOAM_TEMPLATES) $(BACKEND_FLAGS) \
 -Isrc/csr -Isrc/sgs -Isrc/kernels -Isrc/capture -Isrc/correction -Isrc/solver -Isrc/vof \
 -I$(SDK)/src/dfMatrices/lnInclude -I$(SDK)/src/dfUtils/lnInclude \
 -I$(SDK)/src/finiteVolume/lnInclude -I$(SDK)/src/meshTools/lnInclude \
 -I$(SDK)/src/OpenFOAM/lnInclude -I$(SDK)/src/OSspecific/POSIX/lnInclude \
 -I$(SDK)/src/Pstream/mpi \
 -I$(MPI_ROOT)/include
CXXFLAGS := $(HOST_NUMERICS) $(COMPATIBILITY) $(DEBUG_INFO) \
 $(CSR_LANGUAGE) $(CSR_PROFILING)

CSR_UNITS := CSRPCG CSRGAMGSolver CSRGAMGSolverAgglomerateMatrix \
 CSRGAMGSolverSolve CSRGAMGPreconditioner CSRGAMGAgglomerateLduAddressing \
 CsrPrimitiveMeshBridge csrMatrixSmoother csrSymGaussSeidelSmoother \
 csrMatrixAmul csrMatrixSolve csrMatrixSolver csrMatrixSpMV CSRGAMGSolverScale
CSR_OBJECTS := $(addprefix $(BUILD)/csr/,$(addsuffix .o,$(CSR_UNITS)))

# These private units were originally compiled with contraction explicitly off.
NO_CONTRACT_UNITS := CSRPCG CSRGAMGSolver CSRGAMGSolverAgglomerateMatrix \
 CSRGAMGSolverSolve CSRGAMGPreconditioner CSRGAMGAgglomerateLduAddressing \
 CsrPrimitiveMeshBridge csrMatrixSmoother csrSymGaussSeidelSmoother
$(addprefix $(BUILD)/csr/,$(addsuffix .o,$(NO_CONTRACT_UNITS))): CXXFLAGS += $(CSR_NUMERICS)

.PHONY: help csr-objects host-kernels check-sdk
help:
	@echo 'Current target: make csr-objects BACKEND=host|cpe SDK=/absolute/pinned/OpenFOAM-7'
	@echo 'Inspect commands: make -n csr-objects BACKEND=host SDK=/absolute/pinned/OpenFOAM-7'
	@echo 'Full executable: make solver BACKEND=host|cpe (target validation pending)'

check-sdk:
	test -r "$(SDK)/src/OpenFOAM/lnInclude/scalar.H"
	test -r "$(SDK)/src/dfMatrices/lnInclude/csrMatrix.H"

csr-objects: $(CSR_OBJECTS)

SDK_LIB := $(SDK)/platforms/linux64sw9gccDPInt64Opt/lib
CPE_PACK_OBJECTS := $(BUILD)/kernels/lnjNumericPack_CPE.o $(BUILD)/kernels/lnjTopologyMatch_CPE.o $(BUILD)/kernels/lnjCombinedSgs_CPE.o
ifeq ($(BACKEND),host)
CSR_KERNEL_OBJECTS := $(BUILD)/kernels/lnjCombinedSgs_HOST.o $(BUILD)/kernels/lnjPacking_HOST.o
else
CSR_KERNEL_OBJECTS := $(CPE_PACK_OBJECTS)
endif

.PHONY: csr-library
csr-library: $(BUILD)/lib/libdfMatrices.a

$(BUILD)/lib/libdfMatrices.a: $(SDK_LIB)/libdfMatrices.a $(CSR_OBJECTS) $(CSR_KERNEL_OBJECTS)
	mkdir -p $(@D)
	cp $< $@.tmp
ifeq ($(BACKEND),host)
	$(AR) d $@.tmp csrMatrix_SpMV_CPE.o csrJacobiSmoother_CPE.o csrGAMGScale_CPE.o
endif
	$(AR) rcs $@.tmp $(CSR_OBJECTS) $(CSR_KERNEL_OBJECTS)
	mv $@.tmp $@

$(BUILD)/kernels/%_CPE.o: src/kernels/%_CPE.c
	mkdir -p $(@D)
	$(CC) -Isrc/kernels $(MULES_CPE_FLAGS) -MMD -MP -c $< -o $@

HOST_KERNEL_OBJECTS := $(BUILD)/kernels/lnjCombinedSgs_HOST.o $(BUILD)/kernels/lnjPacking_HOST.o $(BUILD)/kernels/lnjMules_HOST.o
host-kernels: $(HOST_KERNEL_OBJECTS)

$(BUILD)/kernels/%.o: src/kernels/%.C
	mkdir -p $(@D)
	$(CXX) $(FOAM_ABI) $(HOST_BACKEND) -Isrc/kernels $(HOST_NUMERICS) $(CSR_NUMERICS) $(COMPATIBILITY) $(DEBUG_INFO) $(CSR_LANGUAGE) -MMD -MP -c $< -o $@

$(BUILD)/kernels/lnjMules_HOST.o: src/solver/lnjMules_HOST.C src/solver/lnjMulesParam.h
	mkdir -p $(@D)
	$(CXX) $(FOAM_ABI) $(HOST_BACKEND) -Isrc/solver $(HOST_NUMERICS) $(CSR_NUMERICS) $(COMPATIBILITY) $(DEBUG_INFO) $(CXX_LANGUAGE) -MMD -MP -c $< -o $@

$(BUILD)/csr/%.o: src/csr/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(CSR_OBJECTS:.o=.d)
-include $(HOST_KERNEL_OBJECTS:.o=.d)
-include $(CPE_PACK_OBJECTS:.o=.d)
include build-config/solver.mk
