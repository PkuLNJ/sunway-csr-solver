#include "lnjMulesParam.h"
#ifdef LNJ_PORTABLE_TEST
#include <string.h>
extern int lnjTestTid;
#define CRTS_tid lnjTestTid
#define CRTS_dma_get(dst,src,n) memcpy(dst,src,n)
#define CRTS_dma_put(dst,src,n) memcpy(dst,src,n)
#else
#include <crts.h>
#endif
#define BLOCK 128
#define SLOTS 8
typedef struct {int64_t tags[SLOTS];double p[SLOTS][BLOCK],m[SLOTS][BLOCK];} Cache;
static void gather(Cache* cache,const lnjMulesParam* p,int64_t cell,double* plus,double* minus){
    int64_t tile=cell/BLOCK,start=tile*BLOCK,n=p->cells-start;
    int slot=(int)(tile%SLOTS);if(n>BLOCK)n=BLOCK;
    if(cache->tags[slot]!=tile){
        CRTS_dma_get(cache->p[slot],p->b+start,n*sizeof(double));
        CRTS_dma_get(cache->m[slot],p->c+start,n*sizeof(double));
        cache->tags[slot]=tile;
    }
    *plus=cache->p[slot][cell-start];*minus=cache->m[slot][cell-start];
}
void lnjMules_CPE(lnjMulesParam* input){
    lnjMulesParam p;CRTS_dma_get(&p,input,sizeof(p));
    int64_t begin=p.count*CRTS_tid/64,end=p.count*(CRTS_tid+1)/64;
    double x[BLOCK],y[BLOCK],a[BLOCK],b[BLOCK],c[BLOCK],d[BLOCK];
    int64_t owner[BLOCK],neighbour[BLOCK];Cache cache;
    for(int i=0;i<SLOTS;++i)cache.tags[i]=-1;
    for(int64_t first=begin;first<end;first+=BLOCK){
        int64_t n=end-first;if(n>BLOCK)n=BLOCK;
        CRTS_dma_get(x,p.out0+first,n*sizeof(double));
        CRTS_dma_get(a,p.a+first,n*sizeof(double));
        if(p.mode==0){
            CRTS_dma_get(y,p.out1+first,n*sizeof(double));
            CRTS_dma_get(b,p.b+first,n*sizeof(double));
            CRTS_dma_get(c,p.c+first,n*sizeof(double));
            CRTS_dma_get(d,p.d+first,n*sizeof(double));
            for(int64_t i=0;i<n;++i){x[i]=lnjRatio(x[i],a[i],b[i],p.tiny);y[i]=lnjRatio(y[i],c[i],d[i],p.tiny);}
            CRTS_dma_put(p.out1+first,y,n*sizeof(double));
        }else{
            CRTS_dma_get(owner,p.owner+first,n*sizeof(int64_t));
            CRTS_dma_get(neighbour,p.neighbour+first,n*sizeof(int64_t));
            for(int64_t i=0;i<n;++i){double op,om,np,nm;
                gather(&cache,&p,owner[i],&op,&om);gather(&cache,&p,neighbour[i],&np,&nm);
                x[i]=lnjFace(x[i],a[i],op,om,np,nm);
            }
        }
        CRTS_dma_put(p.out0+first,x,n*sizeof(double));
    }
}
