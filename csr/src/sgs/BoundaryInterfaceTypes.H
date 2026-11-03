#ifndef LNJ_BOUNDARY_INTERFACE_TYPES_H
#define LNJ_BOUNDARY_INTERFACE_TYPES_H
#include "ExactPsiReadOnlyTypes.H"
namespace lnj {
template<class PlainProcessor,class CoarseProcessor,class Interface>
bool boundaryTypeAudited(const Interface& field){
 return lnjCandidate::exactPsiReadOnlyType<PlainProcessor,CoarseProcessor>(field);
}
}
#endif
