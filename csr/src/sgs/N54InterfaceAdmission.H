#ifndef LNJ_N54_INTERFACE_ADMISSION_H
#define LNJ_N54_INTERFACE_ADMISSION_H
#include "processorFvPatchField.H"
#include "processorCSRGAMGInterfaceField.H"
#include "ExactPsiReadOnlyTypes.H"
#include <cstdlib>
#include <cstring>
namespace {
inline bool n54RetainPsiEnabled(){
 const char* value=std::getenv("LNJ_SGS_RETAIN_PSI");
 if(!value||std::strcmp(value,"0")==0)return false;
 if(std::strcmp(value,"1")==0)return true;
 FatalErrorInFunction<<"LNJ_SGS_RETAIN_PSI must be 0 or 1"<<Foam::exit(Foam::FatalError);
 return false;
}
inline bool n54ImmutablePsiInterfaces(const Foam::lduInterfaceFieldPtrsList& interfaces){
 forAll(interfaces,p)if(interfaces.set(p)){
  if(!lnjCandidate::exactPsiReadOnlyType<Foam::processorFvPatchField<Foam::scalar>,Foam::processorCSRGAMGInterfaceField>(interfaces[p]))return false;
 }
 return true;
}
}
#endif
