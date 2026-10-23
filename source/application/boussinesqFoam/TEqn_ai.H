// TEqn_ai.H
// Manual assembly of implicit ddt(rhoCp, TT) term for temperature/enthalpy equation.
// Usage in TEqn.H:
//   #include "TEqn_ai.H"
//   fvScalarMatrix TEqn
//   {
//       DDTai
//     + fvm::div(rhoPhiCP, TT, "div(phi,T)")
//     - fvm::Sp(fvc::ddt(rhoCp) + fvc::div(rhoPhiCP), TT)
//     - fvm::laplacian(kk, TT, "laplacian(kk,T)")
//   };
//   TEqn.relax();
//   fvOptions.constrain(TEqn);
//   TEqn.solve();

const fvMesh& mesh = TT.mesh();
const scalar rDeltaT = 1.0/runTime.deltaT();

// Build a matrix with correct physical dimensions for the ddt term
tmp<fvScalarMatrix> tfvm_DDTai
(
    new fvScalarMatrix
    (
        TT,
        rhoCp.dimensions()*TT.dimensions()*dimVol/dimTime
    )
);
fvScalarMatrix& DDTai = tfvm_DDTai.ref();

// Internal fields
const scalarField& V = mesh.V();
const scalarField& a = rhoCp.internalField();      // a = rho*Cp
const scalarField& Told = TT.oldTime().internalField();

// Assemble implicit diagonal and explicit source
scalarField& diag = DDTai.diag();
scalarField& src  = DDTai.source();

diag = a*V*rDeltaT;        // implicit coefficient: a*V/dt
src  = diag*Told;          // explicit old-time contribution: a*V/dt*T_old

// Boundary conditions are handled by TEqn insertion/solve; no off-diagonals for ddt.
