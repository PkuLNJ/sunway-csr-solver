#include "createRDeltaT.H"

Info<< "Reading field p_rgh\n" << endl;
volScalarField p_rgh
(
    IOobject
    (
        "p_rgh",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

Info<< "Reading field U\n" << endl;
volVectorField U
(
    IOobject
    (
        "U",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

#include "createPhi.H"


Info<< "Reading transportProperties\n" << endl;
immiscibleIncompressibleTwoPhaseMixture mixture(U, phi);

volScalarField& alpha1(mixture.alpha1());
volScalarField& alpha2(mixture.alpha2());

const dimensionedScalar& rho1 = mixture.rho1();
const dimensionedScalar& rho2 = mixture.rho2();


// Need to store rho for ddt(rho, U)
volScalarField rho
(
    IOobject
    (
        "rho",
        runTime.timeName(),
        mesh,
        IOobject::READ_IF_PRESENT
    ),
    alpha1*rho1 + alpha2*rho2
);
rho.oldTime();


// Mass flux
surfaceScalarField rhoPhi
(
    IOobject
    (
        "rhoPhi",
        runTime.timeName(),
        mesh,
        IOobject::NO_READ,
        IOobject::NO_WRITE
    ),
    fvc::interpolate(rho)*phi
);


// Construct incompressible turbulence model
autoPtr<incompressible::turbulenceModel> turbulence
(
    incompressible::turbulenceModel::New(U, phi, mixture)
);


#include "readGravitationalAcceleration.H"
#include "readhRef.H"
#include "gh.H"


volScalarField p
(
    IOobject
    (
        "p",
        runTime.timeName(),
        mesh,
        IOobject::NO_READ,
        IOobject::AUTO_WRITE
    ),
    p_rgh + rho*gh
);

label pRefCell = 0;
scalar pRefValue = 0.0;
setRefCell
(
    p,
    p_rgh,
    pimple.dict(),
    pRefCell,
    pRefValue
);

if (p_rgh.needReference())
{
    p += dimensionedScalar
    (
        "p",
        p.dimensions(),
        pRefValue - getRefCellValue(p, pRefCell)
    );
    p_rgh = p - rho*gh;
}

mesh.setFluxRequired(p_rgh.name());
mesh.setFluxRequired(alpha1.name());

#include "createMRF.H"
#include "createFvOptions.H"

Info<<"Reading boussinesqProperties\n"<<endl;

IOdictionary boussinesqProperties
(
    IOobject
    (
        "boussinesqProperties",
        runTime.constant(),
        mesh,
        IOobject::MUST_READ_IF_MODIFIED,
        IOobject::NO_WRITE
    )
); 

//dimensionedScalar Tref(boussinesqProperties.lookup("Tref"));
//dimensionedScalar beta(boussinesqProperties.lookup("beta"));
dimensionedScalar Cpa(boussinesqProperties.lookup("Cpa"));
dimensionedScalar ka(boussinesqProperties.lookup("ka"));
dimensionedScalar Dsa(boussinesqProperties.lookup("Dsa"));
dimensionedScalar mua(boussinesqProperties.lookup("mua"));
dimensionedScalar muw(boussinesqProperties.lookup("muw"));
dimensionedScalar initField(boussinesqProperties.lookup("initField"));
dimensionedScalar sealevel(boussinesqProperties.lookup("sealevel"));

volScalarField rhok
(
    IOobject
    (
        "rhok",
        runTime.timeName(),
        mesh,
        IOobject::READ_IF_PRESENT,
        IOobject::AUTO_WRITE
    ),
    rho
);

volScalarField TT
(
    IOobject
    (
        "TT",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

volScalarField S
(
    IOobject
    (
        "S",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);
volScalarField salinity
(
    IOobject
    (
        "salinity",
        runTime.timeName(),
        mesh,
        IOobject::NO_READ,
        IOobject::AUTO_WRITE
    ),
    S*alpha1
);

volScalarField kk
(
    IOobject
    (
        "kk",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

volScalarField Cp
(
    IOobject
    (
        "Cp",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

volScalarField Ds
(
    IOobject
    (
        "Ds",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

volScalarField muu
(
    IOobject
    (
        "muu",
        runTime.timeName(),
        mesh,
        IOobject::MUST_READ,
        IOobject::AUTO_WRITE
    ),
    mesh
);

Info<<"Reading boussinesqProperties\n"<<endl;
if(initField.value()==1)
{
	Info<<"Reading initFieldcoeff\n"<<endl;
	IOdictionary initFieldcoeff
	(
		IOobject
		(
		    "initFieldcoeff",
		    runTime.constant(),
		    mesh,
		    IOobject::MUST_READ,
		    IOobject::NO_WRITE
		)
	);

	List<List<scalar>> initFieldcoeffs(initFieldcoeff.lookup("initFieldcoeffs"));

	forAll(U, celli)
	{
	    scalar x = mesh.C()[celli].component(2);
	    scalar z = mesh.C()[celli].component(1);
	    scalar y = mesh.C()[celli].component(0);

	    bool matched = false;

	    forAll(initFieldcoeffs, i)  // i 表示每个列表元素
	    {
		List<scalar>& e = initFieldcoeffs[i];

		scalar x1 = e[0]; scalar x2 = e[1];
		scalar y1 = e[2]; scalar y2 = e[3];
		scalar z1 = e[4]; scalar z2 = e[5];
		scalar aU = e[6]; scalar bU = e[7];
		scalar aV = e[8]; scalar bV = e[9];
		scalar aT = e[10]; scalar bT = e[11];
		scalar aS = e[12]; scalar bS = e[13];

		if(x > x1 && x <= x2 &&
		   y > y1 && y <= y2 &&
		   z <= z1 && z > z2)
		{
//		    U[celli].component(2) += (aU*z + bU);
//		    U[celli].component(0) += Foam::sqrt(Foam::sqr(aV*z + bV) + Foam::sqr(aU*z + bU));
		    TT[celli]    = (aT*z + bT);
		    S[celli]     = (aS*z + bS);
		    matched = true;
		    break;
		}
	    }

	    if(!matched)
	    {
		//U[celli].x() = 0.0;
		//U[celli].y() = -10.0;
		TT[celli]    = 302.59;
		S[celli]     = 33.80;
	    }
	}



	const word inletPatchName("inlet");
	const label inletPatchID = mesh.boundaryMesh().findPatchID(inletPatchName);

	if (inletPatchID == -1)
	{
	    FatalErrorInFunction
	        << "Cannot find patch: " << inletPatchName << nl
	        << exit(FatalError);
	}

	fvPatchScalarField& TTPatch = TT.boundaryFieldRef()[inletPatchID];
	fvPatchScalarField& SPatch  = S.boundaryFieldRef()[inletPatchID];

	const vectorField& Cf = mesh.boundary()[inletPatchID].Cf();

	scalarField TTInlet(Cf.size(), 302.59);
	scalarField SInlet(Cf.size(), 33.80);

	label nInletMatched = 0;
	label nInletUnmatched = 0;

	const scalar eps = 1e-6;

	forAll(Cf, facei)
	{
	    scalar x = Cf[facei].component(2);  
 	    scalar z = Cf[facei].component(1);  
	    scalar y = Cf[facei].component(0);  

	    bool matched = false;

	    forAll(initFieldcoeffs, i)
	    {
	        const List<scalar>& e = initFieldcoeffs[i];

	        scalar x1 = e[0];  scalar x2 = e[1];
	        scalar y1 = e[2];  scalar y2 = e[3];
	        scalar z1 = e[4];  scalar z2 = e[5];

	        scalar aT = e[10]; scalar bT = e[11];
	        scalar aS = e[12]; scalar bS = e[13];

	       if
	        (
        	    x >= x1 - eps && x <= x2 + eps &&
          	    y >= y1 - eps && y <= y2 + eps &&
	            z <= z1 + eps && z >= z2 - eps
       		)
       		{
            		TTInlet[facei] = aT*z + bT;
            		SInlet[facei]  = aS*z + bS;

            		matched = true;
            		nInletMatched++;
           		 break;
        	}
    	   }

		if (!matched)
    		{
        		TTInlet[facei] = 302.59;
        		SInlet[facei]  = 33.80;
        		nInletUnmatched++;
    		}
	}


	TTPatch == TTInlet;
	SPatch  == SInlet;

	TT.correctBoundaryConditions();
	S.correctBoundaryConditions();

	reduce(nInletMatched, sumOp<label>());
	reduce(nInletUnmatched, sumOp<label>());

	Info<< "TT/S inlet profile set from initFieldcoeff" << nl
	<< endl;

//	U.write();
	TT.write();
	S.write();

	Info<<"set initFields and TT/S inlet done\n"<<endl;
}


