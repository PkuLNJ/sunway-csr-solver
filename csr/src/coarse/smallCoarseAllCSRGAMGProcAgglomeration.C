#include "smallCoarseAllCSRGAMGProcAgglomeration.H"
#include "SmallCoarseEstimate.H"
#include "CSRGAMGAgglomeration.H"
#include "CSRGAMGInterface.H"
#include "processorLduInterface.H"
#include "lduMesh.H"
#include "addToRunTimeSelectionTable.H"
#include "PstreamReduceOps.H"
#include "tensor.H"
#include <mpi.h>
#include <cmath>
namespace Foam {
defineTypeNameAndDebug(smallCoarseAllCSRGAMGProcAgglomeration,0);
addToRunTimeSelectionTable(CSRGAMGProcAgglomeration,
    smallCoarseAllCSRGAMGProcAgglomeration,CSRGAMGAgglomeration);
smallCoarseAllCSRGAMGProcAgglomeration::smallCoarseAllCSRGAMGProcAgglomeration
    (CSRGAMGAgglomeration& a,const dictionary& d)
:CSRGAMGProcAgglomeration(a,d),
 byteCap_(d.lookupOrDefault<label>("lnjCoarseGatherMaxBytes",label(256)*1024*1024)),
 innerMaxIter_(d.lookupOrDefault<label>("lnjCoarseMaxIter",8)),attempted_(false) {}
smallCoarseAllCSRGAMGProcAgglomeration::~smallCoarseAllCSRGAMGProcAgglomeration() {
    forAllReverse(comms_,i)if(comms_[i]!=-1)UPstream::freeCommunicator(comms_[i]);
}
bool smallCoarseAllCSRGAMGProcAgglomeration::agglomerate() {
    if(attempted_)return false;
    attempted_=true;
    if(agglom_.size()<2)
        FatalErrorInFunction<<"smallCoarseAll requires at least2 coarse levels"<<exit(FatalError);
    // Same safe gather location as validated lateSix / native masterCoarsest.
    const label level=agglom_.size()-1;
    if(!agglom_.hasMeshLevel(level))
        FatalErrorInFunction<<"smallCoarseAll requires unaggregated hierarchy"<<exit(FatalError);
    const lduMesh& mesh=agglom_.meshLevel(level);
    const label comm=mesh.comm(), np=UPstream::nProcs(comm);
    const double start=MPI_Wtime();
    // Bound and agree controls BEFORE any rank can take an aggregation branch.
    label capMin=byteCap_, capMax=byteCap_, iterMin=innerMaxIter_, iterMax=innerMaxIter_;
    reduce(capMin,minOp<label>(),Pstream::msgType(),comm);
    reduce(capMax,maxOp<label>(),Pstream::msgType(),comm);
    reduce(iterMin,minOp<label>(),Pstream::msgType(),comm);
    reduce(iterMax,maxOp<label>(),Pstream::msgType(),comm);
    if(capMin!=capMax||capMin<1||capMax>label(512)*1024*1024
      ||iterMin!=iterMax||iterMin<1||iterMax>64)
        FatalErrorInFunction<<"smallCoarseAll requires uniform bytecap1..512MiB and inner maxIter1..64"<<exit(FatalError);
    tensor stats=tensor::zero;
    stats[0]=mesh.lduAddr().size();
    stats[1]=mesh.lduAddr().lowerAddr().size();
    stats[6]=1; // count all participating ranks, including empty local matrices
    const lduInterfacePtrsList interfaces=mesh.interfaces();
    bool supported=true;
    forAll(interfaces,p)if(interfaces.set(p)) {
        supported=supported&&interfaces[p].type()=="processor"
            &&isA<CSRGAMGInterface>(interfaces[p])&&isA<processorLduInterface>(interfaces[p]);
        if(!supported)continue;
        const CSRGAMGInterface& c=refCast<const CSRGAMGInterface>(interfaces[p]);
        const processorLduInterface& proc=refCast<const processorLduInterface>(interfaces[p]);
        stats[2]+=c.faceCells().size();stats[3]+=c.faceRestrictAddressing().size();
        stats[4]+=scalar(9)*proc.forwardT().size();stats[5]+=1;
    }
    stats[7]=supported?0:1;
    // One batched positive-count setup reduction, not one reduction per category.
    reduce(stats,sumOp<tensor>(),Pstream::msgType(),comm);
    bool representable=true;
    for(direction c=0;c<8;++c)
        representable=representable&&std::isfinite(stats[c])&&stats[c]>=0
            &&stats[c]<=scalar(1125899906842624.0)&&std::floor(stats[c])==stats[c];
    lnjSmallCoarse::Counts count;
    if(representable) {
        count.rows=static_cast<std::uint64_t>(stats[0]);
        count.internalFaces=static_cast<std::uint64_t>(stats[1]);
        count.processorFaceSides=static_cast<std::uint64_t>(stats[2]);
        count.restrictEntries=static_cast<std::uint64_t>(stats[3]);
        count.transformComponents=static_cast<std::uint64_t>(stats[4]);
        count.interfaces=static_cast<std::uint64_t>(stats[5]);
        count.ranks=static_cast<std::uint64_t>(stats[6]);
    }
    std::uint64_t estimated=0;
    const bool estimateValid=representable&&lnjSmallCoarse::estimate(count,estimated);
    const bool accept=estimateValid&&stats[7]==0&&count.rows>0&&count.ranks>1
        &&estimated<=static_cast<std::uint64_t>(byteCap_);
    const char* reason=!representable?"nonrepresentable_counts":stats[7]!=0?"unsupported_interface":
        !estimateValid?"estimate_overflow":count.rows==0?"empty_global_operator":
        count.ranks<=1?"already_serial":estimated>static_cast<std::uint64_t>(byteCap_)?"bytecap_exceeded":"admitted";
    Info.masterStream(comm)<<"LNJ_SMALL_COARSE_ADMISSION LEVEL="<<level
      <<" GLOBAL_ROWS="<<stats[0]<<" INTERNAL_FACES="<<stats[1]
      <<" PROCESSOR_FACE_SIDES="<<stats[2]<<" RANKS="<<stats[6]
      <<" ESTIMATED_EXTRA_BYTES="<<scalar(estimated)<<" BYTECAP="<<byteCap_
      <<" SUPPORTED="<<label(stats[7]==0)<<" REPRESENTABLE="<<label(representable)
      <<" ACCEPT="<<label(accept)<<" REASON="<<reason<<" INNER_MAXITER="<<innerMaxIter_
      <<" SETUP_GATE_SECONDS="<<MPI_Wtime()-start<<endl;
    if(!accept) {
        Info.masterStream(comm)<<"LNJ_SMALL_COARSE_FALLBACK unchanged_processor_hierarchy"<<endl;
        return false;
    }
    labelList map(np,label(0));
    labelList masters;List<label> members;
    CSRGAMGAgglomeration::calculateRegionMaster(comm,map,masters,members);
    comms_.append(UPstream::allocateCommunicator(comm,masters));
    const double gatherStart=MPI_Wtime();
    CSRGAMGProcAgglomeration::agglomerate(level,map,masters,members,comms_.last());
    Info.masterStream(comm)<<"LNJ_SMALL_COARSE_GATHER CHILD_RANKS=1 PARENT_RANKS="<<np
        <<" SETUP_GATHER_SECONDS="<<MPI_Wtime()-gatherStart<<endl;
    return true;
}
}
