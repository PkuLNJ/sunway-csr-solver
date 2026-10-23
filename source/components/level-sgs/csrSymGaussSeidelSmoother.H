/* Derived from OpenFOAM csrSymGaussSeidelSmoother; GPL-3.0-or-later. */
#ifndef csrSymGaussSeidelSmoother_H
#define csrSymGaussSeidelSmoother_H
#include "csrMatrix.H"
#include "lnjLevelPacked.H"
#include <memory>
#include <vector>
namespace Foam {
class csrSymGaussSeidelSmoother : public csrMatrix::smoother {
    enum executionMode { naturalMode,hostMode,cpeMode };
    executionMode mode_;
    // A smoother belongs to ONE fixed linear-system/hierarchy lifetime.
    // Never process-global, never rebuilt on each PCG preconditioner call.
    mutable std::unique_ptr<lnj::LevelPacked<label,scalar>> packed_;
    mutable scalarField bPrime_;
    const label rows_,entries_;
public:
    TypeName("symGaussSeidel");
    csrSymGaussSeidelSmoother(const word&,const csrMatrix&,
        const FieldField<Field,scalar>&,const FieldField<Field,scalar>&,
        const lduInterfaceFieldPtrsList&);
    static void smooth(const word&,scalarField&,const csrMatrix&,
        const scalarField&,const FieldField<Field,scalar>&,
        const lduInterfaceFieldPtrsList&,const direction,const label);
    virtual void smooth(scalarField&,const scalarField&,const direction,const label) const;
};
}
#endif
