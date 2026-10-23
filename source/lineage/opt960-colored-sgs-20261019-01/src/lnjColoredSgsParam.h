#ifndef LNJ_COLORED_SGS_PARAM_H
#define LNJ_COLORED_SGS_PARAM_H

#include <stdint.h>

// Shared host C/C++ and slave C ABI. Input labels MUST be 64-bit signed labels;
// scalars MUST be double. All pointers refer to main-memory arrays. No OpenFOAM
// types or C++-only qualifiers appear here. Host owns range and extent checks.
struct lnjColoredSgsParam
{
    int64_t begin; // Inclusive first permuted row of ONE color.
    int64_t end;   // Exclusive last permuted row of the same color.
    int64_t* rowPtr;
    int64_t* colPtr;
    double* offDiagPtr;
    double* diagPtr;
    double* psiPtr; // Input and output: earlier colors are already completed.
    double* rhsPtr;
};

#ifdef __cplusplus
extern "C" {
#endif
void lnjColoredSgs_CPE(struct lnjColoredSgsParam* parameter);
#ifdef __cplusplus
}
#endif

#endif
