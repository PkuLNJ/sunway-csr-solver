// SPDX-License-Identifier: GPL-3.0-or-later
// Process-local immutable topology ONLY. No OpenFOAM object/pointer identity.
#ifndef LNJ_TOPOLOGY_PLAN_CACHE_H
#define LNJ_TOPOLOGY_PLAN_CACHE_H
#include "lnjLevelSgs.H"
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

namespace lnj {
template<class Label> struct TopologyGraphView {
    const Label* rowPtr;
    std::size_t rowPtrSize;
    const Label* columns;
    std::size_t entries;
};
template<class Label>
TopologyGraphView<Label> topologyView(const std::vector<Label>& ptr,const std::vector<Label>& columns){
    return {ptr.data(),ptr.size(),columns.data(),columns.size()};
}

template<class Label> struct ImmutableTopologyPlan {
    std::vector<Label> originalRowPtr,originalColumns;
    std::vector<Label> starts,rowPtr,columns,packedToOriginal,originalToPacked;
    std::vector<Label> packedEntryToOriginal;
    std::size_t bytes() const {
        return sizeof(*this)+sizeof(Label)*(originalRowPtr.capacity()+originalColumns.capacity()
            +starts.capacity()+rowPtr.capacity()+columns.capacity()+packedToOriginal.capacity()
            +originalToPacked.capacity()+packedEntryToOriginal.capacity());
    }
    bool matches(const TopologyGraphView<Label>& graph) const {
        return graph.rowPtrSize==originalRowPtr.size() && graph.entries==originalColumns.size()
            && std::equal(originalRowPtr.begin(),originalRowPtr.end(),graph.rowPtr)
            && (!graph.entries || std::equal(originalColumns.begin(),originalColumns.end(),graph.columns));
    }
};

template<class Label>
std::shared_ptr<const ImmutableTopologyPlan<Label>> buildTopologyPlan(const TopologyGraphView<Label>& graph){
    if(!graph.rowPtrSize || !graph.rowPtr || (graph.entries && !graph.columns))
        throw std::invalid_argument("Invalid graph view storage");
    auto result=std::make_shared<ImmutableTopologyPlan<Label>>();
    result->originalRowPtr.assign(graph.rowPtr,graph.rowPtr+graph.rowPtrSize);
    if(graph.entries)result->originalColumns.assign(graph.columns,graph.columns+graph.entries);
    // Exactly the previously tested schedule builder. No independent change
    // to traversal, coloring, row ordering or forward/backward mathematics.
    auto schedule=makeLevelSgsPlan(result->originalRowPtr,result->originalColumns);
    result->starts=std::move(schedule.forward.starts);
    result->packedToOriginal=std::move(schedule.forward.rows);
    const auto n=graph.rowPtrSize-1;
    result->originalToPacked.resize(n);
    result->rowPtr.resize(n+1);result->rowPtr[0]=0;
    result->columns.resize(graph.entries);result->packedEntryToOriginal.resize(graph.entries);
    for(std::size_t p=0;p<n;++p)result->originalToPacked[result->packedToOriginal[p]]=static_cast<Label>(p);
    std::size_t packedEntry=0;
    for(std::size_t p=0;p<n;++p){
        const auto original=static_cast<std::size_t>(result->packedToOriginal[p]);
        for(Label e=result->originalRowPtr[original];e<result->originalRowPtr[original+1];++e){
            result->columns[packedEntry]=result->originalToPacked[result->originalColumns[e]];
            result->packedEntryToOriginal[packedEntry]=e;++packedEntry;
        }
        result->rowPtr[p+1]=static_cast<Label>(packedEntry);
    }
    return result;
}

struct TopologyCacheLimits {
    std::size_t maximumEntries=8;
    std::size_t maximumPayloadBytes=128u*1024u*1024u;
};
struct TopologyCacheStats {
    std::size_t hits=0,misses=0,builds=0,evictions=0,bypasses=0;
    std::size_t exactComparisons=0,residentEntries=0,residentPayloadBytes=0;
};
enum class TopologyLookupState { hit,inserted,disabled,tooLarge,pinnedCapacity };
template<class Label> struct TopologyLookup {
    std::shared_ptr<const ImmutableTopologyPlan<Label>> plan;
    TopologyLookupState state;
};

template<class Label> class TopologyPlanCache {
    using Plan=ImmutableTopologyPlan<Label>;
public:
    using ExactMatcher=bool(*)(const Plan&,const TopologyGraphView<Label>&);
private:
    ExactMatcher matcher_;
    struct Entry { std::shared_ptr<const Plan> plan; };
    TopologyCacheLimits limits_;
    std::list<Entry> lru_; // front MRU; only immutable payloads are shared.
    TopologyCacheStats stats_;
    mutable std::mutex mutex_;
    bool makeRoom(std::size_t bytes){
        while(lru_.size()>=limits_.maximumEntries || bytes>limits_.maximumPayloadBytes-stats_.residentPayloadBytes){
            auto victim=lru_.end();
            for(auto it=lru_.end();it!=lru_.begin();){
                --it;
                // A live smoother lease pins the resident entry.
                if(it->plan.use_count()==1){victim=it;break;}
            }
            if(victim==lru_.end())return false;
            stats_.residentPayloadBytes-=victim->plan->bytes();
            lru_.erase(victim);++stats_.evictions;stats_.residentEntries=lru_.size();
        }
        stats_.residentEntries=lru_.size();return true;
    }
public:
    explicit TopologyPlanCache(TopologyCacheLimits limits={},ExactMatcher matcher=nullptr)
        :matcher_(matcher),limits_(limits){}
    TopologyLookup<Label> get(const TopologyGraphView<Label>& graph){
        if(!graph.rowPtrSize || !graph.rowPtr || (graph.entries && !graph.columns))
            throw std::invalid_argument("Invalid graph view storage");
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto it=lru_.begin();it!=lru_.end();++it){
            if(it->plan->originalRowPtr.size()!=graph.rowPtrSize || it->plan->originalColumns.size()!=graph.entries)continue;
            ++stats_.exactComparisons;
            if(matcher_?matcher_(*it->plan,graph):it->plan->matches(graph)){
                auto plan=it->plan;lru_.splice(lru_.begin(),lru_,it);++stats_.hits;
                return {std::move(plan),TopologyLookupState::hit};
            }
        }
        ++stats_.misses;
        if(!limits_.maximumEntries || !limits_.maximumPayloadBytes){
            ++stats_.bypasses;++stats_.builds;return {buildTopologyPlan(graph),TopologyLookupState::disabled};
        }
        const auto n=graph.rowPtrSize-1;
        // Saturating bound before allocating a graph. Conservative starts<=n+1.
        const auto max=std::numeric_limits<std::size_t>::max();
        if(n>(max-3)/5 || graph.entries>(max-(5*n+3))/3){
            ++stats_.bypasses;++stats_.builds;return {buildTopologyPlan(graph),TopologyLookupState::tooLarge};
        }
        const auto labels=5*n+3+3*graph.entries;
        if(labels>(max-sizeof(Plan))/sizeof(Label)){
            ++stats_.bypasses;++stats_.builds;return {buildTopologyPlan(graph),TopologyLookupState::tooLarge};
        }
        const auto bound=sizeof(Plan)+sizeof(Label)*labels;
        if(bound>limits_.maximumPayloadBytes){++stats_.bypasses;++stats_.builds;return {buildTopologyPlan(graph),TopologyLookupState::tooLarge};}
        if(!makeRoom(bound)){++stats_.bypasses;++stats_.builds;return {buildTopologyPlan(graph),TopologyLookupState::pinnedCapacity};}
        auto plan=buildTopologyPlan(graph);++stats_.builds;
        const auto actual=plan->bytes();
        if(actual>limits_.maximumPayloadBytes || !makeRoom(actual)){
            ++stats_.bypasses;return {std::move(plan),TopologyLookupState::pinnedCapacity};
        }
        lru_.push_front({plan});stats_.residentPayloadBytes+=actual;stats_.residentEntries=lru_.size();
        return {std::move(plan),TopologyLookupState::inserted};
    }
    TopologyCacheStats stats() const {std::lock_guard<std::mutex> lock(mutex_);return stats_;}
};

// Owned by ONE current linear solve, never stored in TopologyPlanCache.
// Every call reads current numeric values and diagonal, even on a cache hit.
template<class Label,class Scalar> struct FreshTopologyNumerics {
    std::shared_ptr<const ImmutableTopologyPlan<Label>> topology;
    std::vector<Scalar> values,diagonal,rhs,psi;
    FreshTopologyNumerics(std::shared_ptr<const ImmutableTopologyPlan<Label>> plan,
        const Scalar* originalValues,std::size_t entries,const Scalar* originalDiagonal,std::size_t n)
        :topology(std::move(plan)),values(entries),diagonal(n),rhs(n),psi(n){
        if(!topology || n!=topology->packedToOriginal.size() || entries!=topology->columns.size()
           || (entries && !originalValues) || (n && !originalDiagonal))
            throw std::invalid_argument("Numeric/topology dimension mismatch");
        for(std::size_t p=0;p<entries;++p){
            const Scalar value=originalValues[topology->packedEntryToOriginal[p]];
            if(!std::isfinite(value))throw std::invalid_argument("Nonfinite fresh coefficient");
            values[p]=value;
        }
        for(std::size_t p=0;p<n;++p){
            const Scalar d=originalDiagonal[topology->packedToOriginal[p]];
            if(!std::isfinite(d)||d==0)throw std::invalid_argument("Invalid fresh diagonal");
            diagonal[p]=d;
        }
    }
    void packVectors(const std::vector<Scalar>& originalRhs,const std::vector<Scalar>& originalPsi){
        if(originalRhs.size()!=psi.size()||originalPsi.size()!=psi.size())throw std::invalid_argument("Vector mismatch");
        for(std::size_t p=0;p<psi.size();++p){const auto i=topology->packedToOriginal[p];rhs[p]=originalRhs[i];psi[p]=originalPsi[i];}
    }
    void hostSweep(){
        const auto& t=*topology;const auto levels=t.starts.size()-1;
        for(int phase=0;phase<2;++phase)for(std::size_t ordinal=0;ordinal<levels;++ordinal){
            const auto level=phase==0?ordinal:levels-1-ordinal;
            for(Label row=t.starts[level];row<t.starts[level+1];++row)
                rowUpdate(row,t.rowPtr,t.columns,values,diagonal,rhs,psi);
        }
    }
    std::vector<Scalar> unpackPsi() const {
        std::vector<Scalar> result(psi.size());
        for(std::size_t p=0;p<psi.size();++p)result[topology->packedToOriginal[p]]=psi[p];
        return result;
    }
};
}
#endif
