#pragma once
#include <crts.h>
#include <assert.h>
#include "GenMatrix_slave_common.h"

#ifdef __cplusplus
namespace Foam{
extern "C" {
#endif

struct GenMatrix_UEqn_param_1_t{
    const label nCells;
    const label nFaces;
    scalar* const diagPtr;
    const scalar* const weightsPtr;
    const scalar* const rhoPhiPtr;
    scalar* const lowerPtr;
    scalar* const upperPtr;
};

struct GenMatrix_UEqn_param_2_t{
    const label nCells;
    const label nFaces;
    scalar rDeltaT;
    scalar* const diagPtr;
    const scalar* const rhoPtr;
    const scalar* const meshVscPtr;
    scalar* const sourcePtr;
    const scalar* const rhoOldTimePtr;
    const scalar* const UOldTimePtr;
    const scalar* const deltaCoeffsPtr;
    const scalar* const gammaMagSfPtr;
    scalar* const lowerPtr;
    scalar* const upperPtr;
};

#ifdef __cplusplus
}
}
#endif
