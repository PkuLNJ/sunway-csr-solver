// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LNJ_COMBINED_SGS_HOST_H
#define LNJ_COMBINED_SGS_HOST_H
#include "lnjCombinedSgsParam.h"
#include "BoundaryTilePlan.H"
#include <crts.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
extern "C" void SLAVE_FUN(lnjCombinedSgs_CPE)(lnjCombinedSgsParam*);
namespace lnj {
static_assert(sizeof(lnjCombinedSgsParam)==144,"Independent n74 ABI");
static_assert(offsetof(lnjCombinedSgsParam,boundaryTileStarts)==120,"n54 prefix retained");
static_assert(std::is_standard_layout<lnjCombinedSgsParam>::value,"C ABI");
inline bool n74RangesOverlap(const double* a,const double* b,std::size_t n){
 if(!n)return false;
 if(n>UINTPTR_MAX/sizeof(double))throw std::invalid_argument("Vector address size overflow");
 const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b),bytes=n*sizeof(double);
 if(x>UINTPTR_MAX-bytes||y>UINTPTR_MAX-bytes)throw std::invalid_argument("Vector address overflow");
 return x<y+bytes&&y<x+bytes;
}
// caller admission: exact n54 processor concrete types ONLY, complete faceCells,
// original source/psi NON-overlap, no other original-interior observer in loop.
// Unsupported types/alias choose admitted=false => FULL pack/unpack EACH sweep.
class CombinedPsiSmoothEpoch {
 const BoundaryTilePlan& plan_;BoundaryMaterializationEpoch state_;bool retain_,valid_=false;
public:
 CombinedPsiSmoothEpoch(const BoundaryTilePlan& plan,std::int64_t sweeps,bool admitted,bool partial=true,bool retain=true)
 :plan_(plan),state_(sweeps,admitted&&partial&&retain),retain_(admitted&&retain){}
 // BoundaryTilePlan's constructor validated/sorted addresses. Owner MUST keep
 // it immutable for this lifetime; no per-invocation O(nBoundaryTiles) recheck.
 CombinedPsiSmoothEpoch(const CombinedPsiSmoothEpoch&)=delete;
 template<class Packed>void sweep(Packed& packed,const double* currentHaloRhs,double* originalPsi,std::array<int64_t,64>& flags){
  const auto mode=state_.begin();
  try{
   const auto n=packed.packedToOriginal.size();
   if(n!=static_cast<std::size_t>(plan_.rows)||packed.originalToPacked.size()!=n||packed.psi.size()!=n||packed.rhs.size()!=n)
       throw std::invalid_argument("Boundary plan/vector dimensions changed inside epoch");
   if(n&&(!currentHaloRhs||!originalPsi))throw std::invalid_argument("Null original vector");
   if(!mode.fullUnpack&&n74RangesOverlap(currentHaloRhs,originalPsi,n))
       throw std::invalid_argument("Aliased original source requires full-materialization admission fallback");
   if(n74RangesOverlap(originalPsi,packed.psi.data(),n)||n74RangesOverlap(currentHaloRhs,packed.psi.data(),n)||n74RangesOverlap(originalPsi,packed.rhs.data(),n)||n74RangesOverlap(currentHaloRhs,packed.rhs.data(),n))
       throw std::invalid_argument("Packed/original vectors alias");
   flags.fill(-1);
   alignas(64) lnjCombinedSgsParam p{static_cast<int64_t>(n),static_cast<int64_t>(packed.starts.size()-1),
    packed.starts.data(),packed.rowPtr.data(),packed.columns.data(),packed.values.data(),packed.diagonal.data(),packed.rhs.data(),packed.psi.data(),
    currentHaloRhs,originalPsi,packed.packedToOriginal.data(),packed.originalToPacked.data(),flags.data(),retain_&&valid_?1:0,
    plan_.starts.data(),static_cast<int64_t>(plan_.starts.size()),mode.fullUnpack?1:0};
   CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjCombinedSgs_CPE)),&p);CRTS_athread_join();
   for(const auto f:flags)if(f!=0)throw std::runtime_error("ALL-packed finite check/worker completion failed");
  }catch(...){valid_=false;state_.complete(false);throw;}
  state_.complete(true);
  valid_=retain_;
 }
 void finish()const{if(!state_.finished())throw std::logic_error("Original interior may be stale: epoch not finished");}
};
}
#endif
