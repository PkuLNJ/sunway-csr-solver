#include "AmrCapture.H"
#include "processorFvPatch.H"
#include <cstdlib>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Foam { namespace amrCapture {
namespace {
std::unique_ptr<lnjcapture::CaptureWriter> writer;
bool initialized=false, topo=false, refining=false, refined=false;
bool inEquation=false, captureEquation=false, gotAmul=false, gotNorm=false;
const void* actualOperator=0;
unsigned amulCount=0;
unsigned startupPcorr=0, postPcorr=0, mainPressure=0;
unsigned startupCorrectPhi=0, postCorrectPhi=0;
std::string current;

void bytes(const std::string& stage,const std::string& tag,
           const std::string& data)
{
    if(writer)writer->write(stage,tag,"bytes",1,data.size(),data.data());
}
template<class List>
void labelArray(const std::string& stage,const std::string& tag,const List& a)
{
    if(writer)writer->write(stage,tag,"i",sizeof(label),a.size(),a.begin());
}
void coefficients(const std::string& stage,const fvScalarMatrix& a)
{
    value(stage,"matrix.hasDiag",a.hasDiag());
    value(stage,"matrix.hasLower",a.hasLower());
    value(stage,"matrix.hasUpper",a.hasUpper());
    value(stage,"matrix.symmetric",a.symmetric());
    value(stage,"matrix.diagonal",a.diagonal());
    if(a.hasDiag())scalars(stage,"diag",a.diag());
    // Const access returns the symmetric alias without allocating a missing
    // triangle. Allocation flags above preserve the original representation.
    if(a.hasLower()||a.hasUpper())
    {
        scalars(stage,"lower",a.lower());
        scalars(stage,"upper",a.upper());
    }
    labels(stage,"lowerAddr",a.lduAddr().lowerAddr());
    labels(stage,"upperAddr",a.lduAddr().upperAddr());
    value(stage,"matrix.patchCount",a.internalCoeffs().size());
    forAll(a.internalCoeffs(),p)
    {
        const std::string tag="patch."+std::to_string(p);
        labels(stage,tag+".faceCells",a.lduAddr().patchAddr(p));
        scalars(stage,tag+".internalCoeffs",a.internalCoeffs()[p]);
        scalars(stage,tag+".boundaryCoeffs",a.boundaryCoeffs()[p]);
    }
}
}
void init()
{
    if(initialized)throw std::runtime_error("AMR capture initialized twice");
    initialized=true;
    const char* directory=std::getenv("LNJ_AMR_CAPTURE_DIR");
    if(!directory||!*directory)return;
    writer.reset(new lnjcapture::CaptureWriter(directory,Pstream::myProcNo()));
    value("capture","rank",Pstream::myProcNo());
    value("capture","ranks",Pstream::nProcs());
    value("capture","scalarWidth",sizeof(scalar));
    value("capture","labelWidth",sizeof(label));
    text("capture","adapter","LNJ_AMR_ADAPTER_V1");
}
bool enabled(){return bool(writer);}
void text(const std::string& s,const std::string& t,const std::string& v)
{bytes(s,t,v);}
void value(const std::string& s,const std::string& t,scalar v)
{if(writer)writer->write(s,t,"f",sizeof(scalar),1,&v);}
void labels(const std::string& s,const std::string& t,const labelUList& v)
{labelArray(s,t,v);}
void scalars(const std::string& s,const std::string& t,const scalarUList& v)
{if(writer)writer->write(s,t,"f",sizeof(scalar),v.size(),v.begin());}
void scalarBytes(const std::string& s,const std::string& t,const scalar* v,label n)
{
    if(!writer)return;
    if(n<0)throw std::runtime_error("negative AMR capture scalar count");
    writer->write(s,t,"f",sizeof(scalar),n,v);
}
void topologyChanged(){if(writer)topo=true;}
bool refineEnabled(){return enabled()&&refining;}
bool hasRefined(){return enabled()&&topo;}
std::string correctPhiStage()
{
    unsigned& counter=topo?postCorrectPhi:startupCorrectPhi;
    return std::string(topo?"postamr.correctPhi.":"startup.correctPhi.")
         +std::to_string(++counter);
}
void coefficient(const std::string& s,const std::string& t,const surfaceScalarField& f)
{
    if(!enabled())return;
    text(s,t+".representation","surfaceScalarField");
    field(s+"/"+t,f);
}
void coefficient(const std::string& s,const std::string& t,const dimensionedScalar& d)
{
    if(!enabled())return;
    text(s,t+".representation","dimensionedScalar");
    value(s,t+".uniform.value",d.value());
    OStringStream os;os<<d.dimensions();
    text(s,t+".uniform.dimensions",os.str().c_str());
}
bool beginRefine()
{
    if(!enabled()||refined||refining)return false;
    refining=true;
    return true;
}
void endRefine(){if(refining){refining=false;refined=true;topo=true;}}
void registered(const std::string& stage,const fvMesh& m)
{
    if(!enabled())return;
    // Iterate existing objects only: lookup never creates/registers anything.
    const objectRegistry& registry=static_cast<const objectRegistry&>(m);
    forAllConstIter(objectRegistry,registry,iter)
    {
        const regIOobject& o=*iter();
        if(isA<volScalarField>(o))field(stage,refCast<const volScalarField>(o));
        else if(isA<volVectorField>(o))field(stage,refCast<const volVectorField>(o));
        else if(isA<surfaceScalarField>(o))field(stage,refCast<const surfaceScalarField>(o));
        else if(isA<surfaceVectorField>(o))field(stage,refCast<const surfaceVectorField>(o));
    }
    writer->checkpoint();
}
void mesh(const std::string& stage,const fvMesh& m)
{
    if(!enabled())return;
    labels(stage,"owner",m.faceOwner());
    labels(stage,"neighbour",m.faceNeighbour());
    const pointField& points=m.points();
    scalarBytes(stage,"points",reinterpret_cast<const scalar*>(points.begin()),3*points.size());
    const faceList& faces=m.faces();
    std::vector<label> ptr(faces.size()+1,0),vertices;
    forAll(faces,f)
    {
        forAll(faces[f],j)vertices.push_back(faces[f][j]);
        ptr[f+1]=vertices.size();
    }
    if(writer)
    {
        writer->write(stage,"facePtr","i",sizeof(label),ptr.size(),ptr.data());
        writer->write(stage,"faceVertices","i",sizeof(label),vertices.size(),vertices.data());
    }
    const fvBoundaryMesh& bm=m.boundary();
    value(stage,"patchCount",bm.size());
    bool supported=true;
    forAll(bm,p)
    {
        const fvPatch& patch=bm[p];
        const std::string tag="patch."+std::to_string(p);
        text(stage,tag+".name",patch.name().c_str());
        text(stage,tag+".type",patch.type().c_str());
        value(stage,tag+".coupled",patch.coupled());
        value(stage,tag+".start",patch.start());
        value(stage,tag+".size",patch.size());
        labels(stage,tag+".faceCells",patch.faceCells());
        if(isA<processorFvPatch>(patch))
        {
            const processorFvPatch& pp=refCast<const processorFvPatch>(patch);
            value(stage,tag+".myProcNo",pp.myProcNo());
            value(stage,tag+".neighbProcNo",pp.neighbProcNo());
            // Exact processor patches use matching face order. Derived cyclic
            // variants need transform/patch pairing and are NOT silently accepted.
            const bool ok=patch.type()=="processor";
            value(stage,tag+".replaySupported",ok);
            supported=supported&&ok;
        }
        else if(patch.coupled())
        {
            value(stage,tag+".replaySupported",0);
            supported=false;
        }
        else value(stage,tag+".replaySupported",1);
    }
    value(stage,"replaySupported",supported);
    writer->checkpoint();
}
void matrix(const std::string& stage,const fvScalarMatrix& a)
{
    if(!equationActive())return;
    coefficients(stage,a);
    scalars(stage,"source.raw",a.source());
    field(stage+"/psi",a.psi());
    text(stage,"role","raw_fvMatrix_at_named_checkpoint");
    writer->checkpoint();
}
void effective(const std::string& stage,const fvScalarMatrix& a,const scalarField& b)
{
    if(!equationActive())return;
    setOperator(static_cast<const lduMatrix*>(&a));
    coefficients(stage,a);
    scalars(stage,"source.effective",b);
    scalars(stage,"x0",a.psi().primitiveField());
    text(stage,"role","actual_boundary_completed_solver_input");
    writer->checkpoint();
}
bool beginEquation(const char* kind)
{
    if(!enabled())return false;
    if(inEquation)throw std::runtime_error("nested AMR capture equation");
    inEquation=true;gotAmul=false;gotNorm=false;captureEquation=false;
    actualOperator=0;amulCount=0;
    const std::string k(kind);
    if(k=="pcorr")
    {
        unsigned& count=topo?postPcorr:startupPcorr;
        ++count;
        current=std::string(topo?"postamr.pcorr.":"startup.pcorr.")+std::to_string(count);
        captureEquation=count<=3;
    }
    else if(k=="main_pressure")
    {
        current="main_pressure."+std::to_string(++mainPressure);
        captureEquation=mainPressure==1;
    }
    else throw std::runtime_error("unknown AMR capture equation kind");
    if(captureEquation)text(current,"equation.kind",k);
    return captureEquation;
}
void endEquation()
{
    if(!enabled())return;
    if(!inEquation)throw std::runtime_error("AMR capture equation end without begin");
    if(captureEquation){value(current,"equation.end",1);writer->checkpoint();}
    inEquation=false;captureEquation=false;current.clear();
}
bool equationActive(){return enabled()&&inEquation&&captureEquation;}
const std::string& activeStage(){return current;}
void firstAmul(const scalarField& input,const scalarField& result)
{
    observeAmul(actualOperator,input,result);
}
void setOperator(const void* p){if(equationActive())actualOperator=p;}
void observeAmul(const void* p,const scalarField& input,const scalarField& result)
{
    if(!equationActive()||!actualOperator||p!=actualOperator||amulCount>=3)return;
    const std::string tag="actual.Ax"+std::to_string(amulCount++);
    scalars(current,tag+".input",input);
    scalars(current,tag,result);gotAmul=true;
    writer->checkpoint();
}
void normFactor(scalar factor)
{
    if(!equationActive()||gotNorm)return;
    value(current,"actual.normFactor",factor);gotNorm=true;
}
void solverControls(const dictionary& d)
{
    if(!equationActive())return;
    OStringStream os;os<<d;text(current,"actual.solverControls",os.str().c_str());
}
void finish()
{
    if(!enabled())return;
    if(inEquation)throw std::runtime_error("AMR capture finish inside equation");
    value("capture","startupPcorrCount",startupPcorr);
    value("capture","postAmrPcorrCount",postPcorr);
    value("capture","mainPressureCount",mainPressure);
    writer->finish();writer.reset();
}
} }
