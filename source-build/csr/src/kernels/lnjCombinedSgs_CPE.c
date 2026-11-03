// SPDX-License-Identifier: GPL-3.0-or-later
// ONE spawn executes a complete forward/backward sweep on all 64 CPEs.
#include <crts.h>
#include "lnjCombinedSgsParam.h"

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

static inline double vectorValue(const struct lnjCombinedSgsParam* p,int64_t column,
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


// Missing lines go to distinct staging buffers. Initial cache hits remain
// untouched until ALL row terms have been consumed. Cache commit uses the
// LAST reference to each conflicting slot, matching the original ending tags.
static __thread_local double rowStage[8][LNJ_CACHE_DOUBLES] __attribute__((aligned(64)));
static __thread_local int64_t rowLines[8],rowLast[8];
static __thread_local unsigned char rowChoice[LNJ_COEFFICIENT_TILE],rowHit[8];
static inline int64_t prepareRow(const struct lnjCombinedSgsParam* p,
    int64_t first,int64_t last) {
    int64_t e,j,count=0;
    if(last-first>LNJ_COEFFICIENT_TILE)return 0;
    for(e=first;e<last;++e) {
        const int64_t line=localCols[e]/LNJ_CACHE_DOUBLES;
        for(j=0;j<count;++j)if(rowLines[j]==line)break;
        if(j==count) {
            if(count==8)return 0;
            rowLines[count++]=line;
        }
        rowChoice[e-first]=(unsigned char)j;
        rowLast[j]=e-first;
    }
    {
        crts_rply_t reply=0;
        unsigned int expectedReplies=0;
        for(j=0;j<count;++j) {
            const int64_t line=rowLines[j],slot=line%LNJ_CACHE_SLOTS;
            const int64_t begin=line*LNJ_CACHE_DOUBLES;
            const int64_t length=p->nRows-begin<LNJ_CACHE_DOUBLES?p->nRows-begin:LNJ_CACHE_DOUBLES;
            rowHit[j]=(unsigned char)(vectorTags[slot]==line);
            if(!rowHit[j]) {
                CRTS_dma_iget(rowStage[j],p->psi+begin,(unsigned long)(length*sizeof(double)),&reply);
                ++expectedReplies;
            }
        }
        if(expectedReplies)CRTS_dma_wait_value(&reply,expectedReplies);
    }
    return count;
}
static inline double rowVectorValue(const struct lnjCombinedSgsParam* p,
    int64_t column,struct lnjLevelSgsCounters* counters,int64_t staged,int64_t ordinal) {
    if(!staged)return vectorValue(p,column,counters);
    {
        const int64_t j=rowChoice[ordinal],offset=column%LNJ_CACHE_DOUBLES;
        return rowHit[j]?vectorCache[rowLines[j]%LNJ_CACHE_SLOTS][offset]:rowStage[j][offset];
    }
}
static inline void finishRow(const struct lnjCombinedSgsParam* p,int64_t count) {
    int64_t j,k;
    for(j=0;j<count;++j) {
        const int64_t line=rowLines[j],slot=line%LNJ_CACHE_SLOTS;
        int later=0;
        // Hit already resides in this slot and needs no copy. An earlier
        // colliding miss is suppressed by the same last-position test below.
        if(rowHit[j])continue;
        for(k=0;k<count;++k)if(rowLines[k]%LNJ_CACHE_SLOTS==slot&&rowLast[k]>rowLast[j]){later=1;break;}
        if(!later) {
            const int64_t begin=line*LNJ_CACHE_DOUBLES;
            const int64_t length=p->nRows-begin<LNJ_CACHE_DOUBLES?p->nRows-begin:LNJ_CACHE_DOUBLES;
            for(k=0;k<length;++k)vectorCache[slot][k]=rowStage[j][k];
            vectorTags[slot]=line;
        }
    }
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
static inline int64_t packOriginalVectors(const struct lnjCombinedSgsParam* p,
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
            if(!p->reusePackedPsi) {
                localResults[i]=permutationValue(p->originalPsi,p->nRows,original,0,counters);
                if(!__builtin_isfinite(localResults[i]))error|=1;
            }
            localRhs[i]=permutationValue(p->originalRhs,p->nRows,original,1,counters);
            if(!__builtin_isfinite(localRhs[i]))error|=2;
        }
        {
            crts_rply_t reply=0;
            if(!p->reusePackedPsi)
                CRTS_dma_iput(p->psi+tile,localResults,(unsigned long)(rows*sizeof(double)),&reply);
            CRTS_dma_iput(p->rhs+tile,localRhs,(unsigned long)(rows*sizeof(double)),&reply);
            const unsigned int expectedReplies=p->reusePackedPsi?1u:2u;
            CRTS_dma_wait_value(&reply,expectedReplies);
        }
        tile+=rows;
    }
    // Original vectors remain read-only until EVERY worker finishes packing.
    CRTS_ssync_array();
    invalidatePermutationCache();
    return error;
}
static inline int64_t unpackOriginalPsi(const struct lnjCombinedSgsParam* p,
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

// N74_PARTIAL_BEGIN: no math/pack/DMA-reply/barrier changes elsewhere.
static inline int64_t unpackBoundaryPsi(const struct lnjCombinedSgsParam* p,
    int64_t tid,struct lnjLevelSgsCounters* counters) {
    int64_t tile,error=0;
    CRTS_ssync_array();
    invalidatePermutationCache();
    // ALL authoritative final row values were checked when computed during
    // backward phase, not merely the boundary rows below; NO extra DMA scan.
    // Original16-row tiles are disjoint; all faceCells must be covered.
    // No additional LDM arrays, no ever-growing DMA reply counter.
    for(tile=tid;tile<p->nBoundaryTiles;tile+=LNJ_WORKERS) {
        int64_t originalFirst,i;
        getBytes(&originalFirst,p->boundaryTileStarts+tile,sizeof(int64_t),counters);
        const int64_t rows=p->nRows-originalFirst<LNJ_ROW_TILE?p->nRows-originalFirst:LNJ_ROW_TILE;
        getBytes(localCols,p->originalToPacked+originalFirst,(unsigned long)(rows*sizeof(int64_t)),counters);
        for(i=0;i<rows;++i) {
            localResults[i]=permutationValue(p->psi,p->nRows,localCols[i],0,counters);
            if(!__builtin_isfinite(localResults[i]))error|=4;
        }
        {
            crts_rply_t reply=0;
            CRTS_dma_iput(p->originalPsi+originalFirst,localResults,(unsigned long)(rows*sizeof(double)),&reply);
            CRTS_dma_wait_value(&reply,1);
        }
    }
    CRTS_ssync_array();
    return error;
}
// N74_PARTIAL_END

void lnjCombinedSgs_CPE(struct lnjCombinedSgsParam* globalParameter) {
    struct lnjCombinedSgsParam p __attribute__((aligned(64)));
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

                {
                    const int64_t coefficientFirst=localRowPtr[0];
                    const int64_t coefficientSpan=localRowPtr[rows]-coefficientFirst;
                    if(coefficientSpan<=LNJ_COEFFICIENT_TILE) {
                        // One contiguous columns+values fetch covers every row
                        // in this tile. Arithmetic/order/cache/barriers unchanged.
                        if(coefficientSpan>0) {
                            getBytes(localCols,p.columns+coefficientFirst,(unsigned long)(coefficientSpan*sizeof(int64_t)),&counters);
                            getBytes(localValues,p.values+coefficientFirst,(unsigned long)(coefficientSpan*sizeof(double)),&counters);
                        }
                        for(localRow=0;localRow<rows;++localRow) {
                            int64_t e;
                            const int64_t firstEntry=localRowPtr[localRow]-coefficientFirst;
                            const int64_t lastEntry=localRowPtr[localRow+1]-coefficientFirst;
                            double sum=localRhs[localRow];
                            const int64_t staged=prepareRow(&p,firstEntry,lastEntry);
                            for(e=firstEntry;e<lastEntry;++e)
                                sum-=localValues[e]*rowVectorValue(&p,localCols[e],&counters,staged,e-firstEntry);
                            finishRow(&p,staged);
                            localResults[localRow]=sum/localDiagonal[localRow];
                        }
                    }else {
                        // Unchanged bounded per-row/128-coefficient fallback.
                        for(localRow=0;localRow<rows;++localRow) {
                            int64_t entry=localRowPtr[localRow];
                            const int64_t end=localRowPtr[localRow+1];
                            double sum=localRhs[localRow];
                            while(entry<end) {
                                const int64_t length=end-entry<LNJ_COEFFICIENT_TILE ? end-entry : LNJ_COEFFICIENT_TILE;
                                int64_t e;
                                getBytes(localCols,p.columns+entry,(unsigned long)(length*sizeof(int64_t)),&counters);
                                getBytes(localValues,p.values+entry,(unsigned long)(length*sizeof(double)),&counters);
                                const int64_t staged=end-localRowPtr[localRow]<=LNJ_COEFFICIENT_TILE ? prepareRow(&p,0,length) : 0;
                                for(e=0;e<length;++e)
                                    sum-=localValues[e]*rowVectorValue(&p,localCols[e],&counters,staged,e);
                                finishRow(&p,staged);
                                entry+=length;
                            }
                            localResults[localRow]=sum/localDiagonal[localRow];
                        }
                    }
                }
                // N74_ALL_FINAL_RESULTS_FINITE_BEGIN
                // Every validated plan row is updated once during backward.
                // Check ALL final values before put, including hidden interior.
                // Do not reject a forward-only transient baseline never checked.
                if(phase==1)for(localRow=0;localRow<rows;++localRow)
                    if(!__builtin_isfinite(localResults[localRow]))permutationError|=4;
                // N74_ALL_FINAL_RESULTS_FINITE_END
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

    permutationError|=p.fullUnpack?unpackOriginalPsi(&p,tid,&counters):unpackBoundaryPsi(&p,tid,&counters);
    {
        crts_rply_t reply=0;
        CRTS_dma_iput(p.errorFlags+tid,&permutationError,sizeof(permutationError),&reply);
        CRTS_dma_wait_value(&reply,1);
    }

}
