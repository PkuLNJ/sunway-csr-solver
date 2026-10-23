#ifndef LNJ_ZERO_INITIAL_HALO_ADMISSION_H
#define LNJ_ZERO_INITIAL_HALO_ADMISSION_H
#include "csrSymGaussSeidelSmoother.H"
#include "processorLduInterface.H"
#include "processorLduInterfaceField.H"
#include "PstreamReduceOps.H"
#include <cmath>
namespace Foam { namespace lnjZeroInitialHalo {
// Actual constructed workspace, no static pointer cache and no new ABI member.
template<class Matrices,class Fields,class Coeffs,class Smoothers,class Corrections,class Sources>
void configure(dictionary& controls,const csrMatrix& finest,bool procAgg,
    const Matrices& matrices,const Fields& fields,const Coeffs& bou,
    const Smoothers& smoothers,const Corrections& corr,const Sources& sources)
{
    controls.set("_lnjZeroInitialHaloAdmitted",false);
    if(!controls.lookupOrDefault<bool>("lnjSkipZeroInitialHalo",false))return;
    const label comm=finest.mesh().comm();
    label countMin=matrices.size(),countMax=matrices.size();
    label bad=procAgg || smoothers.size()!=matrices.size()+1
        || fields.size()!=matrices.size() || bou.size()!=matrices.size()
        || corr.size()!=matrices.size() || sources.size()!=matrices.size()
        || (Pstream::defaultCommsType!=Pstream::commsTypes::blocking
           && Pstream::defaultCommsType!=Pstream::commsTypes::nonBlocking);
    forAll(matrices,i)
    {
        if(i>=fields.size()||i>=bou.size()||i>=corr.size()||i>=sources.size()
          ||!matrices.set(i)||!fields.set(i)||!bou.set(i)||!corr.set(i)||!sources.set(i)
          || i+1>=smoothers.size()||!smoothers.set(i+1)) {bad=1;continue;}
        if(!dynamic_cast<const csrSymGaussSeidelSmoother*>(&smoothers[i+1]))bad=1;
        if(matrices[i].mesh().comm()!=comm)bad=1;
        if(corr[i].size()!=matrices[i].diag().size()
          ||sources[i].size()!=matrices[i].diag().size())bad=1;
        const lduInterfacePtrsList meshIf=matrices[i].mesh().interfaces();
        const auto& fs=fields[i];const auto& bs=bou[i];
        if(meshIf.size()!=fs.size()||meshIf.size()!=bs.size()) {bad=1;continue;}
        forAll(meshIf,p)
        {
            if(meshIf.set(p)!=fs.set(p)){bad=1;continue;}
            if(!meshIf.set(p))continue;
            const auto* pi=dynamic_cast<const processorLduInterface*>(&meshIf[p]);
            const auto* pf=dynamic_cast<const processorLduInterfaceField*>(&fs[p]);
            if(!pi||!pf||meshIf[p].type()!="processor"||fs[p].type()!="processor"
              ||pi->comm()!=comm||pf->comm()!=comm||pf->rank()!=0
              ||pi->myProcNo()!=pf->myProcNo()||pi->neighbProcNo()!=pf->neighbProcNo()
              ||bs[p].size()!=meshIf[p].faceCells().size()) {bad=1;continue;}
            forAll(bs[p],f)if(!std::isfinite(bs[p][f]))bad=1;
            forAll(meshIf[p].faceCells(),f)
                if(meshIf[p].faceCells()[f]<0||meshIf[p].faceCells()[f]>=matrices[i].diag().size())bad=1;
        }
    }
    // Same gate for all original active hierarchy ranks, once per workspace.
    reduce(countMin,minOp<label>(),UPstream::msgType(),comm);
    reduce(countMax,maxOp<label>(),UPstream::msgType(),comm);
    if(countMin!=countMax)bad=1;
    reduce(bad,maxOp<label>(),UPstream::msgType(),comm);
    controls.set("_lnjZeroInitialHaloAdmitted",bad==0);
    Info.masterStream(comm)<<"LNJ_ZERO_INITIAL_HALO_SETUP ADMITTED="<<(bad==0)
        <<" LEVELS="<<countMax<<" PROC_AGG="<<procAgg
        <<" GATE_REDUCTIONS=3 PER_SWEEP_REDUCTIONS=0"<<endl;
}
}}
#endif
