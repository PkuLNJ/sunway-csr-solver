#ifndef LNJ_NUMERIC_PACK_HOST_H
#define LNJ_NUMERIC_PACK_HOST_H
#include "lnjNumericPackParam.h"
#include <crts.h>
#include <array>
#include <stdexcept>
extern "C" void SLAVE_FUN(lnjNumericPack_CPE)(struct lnjNumericPackParam*);
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
    CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjNumericPack_CPE)),&p);
    CRTS_athread_join();
    for(const auto f:flags)if(f!=0)throw std::runtime_error("Numeric pack bad coefficient/diagonal/map or missing worker");
}
}
#endif
