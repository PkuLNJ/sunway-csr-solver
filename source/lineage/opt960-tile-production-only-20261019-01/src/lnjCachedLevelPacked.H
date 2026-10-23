#ifndef LNJ_CACHED_LEVEL_PACKED_H
#define LNJ_CACHED_LEVEL_PACKED_H
#include "lnjTopologyPlanCache.H"
namespace lnj {
// Interface-compatible immutable graph lease + per-solve fresh numeric storage.
// Destruction order: reference aliases have no ownership; base owns the lease.
template<class Label,class Scalar>
struct CachedLevelPacked:FreshTopologyNumerics<Label,Scalar> {
    const std::vector<Label>& starts;
    const std::vector<Label>& rowPtr;
    const std::vector<Label>& columns;
    const std::vector<Label>& packedToOriginal;
    const std::vector<Label>& originalToPacked;
    CachedLevelPacked(std::shared_ptr<const ImmutableTopologyPlan<Label>> t,
        const Scalar* values,std::size_t entries,const Scalar* diagonal,std::size_t n)
        :FreshTopologyNumerics<Label,Scalar>(std::move(t),values,entries,diagonal,n),
         starts(this->topology->starts),rowPtr(this->topology->rowPtr),columns(this->topology->columns),
         packedToOriginal(this->topology->packedToOriginal),originalToPacked(this->topology->originalToPacked){}
    CachedLevelPacked(const CachedLevelPacked&)=delete;
    CachedLevelPacked& operator=(const CachedLevelPacked&)=delete;
};
}
#endif
