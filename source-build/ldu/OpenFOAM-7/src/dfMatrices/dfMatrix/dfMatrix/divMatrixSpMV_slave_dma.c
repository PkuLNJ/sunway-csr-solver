#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

#define ROW_BLOCK_SIZE 32
#define MAX_DISTANCE_COUNT 6

void dfMatrix_SpMV_dma(dfMatrix_SpMV_param_t* para){
    const label ZERO = 0;

    scalar* ApsiPtr = para->ApsiPtr;
    const scalar* psiPtr = para->psiPtr;
    const scalar* diagPtr = para->diagPtr;
    const scalar* off_diag_value_Ptr = para->off_diag_value_Ptr;
    const label* distance_list_ = para->distance_list_;

    label row_ = para->row_;
    label distance_count_ = para->distance_count_;
    label block_count_ = para->block_count_;
    label row_block_bit_ = para->row_block_bit_;
    label row_block_size_ = para->row_block_size_;
    
    label block_start = LOCAL_START(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);
    label block_end = LOCAL_END(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);

    label distanceListBuffer[MAX_DISTANCE_COUNT];
    CRTS_dma_get(distanceListBuffer, distance_list_, distance_count_ * sizeof(label));

    scalar ApsiPtrBuffer[ROW_BLOCK_SIZE];
    scalar diagPtrBuffer[ROW_BLOCK_SIZE];
    scalar psiPtrBuffer[ROW_BLOCK_SIZE];
    scalar offDiagPtrBuffer[ROW_BLOCK_SIZE * MAX_DISTANCE_COUNT];

    crts_rply_t put_reply = 0;

    for(label bi = block_start; bi < block_end; ++bi){
        label rbs = DIV_BLOCK_START(bi);
        label rbe = DIV_BLOCK_END(rbs);
        label rbl = DIV_BLOCK_LEN(rbs,rbe);
        scalar* ApsiPtr_offset = ApsiPtr + rbs;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rbs;

        CRTS_dma_get(diagPtrBuffer, diagPtr_offset, sizeof(scalar) * rbl);
        CRTS_dma_get(psiPtrBuffer, psiPtr + rbs, sizeof(scalar) * rbl);

        for(label br = 0; br < rbl; ++br){
            ApsiPtrBuffer[br] = diagPtrBuffer[br] * psiPtrBuffer[br];
        }
        label index_block_start = DIV_INDEX_BLOCK_START(rbs);

        CRTS_dma_get(offDiagPtrBuffer, (scalar*)off_diag_value_Ptr + index_block_start, sizeof(scalar) * rbl * distance_count_);

        for(label dfcol = 0; dfcol < distance_count_; ++dfcol){
            // label index_dfcol_start = index_block_start + DIV_COL_OFFSET(dfcol);
            label distance = distanceListBuffer[dfcol];
            scalar* offDiagPtrBuffer_col = offDiagPtrBuffer + DIV_COL_OFFSET(dfcol);

            CRTS_dma_get(psiPtrBuffer, (scalar*)psiPtr + distance + rbs, sizeof(scalar) * rbl);

            if(distance + rbs >= 0 && distance + rbe <= row_){
                for(label br = 0; br < rbl; ++br){
                    ApsiPtrBuffer[br] += offDiagPtrBuffer_col[br] * psiPtrBuffer[br];
                }
            }else{
                for(label br = 0; br < rbl; ++br){
                    label col = distance + rbs + br;
                    if(col >= 0 && col < row_){
                        ApsiPtrBuffer[br] += offDiagPtrBuffer_col[br] * psiPtrBuffer[br];  
                    }        
                }      
            }
        }

        CRTS_dma_iput(ApsiPtr_offset, ApsiPtrBuffer, sizeof(scalar) * rbl, &put_reply);

    }

    int put_count = block_end - block_start;
    CRTS_dma_wait_value(&put_reply, put_count);
}