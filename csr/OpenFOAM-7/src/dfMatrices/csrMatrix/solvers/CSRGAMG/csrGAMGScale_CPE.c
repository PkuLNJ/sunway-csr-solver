#define __sw_qd__

#ifdef __sw_qd__

//#define MAX_ROWS_PER_THREAD 4096
#define MAX_ROWS_PER_THREAD 9216
#define ATHREAD_NUM 64

#include "csrGAMGScale_slave_common.h"
#include "csrGAMGScale_slave_param.h"

__thread_local scalar r_s[MAX_ROWS_PER_THREAD] __attribute__((aligned(64)));

__thread_local crts_rply_t dma_rply = 0;
__thread_local unsigned int D_COUNT = 0;

void csrGAMGScale1_CPE(struct csrGAMGScale1_param_t* para_p){

    struct csrGAMGScale1_param_t para_s;
    label start;
    label row;
    label i, j;
    label rows_per_thread;

    CRTS_dma_iget(&para_s, para_p, sizeof(struct csrGAMGScale1_param_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

    rows_per_thread = para_s.nCells / ATHREAD_NUM;
    start = CRTS_tid * rows_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        rows_per_thread += para_s.nCells - (rows_per_thread * ATHREAD_NUM);
    }

    double result1[1] = {0.0};
    double result2[1] = {0.0};
    for(i=0; i<rows_per_thread; i++){
        row = start + i;
	result1[0] += para_s.sourcePtr[row] * para_s.fieldPtr[row];
	result2[0] += para_s.AcfPtr[row] * para_s.fieldPtr[row];
    }

    CRTS_dma_iput(para_s.scalingFactorNumPtr+CRTS_tid, &result1[0], sizeof(double), &dma_rply);
    CRTS_dma_iput(para_s.scalingFactorDenomPtr+CRTS_tid, &result2[0], sizeof(double), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

}

void csrGAMGScale2_CPE(struct csrGAMGScale2_param_t* para_p){

    struct csrGAMGScale2_param_t para_s;
    label start;
    label row;
    label i, j;
    label rows_per_thread;

    CRTS_dma_iget(&para_s, para_p, sizeof(struct csrGAMGScale2_param_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

    rows_per_thread = para_s.nCells / ATHREAD_NUM;
    start = CRTS_tid * rows_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        rows_per_thread += para_s.nCells - (rows_per_thread * ATHREAD_NUM);
    }

    for(i=0; i<rows_per_thread; i++){
        row = start + i;
        r_s[i] = para_s.sf * para_s.fieldPtr[row] + (para_s.sourcePtr[row] - para_s.sf * para_s.AcfPtr[row])/para_s.DPtr[row];
    }

    CRTS_dma_iput(para_s.fieldPtr+start, &r_s[0], rows_per_thread*sizeof(scalar), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

}

#endif
