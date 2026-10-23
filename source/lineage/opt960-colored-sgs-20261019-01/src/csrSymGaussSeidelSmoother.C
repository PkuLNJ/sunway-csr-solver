/*---------------------------------------------------------------------------*\
  OpenFOAM: The Open Source CFD Toolbox
  Copyright (C) 2012-2018 OpenFOAM Foundation
  Derived from csrSymGaussSeidelSmoother.C and the private face-SGS candidate.
  Licensed under GNU GPL version 3 or, at your option, any later version.
  No warranty; see <http://www.gnu.org/licenses/>.
\*---------------------------------------------------------------------------*/
#include "csrSymGaussSeidelSmoother.H"
#include "lnjFaceSymGaussSeidel.H"
#include "lnjColoredSgsParam.h"
#include "Pstream.H"
#include <crts.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>

extern "C"
{
    extern void SLAVE_FUN(lnjColoredSgs_CPE)(struct lnjColoredSgsParam*);
}

namespace Foam
{
    defineTypeNameAndDebug(csrSymGaussSeidelSmoother, 0);
    csrMatrix::smoother::addsymMatrixConstructorToTable<csrSymGaussSeidelSmoother>
        addcsrSymGaussSeidelSmootherSymMatrixConstructorToTable_;
    csrMatrix::smoother::addasymMatrixConstructorToTable<csrSymGaussSeidelSmoother>
        addcsrSymGaussSeidelSmootherAsymMatrixConstructorToTable_;
}

Foam::csrSymGaussSeidelSmoother::csrSymGaussSeidelSmoother
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
        fieldName, matrix, interfaceBouCoeffs, interfaceIntCoeffs, interfaces
    ),
    mode_(naturalMode),
    colored_(),
    packedX_(),
    packedRhs_(),
    bPrime_()
{
    static_assert(std::is_same<label, int64_t>::value,
                  "Colored SGS CPE ABI requires label == int64_t");
    static_assert(std::is_same<scalar, double>::value,
                  "Colored SGS CPE ABI requires scalar == double");
    const char* const environment = std::getenv("LNJ_COLORED_SGS_MODE");
    if (!environment || std::strcmp(environment, "natural") == 0)
        mode_ = naturalMode;
    else if (std::strcmp(environment, "host") == 0)
        mode_ = hostMode;
    else if (std::strcmp(environment, "cpe") == 0)
        mode_ = cpeMode;
    else
    {
        FatalErrorInFunction
            << "LNJ_COLORED_SGS_MODE must be exactly natural, host or cpe; got '"
            << environment << "'" << exit(FatalError);
    }

    const label n = matrix.row();
    if (mode_ != naturalMode && n >= 32768)
    {
        const label nnz = matrix.off_diag_value().size();
        std::vector<label> rowPtr(static_cast<std::size_t>(n) + 1);
        std::vector<label> columns(static_cast<std::size_t>(nnz));
        std::vector<scalar> values(static_cast<std::size_t>(nnz));
        std::vector<scalar> diagonal(static_cast<std::size_t>(n));
        for (label i = 0; i <= n; ++i)
            rowPtr[static_cast<std::size_t>(i)] = matrix.off_diag_rowptr()[i];
        for (label i = 0; i < nnz; ++i)
        {
            columns[static_cast<std::size_t>(i)] = matrix.off_diag_colidx()[i];
            values[static_cast<std::size_t>(i)] = matrix.off_diag_value()[i];
        }
        for (label i = 0; i < n; ++i)
            diagonal[static_cast<std::size_t>(i)] = matrix.diag()[i];
        try
        {
            // OpenFOAM constructs both directed entries from each LDU face.
            // Trusted fast path still checks every edge's color independence.
            colored_.reset(new lnj::ColoredSgs<label, scalar>
            (
                lnj::makeColoredSgsKnownBidirectional(rowPtr, columns, values, diagonal)
            ));
        }
        catch (const std::exception& error)
        {
            FatalErrorInFunction << "Invalid colored SGS matrix: "
                << error.what() << exit(FatalError);
        }
        packedX_.resize(static_cast<std::size_t>(n));
        packedRhs_.resize(static_cast<std::size_t>(n));
        bPrime_.setSize(n);
    }
    if (n >= 32768 && Pstream::master())
    {
        const char* const modeName = mode_ == naturalMode ? "natural"
            : mode_ == hostMode ? "host" : "cpe";
        Info << "LNJ_COLORED_SGS_INIT field=" << fieldName
            << " mode=" << modeName << " rows=" << n
            << " colors=" << (colored_ ? colored_->colorCount() : 0)
            << " threshold=32768" << endl;
    }
}

// Natural-order fallback, using the validated face-SGS arithmetic and the
// original interface sign/init/update/restore sequence. No global cache.
void Foam::csrSymGaussSeidelSmoother::smooth
(
    const word& fieldName_, scalarField& psi, const csrMatrix& matrix_,
    const scalarField& source,
    const FieldField<Field, scalar>& interfaceBouCoeffs_,
    const lduInterfaceFieldPtrsList& interfaces_,
    const direction cmpt, const label nSweeps
)
{
    (void)fieldName_;
    if (nSweeps <= 0) return;
    scalar* __restrict__ psiPtr = psi.begin();
    const label nCells = psi.size();
    scalarField bPrime(nCells);
    scalar* __restrict__ bPrimePtr = bPrime.begin();
    const scalar* const __restrict__ diagPtr = matrix_.diag().begin();
    const lduMatrix& ldu = matrix_.ldu();
    const label* const ownerStart = ldu.lduAddr().ownerStartAddr().begin();
    const label* const neighbour = ldu.lduAddr().upperAddr().begin();
    const scalar* const lower = ldu.lower().begin();
    const scalar* const upper = ldu.upper().begin();
    FieldField<Field, scalar>& mBouCoeffs =
        const_cast<FieldField<Field, scalar>&>(interfaceBouCoeffs_);
    forAll(mBouCoeffs, patchi)
        if (interfaces_.set(patchi)) mBouCoeffs[patchi].negate();
    for (label sweep = 0; sweep < nSweeps; ++sweep)
    {
        bPrime = source;
        matrix_.initMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime, cmpt);
        matrix_.updateMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime, cmpt);
        lnj::faceSymGaussSeidelSweep
        (
            nCells, ownerStart, neighbour, lower, upper, diagPtr,
            psiPtr, bPrimePtr
        );
    }
    forAll(mBouCoeffs, patchi)
        if (interfaces_.set(patchi)) mBouCoeffs[patchi].negate();
}

void Foam::csrSymGaussSeidelSmoother::smoothColor(const std::size_t color) const
{
    lnj::ColoredSgs<label, scalar>& plan = *colored_;
    const label begin = plan.colorStarts[color];
    const label end = plan.colorStarts[color + 1];
    if (begin == end) return;
    if (mode_ == cpeMode)
    {
        alignas(64) lnjColoredSgsParam parameter{};
        parameter.begin = begin;
        parameter.end = end;
        parameter.rowPtr = plan.rowPtr.data();
        parameter.colPtr = plan.columns.data();
        parameter.offDiagPtr = plan.values.data();
        parameter.diagPtr = plan.diagonal.data();
        parameter.psiPtr = packedX_.data();
        parameter.rhsPtr = packedRhs_.data();
        CRTS_athread_spawn
        (
            reinterpret_cast<void*>(SLAVE_FUN(lnjColoredSgs_CPE)), &parameter
        );
        CRTS_athread_join(); // Every prior-color write completes here.
    }
    else
    {
        // Same row arithmetic/order as CPE: no host-only finite scan per row.
        for (label row = begin; row < end; ++row)
        {
            scalar sum = packedRhs_[static_cast<std::size_t>(row)];
            for (label entry = plan.rowPtr[static_cast<std::size_t>(row)];
                 entry < plan.rowPtr[static_cast<std::size_t>(row + 1)]; ++entry)
                sum -= plan.values[static_cast<std::size_t>(entry)]
                    * packedX_[static_cast<std::size_t>
                        (plan.columns[static_cast<std::size_t>(entry)])];
            packedX_[static_cast<std::size_t>(row)] =
                sum / plan.diagonal[static_cast<std::size_t>(row)];
        }
    }
}

void Foam::csrSymGaussSeidelSmoother::smooth
(
    scalarField& psi, const scalarField& source,
    const direction cmpt, const label nSweeps
) const
{
    if (nSweeps <= 0) return;
    if (!colored_)
    {
        smooth(fieldName_, psi, matrix_, source, interfaceBouCoeffs_,
               interfaces_, cmpt, nSweeps);
        return;
    }
    lnj::ColoredSgs<label, scalar>& plan = *colored_;
    const label n = matrix_.row();
    if (psi.size() != n || source.size() != n
        || plan.size() != static_cast<std::size_t>(n)
        || matrix_.off_diag_value().size() != static_cast<label>(plan.values.size()))
    {
        FatalErrorInFunction << "Colored SGS matrix/vector lifetime mismatch"
            << exit(FatalError);
    }
    // CSR diagonal may have been temporarily adjusted for boundary effects.
    // Always refresh it; all other matrix entries/structure stay immutable for
    // this smoother's lifetime. Invalidate/rebuild smoother if that changes.
    for (label original = 0; original < n; ++original)
    {
        const scalar d = matrix_.diag()[original];
        if (!std::isfinite(d) || d == scalar(0))
            FatalErrorInFunction << "Invalid colored SGS diagonal at row "
                << original << exit(FatalError);
        plan.diagonal[static_cast<std::size_t>
            (plan.originalToPermuted[static_cast<std::size_t>(original)])] = d;
    }
    FieldField<Field, scalar>& mBouCoeffs =
        const_cast<FieldField<Field, scalar>&>(interfaceBouCoeffs_);
    forAll(mBouCoeffs, patchi)
        if (interfaces_.set(patchi)) mBouCoeffs[patchi].negate();
    for (label sweep = 0; sweep < nSweeps; ++sweep)
    {
        bPrime_ = source;
        matrix_.initMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime_, cmpt);
        matrix_.updateMatrixInterfaces(mBouCoeffs, interfaces_, psi, bPrime_, cmpt);
        // Identical pack/unpack and finite checks in host and CPE modes.
        for (label original = 0; original < n; ++original)
        {
            const scalar x = psi[original];
            const scalar b = bPrime_[original];
            if (!std::isfinite(x) || !std::isfinite(b))
                FatalErrorInFunction << "Non-finite colored SGS input at row "
                    << original << exit(FatalError);
            const auto permuted = static_cast<std::size_t>
                (plan.originalToPermuted[static_cast<std::size_t>(original)]);
            packedX_[permuted] = x;
            packedRhs_[permuted] = b;
        }
        for (std::size_t c = 0; c < plan.colorCount(); ++c) smoothColor(c);
        for (std::size_t c = plan.colorCount(); c > 0; --c) smoothColor(c - 1);
        for (std::size_t permuted = 0; permuted < plan.size(); ++permuted)
        {
            const scalar x = packedX_[permuted];
            if (!std::isfinite(x))
                FatalErrorInFunction << "Non-finite colored SGS output at row "
                    << permuted << exit(FatalError);
            psi[plan.permutedToOriginal[permuted]] = x;
        }
    }
    forAll(mBouCoeffs, patchi)
        if (interfaces_.set(patchi)) mBouCoeffs[patchi].negate();
}
