#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

void dfMatrix_Block_CSR_GaussSeidel_CPE(struct dfMatrix_Block_CSR_GaussSeidel_param_t* para){

    scalar* const __restrict__ psiPtr = para->psiPtr;
    scalar* const __restrict__ bPrimePtr = para->bPrimePtr;
    const scalar* const __restrict__ diagPtr = para->diagPtr;

    label rowBlockCount = para->rowBlockCount;
    const label* const __restrict__ rowBlockPtr = para->rowBlockPtr;
    const char* const __restrict__ blockFlagList = para->blockFlagList;
    const label* const __restrict__ blockRowList = para->blockRowList;
    const label** const __restrict__ blockRowPtrList = para->blockRowPtrList;
    const label** const __restrict__ blockColIdxList = para->blockColIdxList;
    const scalar** const __restrict__ blockValuesList = para->blockValuesList;
    
    // for(label rbid = rb_start; rbid < rb_end; ++rbid){
    {
        label rbid = CRTS_tid;
        label rowOffset = rowBlockPtr[rbid];
        label rowLen = rowBlockPtr[rbid+1] - rowBlockPtr[rbid];
        scalar* const __restrict__ bPrimePtr_offset = bPrimePtr + rowOffset;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rowOffset;

        // B = b - (L + U) * x
        for(label cbid = 0; cbid < rowBlockCount; ++cbid){
            label bid = bid2d_slave(rbid, cbid);
            if(rbid == cbid)
                continue;
            if(blockFlagList[bid] == 0)
                continue;
            // const dfBlockSubMatrix& offDiagBlock = *blocks_[bid];
            // offDiagBlock.BsubApsi(bPrimePtr_offset, psiPtr + colOffset);
            label colOffset = rowBlockPtr[cbid];
            const scalar* const __restrict__ psiPtr_offset = psiPtr + colOffset;
            label nRow = blockRowList[bid];
            const label* const __restrict__ rowPtr = blockRowPtrList[bid];
            const label* const __restrict__ colIdxPtr = blockColIdxList[bid];
            const scalar* const __restrict__ valuePtr = blockValuesList[bid];

            for(label r = 0; r < nRow; ++r){
                scalar sum = 0.0;
                for(label idx = rowPtr[r]; idx < rowPtr[r+1]; ++idx){
                    sum += valuePtr[idx] * psiPtr_offset[colIdxPtr[idx]];
                }
                bPrimePtr_offset[r] -= sum;
            }
        }
    }

    // #pragma omp barrier
    athread_ssync_array();

    // for(label rbid = rb_start; rbid < rb_end; ++rbid){
    {
        label rbid = CRTS_tid;
        label rowOffset = rowBlockPtr[rbid];
        label rowLen = rowBlockPtr[rbid+1] - rowBlockPtr[rbid];
        scalar* const __restrict__ bPrimePtr_offset = bPrimePtr + rowOffset;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rowOffset;

        label bid = bid2d_slave(rbid, rbid);
        scalar* const __restrict__ psiPtr_offset = psiPtr + rowOffset;
        if(blockFlagList[bid] == 0){
            //  x = B / diag
            for(label r = 0; r < rowLen; ++r){
                psiPtr_offset[r] = bPrimePtr_offset[r] / diagPtr_offset[r];
            }
        }else{
            // const dfBlockSubMatrix& diagBlock = *blocks_[diagBlockIndex];
            // diagBlock.GaussSeidel(psiPtr_offset, bPrimePtr_offset, diagPtr_offset);
            label nRow = blockRowList[bid];
            const label* const __restrict__ rowPtr = blockRowPtrList[bid];
            const label* const __restrict__ colIdxPtr = blockColIdxList[bid];
            const scalar* const __restrict__ valuePtr = blockValuesList[bid];
            for(label r = 0; r < nRow; ++r){
                scalar sum = 0.0;
                for(label idx = rowPtr[r]; idx < rowPtr[r+1]; ++idx){
                    sum += valuePtr[idx] * psiPtr_offset[colIdxPtr[idx]];
                }
                psiPtr_offset[r] = (bPrimePtr_offset[r] - sum) / diagPtr_offset[r];
            }
        }
    }
}