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
#include "mathematicalConstants.H"
#include "mathematicalConstants.H"
#include "slipFvPatchFields.H"
#include "fvCFD.H"

#include <cstdlib>

extern "C"
{
    int MPI_Finalized(int*);
    int MPI_Buffer_detach(void*, int*);
    int MPI_Finalize();
}

using namespace Foam;

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace
{

struct SeaWaveInitData
{
    List<List<scalar>> irregWavePeriods;
    List<List<scalar>> irregWaveHeights;
    List<List<scalar>> irregWaveLengths;
    List<List<scalar>> irregWavePhases;
    List<List<scalar>> irregWaveDirs;
    List<List<scalar>> UOceanCoeffs;

    vector UMean;
    vector UWind;
    bool UOceanFlag;
    scalar waterDepth;
    scalar seaSurface;

    SeaWaveInitData()
    :
        irregWavePeriods(),
        irregWaveHeights(),
        irregWaveLengths(),
        irregWavePhases(),
        irregWaveDirs(),
        UOceanCoeffs(),
        UMean(vector::zero),
        UOceanFlag(false),
        waterDepth(0),
        seaSurface(0)
    {}
};


void finaliseParallelWithoutBufferDelete()
{
    int finalised = 0;
    MPI_Finalized(&finalised);

    if (!finalised)
    {
        #ifndef SGIMPI
        int size = 0;
        char* buff = nullptr;
        MPI_Buffer_detach(&buff, &size);
        #endif

        MPI_Finalize();
    }

    std::_Exit(0);
}

}

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
        const dictionary& dict,
        SeaWaveInitData& data
    )
    {

        dict.lookup("waterDepth") >> data.waterDepth;
        dict.lookup("seaSurface") >> data.seaSurface;
//	dict.lookup("Absorptionx") >> Absorptionx_;
    }

    void readWaveProperties
    (
        const dictionary& dict,
        const scalar gMag,
        const fvMesh& mesh,
        SeaWaveInitData& data
    )
    {

        dict.lookup("wavePeriods") >> data.irregWavePeriods;
        dict.lookup("waveHeights") >> data.irregWaveHeights;
        dict.lookup("wavePhases") >> data.irregWavePhases;
        dict.lookup("waveDirs") >> data.irregWaveDirs;
        dict.lookup("UMean") >> data.UMean;
        
        dict.lookup("UWind") >> data.UWind;
        data.UOceanFlag = dict.lookupOrDefault<bool>("UOceanFlag", false);

        data.UOceanCoeffs.clear();

        if (data.UOceanFlag)
        {
            if (dict.found("UOceanCoeffs"))
            {
                dict.lookup("UOceanCoeffs") >> data.UOceanCoeffs;
                Info<< "UOcean is adding" << nl;
            }
            else
            {
                const fileName coeffFile =
                    mesh.time().path()/mesh.time().constant()/"initFieldcoeff";

                IFstream coeffStream(coeffFile);

                if (!coeffStream.good())
                {
                    FatalErrorInFunction
                        << "Cannot read UOcean coefficients from "
                        << coeffFile << exit(FatalError);
                }

                dictionary coeffDict(coeffStream);
                coeffDict.lookup("initFieldcoeffs") >> data.UOceanCoeffs;
            }

        }
		       
//        dict.lookup("UOceanCoeffs") >> UOceanCoeffs_;

        data.irregWaveLengths = data.irregWaveHeights;

        forAll(data.irregWaveHeights, ii)
        {
            forAll(data.irregWaveHeights[ii], jj)
            {
                data.irregWaveLengths[ii][jj] =
                    waveLength(data.waterDepth, data.irregWavePeriods[ii][jj],gMag);
                data.irregWaveDirs[ii][jj] =
                    data.irregWaveDirs[ii][jj]*constant::mathematical::pi/180;
            }
        }

    }


    Foam::vector calcUOcean
    (
        const vector& C,
        const SeaWaveInitData& data
    )
	{
    		if (!data.UOceanFlag)
    		{
        		return vector::zero;
    		}
    		
        	
    		scalar z = C.z();
    		scalar x = C.x() + SMALL;
    		scalar y = C.y();

    		forAll(data.UOceanCoeffs, i)
    		{
        		const List<scalar>& e = data.UOceanCoeffs[i];

        		scalar z1 = e[0]; scalar z2 = e[1];
        		scalar x1 = e[2]; scalar x2 = e[3];
        		scalar y1 = e[4]; scalar y2 = e[5];

        		scalar aU = e[6]; scalar bU = e[7];
        		scalar aV = e[8]; scalar bV = e[9];

        		if
        		(
            			x > x1 && x <= x2 &&
            			z > z1 && z <= z2 &&
            			y <= y1 && y > y2
        		)
        		{
            			scalar UOceanMag =
                			Foam::sqrt(Foam::sqr(aV*y + bV) + Foam::sqr(aU*y + bU));

            			return vector(UOceanMag, 0, 0);
        		}
    		}

   		 return vector::zero;
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
        const scalar z,
        const vector& USea,
        const SeaWaveInitData& data
    )
    {

        scalar eta = 0;

	const vector Ubg = data.UMean + USea;


        forAll(data.irregWaveHeights, ii)
        {
            forAll(data.irregWaveHeights[ii], jj)
            {
                scalar waveKs = constant::mathematical::twoPi/data.irregWaveLengths[ii][jj];
		scalar waveOmegas =
    		constant::mathematical::twoPi/data.irregWavePeriods[ii][jj]
  		+ waveKs
   		*(
        		Ubg.x()*Foam::cos(data.irregWaveDirs[ii][jj])
      			+ Ubg.z()*Foam::sin(data.irregWaveDirs[ii][jj])
   	 	);

                eta +=
                    calcEta
                    (
                        data.irregWaveHeights[ii][jj],
                        waveKs*Foam::cos(data.irregWaveDirs[ii][jj]),
                        x,
                        waveKs*Foam::sin(data.irregWaveDirs[ii][jj]),
                        z,
                        waveOmegas,
                        t,
                        data.irregWavePhases[ii][jj]
                    );
            }
        }

        level = data.seaSurface + eta;

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
	    const scalar ksy = irregWaveKs*yy;

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
		    const scalar y,
		    const vector& USea,
            	    const SeaWaveInitData& data
		) 
	{
	    scalar u = 0.0;
	    scalar v = 0.0;
	    scalar w = 0.0;

		    const vector Ubg = data.UMean + USea;

	    forAll(data.irregWaveHeights, ii)
	    {
	        forAll(data.irregWaveHeights[ii], jj)
	        {
	         	   scalar waveKs = constant::mathematical::twoPi/data.irregWaveLengths[ii][jj];
           	   	   scalar waveOmegas = constant::mathematical::twoPi/data.irregWavePeriods[ii][jj];

			   scalar totalOmegas =
	 		   constant::mathematical::twoPi/data.irregWavePeriods[ii][jj]
	 		   + waveKs
	 		  *(
	       			 Ubg.x()*Foam::cos(data.irregWaveDirs[ii][jj])
	      			 + Ubg.z()*Foam::sin(data.irregWaveDirs[ii][jj])
	    		   );

           	 scalar phaseTot =
                	waveKs*x*Foam::cos(data.irregWaveDirs[ii][jj])
              		+ waveKs*z*Foam::sin(data.irregWaveDirs[ii][jj])
              		- totalOmegas*t
              		+ data.irregWavePhases[ii][jj];

            	 const vector Uf = calcU
            	 (
                	data.irregWaveHeights[ii][jj],
                	waveOmegas,
                	phaseTot,
                	waveKs,
                	y,
                	h,
                	data.irregWaveDirs[ii][jj]
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
	    
     	    SeaWaveInitData waveData;
	    
	    readSeaWaveDict(setSeaWaveDict, waveData);

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
	    readWaveProperties(waveProperties_subDict, mag(g).value(), mesh, waveData);

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

//      if(!UMean_.x())
//      {
       forAll(alpha, celli)
       {

	       const labelList& pointIndex = mesh.cellPoints()[celli];
	       label nPoint = pointIndex.size();
	       scalar yCoordMin = mesh.points()[pointIndex[0]].y();
	       scalar yCoordMax = mesh.points()[pointIndex[0]].y(); 	

	       for (label pi = 1; pi < nPoint; ++pi)
       	       {
            	    label pointi = pointIndex[pi];
          	    yCoordMin = min(yCoordMin, mesh.points()[pointi].y());
           	    yCoordMax = max(yCoordMax, mesh.points()[pointi].y());
       	       }	

      	  	const vector& C = mesh.C()[celli];  
        	scalar x = C.x();
       		scalar z = C.z();
        	scalar y = C.y();
			vector USea = calcUOcean(C, waveData);
			vector USeaLevel = calcUOcean(vector(x, waveData.seaSurface, z), waveData);

			calcTotalLevel(t, seaLevel, x, z, USeaLevel, waveData);

			scalar Y = min(y + waveData.waterDepth, waveData.waterDepth);        
	    
	                 vector Uf = calcTotalU
	          	 (
	                	waveData.waterDepth,
	                	x,
	               		z,
	                	t,
	                	Y,
	                	USeaLevel,
                        	waveData
	            	);

     	         if (yCoordMax < seaLevel)
       		 {
      		      alpha[celli] = 1.0;
			      U[celli] = alpha[celli]*Uf + waveData.UMean + USea;
       		 }

       		 else if (yCoordMin > seaLevel)
        	 {
            	      alpha[celli] = 0.0;
			      U[celli] = waveData.UMean + waveData.UWind;
        	 }
        	 else
        	 {
            	      scalar dy = seaLevel - yCoordMin;
         	      alpha[celli] = dy/(yCoordMax - yCoordMin);
			      U[celli] = alpha[celli]*Uf + waveData.UMean + USea;
        	 }
        	  
/*
	  if (y <= seaLevel)
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

*/

/*
          if (y <= 0 && x > Absorptionx_)
          {
           const vector Uf = calcTotalU
           (
                waterDepth_,
                x,
                z,
                t,
                y
            );

            U[celli] = UMean_*alpha[celli];
          }
*/

/*
	  else
	  {
	    U[celli] = UMean_;
	  } 
*/

       	}


        auto& alphaBf = alpha.boundaryFieldRef();
	auto& UBf = U.boundaryFieldRef();
       
        forAll(alphaBf, patchi)
        {
          
	    fvPatchScalarField& alphaPatch = alphaBf[patchi];
            const fvPatch& patch = mesh.boundary()[patchi];

	    fvPatchVectorField& UPatch = UBf[patchi];
	    
	    const polyPatch& pp = mesh.boundaryMesh()[patchi];

	    if (pp.coupled())
	    {
   		 continue;
	    }

	    if
	    (
  		  isA<wallPolyPatch>(pp)
	    )
	    {
 		   continue;
	    }
           
            if (patch.size() == 0) continue;

            const vectorField& Cf = patch.Cf();
            vectorField patchU(patch.size(), vector::zero);

            scalarField patchAlpha(patch.size(), 0.0);

	    const faceList& faces = patch.patch().localFaces();

	    const vectorField& Cp = patch.patch().localPoints();

            forAll(Cf, facei)
            {
        
	        const face& f = faces[facei];
        	label nPoint = f.size();
                scalar yMin_facei = Cp[f[0]].y();
                scalar yMax_facei = Cp[f[0]].y();

	        for (label fpi = 1; fpi < nPoint; ++fpi)
        	{
          	  label pointi = f[fpi];
         	  yMin_facei = min(yMin_facei, Cp[pointi].y());
            	  yMax_facei = max(yMax_facei, Cp[pointi].y());
        	}

		const vector& C_f = Cf[facei];
                scalar x = C_f.x();
                scalar z = C_f.z();
		scalar y = C_f.y();
		
			vector USea = calcUOcean(C_f, waveData);
			vector USeaLevel = calcUOcean(vector(x, waveData.seaSurface, z), waveData);

			scalar Y = min(y + waveData.waterDepth, waveData.waterDepth);
			scalar seaLevel = 0;
			calcTotalLevel(t, seaLevel, x, z, USeaLevel, waveData);

	                   vector Uf = calcTotalU
	                   (
	                         waveData.waterDepth,
	                         x,
	                         z,
	                         t,
	                         Y,
	                         USeaLevel,
                            	 waveData
	                   );

		if (yMax_facei < seaLevel)
        	{
           	   patchAlpha[facei] = 1.0;
                   patchU[facei] = patchAlpha[facei]*Uf + waveData.UMean + USea;

        	}
       		else if (yMin_facei > seaLevel)
       		{
            	    patchAlpha[facei] = 0.0;
			    patchU[facei] = waveData.UMean + waveData.UWind;
        	}
        	else
        	{
           	   scalar dy = seaLevel - yMin_facei;
           	   patchAlpha[facei] = dy/(yMax_facei - yMin_facei);
        	   patchU[facei] = patchAlpha[facei]*Uf + waveData.UMean + USea;
		}

/*
		if (y <= seaSurface_)
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
*/

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
//      }
//********************************************************************************************************
/*     if(UMean_.x() > 0)
      {
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
    
		if(x < lambdax_)
		{
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
                }

		else
		{
		 if (yCoordMax < seaSurface_)
                 {
                      alpha[celli] = 1.0;
                 }
                 else if (yCoordMin > seaSurface_)
                 {
                      alpha[celli] = 0.0;
                 }
                 else
                 {
                      scalar dy = seaSurface_ - yCoordMin;
                      alpha[celli] = dy/(yCoordMax - yCoordMin);
                 }
		
		}

          if (y <= 0 && x < lambdax_)
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
	
       }
*/
//********************************************************************* //

/*
	surfaceScalarField phi
	(
 	   IOobject
   	   (
       		 "phi",
       		  runTime.timeName(),
        	  mesh,
        	  IOobject::NO_READ,
        	  IOobject::NO_WRITE
    	   ),
    	  fvc::flux(U)
	);

        alpha.correctBoundaryConditions();
	U.correctBoundaryConditions();
*/	
	
        Info<< "Writing " << alpha.name() << " and " << U.name() << nl;
        alpha.write();
        U.write();
        
    }

    Info<< "End\n" << endl;

    if (Pstream::parRun())
    {
        finaliseParallelWithoutBufferDelete();
    }

    return 0;
}


// ************************************************************************* //
