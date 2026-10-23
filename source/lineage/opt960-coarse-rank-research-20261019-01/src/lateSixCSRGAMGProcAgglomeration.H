// Prototype adapted from the GPLv3+ OpenFOAM Foundation 2013-2019
// masterCoarsest/eager GAMGProcAgglomeration implementations fetched in actual/.
#ifndef lateSixCSRGAMGProcAgglomeration_H
#define lateSixCSRGAMGProcAgglomeration_H
#include "CSRGAMGProcAgglomeration.H"
#include "DynamicList.H"
namespace Foam
{
class lateSixCSRGAMGProcAgglomeration : public CSRGAMGProcAgglomeration
{
    DynamicList<label> comms_;
    bool applied_;
public:
    TypeName("lateSix");
    lateSixCSRGAMGProcAgglomeration
    (
        CSRGAMGAgglomeration& agglom,
        const dictionary& controlDict
    );
    virtual ~lateSixCSRGAMGProcAgglomeration();
    virtual bool agglomerate();
    lateSixCSRGAMGProcAgglomeration
    (
        const lateSixCSRGAMGProcAgglomeration&
    ) = delete;
    void operator=(const lateSixCSRGAMGProcAgglomeration&) = delete;
};
}
#endif
