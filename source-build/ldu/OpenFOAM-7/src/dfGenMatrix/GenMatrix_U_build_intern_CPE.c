#define __sw_qd__

#ifdef __sw_qd__

#define ATHREAD_NUM 64

#include "GenMatrix_slave_common.h"
#include "GenMatrix_slave_param.h"

__thread_local crts_rply_t dma_rply = 0;
__thread_local unsigned int D_COUNT = 0;

void build_UEqn_intern_1_CPE(struct GenMatrix_UEqn_param_1_t* para_p){
    
    struct GenMatrix_UEqn_param_1_t para_s;
    label start_cell, start_face, end_cell, end_face;
    label i,j;
    label cells_per_thread, faces_per_thread;
    scalar flux;

    CRTS_dma_iget(&para_s, para_p, sizeof(struct GenMatrix_UEqn_param_1_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

    cells_per_thread = para_s.nCells / ATHREAD_NUM;
    faces_per_thread = para_s.nFaces / ATHREAD_NUM;

    start_cell = CRTS_tid * cells_per_thread;
    start_face = CRTS_tid * faces_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        cells_per_thread += para_s.nCells - (cells_per_thread * ATHREAD_NUM);
        faces_per_thread += para_s.nFaces - (faces_per_thread * ATHREAD_NUM);
    }

    end_cell = start_cell + cells_per_thread;
    end_face = start_face + faces_per_thread;

    for(i=start_cell; i<end_cell; i++){
        para_s.diagPtr[i] = 0.0;
    }

    for(j=start_face; j<end_face; j++){
        flux = para_s.weightsPtr[j] * para_s.weightsPtr[j];
        para_s.lowerPtr[j] = -flux;
        para_s.upperPtr[j] = -flux + para_s.rhoPhiPtr[j]; 
    }

}

void build_UEqn_intern_2_CPE(struct GenMatrix_UEqn_param_2_t* para_p){
    
    struct GenMatrix_UEqn_param_2_t para_s;
    label start_cell, start_face, end_cell, end_face;
    label i,j;
    label cells_per_thread, faces_per_thread;
    scalar coeffs, tmp;

    CRTS_dma_iget(&para_s, para_p, sizeof(struct GenMatrix_UEqn_param_2_t), &dma_rply);
    D_COUNT++;
    CRTS_dma_wait_value(&dma_rply, D_COUNT);

    cells_per_thread = para_s.nCells / ATHREAD_NUM;
    faces_per_thread = para_s.nFaces / ATHREAD_NUM;

    start_cell = CRTS_tid * cells_per_thread;
    start_face = CRTS_tid * faces_per_thread;

    if(CRTS_tid == ATHREAD_NUM - 1){
        cells_per_thread += para_s.nCells - (cells_per_thread * ATHREAD_NUM);
        faces_per_thread += para_s.nFaces - (faces_per_thread * ATHREAD_NUM);
    }

    end_cell = start_cell + cells_per_thread;
    end_face = start_face + faces_per_thread;

    for(i=start_cell; i<end_cell; i++){
        para_s.diagPtr[i] += para_s.rDeltaT * para_s.rhoPtr[i] * para_s.meshVscPtr[i];
        tmp = para_s.rDeltaT * para_s.rhoOldTimePtr[i] * para_s.meshVscPtr[i];
        para_s.sourcePtr[i * 3] += para_s.UOldTimePtr[i * 3] * tmp;
        para_s.sourcePtr[i * 3 + 1] += para_s.UOldTimePtr[i * 3 + 1] * tmp;
        para_s.sourcePtr[i * 3 + 2] += para_s.UOldTimePtr[i * 3 + 2] * tmp;
    }

    for(j=start_face; j<end_face; j++){
        coeffs = para_s.deltaCoeffsPtr[j] * para_s.gammaMagSfPtr[j];
        para_s.upperPtr[j] -= coeffs;
        para_s.lowerPtr[j] -= coeffs; 
    }

}

#endif
