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

#include "GAMGSolver.H"
#include "PCG.H"
#include "PBiCGStab.H"
#include "SubField.H"
#include "NativeCoarseDicGuard.H"

#include <mpi.h>
// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::solverPerformance Foam::GAMGSolver::solve
(
    scalarField& psi,
    const scalarField& source,
    const direction cmpt
) const
{
    // Setup class containing solver performance data
    solverPerformance solverPerf(typeName, fieldName_);

    // Calculate A.psi used to calculate the initial residual
    scalarField Apsi(psi.size());
    matrix_.Amul(Apsi, psi, interfaceBouCoeffs_, interfaces_, cmpt);

    // Create the storage for the finestCorrection which may be used as a
    // temporary in normFactor
    scalarField finestCorrection(psi.size());

    // Calculate normalisation factor
    scalar normFactor = this->normFactor(psi, source, Apsi, finestCorrection);

    if (debug >= 2)
    {
        Pout<< "   Normalisation factor = " << normFactor << endl;
    }

    // Calculate initial finest-grid residual field
    scalarField finestResidual(source - Apsi);

    // Calculate normalised residual for convergence test
    solverPerf.initialResidual() = gSumMag
    (
        finestResidual,
        matrix().mesh().comm()
    )/normFactor;
    solverPerf.finalResidual() = solverPerf.initialResidual();

    double start, end;
    double checkconvergence_time = 0.0;
    double initVcycle_time = 0.0;
    double vcycle_time = 0.0;
    double residual_time = 0.0;
    double convergence_check_time = 0.0;

    // Check convergence, solve if not converged
    start = MPI_Wtime();

    if
    (
        minIter_ > 0
     || !solverPerf.checkConvergence(tolerance_, relTol_)
    )
    {
        double start1, end1;
        
        // Create coarse grid correction fields
        PtrList<scalarField> coarseCorrFields;

        // Create coarse grid sources
        PtrList<scalarField> coarseSources;

        // Create the smoothers for all levels
        PtrList<lduMatrix::smoother> smoothers;

        // Scratch fields if processor-agglomerated coarse level meshes
        // are bigger than original. Usually not needed
        scalarField scratch1;
        scalarField scratch2;

        // Initialise the above data structures
        start1 = MPI_Wtime();
        initVcycle
        (
            coarseCorrFields,
            coarseSources,
            smoothers,
            scratch1,
            scratch2
        );
        end1 = MPI_Wtime();
        initVcycle_time += end1 - start1;

        spmv_time = 0.0;
        smooth_time = 0.0;
        scale_time = 0.0;
        interpolate_time = 0.0;
        restrictField_time = 0.0;
        prolongField_time = 0.0;
        Vcycle_time = 0.0;
        solveCoarsest_time = 0.0;
        left_time = 0.0;

        do
        {
            start1 = MPI_Wtime();
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
            end1 = MPI_Wtime();
            vcycle_time += end1 - start1;

            // Calculate finest level residual field
            start1 = MPI_Wtime();
            matrix_.Amul(Apsi, psi, interfaceBouCoeffs_, interfaces_, cmpt);
            finestResidual = source;
            finestResidual -= Apsi;

            solverPerf.finalResidual() = gSumMag
            (
                finestResidual,
                matrix().mesh().comm()
            )/normFactor;
            end1 = MPI_Wtime();
            residual_time += end1 - start1;

            start1 = MPI_Wtime();
            if (debug >= 2)
            {
                solverPerf.print(Info.masterStream(matrix().mesh().comm()));
            }
            end1 = MPI_Wtime();
            convergence_check_time += end1 - start1;
            
        } while
        (
            (
              ++solverPerf.nIterations() < maxIter_
            && !solverPerf.checkConvergence(tolerance_, relTol_)
            )
         || solverPerf.nIterations() < minIter_
        );
    }

    end = MPI_Wtime();
    checkconvergence_time += end - start;

    Info << "GAMG solver timing breakdown:" << endl;
    Info << "  Total check convergence time      : " << checkconvergence_time << " s" << endl;
    Info << "  - initVcycle time                 : " << initVcycle_time << " s" << endl;
    Info << "  - Vcycle time                     : " << vcycle_time << " s" << endl;
    Info << "  - Residual calculation time       : " << residual_time << " s" << endl;
    Info << "  - Convergence check time          : " << convergence_check_time << " s" << endl;
    
    Info << "GAMG solver Vcycle timing breakdown:" << endl;
    Info << "  - Total Vcycle time               : " << Vcycle_time << " s" << endl;
    Info << "  - SPMV time                       : " << spmv_time << " s" << endl;
    Info << "  - Smoothing time                  : " << smooth_time << " s" << endl;
    Info << "  - Scaling time                    : " << scale_time << " s" << endl;
    Info << "  - Interpolation time              : " << interpolate_time << " s" << endl;
    Info << "  - Restriction time                : " << restrictField_time << " s" << endl;
    Info << "  - Prolongation time               : " << prolongField_time << " s" << endl;
    Info << "  - solveCoarsest time              : " << solveCoarsest_time << " s" << endl;
    Info << "  - Left time                       : " << left_time << " s" << endl;
    Info << "GAMGSolver::solve end" << endl;

    return solverPerf;
}

void Foam::GAMGSolver::Vcycle
(
    const PtrList<lduMatrix::smoother>& smoothers,
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
    double solveCoarsest_start, solveCoarsest_end;
    double left_start, left_end;

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
                left_start = MPI_Wtime();
                coarseCorrFields[leveli] = 0.0;
                left_end = MPI_Wtime();
                left_time += left_end - left_start;

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

                left_start = MPI_Wtime();
                scalarField::subField ACf
                (
                    scratch1,
                    coarseCorrFields[leveli].size()
                );
                left_end = MPI_Wtime();
                left_time += left_end - left_start;

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
                        matrixLevels_[leveli],
                        interfaceLevelsBouCoeffs_[leveli],
                        interfaceLevels_[leveli],
                        coarseSources[leveli],
                        cmpt
                    );
                }
                scale_end = MPI_Wtime();
                scale_time += scale_end - scale_start;

                spmv_start = MPI_Wtime();
                matrixLevels_[leveli].Amul
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

                left_start = MPI_Wtime();
                coarseSources[leveli] -= ACf;
                left_end = MPI_Wtime();
                left_time += left_end - left_start;
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

    // smooth Coarsest level
    smooth_start = MPI_Wtime();
    // if(nCoarsestSweeps_ > 0){
    //     smoothers[coarsestLevel + 1].smooth
    //     (
    //         coarseCorrFields[coarsestLevel],
    //         coarseSources[coarsestLevel],
    //         cmpt,
    //         nCoarsestSweeps_
    //     );
    // }
    smooth_end = MPI_Wtime();
    smooth_time += smooth_end - smooth_start;

    // Solve Coarsest level with either an iterative or direct solver
    solveCoarsest_start = MPI_Wtime();
    if (coarseCorrFields.set(coarsestLevel))
    {
        solveCoarsestLevel
        (
            coarseCorrFields[coarsestLevel],
            coarseSources[coarsestLevel]
        );
    }
    solveCoarsest_end = MPI_Wtime();
    solveCoarsest_time += solveCoarsest_end - solveCoarsest_start;

    // Smoothing and prolongation of the coarse correction fields
    // (going to finer levels)

    left_start = MPI_Wtime();
    scalarField dummyField(0);
    left_end = MPI_Wtime();
    left_time += left_end - left_start;

    for (label leveli = coarsestLevel - 1; leveli >= 0; leveli--)
    {
        if (coarseCorrFields.set(leveli))
        {
            left_start = MPI_Wtime();
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
            left_end = MPI_Wtime();
            left_time += left_end - left_start;

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

            left_start = MPI_Wtime();
            // Create A.psi for this coarse level as a sub-field of Apsi
            scalarField::subField ACf
            (
                scratch1,
                coarseCorrFields[leveli].size()
            );
            scalarField& ACfRef =
                const_cast<scalarField&>(ACf.operator const scalarField&());
            left_end = MPI_Wtime();
            left_time += left_end - left_start;

            interpolate_start = MPI_Wtime();
            if (interpolateCorrection_) //&& leveli < coarsestLevel - 2)
            {
                if (coarseCorrFields.set(leveli+1))
                {
                    interpolate
                    (
                        coarseCorrFields[leveli],
                        ACfRef,
                        matrixLevels_[leveli],
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
                        matrixLevels_[leveli],
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
                    matrixLevels_[leveli],
                    interfaceLevelsBouCoeffs_[leveli],
                    interfaceLevels_[leveli],
                    coarseSources[leveli],
                    cmpt
                );
            }
            scale_end = MPI_Wtime();
            scale_time += scale_end - scale_start;

            left_start = MPI_Wtime();
            // Only add the preSmoothedCoarseCorrField if pre-smoothing is
            // used
            if (nPreSweeps_)
            {
                coarseCorrFields[leveli] += preSmoothedCoarseCorrField;
            }
            left_end = MPI_Wtime();
            left_time += left_end - left_start;

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

    left_start = MPI_Wtime();
    forAll(psi, i)
    {
        psi[i] += finestCorrection[i];
    }
    left_end = MPI_Wtime();
    left_time += left_end - left_start;

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
}


void Foam::GAMGSolver::initVcycle
(
    PtrList<scalarField>& coarseCorrFields,
    PtrList<scalarField>& coarseSources,
    PtrList<lduMatrix::smoother>& smoothers,
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
        lduMatrix::smoother::New
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
                lduMatrix::smoother::New
                (
                    fieldName_,
                    matrixLevels_[leveli],
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


Foam::dictionary Foam::GAMGSolver::PCGsolverDict
(
    const scalar tol,
    const scalar relTol
) const
{
    //dictionary dict(IStringStream("solver PCG; preconditioner DIC;")());  // none DIC
    dictionary dict(IStringStream("solver PCG; preconditioner none;")());
    dict.add("tolerance", tol);
    dict.add("relTol", relTol);

    // n33: pcorr only; absent/original preserves native none-PCG controls.
    // The fvSolution dictionary is a replicated collective caller contract.
    // Do not configure rank-local dictionaries differently.
    if (fieldName_ == "pcorr")
    {
        const word mode(controlDict_.lookupOrDefault<word>
            ("lnjNativeCoarsePreconditioner", "original"));
        if (mode != "original" && mode != "DIC")
        {
            FatalErrorInFunction << "Invalid lnjNativeCoarsePreconditioner: "
                << mode << exit(FatalError);
        }
        if (mode == "DIC" && lnjNativeCoarseDic::admit
            (matrixLevels_[matrixLevels_.size()-1]))
        {
            dict.set("preconditioner", word("DIC"));
        }
    }

    return dict;
}


Foam::dictionary Foam::GAMGSolver::PBiCGStabSolverDict
(
    const scalar tol,
    const scalar relTol
) const
{
    dictionary dict(IStringStream("solver PBiCGStab; preconditioner DILU;")());
    dict.add("tolerance", tol);
    dict.add("relTol", relTol);

    return dict;
}


void Foam::GAMGSolver::solveCoarsestLevel
(
    scalarField& coarsestCorrField,
    const scalarField& coarsestSource
) const
{
    const label coarsestLevel = matrixLevels_.size() - 1;

    label coarseComm = matrixLevels_[coarsestLevel].mesh().comm();

    if (directSolveCoarsest_)
    {
        Info << "GAMG direct Solve Coarsest" << endl;
        coarsestLUMatrixPtr_->solve(coarsestCorrField, coarsestSource);
    }
    // else if
    //(
    //    agglomeration_.processorAgglomerate()
    // && procMatrixLevels_.set(coarsestLevel)
    //)
    //{
    //    // const labelList& agglomProcIDs = agglomeration_.agglomProcIDs
    //    //(
    //    //    coarsestLevel
    //    //);
    //    //
    //    // scalarField allSource;
    //    //
    //    // globalIndex cellOffsets;
    //    // if (Pstream::myProcNo(coarseComm) == agglomProcIDs[0])
    //    //{
    //    //    cellOffsets.offsets() =
    //    //        agglomeration_.cellOffsets(coarsestLevel);
    //    //}
    //    //
    //    // cellOffsets.gather
    //    //(
    //    //    coarseComm,
    //    //    agglomProcIDs,
    //    //    coarsestSource,
    //    //    allSource
    //    //);
    //    //
    //    // scalarField allCorrField;
    //    // solverPerformance coarseSolverPerf;
    //
    //    label solveComm = agglomeration_.procCommunicator(coarsestLevel);
    //
    //    coarsestCorrField = 0;
    //    solverPerformance coarseSolverPerf;
    //
    //    if (Pstream::myProcNo(solveComm) != -1)
    //    {
    //        const lduMatrix& allMatrix = procMatrixLevels_[coarsestLevel];
    //
    //        {
    //            Pout<< "** Master:Solving on comm:" << solveComm
    //                << " with procs:" << UPstream::procID(solveComm) << endl;
    //
    //            if (allMatrix.asymmetric())
    //            {
    //                coarseSolverPerf = PBiCGStab
    //                (
    //                    "coarsestLevelCorr",
    //                    allMatrix,
    //                    procInterfaceLevelsBouCoeffs_[coarsestLevel],
    //                    procInterfaceLevelsIntCoeffs_[coarsestLevel],
    //                    procInterfaceLevels_[coarsestLevel],
    //                    PBiCGStabSolverDict(tolerance_, relTol_)
    //                ).solve
    //                (
    //                    coarsestCorrField,
    //                    coarsestSource
    //                );
    //            }
    //            else
    //            {
    //                coarseSolverPerf = PCG
    //                (
    //                    "coarsestLevelCorr",
    //                    allMatrix,
    //                    procInterfaceLevelsBouCoeffs_[coarsestLevel],
    //                    procInterfaceLevelsIntCoeffs_[coarsestLevel],
    //                    procInterfaceLevels_[coarsestLevel],
    //                    PCGsolverDict(tolerance_, relTol_)
    //                ).solve
    //                (
    //                    coarsestCorrField,
    //                    coarsestSource
    //                );
    //            }
    //        }
    //    }
    //
    //    Pout<< "done master solve." << endl;
    //
    //    //// Scatter to all processors
    //    // coarsestCorrField.setSize(coarsestSource.size());
    //    // cellOffsets.scatter
    //    //(
    //    //    coarseComm,
    //    //    agglomProcIDs,
    //    //    allCorrField,
    //    //    coarsestCorrField
    //    //);
    //
    //    if (debug >= 2)
    //    {
    //        coarseSolverPerf.print(Info.masterStream(coarseComm));
    //    }
    //
    //    Pout<< "procAgglom: coarsestSource   :" << coarsestSource << endl;
    //    Pout<< "procAgglom: coarsestCorrField:" << coarsestCorrField << endl;
    //}
    else
    {
        coarsestCorrField = 0;
        solverPerformance coarseSolverPerf;

        if (matrixLevels_[coarsestLevel].asymmetric())
        {
            Info << "GAMG PBiCGStab Solve Coarsest" << endl;
            coarseSolverPerf = PBiCGStab
            (
                "coarsestLevelCorr",
                matrixLevels_[coarsestLevel],
                interfaceLevelsBouCoeffs_[coarsestLevel],
                interfaceLevelsIntCoeffs_[coarsestLevel],
                interfaceLevels_[coarsestLevel],
                PBiCGStabSolverDict(tolerance_, relTol_)
            ).solve
            (
                coarsestCorrField,
                coarsestSource
            );
        }
        else
        {
            Info << "GAMG PCG Solve Coarsest" << endl;
            coarseSolverPerf = PCG
            (
                "coarsestLevelCorr",
                matrixLevels_[coarsestLevel],
                interfaceLevelsBouCoeffs_[coarsestLevel],
                interfaceLevelsIntCoeffs_[coarsestLevel],
                interfaceLevels_[coarsestLevel],
                PCGsolverDict(tolerance_, relTol_)
            ).solve
            (
                coarsestCorrField,
                coarsestSource
            );
        }

        // if (debug >= 2)
        // {
        coarseSolverPerf.print(Info.masterStream(coarseComm));

        // n33 optional true diagnostic, identical for none and DIC arms.
        if (fieldName_ == "pcorr" && controlDict_.lookupOrDefault<bool>
            ("lnjNativeCoarseTrueResidual", false))
        {
            const double trueStart=MPI_Wtime();
            scalarField ax(coarsestCorrField.size());
            matrixLevels_[coarsestLevel].Amul(ax, coarsestCorrField,
                interfaceLevelsBouCoeffs_[coarsestLevel],
                interfaceLevels_[coarsestLevel], 0);
            scalar bAbs=0, rAbs=0, rInf=0;
            label bad=0;
            forAll(ax,i)
            {
                const scalar r=coarsestSource[i]-ax[i];
                if (!std::isfinite(r) || !std::isfinite(coarsestSource[i])) bad=1;
                else {bAbs+=mag(coarsestSource[i]);rAbs+=mag(r);rInf=max(rInf,mag(r));}
            }
            reduce(bAbs,sumOp<scalar>(),UPstream::msgType(),coarseComm);
            reduce(rAbs,sumOp<scalar>(),UPstream::msgType(),coarseComm);
            reduce(rInf,maxOp<scalar>(),UPstream::msgType(),coarseComm);
            reduce(bad,maxOp<label>(),UPstream::msgType(),coarseComm);
            const bool finiteSums=std::isfinite(bAbs) && std::isfinite(rAbs)
                && bAbs>=0 && rAbs>=0;
            const scalar trueNorm=finiteSums ? rAbs/(bAbs+solverPerformance::small_) : GREAT;
            const bool finite=!bad && finiteSums && std::isfinite(trueNorm) && std::isfinite(rInf);
            const bool pass=finite && (trueNorm<tolerance_
                || (relTol_>0 && trueNorm<relTol_*coarseSolverPerf.initialResidual()));
            Info.masterStream(coarseComm)<<"LNJ_NATIVE_COARSE_TRUE SOLVER="
                <<coarseSolverPerf.solverName()<<" ITERS="<<coarseSolverPerf.nIterations()
                <<" TRUE_NORM="<<trueNorm<<" TRUE_LINF="<<rInf<<" FINITE="<<finite
                <<" TOL="<<tolerance_<<" RELTOL="<<relTol_<<" PASS="<<pass
                <<" CAP="<<lduMatrix::solver::defaultMaxIter_
                <<" CAP_HIT="<<(coarseSolverPerf.nIterations()>=lduMatrix::solver::defaultMaxIter_)
                <<" NORM=zero_initial_sumAbsB_plus_small EXTRA_AMUL=1 EXTRA_REDUCTIONS=4"
                <<" DIAGNOSTIC_SECONDS="<<(MPI_Wtime()-trueStart)<<endl;
        }
        // }
    }
}


// ************************************************************************* //
