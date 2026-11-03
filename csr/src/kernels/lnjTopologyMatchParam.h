#ifndef LNJ_TOPOLOGY_MATCH_PARAM_H
#define LNJ_TOPOLOGY_MATCH_PARAM_H
#include <stdint.h>
struct lnjTopologyMatchParam {
    int64_t rowPtrSize,entries;
    const int64_t* cachedRowPtr;
    const int64_t* currentRowPtr;
    const int64_t* cachedColumns;
    const int64_t* currentColumns;
    int64_t* mismatchFlags; //64 entries: bit1=rowptr,bit2=columns. All workers report.
};
#endif
