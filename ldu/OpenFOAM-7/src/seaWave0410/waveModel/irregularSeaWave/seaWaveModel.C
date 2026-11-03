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
    scalar u = UMean_.x();
    scalar v = 0.0;
    scalar w = 0.0;

    forAll(irregWaveHeights_, ii)
    {
        forAll(irregWaveHeights_[ii], jj)
        {
            scalar waveKs = mathematical::twoPi/irregWaveLengths_[ii][jj];
            scalar waveOmegas = mathematical::twoPi/irregWavePeriods_[ii][jj];

            scalar phaseTot =
                waveKs*x*cos(irregWaveDirs_[ii][jj])
              + waveKs*z*sin(irregWaveDirs_[ii][jj])
              - waveOmegas*t
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
                scalar waveOmegas =
                    mathematical::twoPi/irregWavePeriods_[ii][jj];

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


void Foam::waveModels::seaWave::setVelocity
(
    const scalar t,
    const scalar tCoeff,
    const scalarField& level
)
{
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

            U_[facei] = fraction*Uf*tCoeff;
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
