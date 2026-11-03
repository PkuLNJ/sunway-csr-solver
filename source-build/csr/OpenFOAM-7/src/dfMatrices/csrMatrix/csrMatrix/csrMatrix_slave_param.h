#pragma once
#include <crts.h>
#include <assert.h>
#include "csrMatrix_slave_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// result = A (offDiag & diag) * b
struct csrMatrix_SpMV_param_t{
    label nRow;
    const label* const rowPtr;
    const label* const colPtr;
    const scalar* const offDiagPtr;
    const scalar* const diagPtr;
    const scalar* const bPtr;
    scalar* const resultPtr;
};


#ifdef __cplusplus
}
#endif