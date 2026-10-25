#ifndef smallCoarseAllCSRGAMGProcAgglomeration_H
#define smallCoarseAllCSRGAMGProcAgglomeration_H
#include "CSRGAMGProcAgglomeration.H"
#include "DynamicList.H"
namespace Foam {
class smallCoarseAllCSRGAMGProcAgglomeration : public CSRGAMGProcAgglomeration {
    DynamicList<label> comms_;
    const label byteCap_, innerMaxIter_;
    bool attempted_;
public:
    TypeName("smallCoarseAll");
    smallCoarseAllCSRGAMGProcAgglomeration(CSRGAMGAgglomeration&,const dictionary&);
    virtual ~smallCoarseAllCSRGAMGProcAgglomeration();
    virtual bool agglomerate();
    smallCoarseAllCSRGAMGProcAgglomeration(const smallCoarseAllCSRGAMGProcAgglomeration&)=delete;
    void operator=(const smallCoarseAllCSRGAMGProcAgglomeration&)=delete;
};
}
#endif
