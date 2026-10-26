#ifndef LNJ_MULES_PARAM_H
#define LNJ_MULES_PARAM_H
#include <stdint.h>
typedef struct {
    int64_t count, cells;
    int mode;
    double tiny;
    double *out0, *out1;
    const double *a, *b, *c, *d;
    const int64_t *owner, *neighbour;
} lnjMulesParam;
/* Match Foam min/max operand order; no reassociation or reciprocal approximation. */
static inline double lnjMin(double a,double b){return a<b?a:b;}
static inline double lnjMax(double a,double b){return a>b?a:b;}
static inline double lnjRatio(double x,double offset,double denominator,double tiny){
    return lnjMax(lnjMin((x+offset)/(denominator+tiny),1.0),0.0);
}
static inline double lnjFace(double old,double flux,double op,double om,double np,double nm){
    return flux>0 ? lnjMin(old,lnjMin(op,nm)) : lnjMin(old,lnjMin(om,np));
}
#endif
