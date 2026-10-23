#ifndef LNJ_RETAINED_PSI_SGS_HOST_H
#define LNJ_RETAINED_PSI_SGS_HOST_H
#include "lnjRetainedPsiSgsParam.h"
#include <crts.h>
#include <array>
#include <stdexcept>
extern "C" void SLAVE_FUN(lnjRetainedPsiSgs_CPE)(lnjRetainedPsiSgsParam*);
namespace lnj {
// ONE production kernel. Flag0 performs the full old pack; no old CPE symbol
// or extra TLS workspace is linked for default-off/unsupported fallback.
template<class Packed>
inline void retainedPsiSgsTileSweep(Packed& packed,const double* source,double* psi,
 std::array<int64_t,64>& flags,bool reuse){
 const auto n=packed.packedToOriginal.size();
 if(n&&(!source||!psi))throw std::invalid_argument("Null original vector");
 flags.fill(-1);
 alignas(64) lnjRetainedPsiSgsParam p{static_cast<int64_t>(n),static_cast<int64_t>(packed.starts.size()-1),
  packed.starts.data(),packed.rowPtr.data(),packed.columns.data(),packed.values.data(),packed.diagonal.data(),
  packed.rhs.data(),packed.psi.data(),source,psi,packed.packedToOriginal.data(),packed.originalToPacked.data(),flags.data(),reuse?1:0};
 CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjRetainedPsiSgs_CPE)),&p);CRTS_athread_join();
 for(const auto flag:flags)if(flag!=0)throw std::runtime_error("Permutation SGS missing worker or nonfinite vector flag");
}
class RetainedPsiSmoothEpoch {
 bool enabled_,valid_;
public:
 explicit RetainedPsiSmoothEpoch(bool enabled):enabled_(enabled),valid_(false){}
 RetainedPsiSmoothEpoch(const RetainedPsiSmoothEpoch&)=delete;
 RetainedPsiSmoothEpoch& operator=(const RetainedPsiSmoothEpoch&)=delete;
 template<class Packed>void sweep(Packed& p,const double* source,double* psi,std::array<int64_t,64>& flags){
  const bool reuse=enabled_&&valid_;valid_=false;
  retainedPsiSgsTileSweep(p,source,psi,flags,reuse);
  valid_=enabled_; // only after all workers joined and finite output accepted
 }
};
}
#endif
