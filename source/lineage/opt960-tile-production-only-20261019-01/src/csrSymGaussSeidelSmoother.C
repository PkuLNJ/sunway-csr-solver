/* Derived from OpenFOAM csrSymGaussSeidelSmoother; GPL-3.0-or-later. */
#include "csrSymGaussSeidelSmoother.H"
#include "lnjFaceSymGaussSeidel.H"
#include "lnjPermutationSgsTileHost.H"
#include "Pstream.H"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>
namespace {
lnj::TopologyPlanCache<Foam::label>& processTopologyCache(){
    static lnj::TopologyPlanCache<Foam::label> cache({8,128u*1024u*1024u});
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
    rows_(matrix.row()),entries_(matrix.off_diag_value().size())
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
            packed_.reset(new lnj::CachedLevelPacked<label,scalar>(lookup.plan,
                matrix.off_diag_value().begin(),static_cast<std::size_t>(entries_),
                matrix.diag().begin(),static_cast<std::size_t>(rows_)));
            if(Pstream::master()){
                const auto stats=processTopologyCache().stats();
                Info<<"LNJ_SGS_TOPOLOGY_CACHE enabled="<<useCache<<" rows="<<rows_
                    <<" hit="<<(lookup.state==lnj::TopologyLookupState::hit)
                    <<" cache_hits="<<stats.hits<<" cache_builds="<<stats.builds
                    <<" resident="<<stats.residentEntries<<" bytes="<<stats.residentPayloadBytes<<endl;
            }
        }catch(const std::exception& e){
            FatalErrorInFunction<<"Level SGS plan error: "<<e.what()<<exit(FatalError);
        }
    }
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
    if(sweeps<=0)return;
    if(psi.size()!=rows_ || source.size()!=rows_ || matrix_.row()!=rows_
       || matrix_.off_diag_value().size()!=entries_)
        FatalErrorInFunction<<"Level SGS matrix lifetime/size mismatch"<<exit(FatalError);
    auto& mutableBou=const_cast<FieldField<Field,scalar>&>(interfaceBouCoeffs_);
    forAll(mutableBou,p)if(interfaces_.set(p))mutableBou[p].negate();
    const auto& ldu=matrix_.ldu();
    for(label sweep=0;sweep<sweeps;++sweep){
        bPrime_=source;
        matrix_.initMatrixInterfaces(mutableBou,interfaces_,psi,bPrime_,cmpt);
        matrix_.updateMatrixInterfaces(mutableBou,interfaces_,psi,bPrime_,cmpt);
        if(!packed_){
            lnj::faceSymGaussSeidelSweep(rows_,ldu.lduAddr().ownerStartAddr().begin(),
                ldu.lduAddr().upperAddr().begin(),ldu.lower().begin(),ldu.upper().begin(),
                matrix_.diag().begin(),psi.begin(),bPrime_.begin());
            continue;
        }
        auto& plan=*packed_;
        try{
            lnj::permutationSgsTileSweep(plan,bPrime_.begin(),psi.begin(),permutationFlags_);
        }catch(const std::exception& e){
            FatalErrorInFunction<<"CPE permutation SGS failed: "<<e.what()<<exit(FatalError);
        }
        // Tile kernel already updated ORIGINAL psi, with unchanged n19
        // boundary/interface preparation and topology cache semantics.
    }
    forAll(mutableBou,p)if(interfaces_.set(p))mutableBou[p].negate();
}
