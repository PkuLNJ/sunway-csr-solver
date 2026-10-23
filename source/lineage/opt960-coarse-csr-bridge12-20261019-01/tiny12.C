// Actual OpenFOAM/MPI integration test draft, NOT the standalone lifecycle mock.
// One MPI initialization, exactly12 ranks in2 groups of6, two fresh registries.
#include "argList.H"
#include "Time.H"
#include "objectRegistry.H"
#include "IOobject.H"
#include "lduPrimitiveMesh.H"
#include "processorGAMGInterface.H"
#include "processorGAMGInterfaceField.H"
#include "processorCSRGAMGInterface.H"
#include "pairCSRGAMGAgglomeration.H"
#include "CSRGAMGSolver.H"
#include "addToRunTimeSelectionTable.H"
#include "PstreamReduceOps.H"
#include "IStringStream.H"
#include <cmath>
#include <cstdio>
#include <unistd.h>

namespace Foam
{
static const label expectedRanks=12;
static const label groupSize=6;
// Pure matrix pair-coarsening avoids faceAreaPair's required fvMesh geometry.
class tinyChainCSRGAMGAgglomeration : public pairCSRGAMGAgglomeration
{
public:
    TypeName("tinyChain");
    tinyChainCSRGAMGAgglomeration(const lduMatrix& a, const dictionary& d)
    : pairCSRGAMGAgglomeration(a.mesh(), d)
    {
        pairCSRGAMGAgglomeration::agglomerate
        (
            a.mesh(), scalarField(a.lduAddr().upperAddr().size(), scalar(1))
        );
    }
};
defineTypeNameAndDebug(tinyChainCSRGAMGAgglomeration, 0);
addToRunTimeSelectionTable
(
    CSRGAMGAgglomeration, tinyChainCSRGAMGAgglomeration, lduMatrix
);

class tinyRegistryMesh : public lduPrimitiveMesh
{
    const objectRegistry& registry_;
public:
    tinyRegistryMesh(label n, labelList& l, labelList& u,
                     label comm, const objectRegistry& registry)
    : lduPrimitiveMesh(n, l, u, comm, false), registry_(registry) {}
    virtual const objectRegistry& thisDb() const { return registry_; }
};

// Local stderr only: no Foam Pout setup and no MPI collective.
static void phase(const char* name, label level=-1)
{
    char host[128]={0}; ::gethostname(host,sizeof(host)-1);
    std::fprintf(stderr,"TINY12_LOCAL_PHASE rank=%lld pid=%ld host=%s level=%lld name=%s\n",
                 static_cast<long long>(Pstream::myProcNo()),
                 static_cast<long>(::getpid()),host,static_cast<long long>(level),name);
    std::fflush(stderr);
}

static void requireAll(bool local, const char* message)
{
    label bad = local ? 0 : 1;
    phase("REQUIRE_ALL_REDUCE_BEFORE");
    std::fprintf(stderr,"TINY12_REQUIRE rank=%lld detail=%s local=%d\n",static_cast<long long>(Pstream::myProcNo()),message,int(local));
    std::fflush(stderr);
    reduce(bad, sumOp<label>());
    phase("REQUIRE_ALL_REDUCE_AFTER");
    if (bad)
    {
        FatalErrorInFunction << "TINY12_FAIL " << message
            << " badRanks=" << bad << exit(FatalError);
    }
}

static scalar globalMax(scalar value)
{
    phase("GLOBAL_MAX_REDUCE_BEFORE");
    reduce(value, maxOp<scalar>());
    phase("GLOBAL_MAX_REDUCE_AFTER");
    return value;
}

class tinyInspector : public CSRGAMGSolver
{
public:
    using CSRGAMGSolver::CSRGAMGSolver;
    void inspect(const lduMatrix& original, const dictionary& controls,
                 const word& mode) const
    {
        phase("AGGLOMERATION_LOOKUP_BEFORE");
        const CSRGAMGAgglomeration& agg =
            CSRGAMGAgglomeration::New(original, controls);
        phase("AGGLOMERATION_LOOKUP_AFTER");
        PtrList<scalarField> corrections, sources;
        PtrList<csrMatrix::smoother> smoothers;
        scalarField scratch1, scratch2;
        phase("INIT_VCYCLE_BEFORE");
        initVcycle(corrections, sources, smoothers, scratch1, scratch2);
        phase("INIT_VCYCLE_AFTER");
        label minLevels = sources.size(), maxLevels = sources.size();
        phase("HIERARCHY_MIN_REDUCE_BEFORE");
        reduce(minLevels, minOp<label>());
        phase("HIERARCHY_MIN_REDUCE_AFTER");
        phase("HIERARCHY_MAX_REDUCE_BEFORE");
        reduce(maxLevels, maxOp<label>());
        phase("HIERARCHY_MAX_REDUCE_AFTER");
        requireAll(minLevels == maxLevels && minLevels >= 2,
                   "hierarchy sizes differ across ranks or fewer than2 levels");

        scalarField ones(original.diag().size(), scalar(1));
        scalarField fineProduct(ones.size(), scalar(0));
        const csrMatrix::smoother& fine = smoothers[0];
        phase("FINE_AMUL_BEFORE");
        fine.matrix().Amul(fineProduct, ones, fine.interfaceBouCoeffs(),
                           fine.interfaces(), 0);
        phase("FINE_AMUL_AFTER");
        phase("RESTRICT_FINE_BEFORE",0);
        agg.restrictField(sources[0], fineProduct, 0, true);
        phase("RESTRICT_FINE_AFTER",0);
        for (label level = 0; level + 1 < sources.size(); ++level)
        {
            if (sources.set(level + 1))
            {
                phase("RESTRICT_COARSE_BEFORE",level+1);
                agg.restrictField(sources[level + 1], sources[level],
                                  level + 1, true);
                phase("RESTRICT_COARSE_AFTER",level+1);
            }
        }
        for (label level = 0; level < sources.size(); ++level)
        {
            bool valid = true;
            scalar error = 0;
            label localRows = 0;
            label active = smoothers.set(level + 1) ? 1 : 0;
            if (active)
            {
                const csrMatrix::smoother& sm = smoothers[level + 1];
                const csrMatrix& a = sm.matrix();
                localRows = a.diag().size();
                valid = &a.ldu().mesh() == &agg.meshLevel(level + 1)
                     && a.diag().size() == a.ldu().diag().size()
                     && sources.set(level)
                     && sources[level].size() == a.diag().size();
                if (valid)
                    forAll(a.diag(), i)
                        valid = valid && a.diag()[i] == a.ldu().diag()[i];
            }
            // Reject any bad owner collectively BEFORE entering subcomm Amul.
            requireAll(valid, "CSR owner/mesh/coefficient/source mismatch");
            if (active)
            {
                const csrMatrix::smoother& sm = smoothers[level + 1];
                const csrMatrix& a = sm.matrix();
                scalarField x(localRows, scalar(1)), ax(localRows, scalar(0));
                phase("ACTIVE_COARSE_AMUL_BEFORE",level);
                a.Amul(ax, x, sm.interfaceBouCoeffs(), sm.interfaces(), 0);
                phase("ACTIVE_COARSE_AMUL_AFTER",level);
                forAll(ax, i)
                {
                    valid = valid && std::isfinite(ax[i]);
                    error = max(error, mag(ax[i] - sources[level][i]));
                }
            }
            requireAll(valid, "nonfinite coarse Amul");
            error = globalMax(error);
            requireAll(error <= 1e-12, "coarse Amul differs from R A P");
            phase("ACTIVE_COUNT_REDUCE_BEFORE",level);
            reduce(active, sumOp<label>());
            phase("ACTIVE_COUNT_REDUCE_AFTER",level);
            phase("GLOBAL_ROWS_REDUCE_BEFORE",level);
            reduce(localRows, sumOp<label>());
            phase("GLOBAL_ROWS_REDUCE_AFTER",level);
            if (Pstream::master())
                Info << "TINY12_LEVEL MODE=" << mode << " LEVEL=" << level
                     << " ACTIVE=" << active << " GLOBAL_ROWS=" << localRows
                     << " RAP_LINF=" << error << endl;
            if (level == sources.size() - 1)
            {
                requireAll(active == (mode == "lateSix" ? Pstream::nProcs()/groupSize : Pstream::nProcs()),
                           "unexpected bottom active-rank count");
                bool validComm = true;
                if (smoothers.set(level + 1))
                {
                    const lduMesh& bottom=smoothers[level + 1].matrix().mesh();
                    validComm = UPstream::nProcs(bottom.comm()) ==
                        (mode == "lateSix" ? Pstream::nProcs()/groupSize : Pstream::nProcs());
                    if (mode == "lateSix")
                    {
                        const lduInterfacePtrsList remaining=bottom.interfaces();
                        label count=0;
                        forAll(remaining,p) if(remaining.set(p))
                        {
                            ++count;
                            validComm=validComm && isA<processorCSRGAMGInterface>(remaining[p]);
                            const processorLduInterface& proc=refCast<const processorLduInterface>(remaining[p]);
                            validComm=validComm && proc.comm()==bottom.comm()
                                && proc.myProcNo()==Pstream::myProcNo()/groupSize
                                && proc.neighbProcNo()==1-proc.myProcNo();
                        }
                        // Twelve-chain / two groups MUST retain one interface each.
                        validComm=validComm && count==1;
                        std::fprintf(stderr,"TINY12_BOUNDARY world=%lld child=%lld interfaces=%lld\n",
                            static_cast<long long>(Pstream::myProcNo()),
                            static_cast<long long>(UPstream::myProcNo(bottom.comm())),
                            static_cast<long long>(count));
                        std::fflush(stderr);
                    }
                }
                requireAll(validComm, "unexpected bottom communicator size");
            }
        }

        // Injection of a constant bottom field must return1 on all fine cells.
        forAll(corrections, level)
            if (corrections.set(level)) corrections[level] = scalar(1);
        scalarField dummy(0);
        for (label level = corrections.size() - 2; level >= 0; --level)
        {
            if (corrections.set(level))
                agg.prolongField(corrections[level],
                    corrections.set(level + 1) ? corrections[level + 1] : dummy,
                    level + 1, true);
        }
        scalarField prolonged(ones.size(), scalar(0));
        agg.prolongField(prolonged, corrections[0], 0, true);
        scalar prolongError = 0;
        forAll(prolonged, i) prolongError = max(prolongError, mag(prolonged[i] - 1));
        requireAll(globalMax(prolongError) <= 1e-12, "constant prolongation mismatch");
        if (Pstream::master()) Info << "TINY12_STRUCTURE_PASS MODE=" << mode << endl;
    }
};
}

int main(int argc, char** argv)
{
    using namespace Foam;
    char beforeHost[128]={0};::gethostname(beforeHost,sizeof(beforeHost)-1);
    std::fprintf(stderr,"TINY12_BEFORE_ARGLIST pid=%ld host=%s argc=%d\n",static_cast<long>(::getpid()),beforeHost,argc);
    std::fflush(stderr);
    argList args(argc, argv);
    phase("ARGLIST_COMPLETE");
    phase("EXACT12_REQUIREALL_BEFORE");
    requireAll(Pstream::nProcs() == expectedRanks, "requires exactly12 actual MPI ranks");
    phase("EXACT12_REQUIREALL_AFTER");
    // This synthetic mesh has no scheduled-communication patch schedule.
    UPstream::defaultCommsType = UPstream::commsTypes::nonBlocking;
    // Keep the old local dictionary parsing unchanged to isolate ctor selection.
    // The basic NO_READ ctor deliberately does NOT consume timeControls or setControls.
    dictionary timeControls(IStringStream
    (
        "application tiny12; startFrom startTime; startTime 0; "
        "stopAt endTime; endTime 1; deltaT 1; writeControl timeStep; "
        "writeInterval 1; runTimeModifiable false; functions {}"
    )());
    phase("TIME_CONSTRUCTOR_BEFORE");
    Time runTime(args.rootPath(), args.caseName(),
                 "system", "constant", false);
    phase("TIME_CONSTRUCTOR_AFTER");
    phase("TIME_SET_SCALAR_BEFORE");
    runTime.setTime(scalar(0), label(0));
    phase("TIME_SET_SCALAR_AFTER");
    for (label modeI = 0; modeI < 2; ++modeI)
    {
        const word mode(modeI ? "lateSix" : "none");
        std::fprintf(stderr,"TINY12_LOCAL_MODE rank=%lld mode=%s\n",
                     static_cast<long long>(Pstream::myProcNo()),mode.c_str());
        std::fflush(stderr);
        phase("REGISTRY_CONSTRUCTOR_BEFORE");
        objectRegistry registry
        (
            IOobject(word("tiny_") + mode, runTime.timeName(), runTime,
                     IOobject::NO_READ, IOobject::NO_WRITE)
        );
        phase("REGISTRY_CONSTRUCTOR_AFTER");
        const label n = 128, rank = Pstream::myProcNo();
        labelList lower(n - 1), upper(n - 1);
        forAll(lower, i) { lower[i] = i; upper[i] = i + 1; }
        phase("MESH_CONSTRUCTOR_BEFORE");
        tinyRegistryMesh mesh(n, lower, upper, UPstream::worldComm, registry);
        phase("MESH_CONSTRUCTOR_AFTER");
        phase("INTERFACES_BEFORE");
        lduInterfacePtrsList patches(2);
        for (label side = 0; side < 2; ++side)
        {
            const label neighbor = rank + (side ? 1 : -1);
            if (neighbor >= 0 && neighbor < Pstream::nProcs())
            {
                labelList cells(label(1), side ? n - label(1) : label(0));
                labelList restriction(label(1), label(0));
                patches.set(side, new processorGAMGInterface
                (
                    side, patches, cells, restriction, UPstream::worldComm,
                    rank, neighbor, tensorField(0), 912 + min(rank, neighbor)
                ));
            }
        }
        mesh.addInterfaces(patches, lduSchedule(0)); // mesh owns interfaces
        PtrList<lduInterfaceField> ownedFields(2);
        lduInterfaceFieldPtrsList fields(2);
        FieldField<Field, scalar> bou(2), internal(2);
        for (label side = 0; side < 2; ++side)
        {
            if (patches.set(side))
            {
                ownedFields.set(side, new processorGAMGInterfaceField
                    (refCast<const GAMGInterface>(patches[side]), false, 0));
                fields.set(side, &ownedFields[side]);
                bou.set(side, new scalarField(1, scalar(1)));
                internal.set(side, new scalarField(1, scalar(1)));
            }
            else
            {
                bou.set(side, new scalarField(0));
                internal.set(side, new scalarField(0));
            }
        }
        phase("INTERFACES_AFTER");
        phase("LDU_CONSTRUCTOR_BEFORE");
        lduMatrix a(mesh);
        a.diag() = scalarField(n, scalar(3));
        a.upper() = scalarField(n - 1, scalar(-1)); // symmetric, no lower allocation
        phase("LDU_CONSTRUCTOR_AFTER");
        phase("CSR_CONSTRUCTOR_BEFORE");
        csrMatrix csr(a);
        phase("CSR_CONSTRUCTOR_AFTER");
        dictionary controls(IStringStream
        (
            // Test-only uncached lifetime destroys hierarchy before fine mesh.
            "solver GAMG; agglomerator tinyChain; cacheAgglomeration false; "
            "nCellsInCoarsestLevel 2; mergeLevels 1; maxLevels 12; "
            "smoother symGaussSeidel; nPreSweeps 2; maxPreSweeps 2; "
            "nPostSweeps 2; maxPostSweeps 2; nFinestSweeps 2; "
            "scaleCorrection true; solveCoarsest true; directSolveCoarsest false; "
            "tolerance 1e-10; relTol 0; maxIter 50; minIter 0;"
        )());
        controls.add("processorAgglomerator", mode);
        {
            phase("SOLVER_CONSTRUCTOR_BEFORE");
            tinyInspector solver("tiny", csr, bou, internal, fields, controls);
            phase("SOLVER_CONSTRUCTOR_AFTER");
            phase("INSPECT_BEFORE");
            solver.inspect(a, controls, mode);
            phase("INSPECT_AFTER");
            scalarField exact(n, scalar(1)), rhs(n, scalar(0)), psi(n, scalar(0));
            phase("EXACT_RHS_AMUL_BEFORE");
            csr.Amul(rhs, exact, bou, fields, 0);
            phase("EXACT_RHS_AMUL_AFTER");
            phase("SOLVE_BEFORE");
            solver.solve(psi, rhs, 0);
            phase("SOLVE_AFTER");
            scalarField ax(n, scalar(0));
            phase("FINAL_TRUE_AMUL_BEFORE");
            csr.Amul(ax, psi, bou, fields, 0);
            phase("FINAL_TRUE_AMUL_AFTER");
            scalar residual = 0, error = 0;
            bool finite = true;
            forAll(psi, i)
            {
                finite = finite && std::isfinite(psi[i]) && std::isfinite(ax[i]);
                residual = max(residual, mag(rhs[i] - ax[i]));
                error = max(error, mag(psi[i] - 1));
            }
            requireAll(finite, "nonfinite solution");
            residual = globalMax(residual); error = globalMax(error);
            requireAll(residual <= 1e-8 && error <= 1e-8, "true residual/solution error");
            if (Pstream::master())
                Info << "TINY12_SOLVE_PASS MODE=" << mode
                     << " TRUE_LINF=" << residual << " SOLUTION_LINF=" << error << endl;
        }
    }
    phase("ALL_TESTS_DONE_BEFORE_TIME_DTORS");
    if (Pstream::master()) Info << "TINY12_ALL_PASS\nEnd" << endl;
    return 0;
}
