// SPDX-License-Identifier: GPL-3.0-or-later
// Same arithmetic as the face-SGS candidate helper, retained as fallback.
#ifndef lnjFaceSymGaussSeidel_H
#define lnjFaceSymGaussSeidel_H
namespace lnj
{
template<class Label, class Scalar>
inline void faceSymGaussSeidelSweep
(
    const Label nCells, const Label* ownerStart, const Label* neighbour,
    const Scalar* lower, const Scalar* upper, const Scalar* diagonal,
    Scalar* psi, Scalar* bPrime
)
{
    for (Label cell = 0; cell < nCells; ++cell)
    {
        const Label first = ownerStart[cell];
        const Label last = ownerStart[cell + 1];
        Scalar value = bPrime[cell];
        for (Label face = first; face < last; ++face)
            value -= upper[face]*psi[neighbour[face]];
        value /= diagonal[cell];
        for (Label face = first; face < last; ++face)
            bPrime[neighbour[face]] -= lower[face]*value;
        psi[cell] = value;
    }
    for (Label cell = nCells; cell > 0;)
    {
        --cell;
        Scalar value = bPrime[cell];
        for (Label face = ownerStart[cell]; face < ownerStart[cell + 1]; ++face)
            value -= upper[face]*psi[neighbour[face]];
        psi[cell] = value/diagonal[cell];
    }
}
}
#endif
