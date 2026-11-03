#ifndef LNJ_TOPOLOGY_MATCH_HOST_H
#define LNJ_TOPOLOGY_MATCH_HOST_H
#include "lnjTopologyPlanCache.H"
#include "lnjTopologyMatchParam.h"
#if defined(LNJ_BACKEND_CPE) && !defined(LNJ_BACKEND_HOST)
#include <crts.h>
#elif !defined(LNJ_BACKEND_HOST) || defined(LNJ_BACKEND_CPE)
#error "Select exactly one execution backend"
#endif
#include <array>
#ifdef LNJ_BACKEND_CPE
extern "C" void SLAVE_FUN(lnjTopologyMatch_CPE)(struct lnjTopologyMatchParam*);
#else
extern "C" void lnjTopologyMatch_HOST(lnjTopologyMatchParam*);
#endif
namespace lnj {
inline bool topologyMatchCpeRaw(lnjTopologyMatchParam& p,std::array<int64_t,64>& flags){
    if(p.rowPtrSize<0 || p.entries<0 ||
       (p.rowPtrSize && (!p.cachedRowPtr || !p.currentRowPtr)) ||
       (p.entries && (!p.cachedColumns || !p.currentColumns)))
        throw std::invalid_argument("Invalid topology compare buffers");
    flags.fill(-1);p.mismatchFlags=flags.data();
#ifdef LNJ_BACKEND_CPE
    CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjTopologyMatch_CPE)),&p);
    CRTS_athread_join();
#else
    lnjTopologyMatch_HOST(&p);
#endif
    int64_t mismatch=0;
    for(auto flag:flags){if(flag<0 || flag>3)throw std::runtime_error("Missing topology compare worker");mismatch|=flag;}
    return mismatch==0;
}
inline bool topologyMatchCpe(const ImmutableTopologyPlan<int64_t>& cached,const TopologyGraphView<int64_t>& current){
    if(cached.originalRowPtr.size()!=current.rowPtrSize || cached.originalColumns.size()!=current.entries)return false;
    if(current.rowPtrSize>static_cast<std::size_t>(std::numeric_limits<int64_t>::max()) ||
       current.entries>static_cast<std::size_t>(std::numeric_limits<int64_t>::max()))
        throw std::overflow_error("Topology compare index overflow");
    lnjTopologyMatchParam p{static_cast<int64_t>(current.rowPtrSize),static_cast<int64_t>(current.entries),
        cached.originalRowPtr.data(),current.rowPtr,cached.originalColumns.data(),current.columns,nullptr};
    std::array<int64_t,64>flags;return topologyMatchCpeRaw(p,flags);
}
}
#endif
