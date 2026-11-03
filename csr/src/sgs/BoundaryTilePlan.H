// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LNJ_BOUNDARY_TILE_PLAN_H
#define LNJ_BOUNDARY_TILE_PLAN_H
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace lnj {
// Immutable exact interface-address plan, not a scan of current solution values.
// Every plain-processor faceCells entry must be included. Including additional
// physical cells is conservative and changes traffic only, never the halo.
struct BoundaryTilePlan {
    static constexpr std::int64_t width=16;
    std::int64_t rows;
    std::vector<std::int64_t> starts;
    BoundaryTilePlan(std::int64_t n,const std::vector<std::int64_t>& cells):rows(n){
        if(n<0)throw std::invalid_argument("negative row count");
        for(const auto c:cells){
            if(c<0||c>=n)throw std::invalid_argument("interface cell out of range");
            starts.push_back(c/width*width);
        }
        std::sort(starts.begin(),starts.end());
        starts.erase(std::unique(starts.begin(),starts.end()),starts.end());
    }
    std::int64_t materializedRows()const{
        std::int64_t total=0;
        for(const auto s:starts)total+=std::min(width,rows-s);
        return total;
    }
    // CPU semantic oracle for the CPE partial-unpack implementation. The
    // permutation must be the validated immutable level packing bijection.
    template<class Scalar>void materialize(const std::vector<Scalar>& packed,
        const std::vector<std::int64_t>& originalToPacked,
        std::vector<Scalar>& original,bool final)const{
        if(packed.size()!=static_cast<std::size_t>(rows)||original.size()!=packed.size()
            ||originalToPacked.size()!=packed.size())throw std::invalid_argument("vector size mismatch");
        auto copy=[&](std::int64_t i){
            const auto p=originalToPacked[i];
            if(p<0||p>=rows)throw std::invalid_argument("invalid permutation entry");
            original[i]=packed[p];
        };
        if(final){for(std::int64_t i=0;i<rows;++i)copy(i);}
        else for(const auto s:starts)for(auto i=s;i<std::min(rows,s+width);++i)copy(i);
    }
};
// A scope owns exactly one configured multi-sweep smooth() invocation. Caller
// supplies fresh RHS/halo each sweep and MUST NOT read stale interior original
// entries between sweeps. An exception invalidates reuse; it does not rollback.
class BoundaryMaterializationEpoch {
    std::int64_t remaining_;bool valid_=false,enabled_,inFlight_=false,failed_=false;
public:
    struct Mode {bool reusePacked;bool fullUnpack;};
    BoundaryMaterializationEpoch(std::int64_t sweeps,bool admitted):remaining_(sweeps),enabled_(admitted){
        if(sweeps<1)throw std::invalid_argument("epoch requires positive sweep count");
    }
    BoundaryMaterializationEpoch(const BoundaryMaterializationEpoch&)=delete;
    Mode begin(){
        if(inFlight_||failed_||remaining_<=0)throw std::logic_error("invalid epoch launch");
        const bool reuse=enabled_&&valid_;valid_=false;inFlight_=true;
        return {reuse,!enabled_||remaining_==1};
    }
    void complete(bool allWorkerFlagsFinite){
        if(!inFlight_)throw std::logic_error("completion without launch");
        inFlight_=false;
        // Unlike n54, original interior can already be stale after a successful
        // partial materialization. Never retry by repacking that stale vector.
        // Caller must abort or explicitly recover a FULL original then new scope.
        if(!allWorkerFlagsFinite){failed_=true;throw std::runtime_error("failed sweep: epoch is terminal");}
        --remaining_;valid_=enabled_;
    }
    bool finished()const{return remaining_==0&&!inFlight_;}
};
}
#endif
