// SPDX-License-Identifier: GPL-3.0-or-later
// ONE spawn executes a complete forward/backward sweep on all 64 CPEs.
#include <crts.h>
#include "lnjLevelSgsParam.h"

#define LNJ_WORKERS 64
#define LNJ_ROW_TILE 16
#define LNJ_COEFFICIENT_TILE 128
#define LNJ_CACHE_SLOTS 64
#define LNJ_CACHE_DOUBLES 8
#define LNJ_STARTS_CACHE 257

static __thread_local int64_t localStarts[LNJ_STARTS_CACHE] __attribute__((aligned(64)));
static __thread_local int64_t localRowPtr[LNJ_ROW_TILE+1] __attribute__((aligned(64)));
static __thread_local int64_t localCols[LNJ_COEFFICIENT_TILE] __attribute__((aligned(64)));
static __thread_local double localValues[LNJ_COEFFICIENT_TILE] __attribute__((aligned(64)));
static __thread_local double localDiagonal[LNJ_ROW_TILE] __attribute__((aligned(64)));
static __thread_local double localRhs[LNJ_ROW_TILE] __attribute__((aligned(64)));
static __thread_local double localResults[LNJ_ROW_TILE] __attribute__((aligned(64)));
static __thread_local double vectorCache[LNJ_CACHE_SLOTS][LNJ_CACHE_DOUBLES] __attribute__((aligned(64)));
static __thread_local int64_t vectorTags[LNJ_CACHE_SLOTS] __attribute__((aligned(64)));

static inline void getBytes(void* local,const void* global,unsigned long bytes,
                           struct lnjLevelSgsCounters* counters) {
    crts_rply_t reply=0;
    CRTS_dma_iget(local,global,bytes,&reply);
    CRTS_dma_wait_value(&reply,1);
    (void)counters;
}

static inline double vectorValue(const struct lnjLevelSgsParam* p,int64_t column,
                                struct lnjLevelSgsCounters* counters) {
    const int64_t line=column/LNJ_CACHE_DOUBLES;
    const int64_t slot=line%LNJ_CACHE_SLOTS;
    if(vectorTags[slot]!=line) {
        const int64_t first=line*LNJ_CACHE_DOUBLES;
        const int64_t count=p->nRows-first<LNJ_CACHE_DOUBLES ? p->nRows-first : LNJ_CACHE_DOUBLES;
        getBytes(vectorCache[slot],p->psi+first,(unsigned long)(count*sizeof(double)),counters);
        vectorTags[slot]=line;

    }
    return vectorCache[slot][column%LNJ_CACHE_DOUBLES];
}

void lnjLevelSgs_CPE(struct lnjLevelSgsParam* globalParameter) {
    struct lnjLevelSgsParam p __attribute__((aligned(64)));
    struct lnjLevelSgsCounters counters={0,0,0,0,0,0,0,0};
    int64_t phase,ordinal,slot;
    const int64_t tid=(int64_t)CRTS_tid;
    getBytes(&p,globalParameter,sizeof(p),&counters);
    if(p.nLevels+1<=LNJ_STARTS_CACHE)
        getBytes(localStarts,p.levelStarts,(unsigned long)((p.nLevels+1)*sizeof(int64_t)),&counters);

    // No CPE returns early: empty work ranges still execute EVERY barrier.
    for(phase=0;phase<2;++phase) {
        for(ordinal=0;ordinal<p.nLevels;++ordinal) {
            const int64_t level=phase==0 ? ordinal : p.nLevels-1-ordinal;
            int64_t endpoints[2],first,last,count,quotient,remainder,start,stop,tile;
            if(p.nLevels+1<=LNJ_STARTS_CACHE) {
                first=localStarts[level];last=localStarts[level+1];
            }else {
                getBytes(endpoints,p.levelStarts+level,2*sizeof(int64_t),&counters);
                first=endpoints[0];last=endpoints[1];
            }
            // All prior-level DMA writes completed BEFORE the prior barrier.
            // Flush software psi cache: no stale read is allowed after a level.
            for(slot=0;slot<LNJ_CACHE_SLOTS;++slot)vectorTags[slot]=-1;
            count=last-first;quotient=count/LNJ_WORKERS;remainder=count%LNJ_WORKERS;
            start=first+quotient*tid+(tid<remainder?tid:remainder);
            stop=start+quotient+(tid<remainder?1:0);
            for(tile=start;tile<stop;) {
                const int64_t rows=stop-tile<LNJ_ROW_TILE ? stop-tile : LNJ_ROW_TILE;
                int64_t localRow;
                getBytes(localRowPtr,p.rowPtr+tile,(unsigned long)((rows+1)*sizeof(int64_t)),&counters);
                getBytes(localDiagonal,p.diagonal+tile,(unsigned long)(rows*sizeof(double)),&counters);
                getBytes(localRhs,p.rhs+tile,(unsigned long)(rows*sizeof(double)),&counters);

                for(localRow=0;localRow<rows;++localRow) {
                    int64_t entry=localRowPtr[localRow];
                    const int64_t end=localRowPtr[localRow+1];
                    double sum=localRhs[localRow];
                    // Coefficients/columns move in bounded blocks, not one DMA
                    // per nonzero. psi gathers use an explicitly invalidated
                    // 4KiB software line cache, not stale hardware-cache loads.
                    while(entry<end) {
                        const int64_t length=end-entry<LNJ_COEFFICIENT_TILE ? end-entry : LNJ_COEFFICIENT_TILE;
                        int64_t e;
                        getBytes(localCols,p.columns+entry,(unsigned long)(length*sizeof(int64_t)),&counters);
                        getBytes(localValues,p.values+entry,(unsigned long)(length*sizeof(double)),&counters);

                        for(e=0;e<length;++e)
                            sum-=localValues[e]*vectorValue(&p,localCols[e],&counters);
                        entry+=length;
                    }
                    localResults[localRow]=sum/localDiagonal[localRow];
                }
                {
                    crts_rply_t reply=0;
                    CRTS_dma_iput(p.psi+tile,localResults,(unsigned long)(rows*sizeof(double)),&reply);
                    CRTS_dma_wait_value(&reply,1);

                }

                tile+=rows;
            }
            // Authoritative target crts.h expands this to synr+sync with a
            // compiler memory clobber. The completed DMA writes plus this
            // whole-array barrier are exercised by the target chain tests.
            CRTS_ssync_array();

        }
    }

}
