#ifndef LNJ_PACKED_NUMERIC_FACTORY_H
#define LNJ_PACKED_NUMERIC_FACTORY_H
#include "lnjCachedLevelPacked.H"
#include "lnjNumericPackHost.H"
#include <cstring>
#include <type_traits>
namespace lnj {
enum class NumericPackingMode {host,cpe};
inline NumericPackingMode parseNumericPackingMode(const char* mode){
    if(!mode || std::strcmp(mode,"host")==0)return NumericPackingMode::host;
    if(std::strcmp(mode,"cpe")==0)return NumericPackingMode::cpe;
    throw std::invalid_argument("LNJ_SGS_NUMERIC_PACK must be host or cpe");
}
template<class Label,class Scalar>
std::unique_ptr<CachedLevelPacked<Label,Scalar>> makePackedNumeric(
    std::shared_ptr<const ImmutableTopologyPlan<Label>> topology,
    const Scalar* originalValues,std::size_t entries,const Scalar* originalDiagonal,
    std::size_t rows,NumericPackingMode mode){
    static_assert(std::is_same<Label,int64_t>::value,"64-bit label required");
    static_assert(std::is_same<Scalar,double>::value,"double required");
    const bool cpe=mode==NumericPackingMode::cpe;
    std::unique_ptr<CachedLevelPacked<Label,Scalar>> packed(new CachedLevelPacked<Label,Scalar>(
        std::move(topology),originalValues,entries,originalDiagonal,rows,cpe));
    if(cpe){
        auto& t=*packed->topology;
        lnjNumericPackParam p{static_cast<int64_t>(rows),static_cast<int64_t>(entries),
            t.packedEntryToOriginal.data(),t.packedToOriginal.data(),originalValues,originalDiagonal,
            packed->values.data(),packed->diagonal.data(),nullptr};
        std::array<int64_t,64> flags;numericPack(p,flags);
    }
    return packed;
}
}
#endif
