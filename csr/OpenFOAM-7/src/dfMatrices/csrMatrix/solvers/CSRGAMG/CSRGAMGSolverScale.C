/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2018 OpenFOAM Foundation
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

\*---------------------------------------------------------------------------*/

#include "CSRGAMGSolver.H"
#include "vector2D.H"
#include <mpi.h>

#define __sw_qd__

#ifdef __sw_qd__
#include "csrGAMGScale_slave_kernel.h"
#define _coarse_limit 700 
#define ATHREAD_NUM 64
#endif

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::CSRGAMGSolver::scale
(
    scalarField& field,
    scalarField& Acf,
    const csrMatrix& A,
    const FieldField<Field, scalar>& interfaceLevelBouCoeffs,
    const lduInterfaceFieldPtrsList& interfaceLevel,
    const scalarField& source,
    const direction cmpt
) const
{
    double start = MPI_Wtime();
    A.Amul
    (
        Acf,
        field,
        interfaceLevelBouCoeffs,
        interfaceLevel,
        cmpt
    );
    double end = MPI_Wtime();
    scale_spmv_time += end - start;

    const label nCells = field.size();
    scalar* fieldPtr = field.begin();
    const scalar* const sourcePtr = source.begin();
    const scalar* const AcfPtr = Acf.begin();

    scalar scalingFactorNum = 0.0;
    scalar scalingFactorDenom = 0.0;

//#ifndef __sw_qd__
#ifdef __sw_qd__
    for (label i=0; i<nCells; i++)
    {
        scalingFactorNum += sourcePtr[i]*fieldPtr[i];
        scalingFactorDenom += AcfPtr[i]*fieldPtr[i];
    }
#else
    if(nCells <= _coarse_limit){
	for (label i=0; i<nCells; i++)
    	{   
            scalingFactorNum += sourcePtr[i]*fieldPtr[i];
            scalingFactorDenom += AcfPtr[i]*fieldPtr[i];
    	}
    }
    else{
	double* scalingFactorNumPtr = (double*)aligned_alloc(64, ATHREAD_NUM * sizeof(double));
	double* scalingFactorDenomPtr = (double*)aligned_alloc(64, ATHREAD_NUM * sizeof(double));
	csrGAMGScale1_param_t param = {
	    nCells,
	    scalingFactorNumPtr,
	    scalingFactorDenomPtr,
	    fieldPtr,
	    sourcePtr,
	    AcfPtr,
	};

	CRTS_athread_spawn(reinterpret_cast<void *>(SLAVE_FUN(csrGAMGScale1_CPE)), &param);
    	CRTS_athread_join();

	for(label i = 0; i < ATHREAD_NUM; i++){
	    scalingFactorNum += scalingFactorNumPtr[i];
	    scalingFactorDenom += scalingFactorDenomPtr[i];
	}

	free(scalingFactorNumPtr);
	free(scalingFactorDenomPtr);

    }
#endif

    vector2D scalingVector(scalingFactorNum, scalingFactorDenom);
    A.mesh().reduce(scalingVector, sumOp<vector2D>());

    const scalar sf = scalingVector.x()/stabilise(scalingVector.y(), vSmall);

    if (debug >= 2)
    {
        Pout<< sf << " ";
    }

    const scalarField& D = A.diag();
    const scalar* const DPtr = D.begin();

#ifndef __sw_qd__
    for (label i=0; i<nCells; i++)
    {
        fieldPtr[i] = sf*fieldPtr[i] + (sourcePtr[i] - sf*AcfPtr[i])/DPtr[i];
    }
#else
    if(nCells <= _coarse_limit){
	for (label i=0; i<nCells; i++)
    	{
            fieldPtr[i] = sf*fieldPtr[i] + (sourcePtr[i] - sf*AcfPtr[i])/DPtr[i];
    	}
    }
    else{
	csrGAMGScale2_param_t param2{
	    nCells,
	    fieldPtr,
	    sf,
	    sourcePtr,
	    AcfPtr,
	    DPtr,
	};
	CRTS_athread_spawn(reinterpret_cast<void *>(SLAVE_FUN(csrGAMGScale2_CPE)), &param2);
    	CRTS_athread_join();
    }
#endif




}


// ************************************************************************* //
