#include "lnjNumericPackParam.h"
#include "lnjTopologyMatchParam.h"
#include <cmath>
// Keep the original worker partition for identical per-worker error reporting.
extern "C" void lnjNumericPack_HOST(lnjNumericPackParam* p) {
    for (int64_t tid=0;tid<64;++tid) {
        int64_t error=0;
        for(int phase=0;phase<2;++phase) {
            const int64_t count=phase==0?p->nEntries:p->nRows;
            const auto map=phase==0?p->packedEntryToOriginal:p->packedToOriginal;
            const auto input=phase==0?p->originalValues:p->originalDiagonal;
            auto result=phase==0?p->packedValues:p->packedDiagonal;
            const int64_t q=count/64,rem=count%64;
            const int64_t first=q*tid+(tid<rem?tid:rem);
            const int64_t stop=first+q+(tid<rem?1:0);
            for(int64_t i=first;i<stop;++i) {
                const int64_t original=map[i];
                if(original<0 || original>=count) {error|=4;result[i]=0;continue;}
                result[i]=input[original];
                if(phase==0) {if(!std::isfinite(result[i]))error|=1;}
                else if(!std::isfinite(result[i]) || result[i]==0)error|=2;
            }
        }
        p->errorFlags[tid]=error;
    }
}
extern "C" void lnjTopologyMatch_HOST(lnjTopologyMatchParam* p) {
    for(int64_t tid=0;tid<64;++tid) {
        int64_t mismatch=0;
        for(int phase=0;phase<2;++phase) {
            const int64_t count=phase==0?p->rowPtrSize:p->entries;
            const auto a=phase==0?p->cachedRowPtr:p->cachedColumns;
            const auto b=phase==0?p->currentRowPtr:p->currentColumns;
            const int64_t q=count/64,rem=count%64;
            const int64_t first=q*tid+(tid<rem?tid:rem);
            const int64_t stop=first+q+(tid<rem?1:0);
            for(int64_t i=first;i<stop;++i)if(a[i]!=b[i])mismatch|=phase==0?1:2;
        }
        p->mismatchFlags[tid]=mismatch;
    }
}
