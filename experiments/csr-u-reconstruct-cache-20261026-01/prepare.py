from pathlib import Path
p=Path('work/csr-u-reconstruct-cache-20261026-01');src=Path('work/csr-construction-profile-20261026-01')
for f in (src/'src').iterdir():
 (p/'src'/f.name).write_bytes(f.read_bytes())
h='''#ifndef LNJ_U_RECONSTRUCT_CACHE_H
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
'''
(p/'src/LnjUReconstructCache.H').write_text(h,newline='\n')
f=p/'src/boussinesqFoam.C';s=f.read_text().replace('#include "fvCFD.H"','#include "fvCFD.H"\n#include "LnjUReconstructCache.H"').replace('    bool amrCaptureFirstParameters = true;','    LnjUReconstructCache lnjUReconstruct;\n    bool amrCaptureFirstParameters = true;').replace('                mesh.update();','                lnjUReconstruct.invalidate();\n                mesh.update();');f.write_text(s,newline='\n')
f=p/'src/UEqn.H';s=f.read_text();assert s.count('fvc::reconstruct')==1;f.write_text(s.replace('fvc::reconstruct','lnjUReconstruct.reconstruct'),newline='\n')
b=(src/'build.sh').read_text().replace(src.name,p.name)
a=b.index('for unit in CSRGAMGSolver');z=b.index('cp -p "$d/lib/libdfMatrices.a"',a)
b=b[:a]+'cp -p "$r/build/csr-construction-profile-20261026-01/obj/CSRGAMGSolver.o" "$r/build/csr-construction-profile-20261026-01/obj/CSRGAMGSolverAgglomerateMatrix.o" obj/\n'+b[z:]
(p/'build.sh').write_text(b,newline='\n')
(p/'make_link.py').write_text((src/'make_link.py').read_text().replace(src.name,p.name),newline='\n')
(p/'remote.py').write_text((src/'remote.py').read_text(),newline='\n')
