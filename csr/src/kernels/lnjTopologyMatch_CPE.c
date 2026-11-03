// SPDX-License-Identifier: GPL-3.0-or-later
// Full exact int64 scans. NO hash, pointer shortcut, static TLS, or early exit.
#include <crts.h>
#include "lnjTopologyMatchParam.h"
#define LNJ_TM_TILE 128
static inline void tmGet(void* dst,const void* src,unsigned long bytes){
    crts_rply_t reply=0;CRTS_dma_iget(dst,src,bytes,&reply);CRTS_dma_wait_value(&reply,1);
}
void lnjTopologyMatch_CPE(struct lnjTopologyMatchParam* globalParameter){
    struct lnjTopologyMatchParam p __attribute__((aligned(64)));
    int64_t left[LNJ_TM_TILE] __attribute__((aligned(64)));
    int64_t right[LNJ_TM_TILE] __attribute__((aligned(64)));
    const int64_t tid=(int64_t)CRTS_tid;
    int64_t mismatch=0,phase;
    tmGet(&p,globalParameter,sizeof(p));
    for(phase=0;phase<2;++phase){
        const int64_t count=phase==0?p.rowPtrSize:p.entries;
        const int64_t* a=phase==0?p.cachedRowPtr:p.cachedColumns;
        const int64_t* b=phase==0?p.currentRowPtr:p.currentColumns;
        const int64_t q=count/64,rem=count%64;
        const int64_t first=q*tid+(tid<rem?tid:rem);
        const int64_t stop=first+q+(tid<rem?1:0);
        int64_t tile;
        for(tile=first;tile<stop;){
            const int64_t length=stop-tile<LNJ_TM_TILE?stop-tile:LNJ_TM_TILE;
            crts_rply_t reply=0;int64_t i;
            CRTS_dma_iget(left,a+tile,(unsigned long)(length*sizeof(int64_t)),&reply);
            CRTS_dma_iget(right,b+tile,(unsigned long)(length*sizeof(int64_t)),&reply);
            CRTS_dma_wait_value(&reply,2);
            for(i=0;i<length;++i)if(left[i]!=right[i])mismatch|=phase==0?1:2;
            tile+=length;
        }
    }
    {crts_rply_t reply=0;CRTS_dma_iput(p.mismatchFlags+tid,&mismatch,sizeof(mismatch),&reply);
     CRTS_dma_wait_value(&reply,1);}
}
