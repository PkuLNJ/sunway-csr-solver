/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2015-2019 OpenFOAM Foundation
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

\*---------------------------------------------------------------------------*/

#include "CorrectPhi.H"
#include "AmrCapture.H"
#include "volFields.H"
#include "surfaceFields.H"
#include "fvScalarMatrix.H"
#include "fvmDdt.H"
#include "fvmLaplacian.H"
#include "fvcDiv.H"
#include "fixedValueFvPatchFields.H"
#include "zeroGradientFvPatchFields.H"
#include "adjustPhi.H"
#include "fvcMeshPhi.H"
#include "nonOrthogonalSolutionControl.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class RAUfType, class DivUType>
void Foam::CorrectPhi
(
    volVectorField& U,
    surfaceScalarField& phi,
    const volScalarField& p,
    const RAUfType& rAUf,
    const DivUType& divU,
    nonOrthogonalSolutionControl& pcorrControl,
    const bool evaluateUBCs
)
{
    const fvMesh& mesh = U.mesh();
    const Time& runTime = mesh.time();

    const std::string cpStage = amrCapture::correctPhiStage();
    amrCapture::field(cpStage + ".before_BCs", U);
    amrCapture::field(cpStage + ".before_BCs", phi);
    correctUphiBCs(U, phi, evaluateUBCs);
    amrCapture::field(cpStage + ".after_BCs", U);
    amrCapture::field(cpStage + ".after_BCs", phi);
    amrCapture::coefficient(cpStage + ".input", "rAUf", rAUf);

    // Initialize BCs list for pcorr to zero-gradient
    wordList pcorrTypes
    (
        p.boundaryField().size(),
        zeroGradientFvPatchScalarField::typeName
    );

    // Set BCs of pcorr to fixed-value for patches at which p is fixed
    forAll(p.boundaryField(), patchi)
    {
        if (p.boundaryField()[patchi].fixesValue())
        {
            pcorrTypes[patchi] = fixedValueFvPatchScalarField::typeName;
        }
    }

    volScalarField pcorr
    (
        IOobject
        (
            "pcorr",
            runTime.timeName(),
            mesh
        ),
        mesh,
        dimensionedScalar(p.dimensions(), 0),
        pcorrTypes
    );

    const bool captureNeedReference = pcorr.needReference();
    amrCapture::value(cpStage + ".input", "needReference", captureNeedReference);
    amrCapture::field(cpStage + ".input", pcorr);
    if (captureNeedReference)
    {
        fvc::makeRelative(phi, U);
        amrCapture::field(cpStage + ".before_adjustPhi", phi);
        adjustPhi(phi, U, pcorr);
        amrCapture::field(cpStage + ".after_adjustPhi", phi);
        fvc::makeAbsolute(phi, U);
    }

    mesh.setFluxRequired(pcorr.name());

    while (pcorrControl.correctNonOrthogonal())
    {
        // Solve for pcorr such that the divergence of the corrected flux
        // matches the divU provided (from previous iteration, time-step...)
        fvScalarMatrix pcorrEqn
        (
            fvm::laplacian(rAUf, pcorr) == fvc::div(phi) - divU
        );

        amrCapture::beginEquation("pcorr");
        const std::string eqStage = amrCapture::activeStage();
        amrCapture::matrix(eqStage + ".before_reference", pcorrEqn);
        pcorrEqn.setReference(0, 0);
        amrCapture::matrix(eqStage + ".raw", pcorrEqn);

        pcorrEqn.solve();
        amrCapture::field(eqStage + ".returned", pcorr);
        amrCapture::endEquation();

        if (pcorrControl.finalNonOrthogonalIter())
        {
            phi -= pcorrEqn.flux();
            amrCapture::field(cpStage + ".after_flux_correction", phi);
        }
    }
}


template<class RAUfType, class DivRhoUType>
void Foam::CorrectPhi
(
    volVectorField& U,
    surfaceScalarField& phi,
    const volScalarField& p,
    const volScalarField& rho,
    const volScalarField& psi,
    const RAUfType& rAUf,
    const DivRhoUType& divRhoU,
    nonOrthogonalSolutionControl& pcorrControl,
    const bool evaluateUBCs
)
{
    const fvMesh& mesh = U.mesh();
    const Time& runTime = mesh.time();

    correctUphiBCs(rho, U, phi, evaluateUBCs);

    // Initialize BCs list for pcorr to zero-gradient
    wordList pcorrTypes
    (
        p.boundaryField().size(),
        zeroGradientFvPatchScalarField::typeName
    );

    // Set BCs of pcorr to fixed-value for patches at which p is fixed
    forAll(p.boundaryField(), patchi)
    {
        if (p.boundaryField()[patchi].fixesValue())
        {
            pcorrTypes[patchi] = fixedValueFvPatchScalarField::typeName;
        }
    }

    volScalarField pcorr
    (
        IOobject
        (
            "pcorr",
            runTime.timeName(),
            mesh
        ),
        mesh,
        dimensionedScalar(p.dimensions(), 0),
        pcorrTypes
    );

    mesh.setFluxRequired(pcorr.name());

    while (pcorrControl.correctNonOrthogonal())
    {
        // Solve for pcorr such that the divergence of the corrected flux
        // matches the divRhoU provided (from previous iteration, time-step...)
        fvScalarMatrix pcorrEqn
        (
            fvm::ddt(psi, pcorr)
          + fvc::div(phi)
          - fvm::laplacian(rAUf, pcorr)
         ==
            divRhoU
        );

        pcorrEqn.solve();

        if (pcorrControl.finalNonOrthogonalIter())
        {
            phi += pcorrEqn.flux();
        }
    }
}


// ************************************************************************* //
