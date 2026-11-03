#include "lnjMulesParam.h"
extern "C" void lnjMules_HOST(lnjMulesParam* p)
{
    if (p->mode == 0) {
        for (int64_t i=0; i<p->count; ++i) {
            p->out0[i]=lnjRatio(p->out0[i],p->a[i],p->b[i],p->tiny);
            p->out1[i]=lnjRatio(p->out1[i],p->c[i],p->d[i],p->tiny);
        }
    } else {
        for (int64_t i=0; i<p->count; ++i) {
            const int64_t owner=p->owner[i], neighbour=p->neighbour[i];
            p->out0[i]=lnjFace(p->out0[i],p->a[i],p->b[owner],
                p->c[owner],p->b[neighbour],p->c[neighbour]);
        }
    }
}
