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
#include "CSRPCG.H"
#include "CSRPBiCGStab.H"
#include "SubField.H"
#include <mpi.h>
#include <cmath>

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::solverPerformance Foam::CSRGAMGSolver::solve
(
    scalarField& psi,
    const scalarField& source,
    const direction cmpt
) const
{
    Info << "Foam::CSRGAMGSolver::solve start" << endl;

    double start_p = 0.;
    double end_p = 0.;
    double vcycle = 0.;
    double amul = 0.;
    double gsummag = 0.;

    start_p = MPI_Wtime();

    // Setup class containing solver performance data
    solverPerformance solverPerf(typeName, fieldName_);

    end_p = MPI_Wtime();
    Info << "solverPerf : " << (end_p - start_p) * 1000 << " ms" << endl;

    start_p = MPI_Wtime();
    
    // Calculate A.psi used to calculate the initial residual
    scalarField Apsi(psi.size());
    matrix_.Amul(Apsi, psi, interfaceBouCoeffs_, interfaces_, cmpt);

    end_p = MPI_Wtime();
    amul += (end_p - start_p) * 1000;

    start_p = MPI_Wtime();

    // Create the storage for the finestCorrection which may be used as a
    // temporary in normFactor
    scalarField finestCorrection(psi.size());

    end_p = MPI_Wtime();
    Info << "finestCorrection : " << (end_p - start_p) * 1000 << " ms" << endl;

    start_p = MPI_Wtime();

    // Calculate normalisation factor
    scalar normFactor = this->normFactor(psi, source, Apsi, finestCorrection);

    end_p = MPI_Wtime();
    Info << "normFactor : " << (end_p - start_p) * 1000 << " ms" << endl;

    if (debug >= 2)
    {
        Pout<< "   Normalisation factor = " << normFactor << endl;
    }

    start_p = MPI_Wtime();

    // Calculate initial finest-grid residual field
    scalarField finestResidual(source - Apsi);

    end_p = MPI_Wtime();
    Info << "finestCorrection : " << (end_p - start_p) * 1000 << " ms" << endl;

    start_p = MPI_Wtime();

    // Calculate normalised residual for convergence test
    solverPerf.initialResidual() = gSumMag
    (
        finestResidual,
        matrix().mesh().comm()
    )/normFactor;
    solverPerf.finalResidual() = solverPerf.initialResidual();

    end_p = MPI_Wtime();
    gsummag += (end_p - start_p) * 1000;

    start_p = MPI_Wtime();


    // Check convergence, solve if not converged
    if
    (
        minIter_ > 0
     || !solverPerf.checkConvergence(tolerance_, relTol_)
    )
    {
        // Create coarse grid correction fields
        PtrList<scalarField> coarseCorrFields;

        // Create coarse grid sources
        PtrList<scalarField> coarseSources;

        // Create the smoothers for all levels
        PtrList<csrMatrix::smoother> smoothers;

        // Scratch fields if processor-agglomerated coarse level meshes
        // are bigger than original. Usually not needed
        scalarField scratch1;
        scalarField scratch2;

        start_p = MPI_Wtime();

        // Initialise the above data structures
        initVcycle
        (
            coarseCorrFields,
            coarseSources,
            smoothers,
            scratch1,
            scratch2
        );

        end_p = MPI_Wtime();
        Info << "initVcycle : " << (end_p - start_p) * 1000 << " ms" << endl;

        do
        {
            start_p = MPI_Wtime();

            Vcycle
            (
                smoothers,
                psi,
                source,
                Apsi,
                finestCorrection,
                finestResidual,

                (scratch1.size() ? scratch1 : Apsi),
                (scratch2.size() ? scratch2 : finestCorrection),

                coarseCorrFields,
                coarseSources,
                cmpt
            );

            end_p = MPI_Wtime();
            vcycle += (end_p - start_p) * 1000;

            start_p = MPI_Wtime();

            // Calculate finest level residual field
            matrix_.Amul(Apsi, psi, interfaceBouCoeffs_, interfaces_, cmpt);

            end_p = MPI_Wtime();
            amul += (end_p - start_p) * 1000;

            finestResidual = source;
            finestResidual -= Apsi;

            start_p = MPI_Wtime();

            solverPerf.finalResidual() = gSumMag
            (
                finestResidual,
                matrix().mesh().comm()
            )/normFactor;

            end_p = MPI_Wtime();
            gsummag += (end_p - start_p) * 1000;

            if (debug >= 2)
            {
                solverPerf.print(Info.masterStream(matrix().mesh().comm()));
            }
        } while
        (
            (
              ++solverPerf.nIterations() < maxIter_
            && !solverPerf.checkConvergence(tolerance_, relTol_)
            )
         || solverPerf.nIterations() < minIter_
        );
    }

    Info << "Vcycle : " << vcycle << " ms" << endl;
    Info << "Amul : " << amul << " ms" << endl;
    Info << "gSumMag : " << gsummag << " ms" << endl;

    Info << "Foam::CSRGAMGSolver::solve end" << endl;

    return solverPerf;
}


void Foam::CSRGAMGSolver::Vcycle
(
    const PtrList<csrMatrix::smoother>& smoothers,
    scalarField& psi,
    const scalarField& source,
    scalarField& Apsi,
    scalarField& finestCorrection,
    scalarField& finestResidual,

    scalarField& scratch1,
    scalarField& scratch2,

    PtrList<scalarField>& coarseCorrFields,
    PtrList<scalarField>& coarseSources,
    const direction cmpt
) const
{
    double spmv_start, spmv_end;
    double smooth_start, smooth_end;
    double scale_start, scale_end;
    double interpolate_start, interpolate_end;
    double restrictField_start, restrictField_end;
    double prolongField_start, prolongField_end;

    double Vcycle_time = 0.f;
    double restrictField_time = 0.f;
    double smooth_time = 0.f;
    double scale_time = 0.f;
    double spmv_time = 0.f;
    double prolongField_time = 0.f;
    double interpolate_time = 0.f;

    double Vcycle_start = MPI_Wtime();

    const label coarsestLevel = matrixLevels_.size() - 1;
    
    // Restrict finest grid residual for the next level up.
    restrictField_start = MPI_Wtime();
    agglomeration_.restrictField(coarseSources[0], finestResidual, 0, true);
    restrictField_end = MPI_Wtime();
    restrictField_time += restrictField_end - restrictField_start;

    // Residual restriction (going to coarser levels)
    for (label leveli = 0; leveli < coarsestLevel; leveli++)
    {
        if (coarseSources.set(leveli + 1))
        {
            // If the optional pre-smoothing sweeps are selected
            // smooth the coarse-grid field for the restricted source
            if (nPreSweeps_)
            {
                coarseCorrFields[leveli] = 0.0;

                smooth_start = MPI_Wtime();
                smoothers[leveli + 1].smooth
                (
                    coarseCorrFields[leveli],
                    coarseSources[leveli],
                    cmpt,
                    min
                    (
                        nPreSweeps_ +  preSweepsLevelMultiplier_*leveli,
                        maxPreSweeps_
                    )
                );
                smooth_end = MPI_Wtime();
                smooth_time += smooth_end - smooth_start;

                scalarField::subField ACf
                (
                    scratch1,
                    coarseCorrFields[leveli].size()
                );

                // Scale coarse-grid correction field
                // but not on the coarsest level because it evaluates to 1
                scale_start = MPI_Wtime();
                if (scaleCorrection_ && leveli < coarsestLevel - 1)
                {
                    scale
                    (
                        coarseCorrFields[leveli],
                        const_cast<scalarField&>
                        (
                            ACf.operator const scalarField&()
                        ),
                        csrMatrixLevels_[leveli],
                        interfaceLevelsBouCoeffs_[leveli],
                        interfaceLevels_[leveli],
                        coarseSources[leveli],
                        cmpt
                    );
                }
                scale_end = MPI_Wtime();
                scale_time += scale_end - scale_start;

                spmv_start = MPI_Wtime();
                csrMatrixLevels_[leveli].Amul
                (
                    const_cast<scalarField&>
                    (
                        ACf.operator const scalarField&()
                    ),
                    coarseCorrFields[leveli],
                    interfaceLevelsBouCoeffs_[leveli],
                    interfaceLevels_[leveli],
                    cmpt
                );
                spmv_end = MPI_Wtime();
                spmv_time += spmv_end - spmv_start;

                coarseSources[leveli] -= ACf;
            }

            // Residual is equal to source
            restrictField_start = MPI_Wtime();
            agglomeration_.restrictField
            (
                coarseSources[leveli + 1],
                coarseSources[leveli],
                leveli + 1,
                true
            );
            restrictField_end = MPI_Wtime();
            restrictField_time += restrictField_end - restrictField_start;
        }
    }

    // n11: bounded distributed coarse Krylov correction, NOT retained SGS.
    // No pre-smoothing here: the inner solve starts from zero anyway.
    smooth_start = MPI_Wtime();
    if (coarseCorrFields.set(coarsestLevel))
    {
        coarseCorrFields[coarsestLevel] = 0.0;
        if (directSolveCoarsest_)
        {
            FatalErrorInFunction
                << "n11 forbids directSolveCoarsest; no global dense LU"
                << abort(FatalError);
        }
        if (solveCoarsest_)
        {
            if
            (
                !coarseSources.set(coarsestLevel)
             || !matrixLevels_.set(coarsestLevel)
             || !csrMatrixLevels_.set(coarsestLevel)
            )
            {
                FatalErrorInFunction
                    << "n11 missing active coarse matrix/source"
                    << abort(FatalError);
            }
            solveCoarsestLevel
            (
                coarseCorrFields[coarsestLevel],
                coarseSources[coarsestLevel]
            );
        }
        // solveCoarsest=false retains an explicit zero correction.
    }
    smooth_end = MPI_Wtime();
    // Existing bucket now includes the full coarse solve; do not add the
    // LNJ_GLOBAL_COARSE INNER_SECONDS to this bucket again.
    smooth_time += smooth_end - smooth_start;


    // Smoothing and prolongation of the coarse correction fields
    // (going to finer levels)

    scalarField dummyField(0);

    for (label leveli = coarsestLevel - 1; leveli >= 0; leveli--)
    {
        if (coarseCorrFields.set(leveli))
        {
            // Create a field for the pre-smoothed correction field
            // as a sub-field of the finestCorrection which is not
            // currently being used
            scalarField::subField preSmoothedCoarseCorrField
            (
                scratch2,
                coarseCorrFields[leveli].size()
            );

            // Only store the preSmoothedCoarseCorrField if pre-smoothing is
            // used
            if (nPreSweeps_)
            {
                preSmoothedCoarseCorrField = coarseCorrFields[leveli];
            }

            prolongField_start = MPI_Wtime();
            agglomeration_.prolongField
            (
                coarseCorrFields[leveli],
                (
                    coarseCorrFields.set(leveli + 1)
                  ? coarseCorrFields[leveli + 1]
                  : dummyField              // dummy value
                ),
                leveli + 1,
                true
            );
            prolongField_end = MPI_Wtime();
            prolongField_time += prolongField_end - prolongField_start;

            // Create A.psi for this coarse level as a sub-field of Apsi
            scalarField::subField ACf
            (
                scratch1,
                coarseCorrFields[leveli].size()
            );
            scalarField& ACfRef =
                const_cast<scalarField&>(ACf.operator const scalarField&());

            interpolate_start = MPI_Wtime();
            if (interpolateCorrection_) //&& leveli < coarsestLevel - 2)
            {
                if (coarseCorrFields.set(leveli+1))
                {
                    interpolate
                    (
                        coarseCorrFields[leveli],
                        ACfRef,
                        csrMatrixLevels_[leveli],
                        interfaceLevelsBouCoeffs_[leveli],
                        interfaceLevels_[leveli],
                        agglomeration_.restrictAddressing(leveli + 1),
                        coarseCorrFields[leveli + 1],
                        cmpt
                    );
                }
                else
                {
                    interpolate
                    (
                        coarseCorrFields[leveli],
                        ACfRef,
                        csrMatrixLevels_[leveli],
                        interfaceLevelsBouCoeffs_[leveli],
                        interfaceLevels_[leveli],
                        cmpt
                    );
                }
            }
            interpolate_end = MPI_Wtime();
            interpolate_time += interpolate_end - interpolate_start;

            // Scale coarse-grid correction field
            // but not on the coarsest level because it evaluates to 1
            scale_start = MPI_Wtime();
            if
            (
                scaleCorrection_
             && (interpolateCorrection_ || leveli < coarsestLevel - 1)
            )
            {
                scale
                (
                    coarseCorrFields[leveli],
                    ACfRef,
                    csrMatrixLevels_[leveli],
                    interfaceLevelsBouCoeffs_[leveli],
                    interfaceLevels_[leveli],
                    coarseSources[leveli],
                    cmpt
                );
            }
            scale_end = MPI_Wtime();
            scale_time += scale_end - scale_start;

            // Only add the preSmoothedCoarseCorrField if pre-smoothing is
            // used
            if (nPreSweeps_)
            {
                coarseCorrFields[leveli] += preSmoothedCoarseCorrField;
            }

            smooth_start = MPI_Wtime();
            smoothers[leveli + 1].smooth
            (
                coarseCorrFields[leveli],
                coarseSources[leveli],
                cmpt,
                min
                (
                    nPostSweeps_ + postSweepsLevelMultiplier_*leveli,
                    maxPostSweeps_
                )
            );
            smooth_end = MPI_Wtime();
            smooth_time += smooth_end - smooth_start;
        }
    }

    // Prolong the finest level correction
    prolongField_start = MPI_Wtime();
    agglomeration_.prolongField
    (
        finestCorrection,
        coarseCorrFields[0],
        0,
        true
    );
    prolongField_end = MPI_Wtime();
    prolongField_time += prolongField_end - prolongField_start;

    interpolate_start = MPI_Wtime();
    if (interpolateCorrection_)
    {
        interpolate
        (
            finestCorrection,
            Apsi,
            matrix_,
            interfaceBouCoeffs_,
            interfaces_,
            agglomeration_.restrictAddressing(0),
            coarseCorrFields[0],
            cmpt
        );
    }
    interpolate_end = MPI_Wtime();
    interpolate_time += interpolate_end - interpolate_start;


    scale_start = MPI_Wtime();
    if (scaleCorrection_)
    {
        // Scale the finest level correction
        scale
        (
            finestCorrection,
            Apsi,
            matrix_,
            interfaceBouCoeffs_,
            interfaces_,
            finestResidual,
            cmpt
        );
    }
    scale_end = MPI_Wtime();
    scale_time += scale_end - scale_start;


    forAll(psi, i)
    {
        psi[i] += finestCorrection[i];
    }

    smooth_start = MPI_Wtime();
    if(nFinestSweeps_ > 0){
        smoothers[0].smooth
        (
            psi,
            source,
            cmpt,
            nFinestSweeps_
        );
    }
    smooth_end = MPI_Wtime();
    smooth_time += smooth_end - smooth_start;

    double Vcycle_end = MPI_Wtime();
    Vcycle_time += Vcycle_end - Vcycle_start;

    Info << "restrictField_time :" << restrictField_time * 1000 << " ms" << endl;
    Info << "smooth_time :" << smooth_time * 1000 << " ms" << endl;
    Info << "scale_time :" << scale_time * 1000 << " ms" << endl;
    Info << "spmv_time :" << spmv_time * 1000 << " ms" << endl;
    Info << "prolongField_time :" << prolongField_time * 1000 << " ms" << endl;
    Info << "interpolate_time :" << interpolate_time * 1000 << " ms" << endl;
    Info << "(Total)Vcycle_time :" << Vcycle_time * 1000 << " ms" << endl;

}

void Foam::CSRGAMGSolver::initVcycle
(
    PtrList<scalarField>& coarseCorrFields,
    PtrList<scalarField>& coarseSources,
    PtrList<csrMatrix::smoother>& smoothers,
    scalarField& scratch1,
    scalarField& scratch2
) const
{
    label maxSize = matrix_.diag().size();

    coarseCorrFields.setSize(matrixLevels_.size());
    coarseSources.setSize(matrixLevels_.size());
    smoothers.setSize(matrixLevels_.size() + 1);

    // Create the smoother for the finest level
    smoothers.set
    (
        0,
        csrMatrix::smoother::New
        (
            fieldName_,
            matrix_,
            interfaceBouCoeffs_,
            interfaceIntCoeffs_,
            interfaces_,
            controlDict_
        )
    );

    forAll(matrixLevels_, leveli)
    {
        if (agglomeration_.nCells(leveli) >= 0)
        {
            label nCoarseCells = agglomeration_.nCells(leveli);

            coarseSources.set(leveli, new scalarField(nCoarseCells));
        }

        if (matrixLevels_.set(leveli))
        {
            const lduMatrix& mat = matrixLevels_[leveli];

            label nCoarseCells = mat.diag().size();

            maxSize = max(maxSize, nCoarseCells);

            coarseCorrFields.set(leveli, new scalarField(nCoarseCells));

            smoothers.set
            (
                leveli + 1,
                csrMatrix::smoother::New
                (
                    fieldName_,
                    csrMatrixLevels_[leveli],
                    interfaceLevelsBouCoeffs_[leveli],
                    interfaceLevelsIntCoeffs_[leveli],
                    interfaceLevels_[leveli],
                    controlDict_
                )
            );
        }
    }

    if (maxSize > matrix_.diag().size())
    {
        // Allocate some scratch storage
        scratch1.setSize(maxSize);
        scratch2.setSize(maxSize);
    }
}


Foam::dictionary Foam::CSRGAMGSolver::PCGsolverDict
(
    const scalar tol,
    const scalar relTol
) const
{
    // Private n11 screening constants; final pressure tolerances are untouched.
    // The inherited GAMG tol/relTol arguments do not set the inner work budget.
    (void)tol;
    (void)relTol;
    dictionary dict(IStringStream("solver PCG; preconditioner diagonal;")());
    dict.add("tolerance", scalar(1e-6));
    dict.add("relTol", scalar(0.1));
    dict.add("minIter", label(0));
    dict.add("maxIter", label(8));

    return dict;
}


Foam::dictionary Foam::CSRGAMGSolver::PBiCGStabSolverDict
(
    const scalar tol,
    const scalar relTol
) const
{
    // dictionary dict(IStringStream("solver PBiCGStab; preconditioner DILU;")());
    dictionary dict(IStringStream("solver PBiCGStab; preconditioner none;")());
    dict.add("tolerance", tol);
    dict.add("relTol", relTol);

    return dict;
}


void Foam::CSRGAMGSolver::solveCoarsestLevel
(
    scalarField& coarsestCorrField,
    const scalarField& coarsestSource
) const
{
    const label coarsestLevel = matrixLevels_.size() - 1;
    if
    (
        directSolveCoarsest_
     || !matrixLevels_.set(coarsestLevel)
     || !csrMatrixLevels_.set(coarsestLevel)
    )
    {
        FatalErrorInFunction
            << "n11 requires an active distributed coarse matrix and forbids LU"
            << abort(FatalError);
    }
    if
    (
        !matrixLevels_[coarsestLevel].symmetric()
     || matrixLevels_[coarsestLevel].asymmetric()
    )
    {
        FatalErrorInFunction
            << "n11 only tests symmetric coarse PCG; no silent PBiCGStab route"
            << abort(FatalError);
    }

    const label coarseComm = matrixLevels_[coarsestLevel].mesh().comm();
    coarsestCorrField = 0.0;
    const double innerStart = MPI_Wtime();
    const solverPerformance coarseSolverPerf = CSRPCG
    (
        "coarsestLevelCorr",
        csrMatrixLevels_[coarsestLevel],
        interfaceLevelsBouCoeffs_[coarsestLevel],
        interfaceLevelsIntCoeffs_[coarsestLevel],
        interfaceLevels_[coarsestLevel],
        PCGsolverDict(scalar(1e-6), scalar(0.1))
    ).solve(coarsestCorrField, coarsestSource, 0);
    const double innerSeconds = MPI_Wtime()-innerStart;
    const scalar initial = coarseSolverPerf.initialResidual();
    const scalar final = coarseSolverPerf.finalResidual();
    const bool finite = std::isfinite(initial) && std::isfinite(final);
    const bool converged = finite && (final < scalar(1e-6) || final < scalar(0.1)*initial);

    // One extra master-only line per coarse call. Existing CSRPCG::print_time()
    // is intentionally retained; this is a logging-bearing screen, not a clean
    // final-performance binary. No extra collective/norm is added by this line.
    Info.masterStream(coarseComm)
        << "LNJ_GLOBAL_COARSE OUTER_FIELD=" << fieldName_
        << " INNER_FIELD=coarsestLevelCorr"
        << " INNER_ITERS=" << coarseSolverPerf.nIterations()
        << " INITIAL_NORM=" << initial
        << " FINAL_RECURSIVE_NORM=" << final
        << " TOLERANCE=1e-6 RELTOL=0.1 MINITER=0 MAXITER=8"
        << " MAXITER_REACHED=" << label(coarseSolverPerf.nIterations() >= 8)
        << " INNER_TOL_CONVERGED=" << label(converged)
        << " FINITE=" << label(finite)
        << " INNER_SECONDS=" << innerSeconds
        << " TIMING_MASTER_LOCAL=1 TRUE_RESIDUAL_CHECKED=0"
        << endl;
    if
    (
        !finite
     || coarseSolverPerf.nIterations() < 0
     || coarseSolverPerf.nIterations() > 8
    )
    {
        // These solver norms and iteration decisions already use coarseComm
        // collectives. Stop the diagnostic candidate; add no new collective.
        FatalErrorInFunction
            << "n11 invalid coarse solver result: finite=" << label(finite)
            << ", iterations=" << coarseSolverPerf.nIterations()
            << abort(FatalError);
    }

}


// ************************************************************************* //


