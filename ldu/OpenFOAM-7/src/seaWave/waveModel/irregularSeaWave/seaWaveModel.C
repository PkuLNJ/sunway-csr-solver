/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2016-2017 OpenCFD Ltd.
     \\/     M anipulation  | Copyright (C) 2015 IH-Cantabria
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

\*---------------------------------------------------------------------------*/

#include "seaWaveModel.H"
#include "mathematicalConstants.H"
#include "addToRunTimeSelectionTable.H"
#include "polyPatch.H"
#include "fvMesh.H"
#include "volFields.H"
#include "fvPatchFields.H"

using namespace Foam::constant;

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace waveModels
{
    defineTypeNameAndDebug(seaWave, 0);
    addToRunTimeSelectionTable
    (
        waveModel,
        seaWave,
        patch
    );
}
}


// * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * * //
Foam::scalar Foam::waveModels::seaWave::eta
(
    const scalar H,
    const scalar Kx,
    const scalar x,
    const scalar Kz,
    const scalar z,
    const scalar omega,
    const scalar t,
    const scalar phase
) const
{
    scalar phaseTot = Kx*x + Kz*z - omega*t + phase;
    return H*0.5*cos(phaseTot);
}


// * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * * //
Foam::scalar Foam::waveModels::seaWave::waveLength
(
    const scalar h,
    const scalar T
) const
{
    scalar L0 = mag(g_)*T*T/(2.0*mathematical::pi);
    scalar L = L0;

    for (label i=1; i<=100; ++i)
    {
        L = L0*tanh(2.0*mathematical::pi*h/L);
    }

    return L;
}

Foam::vector Foam::waveModels::seaWave::Uf
(
    const scalar h,
    const scalar x,
    const scalar z,
    const scalar t,
    const scalar y
) const
{
    scalar u = 0.0;
    scalar v = 0.0;
    scalar w = 0.0;

    forAll(irregWaveHeights_, ii)
    {
        forAll(irregWaveHeights_[ii], jj)
        {
            scalar waveKs = mathematical::twoPi/irregWaveLengths_[ii][jj];
            scalar waveOmegas = mathematical::twoPi/irregWavePeriods_[ii][jj];
	    scalar totalOmegas = mathematical::twoPi/irregWavePeriods_[ii][jj]
				+  waveKs*(UMean_.x()*cos(irregWaveDirs_[ii][jj]) + UMean_.z()*sin(irregWaveDirs_[ii][jj]));
            scalar phaseTot =
                waveKs*x*cos(irregWaveDirs_[ii][jj])
              + waveKs*z*sin(irregWaveDirs_[ii][jj])
              - totalOmegas*t
              + irregWavePhases_[ii][jj];

            const vector Uf = uMultiDirec
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

void Foam::waveModels::seaWave::setLevel
(
    const scalar t,
    const scalar tCoeff,
    scalarField& level
) const
{
    forAll(level, paddlei)
    {
        scalar eta = 0;

        forAll(irregWaveHeights_, ii)
        {
            forAll(irregWaveHeights_[ii], jj)
            {
                scalar waveKs = mathematical::twoPi/irregWaveLengths_[ii][jj];
                scalar waveOmegas = mathematical::twoPi/irregWavePeriods_[ii][jj]
				    +  waveKs*(UMean_.x()*cos(irregWaveDirs_[ii][jj]) + UMean_.z()*sin(irregWaveDirs_[ii][jj]));
			
                eta +=
                    this->eta
                    (
                        irregWaveHeights_[ii][jj],
                        waveKs*cos(irregWaveDirs_[ii][jj]),
                        xPaddle_[paddlei],
                        waveKs*sin(irregWaveDirs_[ii][jj]),
                        zPaddle_[paddlei],
                        waveOmegas,
                        t,
                        irregWavePhases_[ii][jj]
                    );
            }
        }

        level[paddlei] = waterDepthRef_ + tCoeff*eta;
    }
}

Foam::vector Foam::waveModels::seaWave::uMultiDirec
(
    const scalar irregH,
    const scalar irregWaveOmega,
    const scalar pha,
    const scalar irregWaveKs,
    const scalar yy,
    const scalar hh,
    const scalar irregDir
) const
{
    const scalar ksh = irregWaveKs*hh;
    const scalar ksy = irregWaveKs*yy;

    scalar u =
        irregH*0.5*irregWaveOmega*cos(pha)*(cosh(ksy)/sinh(ksh))*cos(irregDir);

    scalar w =
        irregH*0.5*irregWaveOmega*cos(pha)*(cosh(ksy)/sinh(ksh))*sin(irregDir);

    scalar v =
        irregH*0.5*irregWaveOmega*sin(pha)*(sinh(ksy)/sinh(ksh));

    return vector(u, v, w);
}

/*
void Foam::waveModels::seaWave::UOcean()
{
  
 const vectorField& Cf = patch_.faceCentres();
  
 forAll(Cf, facei)
 {
     const vector c(Cf[facei]);
     scalar x = c.x() + 1.0;
     scalar y = c.y();
     scalar z = c.z();
 
   forAll(UOceanCoeffs_, i) 
   {
     List<scalar>& e = UOceanCoeffs_[i];
 
     scalar x1 = e[0]; scalar x2 = e[1];
     scalar y1 = e[2]; scalar y2 = e[3];
     scalar z1 = e[4]; scalar z2 = e[5];
     scalar aU = e[6]; scalar bU = e[7];
     scalar aV = e[8]; scalar bV = e[9];
	 if(z > x1 && z <= x2 &&
	    x > y1 && x <= y2 &&
	    y <= z1 && y > z2)
 	{
	    UOcean_[facei].x() = sqrt(sqr(aV*y + bV) + sqr(aU*y + bU));
	    break;
 	}
   }

 }
 Info<< "Finishing the Ocean U setting" << nl;

}
*/

/*
void Foam::waveModels::seaWave::UOcean
(
) 
{
    const volVectorField& U = mesh_.lookupObject<volVectorField>(UName_);
    UOcean_ = U.boundaryField()[patch_.index()].patchInternalField();
}
*/    

void Foam::waveModels::seaWave::setVelocity
(
    const scalar t,
    const scalar tCoeff,
    const scalarField& level
)
{
/*
    if(UOceanFlag_)
    {
         Info<< "Starting the Ocean U setting" << nl;
   	 UOcean();
	 UOceanFlag_ = 0;
    }
*/
    forAll(U_, facei)
    {
	// Fraction of geometry represented by paddle - to be set
        scalar fraction = 1;

        // Height - to be set
        scalar y = 0;

        setPaddlePropeties(level, facei, fraction, y);

        if (fraction > 0)
        {
            const label paddlei = faceToPaddle_[facei];

            const vector Uf = this->Uf
            (
                waterDepthRef_,
                xPaddle_[paddlei],
                zPaddle_[paddlei],
                t,
                y
            );
		U_[facei] = fraction*Uf*tCoeff + UMean_;
//            U_[facei] = fraction*Uf*tCoeff + UOcean_[facei];
        }

	else
	{
	    U_[facei] = UMean_;
	}

    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::waveModels::seaWave::seaWave
(
    const dictionary& dict,
    const fvMesh& mesh,
    const polyPatch& patch,
    const bool readFields
)
:
    irregularWaveModel(dict, mesh, patch, false)
//    UOcean_(patch.size(), vector::zero)
{
    if (readFields)
    {
        readDict(dict);
    }
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::waveModels::seaWave::~seaWave()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::waveModels::seaWave::readDict
(
    const dictionary& overrideDict
)
{
    if (irregularWaveModel::readDict(overrideDict))
    {
        lookup("wavePeriods") >> irregWavePeriods_;
        lookup("waveHeights") >> irregWaveHeights_;
        lookup("wavePhases") >> irregWavePhases_;
        lookup("waveDirs") >> irregWaveDirs_;
        lookup("UMean") >> UMean_;
//	lookup("UOceanCoeffs") >> UOceanCoeffs_;
//      lookup("UOceanFlag") >> UOceanFlag_;

        irregWaveLengths_ = irregWaveHeights_;

        forAll(irregWaveHeights_, ii)
        {
            forAll(irregWaveHeights_[ii], jj)
            {
                irregWaveLengths_[ii][jj] =
                    waveLength(waterDepthRef_, irregWavePeriods_[ii][jj]);
                irregWaveDirs_[ii][jj] =
                    irregWaveDirs_[ii][jj]*mathematical::pi/180;
            }
        }

        return true;
    }

    return false;
}


void Foam::waveModels::seaWave::info(Ostream& os) const
{
    irregularWaveModel::info(os);

    os  << "    Wave periods    : " << irregWavePeriods_.size() << nl
        << "    Wave heights    : " << irregWaveHeights_.size() << nl
        << "    Wave phases     : " << irregWavePhases_.size() << nl
        << "    Wave lengths    : " << irregWaveLengths_.size() << nl
        << "    Wave directions : " << irregWaveDirs_.size() << nl;
}


// ************************************************************************* //
