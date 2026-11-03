# Full executable wiring. Target validation is still required.
FOAM_MODULES := transportModels/twoPhaseMixture transportModels/incompressible \
 transportModels/interfaceProperties TurbulenceModels/turbulenceModels \
 TurbulenceModels/incompressible transportModels/immiscibleIncompressibleTwoPhaseMixture \
 parallel/decompose/decompositionMethods mesh/snappyHexMesh fileFormats triSurface \
 dynamicFvMesh dynamicMesh renumber/renumberMethods renumber/zoltanRenumber \
 dfRenumber dfSchedule dfGenMatrix waves
WAVE_DIRS := waveModel waveModel/irregularSeaWave waveAbsorptionModels \
 derivedFvPatchFields/waveVelocity derivedFvPatchFields/waveAlpha
CPPFLAGS += -Isrc/openfoam -Isrc/coarse $(addprefix -Isrc/seaWave/,$(WAVE_DIRS)) \
 $(addprefix -I$(SDK)/src/,$(addsuffix /lnInclude,$(FOAM_MODULES))) \
 -I$(SDK)/src/transportModels -I$(SDK)/src/finiteVolume/cfdTools \
 -I$(SDK)/applications/solvers/multiphase/VoF
APP_FLAGS := $(HOST_NUMERICS) $(COMPATIBILITY) $(DEBUG_INFO) $(CXX_LANGUAGE)
WAVE_UNITS := waveModel/waveModel waveModel/waveModelNew \
 waveModel/irregularSeaWave/waveGenerationModel waveModel/irregularSeaWave/irregularWaveModel \
 waveModel/irregularSeaWave/seaWaveModel waveAbsorptionModels/waveAbsorptionModel \
 waveAbsorptionModels/shallowWaterAbsorption derivedFvPatchFields/waveVelocity/waveVelocityFvPatchVectorField \
 derivedFvPatchFields/waveAlpha/waveAlphaFvPatchScalarField
WAVE_OBJECTS := $(addprefix $(BUILD)/seaWave/,$(addsuffix .o,$(WAVE_UNITS)))
APP_OBJECTS := $(BUILD)/solver/boussinesqFoam.o $(BUILD)/solver/ConstructProfile.o \
 $(BUILD)/fixes/MainExit.o $(BUILD)/fixes/SqrVectorFieldVerified.o \
 $(BUILD)/fixes/ProfilingZeroGuard.o $(BUILD)/capture/AmrCapture.o $(BUILD)/capture/CaptureWriter.o \
 $(BUILD)/coarse/smallCoarseAllCSRGAMGProcAgglomeration.o
ifeq ($(BACKEND),host)
APP_OBJECTS += $(BUILD)/kernels/lnjMules_HOST.o
else
APP_OBJECTS += $(BUILD)/kernels/lnjMules_CPE.o
endif

$(BUILD)/seaWave/%.o: src/seaWave/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(APP_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/solver/%.o: src/solver/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(APP_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/capture/%.o: src/capture/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(APP_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/fixes/%.o: src/fixes/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(APP_FLAGS) $(FIX_FLAGS) -MMD -MP -c $< -o $@
# Preserve the verified scalar-fix recipe: no SIMD option for this unit.
$(BUILD)/fixes/SqrVectorFieldVerified.o: APP_FLAGS := $(filter-out -msimd,$(APP_FLAGS))
$(BUILD)/fixes/SqrVectorFieldVerified.o: build-config/solver.mk
$(BUILD)/fixes/SqrVectorFieldVerified.o: FIX_FLAGS := -fno-tree-vectorize -fno-tree-slp-vectorize
$(BUILD)/coarse/%.o: src/coarse/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CSR_NUMERICS) -MMD -MP -c $< -o $@
$(BUILD)/openfoam/%.o: src/openfoam/%.C | check-sdk
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(APP_FLAGS) -MMD -MP -c $< -o $@
$(BUILD)/kernels/lnjMules_CPE.o: src/solver/lnjMules_CPE.c src/solver/lnjMulesParam.h
	mkdir -p $(@D)
	$(CC) -Isrc/solver $(MULES_CPE_FLAGS) -MMD -MP -c $< -o $@

$(BUILD)/lib/libseaWave.a: $(WAVE_OBJECTS)
	mkdir -p $(@D)
	$(AR) rcs $@ $(WAVE_OBJECTS)

OF_OBJECTS := $(addprefix $(BUILD)/openfoam/,lduMatrixATmul.o lduMatrixSolver.o GAMGSolverSolve.o)
$(BUILD)/lib/libOpenFOAM.a: $(SDK_LIB)/libOpenFOAM.a $(OF_OBJECTS)
	mkdir -p $(@D)
	cp $< $@.tmp
	$(AR) rcs $@.tmp $(OF_OBJECTS)
	mv $@.tmp $@
$(BUILD)/lib/libfiniteVolume.a: $(SDK_LIB)/libfiniteVolume.a $(BUILD)/openfoam/fvScalarMatrix.o
	mkdir -p $(@D)
	cp $< $@.tmp
	$(AR) rcs $@.tmp $(BUILD)/openfoam/fvScalarMatrix.o
	mv $@.tmp $@
$(BUILD)/lib/libdynamicFvMesh.a: $(SDK_LIB)/libdynamicFvMesh.a $(BUILD)/openfoam/dynamicRefineFvMesh.o
	mkdir -p $(@D)
	cp $< $@.tmp
	$(AR) rcs $@.tmp $(BUILD)/openfoam/dynamicRefineFvMesh.o
	mv $@.tmp $@

LIB_NAMES := seaWave atmosphericModels immiscibleIncompressibleTwoPhaseMixture interfaceProperties \
 twoPhaseProperties incompressibleTransportModels fvMotionSolvers fieldFunctionObjects \
 incompressibleTurbulenceModels combustionModels turbulenceModels compressibleTurbulenceModels \
 chemistryModel ODE forces fvOptions sampling twoPhaseMixture triSurface fileFormats surfMesh \
 compressibleTransportModels fluidThermophysicalModels radiationModels solidThermo thermophysicalProperties \
 reactionThermophysicalModels specie solidSpecie finiteVolume decompositionMethods lagrangian conversion \
 dynamicFvMesh topoChangerFvMesh dynamicMesh extrudeModel meshTools utilityFunctionObjects \
 OpenFOAM OSspecific Pstream renumberMethods dfUtils dfMatrices
PRIVATE_LIBS := seaWave finiteVolume dynamicFvMesh OpenFOAM dfMatrices
LIBRARIES := $(foreach lib,$(LIB_NAMES),$(if $(filter $(lib),$(PRIVATE_LIBS)),$(BUILD)/lib/lib$(lib).a,$(if $(filter Pstream,$(lib)),$(SDK_LIB)/mpi/libPstream.a,$(SDK_LIB)/lib$(lib).a)))
SYSTEM_LIBS := -lz -lm -L$(MPI_ROOT)/lib/multi_dynamic -lmpicxx -lmpi \
 -L/usr/sw/lib -lswverbs_perf -lswtm -lm -ldl -lrt -lpthread
# Retain reference override resolution while verifying source reconstruction.
# Inspect the map before removing -zmuldefs; it is not an algorithm option.
LINK_FLAGS := -mdynamic -Wl,--wrap=main -Wl,--wrap=MPI_Init_thread -Wl,--wrap=MPI_Init -Wl,-zmuldefs
.PHONY: solver
solver: $(BUILD)/bin/boussinesqFoam
$(BUILD)/bin/boussinesqFoam: $(APP_OBJECTS) $(LIBRARIES)
	mkdir -p $(@D)
	$(CXX) $(LINK_FLAGS) -Wl,-Map,$@.map -o $@ -Wl,--whole-archive $(APP_OBJECTS) $(LIBRARIES) $(SYSTEM_LIBS)

-include $(APP_OBJECTS:.o=.d) $(WAVE_OBJECTS:.o=.d) $(OF_OBJECTS:.o=.d)
-include $(BUILD)/openfoam/fvScalarMatrix.d $(BUILD)/openfoam/dynamicRefineFvMesh.d
