#ifndef LNJ_N74_INTERFACE_ADMISSION_H
#define LNJ_N74_INTERFACE_ADMISSION_H
#include "processorFvPatchField.H"
#include "processorCSRGAMGInterfaceField.H"
#include "BoundaryInterfaceTypes.H"
#include "lnjCombinedSgsHost.H"
#include <cstdlib>
#include <cstring>
namespace lnj {
inline bool boundaryMaterializationRequested(){
 const char* p=std::getenv("LNJ_SGS_BOUNDARY_MATERIALIZE");
 if(!p||std::strcmp(p,"0")==0)return false;
 if(std::strcmp(p,"1")==0)return true;
 FatalErrorInFunction<<"LNJ_SGS_BOUNDARY_MATERIALIZE must be0 or1"<<Foam::abort(Foam::FatalError);return false;
}
struct BoundaryAdmission {
 BoundaryTilePlan plan;bool admitted;
};
// Constructor ONLY: immutable addresses, no source/psi/numeric values read.
// Exact types reject unknown derived/cyclic implementations.
inline BoundaryAdmission boundaryPlanAdmission(const Foam::lduInterfaceFieldPtrsList& fields,
 const std::int64_t n){
 try{
 forAll(fields,p)if(fields.set(p)){
  if(!boundaryTypeAudited<Foam::processorFvPatchField<Foam::scalar>,Foam::processorCSRGAMGInterfaceField>(fields[p]))return {BoundaryTilePlan(n,{}),false};
 }
 std::vector<std::int64_t>cells;
 forAll(fields,p)if(fields.set(p)){
  if(typeid(fields[p])==typeid(Foam::processorFvPatchField<Foam::scalar>)){
   const auto& field=dynamic_cast<const Foam::processorFvPatchField<Foam::scalar>&>(fields[p]);
   const auto& fc=field.patch().faceCells(); // SAME patch used by actual init/send getter.
   forAll(fc,i)cells.push_back(fc[i]);
  }else{
   const auto& field=dynamic_cast<const Foam::processorCSRGAMGInterfaceField&>(fields[p]);
   const auto& actual=static_cast<const Foam::CSRGAMGInterfaceField&>(field).interface();
   const auto& fc=actual.faceCells(); // SAME GAMGCp as private procInterface_, verified ctor.
   forAll(fc,i)cells.push_back(fc[i]);
  }
 }
 return {BoundaryTilePlan(n,cells),true};
 }catch(const std::exception& e){
  FatalErrorInFunction<<"Boundary materialization admission failure is terminal: "<<e.what()<<Foam::abort(Foam::FatalError);
  throw;
 }
}
// O(1) per smooth invocation, BEFORE bPrime copies all source rows. Alias can
// change from one invocation to another and must NEVER be cached as admitted.
inline bool boundaryCallAliasFree(const Foam::scalarField& source,const Foam::scalarField& psi,const std::int64_t n){
 if(source.size()!=n||psi.size()!=n)FatalErrorInFunction<<"Boundary SGS call dimensions changed"<<Foam::abort(Foam::FatalError);
 return !n74RangesOverlap(source.begin(),psi.begin(),static_cast<std::size_t>(n));
}
}
#endif
