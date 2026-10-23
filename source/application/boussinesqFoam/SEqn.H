fvScalarMatrix SEqn
{
    fvm::ddt(rho,S) + fvm::div(rhoPhi,S, "div(phi,S)") 
    - fvm::Sp(fvc::ddt(rho) + fvc::div(rhoPhi), S) 
    - fvm::laplacian(Ds*rho,S,"laplacian(Ds*rho,S)") 
    - fvc::div(kk/(Cp*TT)*fvc::grad(TT))
};
SEqn.relax();
fvOptions.constrain(SEqn);
SEqn.solve();
