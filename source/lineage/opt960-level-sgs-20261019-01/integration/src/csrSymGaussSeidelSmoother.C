/* Derived from OpenFOAM csrSymGaussSeidelSmoother; GPL-3.0-or-later. */
#include "csrSymGaussSeidelSmoother.H"
#include "lnjFaceSymGaussSeidel.H"
#include "lnjLevelSgsParam.h"
#include "Pstream.H"
#include <crts.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <type_traits>
extern "C" void SLAVE_FUN(lnjLevelSgs_CPE)(struct lnjLevelSgsParam*);
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
    else if(std::strcmp(env,"host")==0)mode_=hostMode;
    else if(std::strcmp(env,"cpe")==0)mode_=cpeMode;
    else FatalErrorInFunction<<"LNJ_LEVEL_SGS_MODE must be natural, host or cpe"<<exit(FatalError);

    // Generic experimental dispatch cutoff, NOT a case- or rank-specific rule.
    // Its performance suitability is not established by this implementation.
    const label threshold=32768;
    if(mode_!=naturalMode && rows_>=threshold){
        std::vector<label> ptr(static_cast<std::size_t>(rows_)+1),cols(static_cast<std::size_t>(entries_));
        std::vector<scalar> values(static_cast<std::size_t>(entries_));
        for(label i=0;i<=rows_;++i)ptr[static_cast<std::size_t>(i)]=matrix.off_diag_rowptr()[i];
        for(label e=0;e<entries_;++e){
            cols[static_cast<std::size_t>(e)]=matrix.off_diag_colidx()[e];
            values[static_cast<std::size_t>(e)]=matrix.off_diag_value()[e];
            if(!std::isfinite(values[static_cast<std::size_t>(e)]))
                FatalErrorInFunction<<"Nonfinite SGS coefficient"<<exit(FatalError);
        }
        try{
            auto plan=lnj::makeLevelSgsPlan(ptr,cols);
            packed_.reset(new lnj::LevelPacked<label,scalar>(plan,ptr,cols,values));
            // This diagonal is fixed for the immutable matrix/linear solve.
            // Recreate the smoother if its diagonal or coefficients can change.
            for(label i=0;i<rows_;++i){
                const scalar d=matrix.diag()[i];
                if(!std::isfinite(d) || d==0)
                    FatalErrorInFunction<<"Invalid SGS diagonal at "<<i<<exit(FatalError);
                packed_->diagonal[static_cast<std::size_t>(packed_->originalToPacked[static_cast<std::size_t>(i)])]=d;
            }
        }catch(const std::exception& e){
            FatalErrorInFunction<<"Level SGS plan error: "<<e.what()<<exit(FatalError);
        }
    }
    if(rows_>=threshold && Pstream::master()){
        const char* mode=mode_==naturalMode?"natural":mode_==hostMode?"host":"cpe";
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
        // No graph, coefficient, diagonal or workspace allocation here.
        // Same finite checks and packing in host and CPE modes.
        for(label original=0;original<rows_;++original){
            const scalar x=psi[original],b=bPrime_[original];
            if(!std::isfinite(x) || !std::isfinite(b))
                FatalErrorInFunction<<"Nonfinite level SGS input"<<exit(FatalError);
            const auto p=static_cast<std::size_t>(plan.originalToPacked[static_cast<std::size_t>(original)]);
            plan.psi[p]=x;plan.rhs[p]=b;
        }
        if(mode_==hostMode)plan.hostSweep();
        else{
            alignas(64) lnjLevelSgsParam p{
                rows_,static_cast<int64_t>(plan.starts.size()-1),plan.starts.data(),plan.rowPtr.data(),
                plan.columns.data(),plan.values.data(),plan.diagonal.data(),plan.rhs.data(),
                plan.psi.data(),nullptr};
            CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjLevelSgs_CPE)),&p);
            CRTS_athread_join();
        }
        for(label p=0;p<rows_;++p){
            const scalar x=plan.psi[static_cast<std::size_t>(p)];
            if(!std::isfinite(x))
                FatalErrorInFunction<<"Nonfinite level SGS output"<<exit(FatalError);
            psi[plan.packedToOriginal[static_cast<std::size_t>(p)]]=x;
        }
    }
    forAll(mutableBou,p)if(interfaces_.set(p))mutableBou[p].negate();
}
