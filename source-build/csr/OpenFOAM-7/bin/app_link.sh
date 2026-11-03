#!/bin/bash

set -x

ObjectsPath=$FOAM_INST_DIR/OpenFOAM-8/platforms/linux64sw9gccDPInt64Opt/`pwd | awk  -F"[8]" '{print $(NF)}'`
#Objects=${ObjectsPath}/*.o
Objects=`find ${ObjectsPath} -name '*.o'`
EXE=$FOAM_APPBIN/`awk -F"/" '/EXE/ {print $(NF)}' Make/files`

sw9g++ -mhybrid -static \
	-Wl,--whole-archive \
	-o ${EXE} \
	${Objects} \
	-L$FOAM_INST_DIR/OpenFOAM-8/platforms/linux64sw9gccDPInt64Opt/lib \
	-L$FOAM_INST_DIR/OpenFOAM-8/platforms/linux64sw9gccDPInt64Opt/lib/mpi \
	-L$FOAM_INST_DIR/ThirdParty-8/platforms/linux64sw9gccDPInt64/lib \
	-L$FOAM_INST_DIR/ThirdParty-8/platforms/linux64sw9gccDPInt64/lib/mpi \
	-L/usr/sw/mpi/mpisw9gcc_20200420/lib \
	-lincompressibleMomentumTransportModels -lincompressibleTransportModels \
	-lmomentumTransportModels \
	-lfieldFunctionObjects	\
	-lthermophysicalTransportModels \
	-lfluidThermoMomentumTransportModels \
	-lforces \
	-lfvOptions -lsampling -ltwoPhaseMixture -ltriSurface -lfileFormats -lsurfMesh  -lfluidThermophysicalModels  -lsolidThermo -lspecie -lthermophysicalProperties  -lreactionThermophysicalModels -llagrangian -lconversion -ldynamicFvMesh -ltopoChangerFvMesh -ldynamicMesh -lextrudeModel -lfiniteVolume -lmeshTools -lOpenFOAM  -lOSspecific -lPstream -lz -lm \
	-lmpicxx -lmpi -L/usr/sw/lib -lswverbs_perf -L/usr/sw/lib -lswtm -lm -Wl,-zmuldefs -ldl -lrt -lpthread \
	-Wl,--no-whole-archive
