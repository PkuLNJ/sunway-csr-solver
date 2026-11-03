#ifndef LNJ_NUMERIC_PACK_PARAM_H
#define LNJ_NUMERIC_PACK_PARAM_H
#include <stdint.h>
// Independent 72-byte ABI. Existing SGS 112-byte ABI is untouched.
struct lnjNumericPackParam {
    int64_t nRows,nEntries;
    const int64_t* packedEntryToOriginal;
    const int64_t* packedToOriginal;
    const double* originalValues;
    const double* originalDiagonal;
    double* packedValues;
    double* packedDiagonal;
    int64_t* errorFlags; // 64 entries: 1=nonfinite coefficient;2=bad diagonal;4=bad map.
};
#endif
