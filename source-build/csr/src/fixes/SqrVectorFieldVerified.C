#include "vectorField.H"
#include "symmTensorField.H"

// General vector outer product, not a boundary/case-specific correction.
// Build this private override with -fno-tree-vectorize. Volatile scalar
// operands also keep the six products independent of the faulty packed
// implementation found in the historical CSR vectorField.o.
void Foam::sqr
(
    Field<symmTensor>& result,
    const UList<vector>& input
)
{
    forAll(result, i)
    {
        volatile scalar x = input[i].x();
        volatile scalar y = input[i].y();
        volatile scalar z = input[i].z();
        symmTensor& r = result[i];
        r.xx() = x*x;
        r.xy() = x*y;
        r.xz() = x*z;
        r.yy() = y*y;
        r.yz() = y*z;
        r.zz() = z*z;
    }
}
