/*---------------------------------------------------------------------------*\
  OpenFOAM: The Open Source CFD Toolbox
  Copyright (C) 2012-2018 OpenFOAM Foundation
  Derived from csrSymGaussSeidelSmoother.H.
  Licensed under GNU GPL version 3 or, at your option, any later version.
  No warranty; see <http://www.gnu.org/licenses/>.
\*---------------------------------------------------------------------------*/
#ifndef csrSymGaussSeidelSmoother_H
#define csrSymGaussSeidelSmoother_H

#include "csrMatrix.H"
#include "lnjColoredSgs.H"
#include <memory>
#include <vector>

namespace Foam
{
class csrSymGaussSeidelSmoother
:
    public csrMatrix::smoother
{
    enum executionMode { naturalMode, hostMode, cpeMode };
    executionMode mode_;
    // One plan per smoother lifetime; never shared across matrices/ranks.
    // const smooth refreshes the diagonal/work vectors, not the CSR structure.
    mutable std::unique_ptr<lnj::ColoredSgs<label, scalar>> colored_;
    mutable std::vector<scalar> packedX_;
    mutable std::vector<scalar> packedRhs_;
    mutable scalarField bPrime_;

    void smoothColor(const std::size_t color) const;

public:
    TypeName("symGaussSeidel");

    csrSymGaussSeidelSmoother
    (
        const word& fieldName,
        const csrMatrix& matrix,
        const FieldField<Field, scalar>& interfaceBouCoeffs,
        const FieldField<Field, scalar>& interfaceIntCoeffs,
        const lduInterfaceFieldPtrsList& interfaces
    );

    // Explicit static calls retain natural-order face SGS, with no cache.
    static void smooth
    (
        const word& fieldName,
        scalarField& psi,
        const csrMatrix& matrix,
        const scalarField& source,
        const FieldField<Field, scalar>& interfaceBouCoeffs,
        const lduInterfaceFieldPtrsList& interfaces,
        const direction cmpt,
        const label nSweeps
    );

    virtual void smooth
    (
        scalarField& psi,
        const scalarField& source,
        const direction cmpt,
        const label nSweeps
    ) const;
};
}
#endif
