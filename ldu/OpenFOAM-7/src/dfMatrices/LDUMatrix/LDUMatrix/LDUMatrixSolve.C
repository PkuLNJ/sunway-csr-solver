#include "LDUMatrix.H"
#include <cassert>
#include <sstream>
#include <cstdio>
#include "PstreamGlobals.H"
#include "Residuals.H"

namespace Foam{

template<class Type>
SolverPerformance<Type> LDUMatrix::solve(
    GeometricField<Type, fvPatchField, volMesh>& psi,
    const Field<Type>& source,
    const FieldField<Field, Type>& internalCoeffs,
    const FieldField<Field, Type>& boundaryCoeffs,
    const dictionary& solverControls
){
    SolverPerformance<Type> solverPerfVec
    (
        "LDUMatrix::solve",
        psi.name()
    );

    scalarField saveDiag(diag());

    Field<Type> sourceCpy(source);

    addBoundarySource(sourceCpy, psi, boundaryCoeffs);

    typename Type::labelType validComponents
    (
        psi.mesh().template validComponents<Type>()
    );

    for (direction cmpt=0; cmpt<Type::nComponents; cmpt++)
    {
        if (validComponents[cmpt] == -1) continue;

        // copy field and source
        scalarField psiCmpt(psi.primitiveField().component(cmpt));
        addBoundaryDiag(diag(), internalCoeffs, cmpt);

        scalarField sourceCmpt(sourceCpy.component(cmpt));

        FieldField<Field, scalar> bouCoeffsCmpt
        (
            boundaryCoeffs.component(cmpt)
        );

        FieldField<Field, scalar> intCoeffsCmpt
        (
            internalCoeffs.component(cmpt)
        );

        lduInterfaceFieldPtrsList interfaces =
            psi.boundaryField().scalarInterfaces();

        // Use the initMatrixInterfaces and updateMatrixInterfaces to correct
        // bouCoeffsCmpt for the explicit part of the coupled boundary
        // conditions
        initMatrixInterfaces
        (
            bouCoeffsCmpt,
            interfaces,
            psiCmpt,
            sourceCmpt,
            cmpt
        );

        updateMatrixInterfaces
        (
            bouCoeffsCmpt,
            interfaces,
            psiCmpt,
            sourceCmpt,
            cmpt
        );

        solverPerformance solverPerf;

        // Solver call
        solverPerf = LDUMatrix::solver::New
        (
            psi.name() + pTraits<Type>::componentNames[cmpt],
            *this,
            bouCoeffsCmpt,
            intCoeffsCmpt,
            interfaces,
            solverControls
        )->solve(psiCmpt, sourceCmpt, cmpt);

        if (SolverPerformance<Type>::debug)
        {
            solverPerf.print(Info.masterStream(this->mesh().comm()));
        }

        solverPerfVec.replace(cmpt, solverPerf);
        solverPerfVec.solverName() = solverPerf.solverName();

        psi.primitiveFieldRef().replace(cmpt, psiCmpt);
        diag() = saveDiag;
    }

    psi.correctBoundaryConditions();

    Residuals<Type>::append(psi.mesh(), solverPerfVec);

    return solverPerfVec;
}

template<>
solverPerformance LDUMatrix::solve
(
    GeometricField<scalar, fvPatchField, volMesh>& psi,
    const Field<scalar>& source,
    const FieldField<Field, scalar>& internalCoeffs,
    const FieldField<Field, scalar>& boundaryCoeffs,
    const dictionary& solverControls
)
{
    double start, end;

    double LDU_1_addBoundaryDiag_time = 0.0;
    double LDU_2_addBoundarySource_time = 0.0;
    double LDU_3_solverNew_time = 0.0;
    double LDU_4_solverSolve_time = 0.0;
    double LDU_5_debugPrint_time = 0.0;
    double LDU_6_restoreDiag_time = 0.0;
    double LDU_7_correctBC_time = 0.0;
    double LDU_8_appendResiduals_time = 0.0;
    double LDU_9_solverPrintTime_time = 0.0;
    
    start = MPI_Wtime();
    scalarField saveDiag(diag());
    addBoundaryDiag(diag(), internalCoeffs, 0);
    const_cast<scalarField&>(ldu().diag()) = diag();
    end = MPI_Wtime();
    LDU_1_addBoundaryDiag_time += end - start;

    start = MPI_Wtime();
    scalarField sourceCpy(source);
    addBoundarySource(sourceCpy, psi, boundaryCoeffs, false);
    end = MPI_Wtime();
    LDU_2_addBoundarySource_time += end - start;

    start = MPI_Wtime();
    Foam::autoPtr<Foam::LDUMatrix::solver> solver = LDUMatrix::solver::New
    (
        psi.name(),
        *this,
        boundaryCoeffs,
        internalCoeffs,
        psi.boundaryField().scalarInterfaces(),
        solverControls
    );
    end = MPI_Wtime();
    LDU_3_solverNew_time += end - start;
    
    start = MPI_Wtime();
    // Solver call
    solverPerformance solverPerf = solver->solve(psi.primitiveFieldRef(), sourceCpy);
    end = MPI_Wtime();
    LDU_4_solverSolve_time += end - start;

    start = MPI_Wtime();
    if (solverPerformance::debug)
    {
        solverPerf.print(Info.masterStream(mesh().comm()));
    }
    end = MPI_Wtime();
    LDU_5_debugPrint_time += end - start;

    start = MPI_Wtime();
    diag() = saveDiag;
    const_cast<scalarField&>(ldu().diag()) = saveDiag;
    end = MPI_Wtime();
    LDU_6_restoreDiag_time += end - start;

    start = MPI_Wtime();
    psi.correctBoundaryConditions();
    end = MPI_Wtime();
    LDU_7_correctBC_time += end - start;

    start = MPI_Wtime();
    Residuals<scalar>::append(psi.mesh(), solverPerf);
    end = MPI_Wtime();
    LDU_8_appendResiduals_time += end - start;

#ifdef _PROFILING_
    start = MPI_Wtime();
    solver->print_time();
    end = MPI_Wtime();
    LDU_9_solverPrintTime_time += end - start;
#endif

    Info << "LDUMatrix::solve timing summary :" << endl;
    Info << " 1. addBoundaryDiag time          : " << LDU_1_addBoundaryDiag_time << " s" << endl;
    Info << " 2. addBoundarySource time        : " << LDU_2_addBoundarySource_time << " s" << endl;
    Info << " 3. solver New time               : " << LDU_3_solverNew_time << " s" << endl;
    Info << " 4. solver solve time             : " << LDU_4_solverSolve_time << " s" << endl;
    Info << " 5. debug print time              : " << LDU_5_debugPrint_time << " s" << endl;
    Info << " 6. restore diag time             : " << LDU_6_restoreDiag_time << " s" << endl;
    Info << " 7. correct BC time               : " << LDU_7_correctBC_time << " s" << endl;
    Info << " 8. append Residuals time         : " << LDU_8_appendResiduals_time << " s" << endl;
#ifdef _PROFILING_
    Info << " 9. solver print_time time        : " << LDU_9_solverPrintTime_time << " s" << endl;
#endif  

    return solverPerf;
}

template<class Type>
SolverPerformance<Type> LDUMatrix::solve(
    GeometricField<Type, fvPatchField, volMesh>& psi,
    const Field<Type>& source,
    const FieldField<Field, Type>& internalCoeffs,
    const FieldField<Field, Type>& boundaryCoeffs

){
    const dictionary solverControls = psi.mesh().solverDict
    (
        psi.select
        (
            psi.mesh().data::template lookupOrDefault<bool>
            ("finalIteration", false)
        )
    );
    return solve(psi,source,internalCoeffs,boundaryCoeffs,solverControls);
}

template<class Type>
SolverPerformance<Type> LDUMatrix::solve(
    GeometricField<Type, fvPatchField, volMesh>& psi,
    const Field<Type>& source,
    const FieldField<Field, Type>& internalCoeffs,
    const FieldField<Field, Type>& boundaryCoeffs,
    const word& name
){
    const dictionary solverControls = psi.mesh().solverDict
    (
        psi.mesh().data::template lookupOrDefault<bool>
        ("finalIteration", false)
        ? word(name + "Final")
        : name
    );
    return solve(psi,source,internalCoeffs,boundaryCoeffs,solverControls);
}


template<class Type>
void LDUMatrix::addBoundarySource
(
    Field<Type>& source,
    const GeometricField<Type, fvPatchField, volMesh>& psi,
    const FieldField<Field, Type>& boundaryCoeffs,
    const bool couples
) const {
    forAll(psi.boundaryField(), patchi)
    {
        const fvPatchField<Type>& ptf = psi.boundaryField()[patchi];
        const Field<Type>& pbc = boundaryCoeffs[patchi];

        if (!ptf.coupled())
        {
            addToInternalField(lduAddr().patchAddr(patchi), pbc, source);
        }
        else if (couples)
        {
            const tmp<Field<Type>> tpnf = ptf.patchNeighbourField();
            const Field<Type>& pnf = tpnf();

            const labelUList& addr = lduAddr().patchAddr(patchi);

            forAll(addr, facei)
            {
                source[addr[facei]] += cmptMultiply(pbc[facei], pnf[facei]);
            }
        }
    }
}

template<class Type>
void LDUMatrix::addBoundaryDiag
(
    scalarField& diag,
    const FieldField<Field, Type>& internalCoeffs,
    const direction solveCmpt
) const {
    forAll(internalCoeffs, patchi)
    {
        addToInternalField
        (
            lduAddr().patchAddr(patchi),
            internalCoeffs[patchi].component(solveCmpt),
            diag
        );
    }
}


template<class Type2>
void LDUMatrix::addToInternalField
(
    const labelUList& addr,
    const Field<Type2>& pf,
    Field<Type2>& intf
) const {
    if (addr.size() != pf.size())
    {
        FatalErrorInFunction
            << "sizes of addressing and field are different"
            << abort(FatalError);
    }

    forAll(addr, facei)
    {
        intf[addr[facei]] += pf[facei];
    }
}

template<class Type2>
void LDUMatrix::addToInternalField
(
    const labelUList& addr,
    const tmp<Field<Type2>>& tpf,
    Field<Type2>& intf
) const
{
    addToInternalField(addr, tpf(), intf);
    tpf.clear();
}

template
void LDUMatrix::addToInternalField<scalar>
(
    const labelUList& addr,
    const Field<scalar>& pf,
    Field<scalar>& intf
) const;

template
void LDUMatrix::addToInternalField<vector>
(
    const labelUList& addr,
    const Field<vector>& pf,
    Field<vector>& intf
) const;

template
void LDUMatrix::addBoundarySource<scalar>
(
    Field<scalar>& source,
    const GeometricField<scalar, fvPatchField, volMesh>& psi,
    const FieldField<Field, scalar>& boundaryCoeffs,
    const bool couples
) const;

template
void LDUMatrix::addBoundarySource<vector>
(
    Field<vector>& source,
    const GeometricField<vector, fvPatchField, volMesh>& psi,
    const FieldField<Field, vector>& boundaryCoeffs,
    const bool couples
) const;

template
void LDUMatrix::addBoundaryDiag<scalar>
(
    scalarField& diag,
    const FieldField<Field, scalar>& internalCoeffs,
    const direction solveCmpt
) const;

template
void LDUMatrix::addBoundaryDiag<vector>
(
    scalarField& diag,
    const FieldField<Field, vector>& internalCoeffs,
    const direction solveCmpt
) const;

template
SolverPerformance<vector> LDUMatrix::solve<vector>(
    GeometricField<vector, fvPatchField, volMesh>& psi,
    const Field<vector>& source,
    const FieldField<Field, vector>& internalCoeffs,
    const FieldField<Field, vector>& boundaryCoeffs,
    const dictionary& solverControls
);

template
SolverPerformance<scalar> LDUMatrix::solve<scalar>(
    GeometricField<scalar, fvPatchField, volMesh>& psi,
    const Field<scalar>& source,
    const FieldField<Field, scalar>& internalCoeffs,
    const FieldField<Field, scalar>& boundaryCoeffs
);

template
SolverPerformance<vector> LDUMatrix::solve<vector>(
    GeometricField<vector, fvPatchField, volMesh>& psi,
    const Field<vector>& source,
    const FieldField<Field, vector>& internalCoeffs,
    const FieldField<Field, vector>& boundaryCoeffs
);

template
SolverPerformance<scalar> LDUMatrix::solve<scalar>(
    GeometricField<scalar, fvPatchField, volMesh>& psi,
    const Field<scalar>& source,
    const FieldField<Field, scalar>& internalCoeffs,
    const FieldField<Field, scalar>& boundaryCoeffs,
    const word& name
);

template
SolverPerformance<vector> LDUMatrix::solve<vector>(
    GeometricField<vector, fvPatchField, volMesh>& psi,
    const Field<vector>& source,
    const FieldField<Field, vector>& internalCoeffs,
    const FieldField<Field, vector>& boundaryCoeffs,
    const word& name
);




}