#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

#define TILE_SIZE 1024

void dfMatrix_value_transfer_navie(dfMatrix_value_transfer_param_t* para_p){
    dfMatrix_value_transfer_param_t para;
    CRTS_dma_get(&para, para_p, sizeof(dfMatrix_value_transfer_param_t));

    label row = para.row;
    label lowerUpperSize = para.lowerUpperSize;

    scalarPtr dfDiag = para.dfDiag;
    scalarPtr dfOffDiagValue = para.dfOffDiagValue;

    constScalarPtr lduDiag = para.lduDiag;
    constScalarPtr lduLower = para.lduLower;
    constScalarPtr lduUpper = para.lduUpper;
    constLabelPtr dfFace2Lower = para.dfFace2Lower;
    constLabelPtr dfFace2Upper = para.dfFace2Upper;

    label localRowStart = LOCAL_START(row, CRTS_tid, CRTS_MAX_SPE_NUM);
    label localRowEnd = LOCAL_END(row, CRTS_tid, CRTS_MAX_SPE_NUM);
    label localRowLen = localRowEnd - localRowStart;

    label localLowerUpperSizeStart = LOCAL_START(lowerUpperSize, CRTS_tid, CRTS_MAX_SPE_NUM);
    label localLowerUpperSizeEnd = LOCAL_END(lowerUpperSize, CRTS_tid, CRTS_MAX_SPE_NUM);
    label localLowerUpperSizeLen = localLowerUpperSizeEnd - localLowerUpperSizeStart;

    scalar diagBuffer[TILE_SIZE];

    for(label brs = localRowStart; brs < localRowEnd; brs += TILE_SIZE){
        label bre = slave_min(brs + TILE_SIZE, localRowEnd);
        label brl = bre - brs;
        CRTS_dma_get(diagBuffer, (scalar*)lduDiag + brs, brl * sizeof(scalar));
        CRTS_dma_put(dfDiag + brs, diagBuffer, brl * sizeof(scalar));
    }

    label dfFace2LowerBuffer[TILE_SIZE];
    label dfFace2UpperBuffer[TILE_SIZE];
    scalar lduLowerBuffer[TILE_SIZE];
    scalar lduUpperBuffer[TILE_SIZE];

    for(label bis = localLowerUpperSizeStart; bis < localLowerUpperSizeEnd; bis += TILE_SIZE){
        label bie = slave_min(bis + TILE_SIZE, localLowerUpperSizeEnd);
        label bil = bie - bis;
        CRTS_dma_get(dfFace2LowerBuffer, (label*)dfFace2Lower + bis, bil * sizeof(label));
        CRTS_dma_get(dfFace2UpperBuffer, (label*)dfFace2Upper + bis, bil * sizeof(label));
        CRTS_dma_get(lduLowerBuffer, (scalar*)lduLower + bis, bil * sizeof(scalar));
        CRTS_dma_get(lduUpperBuffer, (scalar*)lduUpper + bis, bil * sizeof(scalar));
        for(label i = 0; i < bil; ++i){
            dfOffDiagValue[dfFace2LowerBuffer[i]] = lduLowerBuffer[i];
            dfOffDiagValue[dfFace2UpperBuffer[i]] = lduUpperBuffer[i];
        }
    }
}

