#include "CsrPrimitiveMeshBridge.H"
#include "CSRGAMGInterface.H"
#include "processorCSRGAMGInterface.H"
#include "processorGAMGInterface.H"
#include "processorLduInterface.H"
#include "autoPtr.H"

namespace Foam { namespace lnjCsrMeshBridge {
namespace {
// This deliberately targets plain processor interfaces. It is not an AMI,
// cyclic, processorCyclic or arbitrary coupled-interface transport implementation.
void requirePlainProcessor(const lduInterface& i) {
    if(i.type()!="processor"||!isA<processorLduInterface>(i))
        FatalErrorInFunction<<"CSR mesh bridge unsupported interface "<<i.type()
            <<exit(FatalError);
}
autoPtr<lduPrimitiveMesh> blankLike(const lduMesh& source) {
    labelList lower(source.lduAddr().lowerAddr());
    labelList upper(source.lduAddr().upperAddr());
    return autoPtr<lduPrimitiveMesh>(new lduPrimitiveMesh
        (source.lduAddr().size(),lower,upper,source.comm(),true));
}
autoPtr<lduPrimitiveMesh> nativeProxy(const lduMesh& source) {
    // Construct mesh FIRST. Every new interface's coarseInterfaces_ reference
    // then points into this mesh's stable list object, never a local temporary.
    autoPtr<lduPrimitiveMesh> result=blankLike(source);
    const lduInterfacePtrsList originals=source.interfaces();
    lduInterfacePtrsList pointers(originals.size());
    forAll(originals,p)if(originals.set(p)) {
        requirePlainProcessor(originals[p]);
        const CSRGAMGInterface& csr=refCast<const CSRGAMGInterface>(originals[p]);
        const processorLduInterface& proc=refCast<const processorLduInterface>(originals[p]);
        pointers.set(p,new processorGAMGInterface(p,result->rawInterfaces(),
            csr.faceCells(),csr.faceRestrictAddressing(),proc.comm(),proc.myProcNo(),
            proc.neighbProcNo(),proc.forwardT(),proc.tag()));
    }
    // Ownership transfers from heap allocations to result's primitiveInterfaces_.
    // pointers is non-owning; no second owner remains after this call.
    result->addInterfaces(pointers,
        lduPrimitiveMesh::nonBlockingSchedule<processorGAMGInterface>(pointers));
    return result;
}
autoPtr<lduPrimitiveMesh> csrFromMergedNative(const lduPrimitiveMesh& source) {
    autoPtr<lduPrimitiveMesh> result=blankLike(source);
    const lduInterfacePtrsList originals=source.interfaces();
    lduInterfacePtrsList pointers(originals.size());
    forAll(originals,p)if(originals.set(p)) {
        requirePlainProcessor(originals[p]);
        // Must really be native, not merely share the runtime word "processor".
        const processorGAMGInterface& proc=
            refCast<const processorGAMGInterface>(originals[p]);
        pointers.set(p,new processorCSRGAMGInterface(p,result->rawInterfaces(),
            proc.faceCells(),proc.faceRestrictAddressing(),proc.comm(),proc.myProcNo(),
            proc.neighbProcNo(),proc.forwardT(),proc.tag()));
    }
    result->addInterfaces(pointers,
        lduPrimitiveMesh::nonBlockingSchedule<processorCSRGAMGInterface>(pointers));
    return result;
}
}
void gather(const label comm,const lduMesh& mesh,const labelList& procIDs,
            Gathered& group) {
    // All participating original ranks enter in the same order as old gather.
    // No new collective or MPI protocol: actual native gather sees TRUE native
    // interfaces on send; its native receiving factory constructs TRUE native.
    group.localNative=nativeProxy(mesh);
    lduPrimitiveMesh::gather(comm,group.localNative(),procIDs,group.otherMeshes);
    // Received meshes own their interfaces; group retains all native owners.
}
lduPrimitiveMesh* combine(const label comm,const labelList& procAgglomMap,
    const labelList& procIDs,const Gathered& group,
    labelList& cellOffsets,labelList& faceOffsets,labelListList& faceMap,
    labelListList& boundaryMap,labelListListList& boundaryFaceMap) {
    // Called ONLY by each processor-group master, exactly as native constructor.
    autoPtr<lduPrimitiveMesh> merged(new lduPrimitiveMesh(comm,procAgglomMap,
        procIDs,group.localNative(),group.otherMeshes,cellOffsets,faceOffsets,faceMap,
        boundaryMap,boundaryFaceMap));
    // Native merging math/order and all output maps are unchanged. Convert back
    // before meshLevels_ / CSRGAMGInterfaceField::New can see this result.
    autoPtr<lduPrimitiveMesh> csr=csrFromMergedNative(merged());
    return csr.ptr();
}
} }
