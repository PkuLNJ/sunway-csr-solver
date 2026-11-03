volScalarField rhoCp = rho*Cp;
surfaceScalarField rhoPhiCP = fvc::interpolate(Cp)*rhoPhi;

fvScalarMatrix TEqn
{
    fvm::ddt(rhoCp,TT) + fvm::div(rhoPhiCP,TT, "div(phi,T)")
    - fvm::Sp(fvc::ddt(rhoCp) + fvc::div(rhoPhiCP), TT) 
    - fvm::laplacian(kk,TT,  "laplacian(kk,T)")
};
TEqn.relax();
fvOptions.constrain(TEqn);
TEqn.solve();

