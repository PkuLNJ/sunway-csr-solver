/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C)
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

*---------------------------------------------------------------------------*/

#include "csrDICSmoother.H"

namespace Foam
{
    defineTypeNameAndDebug(csrDICSmoother, 0);

    csrMatrix::smoother::addsymMatrixConstructorToTable<csrDICSmoother>
        addcsrDICSmootherSymMatrixConstructorToTable_;

    csrMatrix::smoother::addasymMatrixConstructorToTable<csrDICSmoother>
        addcsrDICSmootherAsymMatrixConstructorToTable_;
}

Foam::csrDICSmoother::csrDICSmoother
(
    const word& fieldName,
    const csrMatrix& matrix,
    const FieldField<Field, scalar>& interfaceBouCoeffs,
    const FieldField<Field, scalar>& interfaceIntCoeffs,
    const lduInterfaceFieldPtrsList& interfaces
)
:
    csrMatrix::smoother
    (
        fieldName,
        matrix,
        interfaceBouCoeffs,
        interfaceIntCoeffs,
        interfaces
    ),
    rD_(matrix_.diag())
{
    calcReciprocalDCSR(rD_, matrix_);
}

void Foam::csrDICSmoother::calcReciprocalDCSR(scalarField& rD, const csrMatrix& A)
{
    const label n = A.diag().size();
    const label* __restrict__ rp = A.off_diag_rowptr().begin();
    const label* __restrict__ ci = A.off_diag_colidx().begin();
    const scalar* __restrict__ av = A.off_diag_value().begin();
    const scalar* __restrict__ d  = A.diag().begin();
    scalar* __restrict__ rDPtr = rD.begin();

    for (label i = 0; i < n; ++i)
    {
        scalar di = d[i];
        for (label k = rp[i]; k < rp[i+1]; ++k)
        {
            const label j = ci[k];
            if (j < i) // lower neighbours
            {
                const scalar aij = av[k];
                di -= aij*aij * rDPtr[j];
            }
        }
        rDPtr[i] = 1.0/di;
    }
}

void Foam::csrDICSmoother::smooth
(
    const word& fieldName_,
    scalarField& psi,
    const csrMatrix& matrix_,
    const scalarField& source,
    const FieldField<Field, scalar>& interfaceBouCoeffs_,
    const lduInterfaceFieldPtrsList& interfaces_,
    const direction cmpt,
    const label nSweeps,
    const scalarField& rD
)
{
    const label n = psi.size();
    scalarField bPrime(n);
    scalarField rA(n);

    // Handle coupled interfaces (same as csrJacobi)
    FieldField<Field, scalar>& mBouCoeffs =
        const_cast<FieldField<Field, scalar>&>(interfaceBouCoeffs_);
    forAll(mBouCoeffs, patchi)
    {
        if (interfaces_.set(patchi)) { mBouCoeffs[patchi].negate(); }
    }

    const label* __restrict__ rp = matrix_.off_diag_rowptr().begin();
    const label* __restrict__ ci = matrix_.off_diag_colidx().begin();
    const scalar* __restrict__ av = matrix_.off_diag_value().begin();
    const scalar* __restrict__ d  = matrix_.diag().begin();
    const scalar* __restrict__ rDPtr = rD.begin();

    scalar* __restrict__ psiPtr = psi.begin();
    scalar* __restrict__ bPrimePtr = bPrime.begin();
    scalar* __restrict__ rAPtr = rA.begin();

    for (label sweep=0; sweep<nSweeps; ++sweep)
    {
        bPrime = source;

        matrix_.initMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime, cmpt);
        matrix_.updateMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime, cmpt);

        // Residual rA = bPrime - A*psi
        for (label i = 0; i < n; ++i)
        {
            scalar sum = d[i]*psiPtr[i];
            for (label k = rp[i]; k < rp[i+1]; ++k)
            {
                sum += av[k]*psiPtr[ci[k]];
            }
            rAPtr[i] = bPrimePtr[i] - sum;
        }

        // Scale by reciprocal diagonal of DIC
        for (label i = 0; i < n; ++i) { rAPtr[i] *= rDPtr[i]; }

        // Forward sweep: use lower neighbours (j < i)
        for (label i = 0; i < n; ++i)
        {
            for (label k = rp[i]; k < rp[i+1]; ++k)
            {
                const label j = ci[k];
                if (j < i) { rAPtr[i] -= rDPtr[i] * av[k] * rAPtr[j]; }
            }
        }

        // Backward sweep: use upper neighbours (j > i)
        for (label i = n-1; i >= 0; --i)
        {
            for (label k = rp[i]; k < rp[i+1]; ++k)
            {
                const label j = ci[k];
                if (j > i) { rAPtr[i] -= rDPtr[i] * av[k] * rAPtr[j]; }
            }
        }

        // Update psi
        for (label i = 0; i < n; ++i) { psiPtr[i] += rAPtr[i]; }
    }

    // Restore interfaceBouCoeffs signs
    forAll(mBouCoeffs, patchi)
    {
        if (interfaces_.set(patchi)) { mBouCoeffs[patchi].negate(); }
    }
}

void Foam::csrDICSmoother::smooth
(
    scalarField& psi,
    const scalarField& source,
    const direction cmpt,
    const label nSweeps
) const
{
    smooth(fieldName_, psi, matrix_, source, interfaceBouCoeffs_, interfaces_, cmpt, nSweeps, rD_);
}

// ************************************************************************* //