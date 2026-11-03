#pragma once
#include <crts.h>
#include <assert.h>
#include "csrGAMGScale_slave_common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct csrGAMGScale1_param_t{
    const label nCells;
    double* scalingFactorNumPtr;
    double* scalingFactorDenomPtr;
    scalar* fieldPtr;
    const scalar* const sourcePtr;
    const scalar* const AcfPtr;
};

struct csrGAMGScale2_param_t{
    const label nCells;
    scalar* fieldPtr;
    const scalar sf;
    const scalar* const sourcePtr;
    const scalar* const AcfPtr;
    const scalar* const DPtr;
};

#ifdef __cplusplus
}
#endif
