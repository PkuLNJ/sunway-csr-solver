#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

void dfMatrix_sumA_naive(dfMatrix_sumA_param_t* para){
    const label ZERO = 0;

    scalar* sumAPtr = para->sumAPtr;
    const scalar* diagPtr = para->diagPtr;
    const scalar* off_diag_value_Ptr = para->off_diag_value_Ptr;

    label row_ = para->row_;
    label distance_count_ = para->distance_count_;
    label block_count_ = para->block_count_;
    label row_block_bit_ = para->row_block_bit_;
    label row_block_size_ = para->row_block_size_;
    
    label block_start = LOCAL_START(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);
    label block_end = LOCAL_END(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);
    
    for(label bi = block_start; bi < block_end; ++bi){
        label rbs = DIV_BLOCK_START(bi);
        label rbe = DIV_BLOCK_END(rbs);
        label rbl = DIV_BLOCK_LEN(rbs,rbe);
        scalar* __restrict__ sumAPtr_offset = sumAPtr + rbs;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rbs;
        for(label br = 0; br < rbl; ++br){
            sumAPtr_offset[br] = diagPtr_offset[br];
        }
        label index_block_start = DIV_INDEX_BLOCK_START(rbs);
        for(label dfcol = 0; dfcol < distance_count_; ++dfcol){
            label index_dfcol_start = index_block_start + DIV_COL_OFFSET(dfcol);
            for(label br = 0; br < rbl; ++br){
                sumAPtr_offset[br] += off_diag_value_Ptr[index_dfcol_start + br];
            }
        }
    }

}