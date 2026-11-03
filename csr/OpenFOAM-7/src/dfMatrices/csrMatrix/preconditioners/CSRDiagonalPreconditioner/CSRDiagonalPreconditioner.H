#ifndef CSRDiagonalPreconditioner_H
#define CSRDiagonalPreconditioner_H

#include "csrMatrix.H"

namespace Foam
{

class CSRDiagonalPreconditioner
:
    public csrMatrix::preconditioner
{

        scalarField rD;


public:

    TypeName("diagonal");



        CSRDiagonalPreconditioner
        (
            const csrMatrix::solver&,
            const dictionary& solverControlsUnused
        );

        CSRDiagonalPreconditioner(const CSRDiagonalPreconditioner&) = delete;


    virtual ~CSRDiagonalPreconditioner()
    {}



        virtual void precondition
        (
            scalarField& wA,
            const scalarField& rA,
            const direction cmpt=0
        ) const;

        virtual void preconditionT
        (
            scalarField& wT,
            const scalarField& rT,
            const direction cmpt=0
        ) const
        {
            return precondition(wT, rT, cmpt);
        }



        void operator=(const CSRDiagonalPreconditioner&) = delete;
};



} 


#endif
