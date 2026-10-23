#ifndef LNJ_PERMUTATION_SGS_PARAM_H
#define LNJ_PERMUTATION_SGS_PARAM_H
#include "lnjLevelSgsParam.h"
struct lnjPermutationSgsParam {
    int64_t nRows,nLevels;
    const int64_t* levelStarts;
    const int64_t* rowPtr;
    const int64_t* columns;
    const double* values;
    const double* diagonal;
    double* rhs;
    double* psi;
    const double* originalRhs;
    double* originalPsi;
    const int64_t* packedToOriginal;
    const int64_t* originalToPacked;
    int64_t* errorFlags; // Exactly64 nonnull entries; zero=all finite.
};
#endif
