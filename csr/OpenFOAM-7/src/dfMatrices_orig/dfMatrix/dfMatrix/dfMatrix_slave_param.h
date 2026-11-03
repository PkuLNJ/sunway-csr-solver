#pragma once
#include <crts.h>
#include <assert.h>
#include "dfMatrix_slave_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dfMatrix_Block_CSR_SpMV_param_t{
    scalar* const ApsiPtr;
    const scalar* const psiPtr;
    const scalar* const diagPtr;
    label rowBlockCount;
    const label* const rowBlockPtr;
    const char* const blockFlagList;
    const label* const blockRowList;
    const label** const blockRowPtrList;
    const label** const blockColIdxList;
    const scalar** const blockValuesList;
};

struct dfMatrix_Block_CSR_GaussSeidel_param_t{
    scalar* const psiPtr;
    scalar* const bPrimePtr;
    const scalar* const diagPtr;
    label rowBlockCount;
    const label* const rowBlockPtr;
    const char* const blockFlagList;
    const label* const blockRowList;
    const label** const blockRowPtrList;
    const label** const blockColIdxList;
    const scalar** const blockValuesList;
};

#ifdef __cplusplus
}
#endif