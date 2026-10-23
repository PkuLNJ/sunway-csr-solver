#ifndef LNJ_PERMUTATION_SGS_HOST_H
#define LNJ_PERMUTATION_SGS_HOST_H
#include "lnjLevelPacked.H"
#include "lnjPermutationSgsParam.h"
#include <crts.h>
#include <array>
#include <stdexcept>
extern "C" void SLAVE_FUN(lnjPermutationSgs_CPE)(struct lnjPermutationSgsParam*);
namespace lnj {
// Caller owns immutable packed CSR/diagonal/maps and reusable64-entry flags.
// This function performs NO host vector packing, unpacking or nRows scan.
template<class Packed>
inline void permutationSgsSweep(Packed& packed,
    const double* originalRhs,double* originalPsi,std::array<int64_t,64>& flags){
    const auto n=packed.packedToOriginal.size();
    if(n && (!originalRhs || !originalPsi))throw std::invalid_argument("Null original vector");
    flags.fill(-1);
    alignas(64) lnjPermutationSgsParam p{
        static_cast<int64_t>(n),static_cast<int64_t>(packed.starts.size()-1),
        packed.starts.data(),packed.rowPtr.data(),packed.columns.data(),packed.values.data(),
        packed.diagonal.data(),packed.rhs.data(),packed.psi.data(),originalRhs,originalPsi,
        packed.packedToOriginal.data(),packed.originalToPacked.data(),flags.data()};
    CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjPermutationSgs_CPE)),&p);
    CRTS_athread_join();
    for(const auto flag:flags)
        if(flag!=0)throw std::runtime_error("Permutation SGS missing worker or nonfinite vector flag");
}
}
#endif
