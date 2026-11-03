#ifndef LNJ_NUMERIC_PACK_HOST_H
#define LNJ_NUMERIC_PACK_HOST_H
#include "lnjNumericPackParam.h"
#if defined(LNJ_BACKEND_CPE) && !defined(LNJ_BACKEND_HOST)
#include <crts.h>
#elif !defined(LNJ_BACKEND_HOST) || defined(LNJ_BACKEND_CPE)
#error "Select exactly one execution backend"
#endif
#include <array>
#include <stdexcept>
#ifdef LNJ_BACKEND_CPE
extern "C" void SLAVE_FUN(lnjNumericPack_CPE)(struct lnjNumericPackParam*);
#else
extern "C" void lnjNumericPack_HOST(lnjNumericPackParam*);
#endif
namespace lnj {
inline void numericPack(lnjNumericPackParam& p,std::array<int64_t,64>& flags){
    if(p.nRows<0 || p.nEntries<0 ||
       (p.nRows && (!p.packedToOriginal || !p.originalDiagonal || !p.packedDiagonal)) ||
       (p.nEntries && (!p.packedEntryToOriginal || !p.originalValues || !p.packedValues)))
        throw std::invalid_argument("Invalid numeric pack buffers");
    // Inputs and outputs MUST be separate, nonoverlapping storage.
    if((p.nRows && p.originalDiagonal==p.packedDiagonal) ||
       (p.nEntries && p.originalValues==p.packedValues))
        throw std::invalid_argument("Numeric pack cannot operate in-place");
    flags.fill(-1);p.errorFlags=flags.data();
#ifdef LNJ_BACKEND_CPE
    CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjNumericPack_CPE)),&p);
    CRTS_athread_join();
#else
    lnjNumericPack_HOST(&p);
#endif
    for(const auto f:flags)if(f!=0)throw std::runtime_error("Numeric pack bad coefficient/diagonal/map or missing worker");
}
}
#endif
