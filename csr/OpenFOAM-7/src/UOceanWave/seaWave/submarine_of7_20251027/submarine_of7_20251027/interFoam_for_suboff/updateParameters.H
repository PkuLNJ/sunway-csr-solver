dimensionedScalar T1 ("T1", dimensionSet(0,0,0,1,0), 1.0);
dimensionedScalar k1 ("k1", dimensionSet(1,1,-3,-1,0), 1.0);
volScalarField Tstar = TT/(300*T1);
volScalarField k_T = 0.797015*pow(Tstar,-0.194)-0.251242*pow(Tstar,-4.717)+0.096437*pow(Tstar,-6.385)-0.032696*pow(Tstar,-2.134);
volScalarField k_T_S = (k_T/(1+0.00022*S))*k1;
kk = k_T_S*alpha1 + ka*alpha2; 

dimensionedScalar Cp1 ("Cp1", dimensionSet(0,2,-2,-1,0), 1.0);
volScalarField  CpA = 5328-9.76*1e1*S+4.04*1e-1*S*S;
volScalarField  CpB = -6.913+7.351*1e-1*S-3.15*1e-3*S*S;
volScalarField  CpC = 9.6*1e-3-1.927*1e-3*S+8.23*1e-6*S*S;
volScalarField  CpD = 2.5*1e-6+1.666*1e-6*S-7.125*1e-9*S*S;
volScalarField  Cpw = (CpA + CpB*(TT/T1) + CpC*(TT/T1)*(TT/T1) + CpD*(TT/T1)*(TT/T1)*(TT/T1))*Cp1;
Cp =  Cpw*alpha1 + Cpa*alpha2;

dimensionedScalar Ds1 ("Ds1", dimensionSet(0,2,-1,0,0), 1.0);
volScalarField Dsw = (8.16+0.255*(TT/T1)+0.00254*(TT/T1)*(TT/T1)-0.028*S+0.000147*S*S)*1e-10*Ds1;
Ds = Dsw*alpha1 + Dsa*alpha2;


dimensionedScalar rho11 ("rho11", dimensionSet(1,-3,0,0,0), 1.0);
volScalarField tt1 = TT/T1 - 273.15;
volScalarField SS = S/1000;
volScalarField rhow = ((9.999*1e2 + 2.034*1e-2*tt1 - 6.162*1e-3*tt1*tt1 + 2.261*1e-5*tt1*tt1*tt1 - 4.657*1e-8*pow(tt1,4))
                    + (8.020*1e2*SS - 2.001*SS*tt1 + 1.677*1e-2*SS*tt1*tt1 - 3.060*1e-5*SS*pow(tt1,3) - 1.613*1e-5*SS*SS*tt1*tt1))*rho11;
rhok = rhow*alpha1 + rho2*alpha2;
/*
dimensionedScalar mu1 ("mu1", dimensionSet(1,-1,-1,0,0), 1.0);
volScalarField muA = 1.541+1.998*1e-2*tt1-9.52*1e-5*tt1*tt1;
volScalarField muB = 7.974 - 7.561*1e-2*tt1 + 4.724*1e-4*tt1*tt1;
volScalarField muww = muw*(1+muA*SS+muB*SS*SS);
muu = muww*alpha1 + mua*alpha2;
*/
salinity = S*alpha1;
