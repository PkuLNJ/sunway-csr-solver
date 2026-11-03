#ifndef LNJ_LEVEL_SGS_PARAM_H
#define LNJ_LEVEL_SGS_PARAM_H
#include <stdint.h>

struct lnjLevelSgsCounters {
    int64_t rows, coefficientBlocks, vectorCacheHits, vectorCacheMisses;
    int64_t dmaGetBytes, dmaPutBytes, barriers, metadataTiles;
};

struct lnjLevelSgsParam {
    int64_t nRows, nLevels;
    const int64_t* levelStarts;
    const int64_t* rowPtr;
    const int64_t* columns;
    const double* values;
    const double* diagonal;
    const double* rhs;
    double* psi;
    struct lnjLevelSgsCounters* counters; /* Exactly 64 records, or null. */
};
#endif
