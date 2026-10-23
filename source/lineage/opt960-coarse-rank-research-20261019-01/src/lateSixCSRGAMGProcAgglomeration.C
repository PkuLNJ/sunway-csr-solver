// Prototype adapted from the GPLv3+ OpenFOAM Foundation 2013-2019
// masterCoarsest/eager GAMGProcAgglomeration implementations fetched in actual/.
#include "lateSixCSRGAMGProcAgglomeration.H"
#include "CSRGAMGAgglomeration.H"
#include "lduMesh.H"
#include "addToRunTimeSelectionTable.H"
namespace Foam
{
    defineTypeNameAndDebug(lateSixCSRGAMGProcAgglomeration, 0);
    addToRunTimeSelectionTable
    (
        CSRGAMGProcAgglomeration,
        lateSixCSRGAMGProcAgglomeration,
        CSRGAMGAgglomeration
    );
}
Foam::lateSixCSRGAMGProcAgglomeration::lateSixCSRGAMGProcAgglomeration
(
    CSRGAMGAgglomeration& agglom,
    const dictionary& controlDict
)
: CSRGAMGProcAgglomeration(agglom, controlDict), applied_(false)
{}
Foam::lateSixCSRGAMGProcAgglomeration::~lateSixCSRGAMGProcAgglomeration()
{
    forAllReverse(comms_, i)
    {
        if (comms_[i] != -1) UPstream::freeCommunicator(comms_[i]);
    }
}
bool Foam::lateSixCSRGAMGProcAgglomeration::agglomerate()
{
    if (applied_) return false;
    // Match native masterCoarsest: one-but-last grid, because the next
    // restriction addressing must also be gathered/rebuilt.
    if (agglom_.size() < 2)
    {
        FatalErrorInFunction << "lateSix requires at least two coarse levels"
            << exit(FatalError);
    }
    const label leveli = agglom_.size() - 1;
    if (!agglom_.hasMeshLevel(leveli))
    {
        FatalErrorInFunction << "lateSix expects an unaggregated hierarchy"
            << exit(FatalError);
    }
    const lduMesh& mesh = agglom_.meshLevel(leveli);
    const label comm = mesh.comm();
    const label n = UPstream::nProcs(comm);
    if (n < 6 || n % 6)
    {
        FatalErrorInFunction << "lateSix requires communicator size divisible by6"
            << exit(FatalError);
    }
    labelList map(n);
    forAll(map, rank) map[rank] = rank / 6;
    labelList masters;
    List<label> members;
    CSRGAMGAgglomeration::calculateRegionMaster(comm, map, masters, members);
    comms_.append(UPstream::allocateCommunicator(comm, masters));
    CSRGAMGProcAgglomeration::agglomerate
    (
        leveli, map, masters, members, comms_.last()
    );
    applied_ = true;
    return true;
}
