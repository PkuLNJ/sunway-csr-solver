// SPDX-License-Identifier: GPL-3.0-or-later
// All storage is AUTOMATIC: no static/TLS LDM allocation across slave kernels.
#include <crts.h>
#include "lnjNumericPackParam.h"
#define LNJ_NP_TILE 128
#define LNJ_NP_SLOTS 64
#define LNJ_NP_LINE 8
static inline void npGet(void* dst,const void* src,unsigned long bytes){
    crts_rply_t reply=0;CRTS_dma_iget(dst,src,bytes,&reply);CRTS_dma_wait_value(&reply,1);
}
static inline void npPut(void* dst,const void* src,unsigned long bytes){
    crts_rply_t reply=0;CRTS_dma_iput(dst,src,bytes,&reply);CRTS_dma_wait_value(&reply,1);
}
static inline double npGather(const double* input,int64_t count,int64_t index,
    double cache[LNJ_NP_SLOTS][LNJ_NP_LINE],int64_t tags[LNJ_NP_SLOTS]){
    const int64_t line=index/LNJ_NP_LINE,slot=line%LNJ_NP_SLOTS;
    if(tags[slot]!=line){
        const int64_t first=line*LNJ_NP_LINE;
        const int64_t length=count-first<LNJ_NP_LINE?count-first:LNJ_NP_LINE;
        npGet(cache[slot],input+first,(unsigned long)(length*sizeof(double)));tags[slot]=line;
    }
    return cache[slot][index%LNJ_NP_LINE];
}
void lnjNumericPack_CPE(struct lnjNumericPackParam* globalParameter){
    struct lnjNumericPackParam p __attribute__((aligned(64)));
    int64_t indices[LNJ_NP_TILE] __attribute__((aligned(64)));
    double output[LNJ_NP_TILE] __attribute__((aligned(64)));
    double cache[LNJ_NP_SLOTS][LNJ_NP_LINE] __attribute__((aligned(64)));
    int64_t tags[LNJ_NP_SLOTS] __attribute__((aligned(64)));
    int64_t error=0,phase,slot;
    const int64_t tid=(int64_t)CRTS_tid;
    npGet(&p,globalParameter,sizeof(p));
    for(phase=0;phase<2;++phase){
        // Explicit cache invalidation separates coefficient and diagonal fields.
        for(slot=0;slot<LNJ_NP_SLOTS;++slot)tags[slot]=-1;
        const int64_t count=phase==0?p.nEntries:p.nRows;
        const int64_t* map=phase==0?p.packedEntryToOriginal:p.packedToOriginal;
        const double* input=phase==0?p.originalValues:p.originalDiagonal;
        double* result=phase==0?p.packedValues:p.packedDiagonal;
        const int64_t q=count/64,rem=count%64;
        const int64_t first=q*tid+(tid<rem?tid:rem);
        const int64_t stop=first+q+(tid<rem?1:0);
        int64_t tile;
        for(tile=first;tile<stop;){
            const int64_t size=stop-tile<LNJ_NP_TILE?stop-tile:LNJ_NP_TILE;
            int64_t i;npGet(indices,map+tile,(unsigned long)(size*sizeof(int64_t)));
            for(i=0;i<size;++i){
                const int64_t original=indices[i];
                if(original<0 || original>=count){error|=4;output[i]=0;continue;}
                output[i]=npGather(input,count,original,cache,tags);
                if(phase==0){if(!__builtin_isfinite(output[i]))error|=1;}
                else if(!__builtin_isfinite(output[i]) || output[i]==0)error|=2;
            }
            npPut(result+tile,output,(unsigned long)(size*sizeof(double)));tile+=size;
        }
    }
    // No cross-worker dependencies: DMA completion + host join is sufficient.
    npPut(p.errorFlags+tid,&error,sizeof(error));
}
