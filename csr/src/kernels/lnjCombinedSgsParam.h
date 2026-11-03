// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LNJ_COMBINED_SGS_PARAM_H
#define LNJ_COMBINED_SGS_PARAM_H
#include "lnjLevelSgsParam.h"
// New independent ABI/symbol. The frozen n54 120-byte header is NOT modified.
struct lnjCombinedSgsParam {
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
    int64_t* errorFlags;
    int64_t reusePackedPsi;
    const int64_t* boundaryTileStarts; // ORIGINAL aligned16 starts, immutable.
    int64_t nBoundaryTiles;
    int64_t fullUnpack; // host epoch FORCES1 on last sweep or unsupported path.
};
#endif
