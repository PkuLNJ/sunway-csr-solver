#ifdef USE_ATHREAD

#include <slave.h>
#include <csrMatrix.H>

void spmv_athread(struct spmv_param* sp){
    for (label r = 0; r < sp._row; ++r){
        scalar tmp = sp._diagPtr[r];
        for(label index = sp._off_diag_rowptr[r]; index < sp._off_diag_rowptr[r+1]; ++index){
            tmp += sp._off_diag_value[index];
        }
        sp._sumAPtr[r] = tmp;
    } 


}

#endif
