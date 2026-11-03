#ifndef LNJ_N77_RHS_ADMISSION_H
#define LNJ_N77_RHS_ADMISSION_H
#include <cstdlib>
#include <cstring>
#include "ZeroStartWriteSet.H"
namespace lnj {
inline bool rhsResetRequested(){
 const char* p=std::getenv("LNJ_SGS_BOUNDARY_RHS_RESET");
 if(!p||std::strcmp(p,"0")==0)return false;
 if(std::strcmp(p,"1")==0)return true;
 FatalErrorInFunction<<"LNJ_SGS_BOUNDARY_RHS_RESET must be0 or1"<<Foam::abort(Foam::FatalError);return false;
}
// Original zeroStart branch uses meshIf[p].faceCells(), NOT the actual field
// getter. Verify this second metadata list and conservatively UNION its cells.
// A missing/bad list is NOT presumed equal: keep actual plan, covered=false.
inline ZeroStartWriteSet rhsZeroWriteSet(BoundaryTilePlan actual,
 const Foam::lduInterfaceFieldPtrsList& fields,const Foam::lduInterfacePtrsList& meshFields){
 std::vector<std::int64_t>cells;
 forAll(fields,p)if(fields.set(p)){
  if(p>=meshFields.size()||!meshFields.set(p))return zeroStartWriteSet(std::move(actual),{},false);
  const auto& fc=meshFields[p].faceCells();forAll(fc,i)cells.push_back(fc[i]);
 }
 return zeroStartWriteSet(std::move(actual),cells,true);
}
}
#endif
