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

#include "CSRGAMGPreconditioner.H"
#include <mpi.h>
#include <cstdlib>
#include <cstring>

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(CSRGAMGPreconditioner, 0);

    csrMatrix::preconditioner::addsymMatrixConstructorToTable
    <CSRGAMGPreconditioner> addCSRGAMGPreconditionerSymMatrixConstructorToTable_;

    csrMatrix::preconditioner::addasymMatrixConstructorToTable
    <CSRGAMGPreconditioner> addCSRGAMGPreconditionerAsymMatrixConstructorToTable_;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::CSRGAMGPreconditioner::CSRGAMGPreconditioner
(
    const csrMatrix::solver& sol,
    const dictionary& solverControls
)
:
    CSRGAMGSolver
    (
        sol.fieldName(),
        sol.matrix(),
        sol.interfaceBouCoeffs(),
        sol.interfaceIntCoeffs(),
        sol.interfaces(),
        solverControls
    ),
    csrMatrix::preconditioner(sol),
    nVcycles_(2)
{
    readControls();
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::CSRGAMGPreconditioner::~CSRGAMGPreconditioner()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::CSRGAMGPreconditioner::invalidateWorkspace() const
{
    workspaceReady_ = false;
    workspaceMatrix_ = nullptr;
    workspaceSmoothers_.clear();
    workspaceCoarseCorrFields_.clear();
    workspaceCoarseSources_.clear();
    workspaceAwA_.clear();
    workspaceFinestCorrection_.clear();
    workspaceFinestResidual_.clear();
    workspaceApsiScratch_.clear();
    workspaceFinestCorrectionScratch_.clear();
}


void Foam::CSRGAMGPreconditioner::readControls()
{
    invalidateWorkspace();
    CSRGAMGSolver::readControls();
    nVcycles_ = controlDict_.lookupOrDefault<label>("nVcycles", 2);
}


void Foam::CSRGAMGPreconditioner::precondition
(
    scalarField& wA,
    const scalarField& rA,
    const direction cmpt
) const
{
    double start ,end;

    double procondition_start = MPI_Wtime();
    const char* resetMode = std::getenv("LNJ_VCYCLE_RESET");
    const bool minimalReset = resetMode && std::strcmp(resetMode, "minimal") == 0;
    if (resetMode && !minimalReset && std::strcmp(resetMode, "full") != 0)
    {
        FatalErrorInFunction
            << "LNJ_VCYCLE_RESET must be full or minimal, got: " << resetMode
            << exit(FatalError);
    }
    if (wA.size() != rA.size() || wA.size() != matrix_.diag().size())
    {
        FatalErrorInFunction
            << "GAMG workspace vector/matrix size mismatch: wA="
            << wA.size() << ", rA=" << rA.size()
            << ", matrix=" << matrix_.diag().size()
            << exit(FatalError);
    }

    wA = 0.0;
    start = MPI_Wtime();

    if
    (
        !workspaceReady_
     || workspaceMatrix_ != &matrix_
     || workspaceAwA_.size() != wA.size()
     || workspaceFinestCorrection_.size() != wA.size()
     || workspaceFinestResidual_.size() != wA.size()
    )
    {
        invalidateWorkspace();
        workspaceAwA_.setSize(wA.size());
        workspaceFinestCorrection_.setSize(wA.size());
        workspaceFinestResidual_.setSize(wA.size());

        // initVcycle alone constructs the hierarchy-dependent fields/smoothers.
        // The hierarchy is immutable for this linear-solver instance; neither
        // controls nor another matrix may reuse this storage without invalidation.
        initVcycle
        (
            workspaceCoarseCorrFields_,
            workspaceCoarseSources_,
            workspaceSmoothers_,
            workspaceApsiScratch_,
            workspaceFinestCorrectionScratch_
        );
        workspaceMatrix_ = &matrix_;
        workspaceReady_ = true;
    }

    // Reuse allocations, never residual/correction values from an earlier PCG
    // iteration. Only set coarse entries actually created on this rank.
    workspaceAwA_ = 0.0;
    if (!minimalReset) workspaceFinestCorrection_ = 0.0;
    workspaceFinestResidual_ = rA;
    workspaceApsiScratch_ = 0.0;
    if (!minimalReset) workspaceFinestCorrectionScratch_ = 0.0;
    forAll(workspaceCoarseCorrFields_, leveli)
    {
        if (workspaceCoarseCorrFields_.set(leveli))
        {
            workspaceCoarseCorrFields_[leveli] = 0.0;
        }
    }
    if (!minimalReset)
    {
        forAll(workspaceCoarseSources_, leveli)
        {
            if (workspaceCoarseSources_.set(leveli))
            {
                workspaceCoarseSources_[leveli] = 0.0;
            }
        }
    }

    scalarField& AwA = workspaceAwA_;
    scalarField& finestCorrection = workspaceFinestCorrection_;
    scalarField& finestResidual = workspaceFinestResidual_;
    PtrList<scalarField>& coarseCorrFields = workspaceCoarseCorrFields_;
    PtrList<scalarField>& coarseSources = workspaceCoarseSources_;
    PtrList<csrMatrix::smoother>& smoothers = workspaceSmoothers_;
    scalarField& ApsiScratch = workspaceApsiScratch_;
    scalarField& finestCorrectionScratch = workspaceFinestCorrectionScratch_;
    end = MPI_Wtime();
    // Includes allocation on first call and all reset/setup work on every call.
    initVcycle_time += end - start;


    start = MPI_Wtime();
    for (label cycle=0; cycle<nVcycles_; cycle++)
    {
        Vcycle
        (
            smoothers,
            wA,
            rA,
            AwA,
            finestCorrection,
            finestResidual,

            (ApsiScratch.size() ? ApsiScratch : AwA),
            (
                finestCorrectionScratch.size()
              ? finestCorrectionScratch
              : finestCorrection
            ),

            coarseCorrFields,
            coarseSources,
            cmpt
        );

        if (cycle < nVcycles_-1)
        {
            // Calculate finest level residual field
            matrix_.Amul(AwA, wA, interfaceBouCoeffs_, interfaces_, cmpt);
            finestResidual = rA;
            finestResidual -= AwA;
        }
    }
    end = MPI_Wtime();
    Vcycle_time += end - start;

    double procondition_end = MPI_Wtime();
    procondition_time += procondition_end - procondition_start;
}


// ************************************************************************* //
