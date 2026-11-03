// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LNJ_RHS_RESET_EPOCH_H
#define LNJ_RHS_RESET_EPOCH_H
#include "BoundaryTilePlan.H"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace lnj {
inline bool rhsResetOverlap(const double* a,const double* b,std::size_t n){
 if(!n)return false;
 if(n>UINTPTR_MAX/sizeof(double))throw std::invalid_argument("RHS range size overflow");
 const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b),bytes=n*sizeof(double);
 if(x>UINTPTR_MAX-bytes||y>UINTPTR_MAX-bytes)throw std::invalid_argument("RHS range overflow");
 return x<y+bytes&&y<x+bytes;
}
// One lexical smoothImpl invocation ONLY. source numerical values are immutable
// under that caller's ownership (not merely declared const on one reference).
// Plan is immutable, cached by same fixed-matrix smoother, not rebuilt here.
class RhsResetEpoch {
 const BoundaryTilePlan& plan_;const double* source_;double* rhs_;
 std::int64_t remaining_;bool admitted_,first_=true,inFlight_=false,failed_=false;
public:
 struct Stats{std::uint64_t fullCopies=0,partialCopies=0,copiedRows=0,tileLoops=0;}stats;
 RhsResetEpoch(const BoundaryTilePlan& plan,const double* source,double* rhs,const double* psi,
               std::int64_t sweeps,bool supported):plan_(plan),source_(source),rhs_(rhs),remaining_(sweeps),admitted_(false){
  if(plan.rows<0||sweeps<1)throw std::invalid_argument("Invalid RHS reset dimensions/sweeps");
  const auto n=static_cast<std::size_t>(plan.rows);
  if(n>SIZE_MAX/sizeof(double))throw std::invalid_argument("RHS copy byte size overflow");
  if(n&&(!source||!rhs||!psi))throw std::invalid_argument("Null RHS reset vector");
  admitted_=supported&&!rhsResetOverlap(source,psi,n)&&!rhsResetOverlap(source,rhs,n)&&!rhsResetOverlap(rhs,psi,n);
 }
 RhsResetEpoch(const RhsResetEpoch&)=delete;
 bool admitted()const{return admitted_;}
 void reset(){
  if(failed_||inFlight_||remaining_<=0){fail();throw std::logic_error("RHS reset epoch terminal/inflight/finished");}
  inFlight_=true;
  if(first_||!admitted_){
   const auto n=static_cast<std::size_t>(plan_.rows);
   // memmove handles alias fallback without reading a cached snapshot. Same
   // current source values as baseline full assignment, including signed zero.
   if(n)std::memmove(rhs_,source_,n*sizeof(double));
   ++stats.fullCopies;stats.copiedRows+=n;
  }else{
   for(const auto start:plan_.starts){
    const auto count=std::min(BoundaryTilePlan::width,plan_.rows-start);
    std::copy_n(source_+start,count,rhs_+start);stats.copiedRows+=count;++stats.tileLoops;
   }
   ++stats.partialCopies;
  }
 }
 void fail()noexcept{failed_=true;inFlight_=false;}
 void complete(bool success){
  if(failed_||!inFlight_){fail();throw std::logic_error("RHS completion without valid reset");}
  if(!success){fail();throw std::runtime_error("RHS reset epoch failure is terminal");}
  inFlight_=false;first_=false;--remaining_;
 }
 void finish(){if(failed_||inFlight_||remaining_!=0){fail();throw std::logic_error("Incomplete/failed RHS reset epoch");}}
};
}
#endif
