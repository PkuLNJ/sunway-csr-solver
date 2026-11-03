#ifndef LNJ_AMR_CAPTURE_H
#define LNJ_AMR_CAPTURE_H
#include "fvCFD.H"
#include "CaptureWriter.H"
#include "AmrCaptureSolver.H"
#include <string>

// Pure observation: none of these functions evaluates boundary conditions,
// asks for oldTime(), or requests Sf/C/Cf/V caches.
namespace Foam { namespace amrCapture {
void init();
bool enabled();
void finish();
void text(const std::string&, const std::string&, const std::string&);
void value(const std::string&, const std::string&, scalar);
void labels(const std::string&, const std::string&, const labelUList&);
void scalars(const std::string&, const std::string&, const scalarUList&);
void scalarBytes(const std::string&, const std::string&, const scalar*, label);
void topologyChanged();
bool refineEnabled();
bool hasRefined();
std::string correctPhiStage();
void coefficient(const std::string&, const std::string&, const surfaceScalarField&);
void coefficient(const std::string&, const std::string&, const dimensionedScalar&);
// Called before first refine (at its natural site), returns true only once.
bool beginRefine();
void endRefine();
void registered(const std::string&, const fvMesh&);
void mesh(const std::string&, const fvMesh&);
void matrix(const std::string&, const fvScalarMatrix&);
void effective(const std::string&, const fvScalarMatrix&, const scalarField&);
// pcorr identifies startup versus post-AMR using topologyChanged().
// The caller must end every equation, even when beginEquation returns false.
bool beginEquation(const char* kind);
void endEquation();
bool equationActive();
const std::string& activeStage();
void firstAmul(const scalarField& actualInput, const scalarField& actualResult);
void normFactor(scalar actualValue);
void solverControls(const dictionary&);

template<class Type, template<class> class PatchField, class GeoMesh>
void field(const std::string& stage,
           const GeometricField<Type, PatchField, GeoMesh>& f)
{
    if (!enabled()) return;
    const std::string s=stage+"/"+f.name().c_str();
    const unsigned nc=pTraits<Type>::nComponents;
    static_assert(sizeof(Type)==pTraits<Type>::nComponents*sizeof(scalar),
                  "field storage must be contiguous scalar components");
    text(s,"field.name",f.name().c_str());
    text(s,"field.type",f.type().c_str());
    OStringStream ds; ds<<f.dimensions();
    text(s,"field.dimensions",ds.str().c_str());
    value(s,"field.components",nc);
    value(s,"field.timeIndex",f.timeIndex());
    value(s,"field.globalTimeIndex",f.time().timeIndex());
    value(s,"field.existingOldTimeCount",f.nOldTimes());
    const Field<Type>& internal=f.primitiveField();
    // Raw bytes only: no arithmetic, temporary component field, or conversion.
    scalarBytes(s,"internal",reinterpret_cast<const scalar*>(internal.begin()),
                nc*internal.size());
    const auto& bf=f.boundaryField();
    value(s,"field.patchCount",bf.size());
    forAll(bf,p)
    {
        const std::string tag="patch."+std::to_string(p);
        text(s,tag+".name",bf[p].patch().name().c_str());
        text(s,tag+".fieldType",bf[p].type().c_str());
        text(s,tag+".meshType",bf[p].patch().type().c_str());
        value(s,tag+".coupled",bf[p].coupled());
        scalarBytes(s,tag+".values",reinterpret_cast<const scalar*>(bf[p].begin()),
                    nc*bf[p].size());
    }
}
} }
#endif
