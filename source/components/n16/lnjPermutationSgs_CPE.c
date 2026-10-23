// SPDX-License-Identifier: GPL-3.0-or-later
// ONE spawn executes a complete forward/backward sweep on all 64 CPEs.
#include <crts.h>
#include "lnjPermutationSgsParam.h"

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

static inline double vectorValue(const struct lnjPermutationSgsParam* p,int64_t column,
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


// field0=psi,field1=rhs. Tags distinguish two original vectors even when their
// line numbers coincide. SGS uses the same storage only after invalidation.
static inline double permutationValue(const double* source,int64_t rows,
    int64_t index,int64_t field,struct lnjLevelSgsCounters* counters) {
    const int64_t line=index/LNJ_CACHE_DOUBLES;
    const int64_t tag=line*2+field;
    const int64_t slot=tag%LNJ_CACHE_SLOTS;
    if(vectorTags[slot]!=tag) {
        const int64_t first=line*LNJ_CACHE_DOUBLES;
        const int64_t count=rows-first<LNJ_CACHE_DOUBLES ? rows-first : LNJ_CACHE_DOUBLES;
        getBytes(vectorCache[slot],source+first,(unsigned long)(count*sizeof(double)),counters);
        vectorTags[slot]=tag;
    }
    return vectorCache[slot][index%LNJ_CACHE_DOUBLES];
}
static inline void invalidatePermutationCache(void) {
    int64_t slot;
    for(slot=0;slot<LNJ_CACHE_SLOTS;++slot)vectorTags[slot]=-1;
}
static inline int64_t packOriginalVectors(const struct lnjPermutationSgsParam* p,
    int64_t tid,struct lnjLevelSgsCounters* counters) {
    const int64_t quotient=p->nRows/LNJ_WORKERS,remainder=p->nRows%LNJ_WORKERS;
    const int64_t first=quotient*tid+(tid<remainder?tid:remainder);
    const int64_t stop=first+quotient+(tid<remainder?1:0);
    int64_t tile,error=0;
    invalidatePermutationCache();
    for(tile=first;tile<stop;) {
        const int64_t rows=stop-tile<LNJ_ROW_TILE ? stop-tile : LNJ_ROW_TILE;
        int64_t i;
        getBytes(localCols,p->packedToOriginal+tile,(unsigned long)(rows*sizeof(int64_t)),counters);
        for(i=0;i<rows;++i) {
            const int64_t original=localCols[i];
            localResults[i]=permutationValue(p->originalPsi,p->nRows,original,0,counters);
            localRhs[i]=permutationValue(p->originalRhs,p->nRows,original,1,counters);
            if(!__builtin_isfinite(localResults[i]))error|=1;
            if(!__builtin_isfinite(localRhs[i]))error|=2;
        }
        {
            crts_rply_t reply=0;
            CRTS_dma_iput(p->psi+tile,localResults,(unsigned long)(rows*sizeof(double)),&reply);
            CRTS_dma_iput(p->rhs+tile,localRhs,(unsigned long)(rows*sizeof(double)),&reply);
            CRTS_dma_wait_value(&reply,2);
        }
        tile+=rows;
    }
    // Original vectors remain read-only until EVERY worker finishes packing.
    CRTS_ssync_array();
    invalidatePermutationCache();
    return error;
}
static inline int64_t unpackOriginalPsi(const struct lnjPermutationSgsParam* p,
    int64_t tid,struct lnjLevelSgsCounters* counters) {
    const int64_t quotient=p->nRows/LNJ_WORKERS,remainder=p->nRows%LNJ_WORKERS;
    const int64_t first=quotient*tid+(tid<remainder?tid:remainder);
    const int64_t stop=first+quotient+(tid<remainder?1:0);
    int64_t tile,error=0;
    // Explicit phase boundary, including nRows=0/nLevels=0.
    CRTS_ssync_array();
    invalidatePermutationCache();
    for(tile=first;tile<stop;) {
        const int64_t rows=stop-tile<LNJ_ROW_TILE ? stop-tile : LNJ_ROW_TILE;
        int64_t i;
        getBytes(localCols,p->originalToPacked+tile,(unsigned long)(rows*sizeof(int64_t)),counters);
        for(i=0;i<rows;++i) {
            localResults[i]=permutationValue(p->psi,p->nRows,localCols[i],0,counters);
            if(!__builtin_isfinite(localResults[i]))error|=4;
        }
        {
            crts_rply_t reply=0;
            CRTS_dma_iput(p->originalPsi+tile,localResults,(unsigned long)(rows*sizeof(double)),&reply);
            CRTS_dma_wait_value(&reply,1);
        }
        tile+=rows;
    }
    CRTS_ssync_array();
    return error;
}

void lnjPermutationSgs_CPE(struct lnjPermutationSgsParam* globalParameter) {
    struct lnjPermutationSgsParam p __attribute__((aligned(64)));
    struct lnjLevelSgsCounters counters={0,0,0,0,0,0,0,0};
    int64_t phase,ordinal,slot;
    int64_t permutationError=0;
    const int64_t tid=(int64_t)CRTS_tid;
    getBytes(&p,globalParameter,sizeof(p),&counters);
    if(p.nLevels+1<=LNJ_STARTS_CACHE)
        getBytes(localStarts,p.levelStarts,(unsigned long)((p.nLevels+1)*sizeof(int64_t)),&counters);

    permutationError=packOriginalVectors(&p,tid,&counters);

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

    permutationError|=unpackOriginalPsi(&p,tid,&counters);
    {
        crts_rply_t reply=0;
        CRTS_dma_iput(p.errorFlags+tid,&permutationError,sizeof(permutationError),&reply);
        CRTS_dma_wait_value(&reply,1);
    }

}
