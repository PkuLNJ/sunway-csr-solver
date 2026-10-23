//#define __sw_qd__

#ifdef __sw_qd__

//#define MAX_ROWS_PER_THREAD 4096
#define MAX_ROWS_PER_THREAD 9216
#define ATHREAD_NUM 64

#include "csrJacobiSmoother_slave_common.h"
#include "csrJacobiSmoother_slave_param.h"

__thread_local scalar r_s[MAX_ROWS_PER_THREAD] __attribute__((aligned(64)));

__thread_local crts_rply_t dma_rply = 0;
__thread_local unsigned int D_COUNT = 0;

void csrJacobiSmoother_CPE(struct csrJacobiSmoother_param_t* para_p){

    struct csrJacobiSmoother_param_t para_s;
    label start;
    label row;
    label i, j;
    label rows_per_thread;

    /* 1. get parameters from host */
    CRTS_dma_iget(&para_s, para_p, sizeof(struct csrJacobiSmoother_param_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);
    
    rows_per_thread = para_s.nCells / ATHREAD_NUM;
    start = CRTS_tid * rows_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        rows_per_thread += para_s.nCells - (rows_per_thread * ATHREAD_NUM);
    }

    /* 2. coculate c */
    //scalar tmp;
    for(i=0; i<rows_per_thread; i++){
        row = start + i;
        r_s[i] = para_s.bPrimePtr[row];
        for(j=para_s.off_diag_rowptr_Ptr[row]; j<para_s.off_diag_rowptr_Ptr[row+1]; ++j){
            r_s[i] -= para_s.off_diag_value_Ptr[j] * para_s.psiCopyPtr[para_s.off_diag_colidx_Ptr[j]];
	    }
        r_s[i] = r_s[i] / para_s.diagPtr[row];
    }

    /* 3. put C back to host */
    CRTS_dma_iput(para_s.psiPtr+start, &r_s[0], rows_per_thread*sizeof(scalar), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

}

#endif
