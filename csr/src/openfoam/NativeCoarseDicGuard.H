#ifndef LNJ_NATIVE_COARSE_DIC_GUARD_H
#define LNJ_NATIVE_COARSE_DIC_GUARD_H
#include "PstreamReduceOps.H"
#include <cmath>
#include <limits>

// Native PCG's symmetric, reference-treated operator remains a caller contract.
// This is NOT a new global SPD or nullspace proof. Test the exact local DIC
// recurrence used by the target before its unchecked reciprocal calculation.
namespace Foam { namespace lnjNativeCoarseDic {
inline bool admit(const lduMatrix& A)
{
    const scalarField& d = A.diag();
    const labelUList& lo = A.lduAddr().lowerAddr();
    const labelUList& hi = A.lduAddr().upperAddr();
    // Do not call upper() for diagonal-only matrices: that accessor can Fatal.
    label bad = (!A.symmetric() || A.asymmetric());
    const scalarField* up = nullptr;
    if (!bad) up=&A.upper();
    if (up && (up->size()!=lo.size() || up->size()!=hi.size())) bad=1;
    label signMin=1, signMax=-1;
    scalarField pivot(d);
    forAll(d,i)
    {
        if (!std::isfinite(d[i]) || d[i]==0) {bad=1;continue;}
        const label s = d[i]>0 ? 1 : -1;
        signMin=min(signMin,s); signMax=max(signMax,s);
    }
    label localSign=signMax;
    if (d.empty()) localSign=1;
    const scalar eps=64*std::numeric_limits<scalar>::epsilon();
    const scalar largest=std::numeric_limits<scalar>::max();
    const scalar squareLimit=std::sqrt(largest);
    // Invalid local shape must skip indexing, but still enter all reductions.
    if (!bad) forAll(*up,f)
    {
        const label l=lo[f], h=hi[f];
        const scalar u=(*up)[f];
        if(l<0 || h<=l || h>=d.size() || !std::isfinite(u)
          || (f && lo[f]<lo[f-1]))
        {bad=1;continue;}
        const scalar threshold=max(eps*mag(d[l]),std::numeric_limits<scalar>::min());
        if(!std::isfinite(pivot[l]) || localSign*pivot[l]<=threshold)
        {bad=1;continue;}
        // Guard BEFORE arithmetic: FOAM_SIGFPE may trap before isfinite runs.
        // Normal accepted values retain native u*u/pivot operation order.
        if (mag(u)>squareLimit) {bad=1;continue;}
        const scalar square=u*u;
        const scalar denominator=mag(pivot[l]);
        if(denominator<1 && square>largest*denominator) {bad=1;continue;}
        const scalar delta=square/pivot[l];
        if(!std::isfinite(pivot[h])
          || (delta>0 && pivot[h]<-largest+delta)
          || (delta<0 && pivot[h]>largest+delta)) {bad=1;continue;}
        pivot[h] -= delta;
    }
    forAll(d,i)
    {
        const scalar threshold=max(eps*mag(d[i]),std::numeric_limits<scalar>::min());
        if(!std::isfinite(pivot[i]) || localSign*pivot[i]<=threshold
          || !std::isfinite(1/pivot[i])) bad=1;
    }
    const label comm=A.mesh().comm();
    reduce(signMin,minOp<label>(),UPstream::msgType(),comm);
    reduce(signMax,maxOp<label>(),UPstream::msgType(),comm);
    if(signMin!=signMax) bad=1;
    reduce(bad,maxOp<label>(),UPstream::msgType(),comm);
    Info.masterStream(comm) << "LNJ_NATIVE_COARSE_DIC ACCEPT=" << (bad==0)
        << " REASON=" << (bad ? "shape_sign_nonfinite_or_pivot" : "local_pivots_valid_native_PCG_contract")
        << " COMM_SIZE=" << UPstream::nProcs(comm)
        << " EXTRA_SCALAR_REDUCTIONS=3 GLOBAL_SPD_PROVEN=0" << endl;
    return !bad;
}
}}
#endif
