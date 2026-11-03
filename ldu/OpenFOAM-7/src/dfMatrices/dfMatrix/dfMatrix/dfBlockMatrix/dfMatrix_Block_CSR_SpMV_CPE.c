#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

void dfMatrix_Block_CSR_SpMV_CPE(struct dfMatrix_Block_CSR_SpMV_param_t* para){

    scalar* const __restrict__ ApsiPtr = para->ApsiPtr;
    const scalar* const __restrict__ psiPtr = para->psiPtr;
    const scalar* const __restrict__ diagPtr = para->diagPtr;

    label rowBlockCount = para->rowBlockCount;
    const label* const __restrict__ rowBlockPtr = para->rowBlockPtr;
    const char* const __restrict__ blockFlagList = para->blockFlagList;
    const label* const __restrict__ blockRowList = para->blockRowList;
    const label** const __restrict__ blockRowPtrList = para->blockRowPtrList;
    const label** const __restrict__ blockColIdxList = para->blockColIdxList;
    const scalar** const __restrict__ blockValuesList = para->blockValuesList;
    
    // for(label rbid = 0; rbid < rowBlockCount_; ++rbid){
    {
        label rbid = CRTS_tid;
        label rowOffset = rowBlockPtr[rbid];
        label rowLen = rowBlockPtr[rbid + 1] - rowBlockPtr[rbid];
        scalar* const __restrict__ ApsiPtr_offset = ApsiPtr + rowOffset;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rowOffset;
        const scalar* const __restrict__ psiPtr_rowOffset = psiPtr + rowOffset;
        for(label r = 0; r < rowLen; ++r){
            ApsiPtr_offset[r] = diagPtr_offset[r] * psiPtr_rowOffset[r];
        }
        for(label cbid = 0; cbid < rowBlockCount; ++cbid){
            label bid = bid2d_slave(rbid, cbid);
            if(blockFlagList[bid] == 0){
                continue;
            }
            label colOffset = rowBlockPtr[cbid];
            const scalar* const __restrict__ psiPtr_offset = psiPtr + colOffset;
            label nRow = blockRowList[bid];
            const label* const __restrict__ rowPtr = blockRowPtrList[bid];
            const label* const __restrict__ colIdxPtr = blockColIdxList[bid];
            const scalar* const __restrict__ valuePtr = blockValuesList[bid];

            for(label r = 0; r < nRow; ++r){
                scalar sum = 0.;
                for(label idx = rowPtr[r]; idx < rowPtr[r+1]; ++idx){
                    sum += valuePtr[idx] * psiPtr_offset[colIdxPtr[idx]];
                }
                ApsiPtr_offset[r] += sum;
            }
        }
    }
}