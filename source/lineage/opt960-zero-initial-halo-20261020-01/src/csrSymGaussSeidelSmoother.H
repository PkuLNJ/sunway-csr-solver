/* Derived from OpenFOAM csrSymGaussSeidelSmoother; GPL-3.0-or-later. */
#ifndef csrSymGaussSeidelSmoother_H
#define csrSymGaussSeidelSmoother_H
#include "csrMatrix.H"
#include "lnjCachedLevelPacked.H"
#include <memory>
#include <array>
#include <cstdint>
#include <vector>
namespace Foam {
class csrSymGaussSeidelSmoother : public csrMatrix::smoother {
    enum executionMode { naturalMode,tilepermuteMode };
    executionMode mode_;
    // A smoother belongs to ONE fixed linear-system/hierarchy lifetime.
    // Never process-global, never rebuilt on each PCG preconditioner call.
    mutable std::unique_ptr<lnj::CachedLevelPacked<label,scalar>> packed_;
    mutable scalarField bPrime_;
    const label rows_,entries_;
    // Constant64 host work; no original-vector scan in permute mode.
    mutable std::array<std::int64_t,64> permutationFlags_;
    void smoothImpl(scalarField&,const scalarField&,const direction,const label,bool) const;
public:
    TypeName("symGaussSeidel");
    csrSymGaussSeidelSmoother(const word&,const csrMatrix&,
        const FieldField<Field,scalar>&,const FieldField<Field,scalar>&,
        const lduInterfaceFieldPtrsList&);
    static void smooth(const word&,scalarField&,const csrMatrix&,
        const scalarField&,const FieldField<Field,scalar>&,
        const lduInterfaceFieldPtrsList&,const direction,const label);
    virtual void smooth(scalarField&,const scalarField&,const direction,const label) const;
    // Contract: entire peer communicator admitted, caller assigned +0; no pending requests.
    void smoothFromZero(scalarField&,const scalarField&,const direction,const label) const;
};
}
#endif
