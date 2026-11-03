set -x
mpicxx -mdynamic \
-o $FOAM_INST_DIR/OpenFOAM-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64Opt/bin/setSeaWave \
-Wl,--whole-archive \
$FOAM_INST_DIR/OpenFOAM-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64Opt/src/seaWave/setSeaWave/setSeaWave.o \
-L$FOAM_INST_DIR/OpenFOAM-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64Opt/lib \
-L$FOAM_INST_DIR/OpenFOAM-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64Opt/lib/mpi \
-L$FOAM_INST_DIR/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib \
-L$FOAM_INST_DIR/ThirdParty-${WM_PROJECT_VERSION}/platforms/linux64sw9gccDPInt64/lib/mpi \
 -lseaWave -latmosphericModels \
 -limmiscibleIncompressibleTwoPhaseMixture -linterfaceProperties -ltwoPhaseProperties -lincompressibleTransportModels \
 -lfvMotionSolvers -lfieldFunctionObjects -lincompressibleTurbulenceModels -lcombustionModels -lturbulenceModels -lcompressibleTurbulenceModels -lchemistryModel -lODE -lforces \
 -lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh -lcompressibleTransportModels \
 -lfluidThermophysicalModels -lradiationModels -lsolidThermo -lthermophysicalProperties \
 -lreactionThermophysicalModels -lspecie -lsolidSpecie -lfiniteVolume -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel \
 -lmeshTools -lutilityFunctionObjects  -lOpenFOAM  -lOSspecific -lPstream -lz -lm \
-L/usr/sw/mpi/mpi_20220608_SEA/lib/multi_dynamic -lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -L/usr/sw/lib -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread
set +x
