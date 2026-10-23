#pragma once
#include <crts.h>
#include <assert.h>
#include "csrJacobiSmoother_slave_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// result = A (offDiag & diag) * b
struct csrJacobiSmoother_param_t{
    const label nCells;
    const scalar* const bPrimePtr;
    const label* const off_diag_rowptr_Ptr;
    const scalar* const off_diag_value_Ptr;
    const label* const off_diag_colidx_Ptr;
    const scalar* const psiCopyPtr;
    scalar* const psiPtr;
    const scalar* const diagPtr;
};


#ifdef __cplusplus
}
#endif