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

#include "waveModel.H"
#include "fvMesh.H"
#include "polyPatch.H"
#include "uniformDimensionedFields.H"
#include "volFields.H"
#include "fvPatchFields.H"
#include "processorPolyPatch.H"

using namespace Foam::constant;

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(waveModel, 0);
    defineRunTimeSelectionTable(waveModel, patch);
}

const Foam::word Foam::waveModel::dictName("waveProperties");


Foam::word Foam::waveModel::modelName(const word& patchName)
{
    return dictName + '.' + patchName;
}


// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

void Foam::waveModel::initialiseGeometry()
{
    vector x(-gAverage(patch_.faceAreas()));
    x /= mag(x) + ROOTVSMALL;
    vector y = -g_/mag(g_);
    vector z = x^y;

    // Rotation from global<->local about global origin
    Rlg_ = tensor(x, y, z);
    Rgl_ = Rlg_.T();

    // Global patch extents
    const vectorField& Cp = patch_.localPoints();
    const vectorField CpLocal(Rgl_ & Cp);
    boundBox bb(CpLocal, true);
    const scalar xMin = bb.min().x();
    const scalar xMax = bb.max().x();
    const scalar zMin = bb.min().z();
    const scalar zMax = bb.max().z();
    ySpan_ = bb.max().y() - bb.min().y();

    // Global x, z positions of the paddle centres
    xPaddle_.setSize(nPaddle_, 0);
    zPaddle_.setSize(nPaddle_, 0);
    const scalar xMid = xMin + 0.5*(xMax - xMin);
    const scalar paddleDz = (zMax - zMin)/scalar(nPaddle_);
    for (label paddlei = 0; paddlei < nPaddle_; ++paddlei)
    {
        xPaddle_[paddlei] = xMid;
        zPaddle_[paddlei] = paddlei*paddleDz + zMin + 0.5*paddleDz;;
    }

    // Local face centres
    const vectorField& Cf = patch_.faceCentres();
    const vectorField CfLocal(Rgl_ & Cf);
    y_ = CfLocal.component(1);

    // Local face extents in z-direction
    yMin_.setSize(patch_.size());
    yMax_.setSize(patch_.size());
    const faceList& faces = patch_.localFaces();
    forAll(faces, facei)
    {
        const face& f = faces[facei];
        const label nPoint = f.size();
        yMin_[facei] = CpLocal[f[0]].y();
        yMax_[facei] = CpLocal[f[0]].y();

        for (label fpi = 1; fpi < nPoint; ++fpi)
        {
            const label pointi = f[fpi];
            yMin_[facei] = min(yMin_[facei], CpLocal[pointi].y());
            yMax_[facei] = max(yMax_[facei], CpLocal[pointi].y());
        }
    }

    // Set minimum y reference level
    yMin0_ = gMin(yMin_);

    faceToPaddle_.setSize(patch_.size(), -1);
    forAll(faceToPaddle_, facei)
    {
        faceToPaddle_[facei] = floor((CfLocal[facei].z() - zMin)/paddleDz);
    }
}


void Foam::waveModel::refreshGeometry()
{
    const label oldSize = U_.size();
    U_.setSize(patch_.size());
    alpha_.setSize(patch_.size());
    U_ = vector::zero;
    alpha_ = 0;
    initialiseGeometry();
    currTimeIndex_ = -1;
    if (getenv("LNJ_WAVE_GEOMETRY_TRACE"))
        Pout<< "WAVE_GEOMETRY_REFRESH patch=" << patch_.name()
            << " old=" << oldSize << " new=" << patch_.size() << endl;
}

void Foam::waveModel::refreshMappedGeometry(const fvMesh& mesh)
{
    // Physical patches occur in the same order even on ranks with zero faces.
    forAll(mesh.boundaryMesh(), patchi)
    {
        const polyPatch& patch = mesh.boundaryMesh()[patchi];
        if (isA<processorPolyPatch>(patch)) continue;
        const word name = modelName(patch.name());
        const label present = mesh.foundObject<waveModel>(name) ? 1 : 0;
        const label count = returnReduce(present, sumOp<label>());
        if (!count) continue;
        if (count != Pstream::nProcs())
            FatalErrorInFunction << "Inconsistent wave model registration for "
                << name << ": " << count << " ranks" << abort(FatalError);
        const_cast<waveModel&>(mesh.lookupObject<waveModel>(name)).refreshGeometry();
    }
}

Foam::tmp<Foam::scalarField> Foam::waveModel::waterLevel() const
{
    // Note: initialising as initial depth
    tmp<scalarField> tlevel(new scalarField(nPaddle_, initialDepth_));
    scalarField& level = tlevel.ref();

    const volScalarField& alpha =
        mesh_.lookupObject<volScalarField>(alphaName_);
    const fvPatchScalarField& alphap = alpha.boundaryField()[patch_.index()];
    const scalarField alphac(alphap.patchInternalField());

    if (faceToPaddle_.size() != alphac.size())
        FatalErrorInFunction << "Stale wave geometry on patch " << patch_.name()
            << ": cached=" << faceToPaddle_.size() << " current=" << alphac.size()
            << abort(FatalError);
    const scalarField& magSf = alphap.patch().magSf();
    scalarList paddleMagSf(nPaddle_, 0.0);
    scalarList paddleWettedMagSf(nPaddle_, 0.0);

    forAll(alphac, facei)
    {
        label paddlei = faceToPaddle_[facei];
        if (paddlei < 0 || paddlei >= nPaddle_)
            FatalErrorInFunction << "Invalid paddle " << paddlei << " face "
                << facei << " patch " << patch_.name() << abort(FatalError);
        paddleMagSf[paddlei] += magSf[facei];
        paddleWettedMagSf[paddlei] += magSf[facei]*alphac[facei];
    }

    forAll(paddleMagSf, paddlei)
    {
        reduce(paddleMagSf[paddlei], sumOp<scalar>());
        reduce(paddleWettedMagSf[paddlei], sumOp<scalar>());
        level[paddlei] +=
            paddleWettedMagSf[paddlei]*ySpan_
           /(paddleMagSf[paddlei] + ROOTVSMALL);
    }

    return tlevel;
}


void Foam::waveModel::setAlpha(const scalarField& level)
{
    forAll(alpha_, facei)
    {
        const label paddlei = faceToPaddle_[facei];
        const scalar paddleCalc = level[paddlei];

        const scalar yMin0 = yMin_[facei] - yMin0_;
        const scalar yMax0 = yMax_[facei] - yMin0_;

        if (yMax0 < paddleCalc)
        {
            alpha_[facei] = 1.0;
        }
        else if (yMin0 > paddleCalc)
        {
            alpha_[facei] = 0.0;
        }
        else
        {
            scalar dy = paddleCalc - yMin0;
            alpha_[facei] = dy/(yMax0 - yMin0);
        }
    }
}


void Foam::waveModel::setPaddlePropeties
(
    const scalarField& level,
    const label facei,
    scalar& fraction,
    scalar& y
) const
{
    const label paddlei = faceToPaddle_[facei];
    const scalar paddleCalc = level[paddlei];
    const scalar paddleHeight = min(paddleCalc, waterDepthRef_);
    const scalar yMin = yMin_[facei] - yMin0_;
    const scalar yMax = yMax_[facei] - yMin0_;

    fraction = 1;
    y = 0;

    if (yMax < paddleHeight)
    {
        y = y_[facei] - yMin0_;
    }
    else if (yMin > paddleCalc)
    {
        fraction = -1;
    }
    else
    {
        if (paddleCalc < waterDepthRef_)
        {
            if ((yMax > paddleCalc) && (yMin < paddleCalc))
            {
                scalar dy = paddleCalc - yMin;
                fraction = dy/(yMax - yMin);
                y = y_[facei] - yMin0_;
            }
        }
        else
        {
            if (yMax < paddleCalc)
            {
                y = waterDepthRef_;
            }
            else if ((yMax > paddleCalc) && (yMin < paddleCalc))
            {
                scalar dy = paddleCalc - yMin;
                fraction = dy/(yMax - yMin);
                y = waterDepthRef_;
            }
        }
    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::waveModel::waveModel
(
    const dictionary& dict,
    const fvMesh& mesh,
    const polyPatch& patch,
    const bool readFields
)
:
    IOdictionary
    (
        IOobject
        (
            modelName(patch.name()),
            Time::timeName(mesh.time().startTime().value()),
            "uniform",
            mesh,
            IOobject::NO_READ,
            IOobject::AUTO_WRITE
        )
    ),
    mesh_(mesh),
    patch_(patch),
    g_(mesh.lookupObject<uniformDimensionedVectorField>("g").value()),
    UName_("U"),
    alphaName_("alpha"),
    Rgl_(tensor::I),
    Rlg_(tensor::I),
    nPaddle_(1),
    xPaddle_(),
    zPaddle_(),
    y_(),
    ySpan_(0),
    yMin_(),
    yMax_(),
    waterDepthRef_(0),
    initialDepth_(0),
    currTimeIndex_(-1),
    activeAbsorption_(false),
    U_(patch.size(), vector::zero),
    alpha_(patch.size(), 0)
{
    if (readFields)
    {
        readDict(dict);
    }
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::waveModel::~waveModel()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::waveModel::readDict(const dictionary& overrideDict)
{
    readOpt() = IOobject::READ_IF_PRESENT;
    if (headerOk())
    {
        IOdictionary::regIOobject::read();
    }

    merge(overrideDict);

    readIfPresent("U", UName_);
    readIfPresent("alpha", alphaName_);

    lookup("nPaddle") >> nPaddle_;
    lookup("UMean") >> UMean_;

    if (nPaddle_ < 1)
    {
        FatalIOErrorInFunction(*this)
            << "Number of paddles must be greater than zero.  Supplied"
            << " value nPaddles = " << nPaddle_
            << exit(FatalIOError);
    }

    readIfPresent("initialDepth", initialDepth_);

    // Need to intialise the geometry before calling waterLevel()
    initialiseGeometry();

    // Set the reference water depth
    if (!readIfPresent("waterDepthRef", waterDepthRef_))
    {
        scalar waterDepth = 0;
        if (readIfPresent("waterDepth", waterDepth))
        {
            waterDepthRef_ = waterDepth;
        }
        else
        {
            const scalarField level(waterLevel());
            if (level.size())
            {
                waterDepthRef_ = level.first();
            }
        }

        // Avoid potential zero...
        waterDepthRef_ += SMALL;

        // Insert the reference water depth into [this] to enable restart
        add("waterDepthRef", waterDepthRef_);
    }

    return true;
}


void Foam::waveModel::correct(const scalar t)
{
    if (mesh_.time().timeIndex() != currTimeIndex_)
    {
        Info<< "Updating " << type() << " wave model for patch "
            << patch_.name() << endl;

        // Time ramp weight
        const scalar tCoeff = timeCoeff(t);

        // Reset the velocity and phase fraction fields
        U_ = vector::zero;
        alpha_ = 0;

        // Update the calculated water level field
        scalarField calculatedLevel(nPaddle_, 0);

        if (patch_.size())
        {
            // Set wave level
            setLevel(t, tCoeff, calculatedLevel);

            // Update the velocity field
            setVelocity(t, tCoeff, calculatedLevel);

            // Update the phase fraction field
            setAlpha(calculatedLevel);
        }

        if (activeAbsorption_)
        {
            const scalarField activeLevel(this->waterLevel());

            forAll(U_, facei)
            {
                const label paddlei = faceToPaddle_[facei];

                if (yMin_[facei] - yMin0_ < activeLevel[paddlei])
                {
                    scalar UCorr =
                        (calculatedLevel[paddlei] - activeLevel[paddlei])
                       *sqrt(mag(g_)/activeLevel[paddlei]);

                    U_[facei].x() += UCorr;
                }
                else
                {
                    U_[facei].x() += 0;
                }
            }
        }

        // Transform velocity into global co-ordinate system
        U_ = Rlg_ & U_;

        currTimeIndex_ = mesh_.time().timeIndex();
    }
}


const Foam::vectorField& Foam::waveModel::U() const
{
    return U_;
}


const Foam::scalarField& Foam::waveModel::alpha() const
{
    return alpha_;
}


void Foam::waveModel::info(Ostream& os) const
{
    os  << "Wave model: patch " << patch_.name() << nl
        << "    Type : " << type() << nl
        << "    Velocity field name : " << UName_ << nl
	<< "    The velocity of fluids: " << UMean_ << nl
        << "    Phase fraction field name : " << alphaName_ << nl
        << "    Transformation from local to global system : " << Rlg_ << nl
        << "    Number of paddles: " << nPaddle_ << nl
        << "    Reference water depth : " << waterDepthRef_ << nl
        << "    Active absorption: " << activeAbsorption_ << nl;
}


// ************************************************************************* //
