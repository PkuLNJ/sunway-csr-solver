#include "CSRDiagonalPreconditioner.H"

namespace Foam
{
    defineTypeNameAndDebug(CSRDiagonalPreconditioner, 0);

    csrMatrix::preconditioner::
        addsymMatrixConstructorToTable<CSRDiagonalPreconditioner>
        adddiagonalPreconditionerSymMatrixConstructorToTable_;

    csrMatrix::preconditioner::
        addasymMatrixConstructorToTable<CSRDiagonalPreconditioner>
        adddiagonalPreconditionerAsymMatrixConstructorToTable_;
}




Foam::CSRDiagonalPreconditioner::CSRDiagonalPreconditioner
(
    const csrMatrix::solver& sol,
    const dictionary&
)
:
    csrMatrix::preconditioner(sol),
    rD(sol.matrix().diag().size())
{
    scalar* __restrict__ rDPtr = rD.begin();
    const scalar* __restrict__ DPtr = solver_.matrix().diag().begin();

    label nCells = rD.size();

    #pragma omp parallel for
    
    for (label cell=0; cell<nCells; cell++)
    {
        rDPtr[cell] = 1.0/DPtr[cell];
    }
}




void Foam::CSRDiagonalPreconditioner::precondition
(
    scalarField& wA,
    const scalarField& rA,
    const direction
) const
{
    scalar* __restrict__ wAPtr = wA.begin();
    const scalar* __restrict__ rAPtr = rA.begin();
    const scalar* __restrict__ rDPtr = rD.begin();

    label nCells = wA.size();

    #pragma omp parallel for
    for (label cell=0; cell<nCells; cell++)
    {
        wAPtr[cell] = rDPtr[cell]*rAPtr[cell];
    }
}
