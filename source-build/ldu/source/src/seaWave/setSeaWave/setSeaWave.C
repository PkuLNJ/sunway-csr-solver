/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2017-2018 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    setWaves

Description
    Applies wave models to the entire domain for case initialisation uFoam::sing
    level sets for second-order accuracy.

\*---------------------------------------------------------------------------*/
#include "argList.H"
#include "levelSet.H"
#include "pointFields.H"
#include "timeSelector.H"
#include "uniformDimensionedFields.H"
#include "volFields.H"
#include "wallPolyPatch.H"
#include "waveAlphaFvPatchScalarField.H"
#include "waveVelocityFvPatchVectorField.H"
#include "mathematicalConstants.H"
#include "mathematicalConstants.H"
#include "slipFvPatchFields.H"
#include "fvCFD.H"

using namespace Foam;

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
     // Get the wave
    //- Wave periods for seaWave case [seconds]
    List<List<scalar>> irregWavePeriods_;

    //- Wave heights for seaWave case [metres]
    List<List<scalar>> irregWaveHeights_;

    //- Wave lengths for seaWave case [metres]
    List<List<scalar>> irregWaveLengths_;

    //- Wave phases for seaWave case [radians]
    List<List<scalar>> irregWavePhases_;

    vector	UMean_;

    //- Direction of propagation for seaWave case
    //- [degrees] from X axis
    List<List<scalar>> irregWaveDirs_;
    //water depth
    scalar    waterDepth_;
    //sea surface y
    scalar    seaSurface_;    

//******************************************************************************* //

    Foam::scalar  waveLength
    (
      const scalar h,           
      const scalar T,      
      const scalar gMag
    )
    {
    
      scalar L0 = gMag*T*T/(2.0*constant::mathematical::pi);
      scalar L = L0;

      for (label i=1; i<=100; ++i)
      {
          L = L0*Foam::tanh(2.0*constant::mathematical::pi*h/L);
      }

      return L;
   }


    void readSeaWaveDict
    (
        const dictionary& dict
    )
    {

        dict.lookup("waterDepth") >> waterDepth_;
        dict.lookup("seaSurface") >> seaSurface_;

    }

    void readWaveProperties
    (
        const dictionary& dict,
	const scalar gMag
    )
    {

        dict.lookup("wavePeriods") >> irregWavePeriods_;
        dict.lookup("waveHeights") >> irregWaveHeights_;
        dict.lookup("wavePhases") >> irregWavePhases_;
        dict.lookup("waveDirs") >> irregWaveDirs_;
        dict.lookup("UMean") >> UMean_;

        irregWaveLengths_ = irregWaveHeights_;

        forAll(irregWaveHeights_, ii)
        {
            forAll(irregWaveHeights_[ii], jj)
            {
                irregWaveLengths_[ii][jj] =
                    waveLength(waterDepth_, irregWavePeriods_[ii][jj],gMag);
                irregWaveDirs_[ii][jj] =
                    irregWaveDirs_[ii][jj]*constant::mathematical::pi/180;
            }
        }

    }


     Foam::scalar calcEta
     (
            const scalar H,
            const scalar Kx,
            const scalar x,
            const scalar Kz,
            const scalar z,
            const scalar omega,
            const scalar t,
            const scalar phase
     )
     {
         scalar phaseTot = Kx*x + Kz*z - omega*t + phase;
         return H*0.5*Foam::cos(phaseTot);
     }


    void calcTotalLevel
    (
        const scalar t,
        scalar& level,
        const scalar x,
        const scalar z
    )
    {

        scalar eta = 0;

        forAll(irregWaveHeights_, ii)
        {
            forAll(irregWaveHeights_[ii], jj)
            {
                scalar waveKs = constant::mathematical::twoPi/irregWaveLengths_[ii][jj];
                scalar waveOmegas =
                    constant::mathematical::twoPi/irregWavePeriods_[ii][jj];

                eta +=
                    calcEta
                    (
                        irregWaveHeights_[ii][jj],
                        waveKs*Foam::cos(irregWaveDirs_[ii][jj]),
                        x,
                        waveKs*Foam::sin(irregWaveDirs_[ii][jj]),
                        z,
                        waveOmegas,
                        t,
                        irregWavePhases_[ii][jj]
                    );
            }
        }

        level = seaSurface_ + eta;

    }

	Foam::vector calcU
	(
	    const scalar irregH,
	    const scalar irregWaveOmega,
	    const scalar pha,
	    const scalar irregWaveKs,
	    const scalar yy,
	    const scalar hh,
	    const scalar irregDir
	) 
	{
	    const scalar ksh = irregWaveKs*hh;
	    const scalar ksy = irregWaveKs*(yy+hh);

	    scalar u = irregH*0.5*irregWaveOmega*Foam::cos(pha)*(Foam::cosh(ksy)/Foam::sinh(ksh))*Foam::cos(irregDir);

	    scalar w = irregH*0.5*irregWaveOmega*Foam::cos(pha)*(Foam::cosh(ksy)/Foam::sinh(ksh))*Foam::sin(irregDir);

	    scalar v = irregH*0.5*irregWaveOmega*Foam::sin(pha)*(Foam::sinh(ksy)/Foam::sinh(ksh));

	    return vector(u, v, w);
	}


	Foam::vector calcTotalU
	(
	    const scalar h,
	    const scalar x,
	    const scalar z,
	    const scalar t,
	    const scalar y
	) 
	{
	    scalar u = UMean_.x();
	    scalar v = 0.0;
	    scalar w = 0.0;

	    forAll(irregWaveHeights_, ii)
            {
        	forAll(irregWaveHeights_[ii], jj)
        	{
         	   scalar waveKs = constant::mathematical::twoPi/irregWaveLengths_[ii][jj];
           	   scalar waveOmegas = constant::mathematical::twoPi/irregWavePeriods_[ii][jj];

           	 scalar phaseTot =
                	waveKs*x*Foam::cos(irregWaveDirs_[ii][jj])
              		+ waveKs*z*Foam::sin(irregWaveDirs_[ii][jj])
              		- waveOmegas*t
              		+ irregWavePhases_[ii][jj];

            	 const vector Uf = calcU
            	 (
                	irregWaveHeights_[ii][jj],
                	waveOmegas,
                	phaseTot,
                	waveKs,
                	y,
                	h,
                	irregWaveDirs_[ii][jj]
           	 );

   	         u += Uf[0];
        	 v += Uf[1];
            	 w += Uf[2];
        	}
 	   }

	    return vector(u, v, w);
	}


//********************************************************** //
int main(int argc, char *argv[])
{
    timeSelector::addOptions(false, false);

    #include "addDictOption.H"
    #include "addRegionOption.H"

    argList::addOption
    (
        "alpha",
        "name",
        "name of the volume fraction field, default is \"alpha\""
    );

    argList::addOption
    (
        "U",
        "name",
        "name of the velocity field, default is \"U\""
    );

    argList::addBoolOption
    (
        "gas",
        "the volume fraction field is that that of the gas above the wave"
    );

    #include "setRootCase.H"
    #include "createTime.H"

    instantList timeDirs = timeSelector::selectIfPresent(runTime, args);

    #include "createNamedMesh.H"
    #include "readGravitationalAcceleration.H"

    const word dictName("setSeaWaveDict");
    #include "setSystemMeshDictionaryIO.H"
    Info<< "Reading " << dictName << "\n" << endl;
    IOdictionary setSeaWaveDict(dictIO);
    readSeaWaveDict(setSeaWaveDict);

    IOdictionary waveProperties
    (
    	IOobject
        (
       	     "waveProperties",
              runTime.constant(),
              mesh,
              IOobject::MUST_READ_IF_MODIFIED,
              IOobject::NO_WRITE
        )
    );

   const dictionary* wavePropertiesDictPtr = nullptr;
   word findSubDictName = "none";


   for (const auto& entry : waveProperties)
   {
    if (entry.isDict())
    {
        const dictionary& subDict = entry.dict();
        
        if (subDict.found("wavePeriods") || subDict.found("waveHeights"))
        {
            wavePropertiesDictPtr = &subDict;
            findSubDictName = entry.keyword();
            break;      
      	}
    }
  }

    const dictionary& waveProperties_subDict = *wavePropertiesDictPtr;
    readWaveProperties(waveProperties_subDict, mag(g).value());

    // Parse options
    const word alphaName = setSeaWaveDict.lookupOrDefault<word>("alpha", "alpha");
    const word UName = setSeaWaveDict.lookupOrDefault<word>("U", "U");
    const bool liquid = setSeaWaveDict.lookupOrDefault<bool>("liquid", true);


    forAll(timeDirs, timeI)
    {
        runTime.setTime(timeDirs[timeI], timeI);
        const scalar t = runTime.value();

        Info<< "Time = " << runTime.timeName() << nl << endl;

        mesh.readUpdate();

        // Read the fields which are to be set
        volScalarField alpha
        (
            IOobject
            (
                alphaName,
                runTime.timeName(),
                mesh,
                IOobject::MUST_READ
            ),
            mesh
        );
        
	volVectorField U
        (
            IOobject
            (
                UName,
                runTime.timeName(),
                mesh,
                IOobject::MUST_READ
            ),
            mesh
        );



       scalar  seaLevel = 0;

       forAll(alpha, celli)
       {

	       const labelList& pointIndex = mesh.cellPoints()[celli];
	       const label nPoint = pointIndex.size();
	       scalar yCoordMin = mesh.points()[pointIndex[0]].y();
	       scalar yCoordMax = mesh.points()[pointIndex[0]].y(); 	

	       for (label pi = 1; pi < nPoint; ++pi)
       	       {
            	    const label pointi = pointIndex[pi];
          	    yCoordMin = min(yCoordMin, mesh.points()[pointi].y());
           	    yCoordMax = max(yCoordMax, mesh.points()[pointi].y());
       	       }	

      	  	const vector& C = mesh.C()[celli];  
        	scalar x = C.x();
       		scalar z = C.z();
        	scalar y = C.y();

		calcTotalLevel(t,seaLevel,x,z);         
    
     	         if (yCoordMax < seaLevel)
       		 {
      		      alpha[celli] = 1.0;
       		 }

       		 else if (yCoordMin > seaLevel)
        	 {
            	      alpha[celli] = 0.0;
        	 }
        	 else
        	 {
            	      scalar dy = seaLevel - yCoordMin;
         	      alpha[celli] = dy/(yCoordMax - yCoordMin);
        	 }
        	  

	  if (y <= 0)
          {
           const vector Uf = calcTotalU
           (
                waterDepth_,
                x,
                z,
                t,
                y
            );

            U[celli] = alpha[celli]*Uf;
          }

	  else
	  {
	    U[celli] = UMean_;
	  } 


       	}


        auto& alphaBf = alpha.boundaryFieldRef();
	auto& UBf = U.boundaryFieldRef();
       
        forAll(alphaBf, patchi)
        {
          
	    fvPatchScalarField& alphaPatch = alphaBf[patchi];
            const fvPatch& patch = mesh.boundary()[patchi];

	    fvPatchVectorField& UPatch = UBf[patchi];
           
            if (patch.size() == 0) continue;

            const vectorField& Cf = patch.Cf();
            vectorField patchU(patch.size(), vector::zero);

            scalarField patchAlpha(patch.size(), 0.0);

	    const faceList& faces = patch.patch().localFaces();

	    const vectorField& Cp = patch.patch().localPoints();

            forAll(Cf, facei)
            {
        
	        const face& f = faces[facei];
        	const label nPoint = f.size();
                scalar yMin_facei = Cp[f[0]].y();
                scalar yMax_facei = Cp[f[0]].y();

	        for (label fpi = 1; fpi < nPoint; ++fpi)
        	{
          	  const label pointi = f[fpi];
         	  yMin_facei = min(yMin_facei, Cp[pointi].y());
            	  yMax_facei = max(yMax_facei, Cp[pointi].y());
        	}

		const vector& C_f = Cf[facei];
                scalar x = C_f.x();
                scalar z = C_f.z();
		scalar y = C_f.y();
		scalar seaLevel = 0;
                calcTotalLevel(t, seaLevel, x, z);

		if (yMax_facei < seaLevel)
        	{
           	   patchAlpha[facei] = 1.0;

        	}
       		else if (yMin_facei > seaLevel)
       		{
            	    patchAlpha[facei] = 0.0;
        	}
        	else
        	{
           	   scalar dy = seaLevel - yMin_facei;
           	   patchAlpha[facei] = dy/(yMax_facei - yMin_facei);
        	}


		if (y <= 0)
          	{
        	   const vector Uf = calcTotalU
          	   (
               		 waterDepth_,
                	 x,
                	 z,
           		 t,
               		 y
            	   );


     	            patchU[facei] = patchAlpha[facei]*Uf;
                }

		else
		{
		   patchU[facei] = UMean_;
		}

           }


            alphaPatch == patchAlpha; 

	    const word& patchName = mesh.boundary()[patchi].name();
	    if (patchName != "suboff" && patchName != "submarine")
	    {
	         UPatch == patchU;
	    }

//            if (!isA<fixedValueFvPatchVectorField>(UPatch) &&
//                !isA<slipFvPatchVectorField>(UPatch))
//            {
//                UPatch == patchU;
//            }

        }
        
//********************************************************************* //


       // Output
        Info<< "Writing " << alpha.name() << " and " << U.name() << nl;
        alpha.write();
	U.write();
    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //
