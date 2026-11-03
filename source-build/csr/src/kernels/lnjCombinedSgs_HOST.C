// SPDX-License-Identifier: GPL-3.0-or-later
// Host counterpart for the SAME packed level schedule as combined SGS CPE.
// Not yet integrated or validated on Sunway. No change to matrix or ordering.
#include "lnjCombinedSgsParam.h"
#include <cmath>
extern "C" void lnjCombinedSgs_HOST(const lnjCombinedSgsParam* p)
{
    for (int i=0;i<64;++i) p->errorFlags[i]=0;
    for (int64_t i=0;i<p->nRows;++i) {
        const int64_t original=p->packedToOriginal[i];
        if (!p->reusePackedPsi) {
            p->psi[i]=p->originalPsi[original];
            if (!std::isfinite(p->psi[i])) p->errorFlags[0]|=1;
        }
        p->rhs[i]=p->originalRhs[original];
        if (!std::isfinite(p->rhs[i])) p->errorFlags[0]|=2;
    }
    // Validated topology guarantees no dependencies between rows of one level.
    // Each row's coefficient order and subtraction/division match the CPE.
    for (int phase=0;phase<2;++phase)
        for (int64_t ordinal=0;ordinal<p->nLevels;++ordinal) {
            const int64_t level=phase ? p->nLevels-1-ordinal : ordinal;
            for (int64_t row=p->levelStarts[level];row<p->levelStarts[level+1];++row) {
                double sum=p->rhs[row];
                for (int64_t e=p->rowPtr[row];e<p->rowPtr[row+1];++e)
                    sum-=p->values[e]*p->psi[p->columns[e]];
                p->psi[row]=sum/p->diagonal[row];
                if (phase==1 && !std::isfinite(p->psi[row])) p->errorFlags[0]|=4;
            }
        }
    if (p->fullUnpack) {
        for (int64_t i=0;i<p->nRows;++i) p->originalPsi[i]=p->psi[p->originalToPacked[i]];
    } else {
        for (int64_t t=0;t<p->nBoundaryTiles;++t) {
            const int64_t first=p->boundaryTileStarts[t];
            const int64_t stop=first+16<p->nRows ? first+16 : p->nRows;
            for (int64_t i=first;i<stop;++i) p->originalPsi[i]=p->psi[p->originalToPacked[i]];
        }
    }
}
