// Private component bridge; no shared OpenFOAM ABI changes.
#ifndef lnjCsrPrimitiveMeshBridge_H
#define lnjCsrPrimitiveMeshBridge_H
#include "lduPrimitiveMesh.H"
#include "autoPtr.H"
namespace Foam { namespace lnjCsrMeshBridge {
struct Gathered {
    // Keep proxy and received native interfaces alive through the entire caller's
    // proc-addressing/restriction scope, not just the gather helper invocation.
    autoPtr<lduPrimitiveMesh> localNative;
    PtrList<lduPrimitiveMesh> otherMeshes;
};
void gather(const label comm,const lduMesh& mesh,const labelList& procIDs,
            Gathered&);
lduPrimitiveMesh* combine(const label comm,const labelList& procAgglomMap,
    const labelList& procIDs,const Gathered&,
    labelList& cellOffsets,labelList& faceOffsets,labelListList& faceMap,
    labelListList& boundaryMap,labelListListList& boundaryFaceMap);
} }
#endif
