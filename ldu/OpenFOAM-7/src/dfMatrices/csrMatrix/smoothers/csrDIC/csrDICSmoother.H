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

Class
    Foam::csrDICSmoother

Description
    CSR-based diagonal incomplete-Cholesky (DIC) smoother.
    Mirrors LDUDICSmoother behaviour using csrMatrix storage.

SourceFiles
    csrDICSmoother.C

*---------------------------------------------------------------------------*/

#ifndef csrDICSmoother_H
#define csrDICSmoother_H

#include "csrMatrix.H"

namespace Foam
{

class csrDICSmoother
:
    public csrMatrix::smoother
{
    //- The reciprocal preconditioned diagonal (IC(0))
    scalarField rD_;

public:

    //- Runtime type information
    TypeName("DIC");

    //- Construct from components
    csrDICSmoother
    (
        const word& fieldName,
        const csrMatrix& matrix,
        const FieldField<Field, scalar>& interfaceBouCoeffs,
        const FieldField<Field, scalar>& interfaceIntCoeffs,
        const lduInterfaceFieldPtrsList& interfaces
    );

    //- Compute reciprocal diagonal for IC(0) using CSR
    static void calcReciprocalDCSR(scalarField& rD, const csrMatrix& matrix);

    //- Smooth for the given number of sweeps (static helper)
    static void smooth
    (
        const word& fieldName,
        scalarField& psi,
        const csrMatrix& matrix,
        const scalarField& source,
        const FieldField<Field, scalar>& interfaceBouCoeffs,
        const lduInterfaceFieldPtrsList& interfaces,
        const direction cmpt,
        const label nSweeps,
        const scalarField& rD
    );

    //- Smooth the solution for a given number of sweeps
    virtual void smooth
    (
        scalarField& psi,
        const scalarField& source,
        const direction cmpt,
        const label nSweeps
    ) const;
};

} // End namespace Foam

#endif

// ************************************************************************* //