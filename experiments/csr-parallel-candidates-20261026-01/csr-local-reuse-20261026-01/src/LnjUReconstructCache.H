#ifndef LNJ_U_RECONSTRUCT_CACHE_H
#define LNJ_U_RECONSTRUCT_CACHE_H
#include "extrapolatedCalculatedFvPatchFields.H"
#include <cstdlib>
namespace Foam {
class LnjUReconstructCache {
    autoPtr<surfaceVectorField> normals_;
    autoPtr<volTensorField> inverse_;
    bool enabled_;
public:
    LnjUReconstructCache(): enabled_(false) {
        const char* p=std::getenv("LNJ_U_RECONSTRUCT_CACHE");
        enabled_=p && std::string(p)=="1";
    }
    // Called BEFORE every mesh.update, even when update later reports no change.
    // No geometry survives a potential topology or point-motion update.
    void invalidate() { inverse_.clear(); normals_.clear(); }
    tmp<volVectorField> reconstruct(const tmp<surfaceScalarField>& tssf) {
        if(!enabled_) return fvc::reconstruct(tssf);
        const surfaceScalarField& ssf=tssf();const fvMesh& mesh=ssf.mesh();
        if(!normals_.valid()) normals_.reset(new surfaceVectorField(
            IOobject("lnjUReconstructNormals",mesh.time().timeName(),mesh,IOobject::NO_READ,IOobject::NO_WRITE,false),
            mesh.Sf()/mesh.magSf()));
        tmp<volVectorField> result(new volVectorField(
            IOobject("volIntegrate("+ssf.name()+')',ssf.instance(),mesh,IOobject::NO_READ,IOobject::NO_WRITE),
            mesh,dimensioned<vector>("0",ssf.dimensions()/dimArea,Zero),
            extrapolatedCalculatedFvPatchField<vector>::typeName));
        if(mesh.nGeometricD()) {
            if(!inverse_.valid()) inverse_.reset(new volTensorField(
                IOobject("lnjUReconstructInverse",mesh.time().timeName(),mesh,IOobject::NO_READ,IOobject::NO_WRITE,false),
                inv(fvc::surfaceSum(normals_()*mesh.Sf()))));
            result.ref() = inverse_() & fvc::surfaceSum(normals_()*ssf);
            result.ref().correctBoundaryConditions();
        }
        tssf.clear();return result;
    }
};
}
#endif
