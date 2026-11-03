#define __sw_qd__

#ifdef __sw_qd__

//#define MAX_ROWS_PER_THREAD 4096
#define MAX_ROWS_PER_THREAD 9216
#define ATHREAD_NUM 64

#include "csrMatrix_slave_common.h"
#include "csrMatrix_slave_param.h"

__thread_local scalar r_s[MAX_ROWS_PER_THREAD] __attribute__((aligned(64)));

__thread_local crts_rply_t dma_rply = 0;
__thread_local unsigned int D_COUNT = 0;

void csrMatrix_SpMV_CPE(struct csrMatrix_SpMV_param_t* para_p){

    struct csrMatrix_SpMV_param_t para_s;
    label start;
    label row;
    label i, j;
    label rows_per_thread;

    /* 1. get parameters from host */
    CRTS_dma_iget(&para_s, para_p, sizeof(struct csrMatrix_SpMV_param_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);
    
    rows_per_thread = para_s.nRow / ATHREAD_NUM;
    start = CRTS_tid * rows_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        rows_per_thread += para_s.nRow - (rows_per_thread * ATHREAD_NUM);
    }

    /* 2. coculate c */
    //scalar tmp;
    for(i=0; i<rows_per_thread; i++){
        row = start + i;
        r_s[i] = para_s.diagPtr[row] * para_s.bPtr[row];
        //tmp = para_s.diagPtr[row] * para_s.bPtr[row];
        for(j=para_s.rowPtr[row]; j<para_s.rowPtr[row+1]; ++j){
            r_s[i] += para_s.offDiagPtr[j] * para_s.bPtr[para_s.colPtr[j]];
            //tmp += para_s.offDiagPtr[j] * para_s.bPtr[para_s.colPtr[j]];
	}
	//para_s.resultPtr[row] = tmp;
    }

    /* 3. put C back to host */
    CRTS_dma_iput(para_s.resultPtr+start, &r_s[0], rows_per_thread*sizeof(scalar), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

}

#endif
