/* Derived from OpenFOAM csrSymGaussSeidelSmoother; GPL-3.0-or-later. */
#include "csrSymGaussSeidelSmoother.H"
#include "lnjFaceSymGaussSeidel.H"
#include "lnjCombinedSgsHost.H"
#include "N74InterfaceAdmission.H"
#include "N77RhsAdmission.H"
#include "RhsResetEpoch.H"
#include "N54InterfaceAdmission.H"
#include "lnjPackedNumericFactory.H"
#include "lnjTopologyMatchHost.H"
#include "Pstream.H"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>
namespace {
bool cpeTopologyMatcher(){
    const char* mode=std::getenv("LNJ_SGS_TOPOLOGY_MATCH");
    if(!mode || std::strcmp(mode,"host")==0)return false;
    if(std::strcmp(mode,"cpe")==0)return true;
    throw std::invalid_argument("LNJ_SGS_TOPOLOGY_MATCH must be host or cpe");
}
lnj::TopologyPlanCache<Foam::label>& processTopologyCache(){
    // Exactly one process-local cache/budget. Matching policy is immutable for
    // its lifetime, not silently changed after entries have been constructed.
    static const bool cpe=cpeTopologyMatcher();
    if(cpe!=cpeTopologyMatcher())throw std::invalid_argument("Do not change topology matcher during process lifetime");
    static lnj::TopologyPlanCache<Foam::label> cache({8,128u*1024u*1024u},cpe?&lnj::topologyMatchCpe:nullptr);
    return cache;
}
}
namespace Foam {
    defineTypeNameAndDebug(csrSymGaussSeidelSmoother,0);
    csrMatrix::smoother::addsymMatrixConstructorToTable<csrSymGaussSeidelSmoother>
        addcsrSymGaussSeidelSmootherSymMatrixConstructorToTable_;
    csrMatrix::smoother::addasymMatrixConstructorToTable<csrSymGaussSeidelSmoother>
        addcsrSymGaussSeidelSmootherAsymMatrixConstructorToTable_;
}

Foam::csrSymGaussSeidelSmoother::csrSymGaussSeidelSmoother
(
    const word& fieldName,const csrMatrix& matrix,
    const FieldField<Field,scalar>& bou,const FieldField<Field,scalar>& internal,
    const lduInterfaceFieldPtrsList& interfaces
)
:
    csrMatrix::smoother(fieldName,matrix,bou,internal,interfaces),
    mode_(naturalMode),packed_(),bPrime_(matrix.row()),
    rows_(matrix.row()),entries_(matrix.off_diag_value().size()),
    boundaryTilePlan_(),n74TypesAdmitted_(false),
    n74Requested_(lnj::boundaryMaterializationRequested()),n74RetainRequested_(n54RetainPsiEnabled()),
    n77Requested_(lnj::rhsResetRequested()),n77ZeroWriteSetCovered_(false)
{
    static_assert(std::is_same<label,int64_t>::value,"64-bit label ABI required");
    static_assert(std::is_same<scalar,double>::value,"double scalar ABI required");
    if(std::getenv("LNJ_COLORED_SGS_MODE"))
        FatalErrorInFunction<<"Unset LNJ_COLORED_SGS_MODE: level SGS is an independent route"
            <<exit(FatalError);
    const char* env=std::getenv("LNJ_LEVEL_SGS_MODE");
    if(!env || std::strcmp(env,"natural")==0)mode_=naturalMode;
    else if(std::strcmp(env,"tilepermute")==0)mode_=tilepermuteMode;
    else FatalErrorInFunction<<"Production binary supports natural or tilepermute only; research host/cpe/permute modes are not linked"<<exit(FatalError);

    const char* cacheSetting=std::getenv("LNJ_SGS_TOPOLOGY_CACHE");
    if(cacheSetting && std::strcmp(cacheSetting,"0")!=0 && std::strcmp(cacheSetting,"1")!=0)
        FatalErrorInFunction<<"LNJ_SGS_TOPOLOGY_CACHE must be 0 or 1"<<exit(FatalError);

    const char* numericEnv=std::getenv("LNJ_SGS_NUMERIC_PACK");
    if(numericEnv && std::strcmp(numericEnv,"host")!=0 && std::strcmp(numericEnv,"cpe")!=0)
        FatalErrorInFunction<<"LNJ_SGS_NUMERIC_PACK must be host or cpe"<<exit(FatalError);
    const auto numericMode=lnj::parseNumericPackingMode(numericEnv);

    const char* matchEnv=std::getenv("LNJ_SGS_TOPOLOGY_MATCH");
    if(matchEnv && std::strcmp(matchEnv,"host")!=0 && std::strcmp(matchEnv,"cpe")!=0)
        FatalErrorInFunction<<"LNJ_SGS_TOPOLOGY_MATCH must be host or cpe"<<exit(FatalError);

    // Generic experimental dispatch cutoff, NOT a case- or rank-specific rule.
    // Its performance suitability is not established by this implementation.
    const label threshold=32768;
    if(mode_!=naturalMode && rows_>=threshold){
        try{
            const char* cacheEnv=std::getenv("LNJ_SGS_TOPOLOGY_CACHE");
            const bool useCache=cacheEnv && std::strcmp(cacheEnv,"1")==0;
            if(cacheEnv && std::strcmp(cacheEnv,"0")!=0 && std::strcmp(cacheEnv,"1")!=0)
                FatalErrorInFunction<<"LNJ_SGS_TOPOLOGY_CACHE must be 0 or 1"<<exit(FatalError);
            lnj::TopologyGraphView<label> graph{matrix.off_diag_rowptr().begin(),
                static_cast<std::size_t>(rows_)+1,matrix.off_diag_colidx().begin(),
                static_cast<std::size_t>(entries_)};
            // Both arms use the SAME packing builder and fresh numeric reader.
            // Cache=0 builds an independent topology; cache=1 may lease an exact hit.
            auto lookup=useCache?processTopologyCache().get(graph):
                lnj::TopologyLookup<label>{lnj::buildTopologyPlan(graph),lnj::TopologyLookupState::disabled};
            packed_=lnj::makePackedNumeric<label,scalar>(lookup.plan,
                matrix.off_diag_value().begin(),static_cast<std::size_t>(entries_),
                matrix.diag().begin(),static_cast<std::size_t>(rows_),numericMode);
            if(Pstream::master()){
                const auto stats=processTopologyCache().stats();
                Info<<"LNJ_SGS_TOPOLOGY_CACHE numeric_pack="<<(numericMode==lnj::NumericPackingMode::cpe?"cpe":"host")
                    <<" exact_match="<<(cpeTopologyMatcher()?"cpe":"host")<<" enabled="<<useCache<<" rows="<<rows_
                    <<" hit="<<(lookup.state==lnj::TopologyLookupState::hit)
                    <<" cache_hits="<<stats.hits<<" cache_builds="<<stats.builds
                    <<" resident="<<stats.residentEntries<<" bytes="<<stats.residentPayloadBytes<<endl;
            }
        }catch(const std::exception& e){
            FatalErrorInFunction<<"Level SGS plan error: "<<e.what()<<exit(FatalError);
        }
    }
    // N74_CTOR_PLAN_BEGIN: address/type work once, not on each precondition call.
    if((packed_ && n74RetainRequested_) || n77Requested_){
        if((packed_ && n74RetainRequested_ && n74Requested_) || n77Requested_){
            auto admission=lnj::boundaryPlanAdmission(interfaces_,rows_);
            n74TypesAdmitted_=admission.admitted;
            if(n74TypesAdmitted_){
                if(n77Requested_){
                    auto unionPlan=lnj::rhsZeroWriteSet(std::move(admission.plan),interfaces_,matrix_.ldu().mesh().interfaces());
                    n77ZeroWriteSetCovered_=unionPlan.covered;
                    boundaryTilePlan_.reset(new lnj::BoundaryTilePlan(std::move(unionPlan.plan)));
                }else boundaryTilePlan_.reset(new lnj::BoundaryTilePlan(std::move(admission.plan)));
            }
        }else n74TypesAdmitted_=n54ImmutablePsiInterfaces(interfaces_);
    }
    // N74_CTOR_PLAN_END
    if(rows_>=threshold && Pstream::master()){
        const char* mode=mode_==naturalMode?"natural":"tilepermute";
        Info<<"LNJ_LEVEL_SGS_INIT field="<<fieldName<<" mode="<<mode<<" rows="<<rows_
            <<" levels="<<(packed_?packed_->starts.size()-1:0)<<" threshold="<<threshold<<endl;
    }
}

// Explicit static calls keep the existing face SGS without caching.
void Foam::csrSymGaussSeidelSmoother::smooth
(
    const word& fieldName,scalarField& psi,const csrMatrix& matrix,
    const scalarField& source,const FieldField<Field,scalar>& bou,
    const lduInterfaceFieldPtrsList& interfaces,const direction cmpt,const label sweeps
){
    (void)fieldName;
    if(sweeps<=0)return;
    scalarField b(source.size());
    auto& mutableBou=const_cast<FieldField<Field,scalar>&>(bou);
    forAll(mutableBou,p)if(interfaces.set(p))mutableBou[p].negate();
    const auto& ldu=matrix.ldu();
    for(label sweep=0;sweep<sweeps;++sweep){
        b=source;
        matrix.initMatrixInterfaces(mutableBou,interfaces,psi,b,cmpt);
        matrix.updateMatrixInterfaces(mutableBou,interfaces,psi,b,cmpt);
        lnj::faceSymGaussSeidelSweep(psi.size(),ldu.lduAddr().ownerStartAddr().begin(),
            ldu.lduAddr().upperAddr().begin(),ldu.lower().begin(),ldu.upper().begin(),
            matrix.diag().begin(),psi.begin(),b.begin());
    }
    forAll(mutableBou,p)if(interfaces.set(p))mutableBou[p].negate();
}

void Foam::csrSymGaussSeidelSmoother::smooth
(
    scalarField& psi,const scalarField& source,const direction cmpt,const label sweeps
)const{
    smoothImpl(psi,source,cmpt,sweeps,false);
}
void Foam::csrSymGaussSeidelSmoother::smoothFromZero
(scalarField& psi,const scalarField& source,const direction cmpt,const label sweeps)const{
    smoothImpl(psi,source,cmpt,sweeps,true);
}
void Foam::csrSymGaussSeidelSmoother::smoothImpl
(scalarField& psi,const scalarField& source,const direction cmpt,const label sweeps,bool zeroStart)const{
    if(sweeps<=0)return;
    if(psi.size()!=rows_ || source.size()!=rows_ || matrix_.row()!=rows_
       || matrix_.off_diag_value().size()!=entries_)
        FatalErrorInFunction<<"Level SGS matrix lifetime/size mismatch"<<exit(FatalError);
    // N54_SCOPE_BEGIN: fresh stack lifetime, no inter-call numeric reuse.
    if(n74Requested_!=lnj::boundaryMaterializationRequested()||n74RetainRequested_!=n54RetainPsiEnabled())
        FatalErrorInFunction<<"Do not change n74/n54 launch policy during smoother lifetime"<<abort(FatalError);
    if(n77Requested_!=lnj::rhsResetRequested())
        FatalErrorInFunction<<"Do not change n77 policy during smoother lifetime"<<abort(FatalError);
    const bool n74AliasFree=lnj::boundaryCallAliasFree(source,psi,rows_);
    const lnj::BoundaryTilePlan n74EmptyPlan(rows_,{}); // O(1), no address scan/sort.
    const auto& n74Plan=boundaryTilePlan_?*boundaryTilePlan_:n74EmptyPlan;
    lnj::CombinedPsiSmoothEpoch n74Scope(n74Plan,sweeps,
        n74TypesAdmitted_ && n74AliasFree && packed_ && sweeps>1,
        n74Requested_ && bool(boundaryTilePlan_),n74RetainRequested_);
    lnj::RhsResetEpoch n77Epoch(n74Plan,source.begin(),bPrime_.begin(),psi.begin(),sweeps,
        n77Requested_ && n74TypesAdmitted_ && n74AliasFree && (!zeroStart || n77ZeroWriteSetCovered_));
    // N54_SCOPE_END
    auto& mutableBou=const_cast<FieldField<Field,scalar>&>(interfaceBouCoeffs_);
    forAll(mutableBou,p)if(interfaces_.set(p))mutableBou[p].negate();
    const auto& ldu=matrix_.ldu();
    for(label sweep=0;sweep<sweeps;++sweep){
        if(n77Requested_){
            try{n77Epoch.reset();}
            catch(const std::exception& e){
                n77Epoch.fail();
                FatalErrorInFunction<<"RHS reset failure is terminal: "<<e.what()<<abort(FatalError);
            }
        }else bPrime_=source;
        if(zeroStart && sweep==0){
            // No new wait-all: previous completed Amul/SGS has drained its
            // request scope. Unexpected pending requests are a contract error,
            // never a rank-local fallback that could mismatch peer traffic.
            if(Pstream::nRequests()!=0)
                FatalErrorInFunction<<"Zero-start SGS entered with pending requests"<<exit(FatalError);
            const lduInterfacePtrsList meshIf=matrix_.ldu().mesh().interfaces();
            forAll(interfaces_,p)if(interfaces_.set(p)){
                const labelUList& cells=meshIf[p].faceCells();
                // Preserve original face order and IEEE signed-zero arithmetic.
                forAll(cells,f)bPrime_[cells[f]]-=mutableBou[p][f]*scalar(0);
                const_cast<lduInterfaceField&>(interfaces_[p]).updatedMatrix()=true;
            }
        }else{
        matrix_.initMatrixInterfaces(mutableBou,interfaces_,psi,bPrime_,cmpt);
        matrix_.updateMatrixInterfaces(mutableBou,interfaces_,psi,bPrime_,cmpt);
        }
        if(!packed_){
            lnj::faceSymGaussSeidelSweep(rows_,ldu.lduAddr().ownerStartAddr().begin(),
                ldu.lduAddr().upperAddr().begin(),ldu.lower().begin(),ldu.upper().begin(),
                matrix_.diag().begin(),psi.begin(),bPrime_.begin());
            if(n77Requested_)n77Epoch.complete(true);
            continue;
        }
        auto& plan=*packed_;
        try{
            n74Scope.sweep(plan,bPrime_.begin(),psi.begin(),permutationFlags_);
        }catch(const std::exception& e){
            n77Epoch.fail();
            FatalErrorInFunction<<"CPE boundary-materialization SGS failed (terminal): "<<e.what()<<abort(FatalError);
        }
        if(n77Requested_)n77Epoch.complete(true);
        // Tile kernel already updated ORIGINAL psi, with unchanged n19
        // boundary/interface preparation and topology cache semantics.
    }
    if(n77Requested_){
        try{n77Epoch.finish();}
        catch(const std::exception& e){
            n77Epoch.fail();
            FatalErrorInFunction<<"Incomplete RHS reset epoch is terminal: "<<e.what()<<abort(FatalError);
        }
    }
    if(packed_){
        try{n74Scope.finish();}
        catch(const std::exception& e){
            FatalErrorInFunction<<"Incomplete partial-unpack epoch is terminal: "<<e.what()<<abort(FatalError);
        }
    }
    forAll(mutableBou,p)if(interfaces_.set(p))mutableBou[p].negate();
}
