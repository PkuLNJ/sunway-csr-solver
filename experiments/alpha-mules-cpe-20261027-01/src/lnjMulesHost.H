#ifndef LNJ_MULES_HOST_H
#define LNJ_MULES_HOST_H
#include "lnjMulesParam.h"
#include <crts.h>
#include <cstdlib>
#include <cstring>
extern "C" void SLAVE_FUN(lnjMules_CPE)(lnjMulesParam*);
namespace Foam { namespace lnjMules {
inline bool enabled(){static const bool use=[](){const char* v=std::getenv("LNJ_MULES_CPE");return v && std::strcmp(v,"1")==0;}();return use;}
inline void dispatch(lnjMulesParam& p){CRTS_athread_spawn(reinterpret_cast<void*>(SLAVE_FUN(lnjMules_CPE)),&p);CRTS_athread_join();}
inline bool ratios(scalarField& plus,scalarField& minus,const scalarField& maxn,const scalarField& minsum,const scalarField& minn,const scalarField& plussum){
    static_assert(sizeof(scalar)==8 && sizeof(label)==8,"DP Int64 only");
    if(!enabled() || plus.size()<4096)return false;
    lnjMulesParam p={};p.count=plus.size();p.cells=p.count;p.mode=0;p.tiny=rootVSmall;
    p.out0=plus.begin();p.out1=minus.begin();p.a=maxn.begin();p.b=minsum.begin();p.c=minn.begin();p.d=plussum.begin();dispatch(p);return true;
}
inline bool faces(scalarField& lambda,const scalarField& flux,const scalarField& plus,const scalarField& minus,const labelUList& owner,const labelUList& neighbour){
    if(!enabled() || lambda.size()<4096)return false;
    lnjMulesParam p={};p.count=lambda.size();p.cells=plus.size();p.mode=1;p.out0=lambda.begin();p.a=flux.begin();p.b=plus.begin();p.c=minus.begin();
    p.owner=reinterpret_cast<const int64_t*>(owner.begin());p.neighbour=reinterpret_cast<const int64_t*>(neighbour.begin());dispatch(p);return true;
}
}}
#endif
