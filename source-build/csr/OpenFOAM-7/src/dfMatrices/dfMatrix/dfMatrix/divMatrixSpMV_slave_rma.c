#include "dfMatrix_slave_common.h"
#include "dfMatrix_slave_param.h"

#define MAX_LOCAL_ROW 8192

void dfMatrix_SpMV_rma(dfMatrix_SpMV_rma_param_t* para_p){
    dfMatrix_SpMV_rma_param_t para;
    CRTS_dma_get(&para, para_p, sizeof(dfMatrix_SpMV_rma_param_t));

    const label ZERO = 0;
    scalar* ApsiPtr = para.ApsiPtr;
    const scalar* psiPtr = para.psiPtr;
    const scalar* diagPtr = para.diagPtr;
    const scalar* off_diag_value_Ptr = para.off_diag_value_Ptr;
    const label* distance_list_ = para.distance_list_;

    label distance_count_ = para.distance_count_;
    label block_count_ = para.block_count_;
    label row_block_bit_ = para.row_block_bit_;
    label row_block_size_ = para.row_block_size_;
    label row_ = para.row_;

    label local_psi_size_ = para.local_psi_size_;
    const label* local_distance_list_ = para.local_distance_list_;
    label rma_count_ = para.rma_count_;
    const int* rma_thread_ = para.rma_thread_;
    const label* rma_start_ = para.rma_start_;
    const label* rma_end_ = para.rma_end_;
    const label* local_psi_ptr_ = para.local_psi_ptr_;
    

    label block_start = LOCAL_START(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);
    label block_end = LOCAL_END(block_count_, CRTS_tid, CRTS_MAX_SPE_NUM);

    scalar localPsi[MAX_LOCAL_ROW * 3];

    scalar* localRowPsi = localPsi + local_distance_list_[0];

    label row_start = DIV_BLOCK_START(block_start);
    label row_end = DIV_BLOCK_START(block_end);
    label row_len = row_end - row_start;
    
    CRTS_dma_get(localRowPsi, psiPtr + row_start, row_len * sizeof(scalar));

    for(label bi = block_start; bi < block_end; ++bi){
        label rbs = DIV_BLOCK_START(bi);
        label rbe = DIV_BLOCK_END(rbs);
        label rbl = DIV_BLOCK_LEN(rbs,rbe);
        scalar* ApsiPtr_offset = ApsiPtr + rbs;
        const scalar* const __restrict__ diagPtr_offset = diagPtr + rbs;
        for(label br = 0; br < rbl; ++br){
            ApsiPtr_offset[br] = diagPtr_offset[br] * psiPtr[rbs + br];
        }
        label index_block_start = DIV_INDEX_BLOCK_START(rbs);
        for(label dfcol = 0; dfcol < distance_count_; ++dfcol){
            label index_dfcol_start = index_block_start + DIV_COL_OFFSET(dfcol);
            label distance = distance_list_[dfcol];
            if(distance + rbs >= 0 && distance + rbe <= row_){
                for(label br = 0; br < rbl; ++br){
                    ApsiPtr_offset[br] += off_diag_value_Ptr[index_dfcol_start + br] * psiPtr[distance + rbs + br];
                }
            }else{
                for(label br = 0; br < rbl; ++br){
                    label col = distance + rbs + br;
                    col = slave_max(col, ZERO);
                    col = slave_min(col, row_ - 1);
                    ApsiPtr_offset[br] += off_diag_value_Ptr[index_dfcol_start + br] * psiPtr[col];
                }       
            }
        }
    }

}