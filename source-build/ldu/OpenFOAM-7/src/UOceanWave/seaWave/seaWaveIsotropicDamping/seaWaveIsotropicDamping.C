/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2019 OpenFOAM Foundation
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

#include "seaWaveIsotropicDamping.H"
#include "fvMatrix.H"
#include "fvmSup.H"
#include "addToRunTimeSelectionTable.H"
#include "zeroGradientFvPatchField.H"
// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{
    defineTypeNameAndDebug(seaWaveIsotropicDamping, 0);
    addToRunTimeSelectionTable(option, seaWaveIsotropicDamping, dictionary);
}
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::fv::seaWaveIsotropicDamping::add
(
    const volScalarField::Internal& forceCoeff,
    fvMatrix<vector>& eqn
)
{
    eqn -= fvm::Sp(forceCoeff, eqn.psi());
    eqn += forceCoeff*UTarget();
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fv::seaWaveIsotropicDamping::seaWaveIsotropicDamping
(
    const word& name,
    const word& modelType,
    const dictionary& dict,
    const fvMesh& mesh
)
:
    damping(name, modelType, dict, mesh),
    value_("value", dimVelocity, coeffs_.lookup("value")),
    UTargetPtr_(nullptr),
    UTargetReady_(false)
{
    read(dict);
//    UTarget();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::fv::seaWaveIsotropicDamping::addSup
(
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    add(this->forceCoeff(), eqn);
}


void Foam::fv::seaWaveIsotropicDamping::addSup
(
    const volScalarField& rho,
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    add(rho*forceCoeff(), eqn);
}


void Foam::fv::seaWaveIsotropicDamping::addSup
(
    const volScalarField& alpha,
    const volScalarField& rho,
    fvMatrix<vector>& eqn,
    const label fieldi
)
{
    add(alpha()*rho()*this->forceCoeff(), eqn);
}


bool Foam::fv::seaWaveIsotropicDamping::read(const dictionary& dict)
{
    if (damping::read(dict))
    {
        value_ =
            dimensionedVector
            (
                value_.name(),
                value_.dimensions(),
                coeffs_.lookup(value_.name())
            );

	coeffs_.lookup("UOceanCoeffs") >> UOceanCoeffs_;
	
	UTargetReady_ = false;
	calcUTarget();

        return true;
    }
    else
    {
        return false;
    }
}

/*
void Foam::fv::seaWaveIsotropicDamping::UTarget()
{
  
 const vectorField& cellCenters = mesh_.cellCentres();

 forAll(cellCenters, celli)
 {
     const vector c(cellCenters[celli]);
     scalar x = c.x();
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
            UTarget_[celli].x() = sqrt(sqr(aV*y + bV) + sqr(aU*y + bU)) + Foam::mag(value_).value();
            UTarget_[celli].y() = 0.0;
	    UTarget_[celli].z() = 0.0;
	    break;
        }
   }

 }
 Info<< "Finishing the target U setting" << nl;

}
*/


void  Foam::fv::seaWaveIsotropicDamping::calcUTarget()
{

	if (!UTargetPtr_.valid() || UTargetPtr_().size() != mesh_.nCells())
	{
  	  UTargetPtr_.reset
   	 (
    	   new volVectorField::Internal
       	   (
            IOobject
            (
                "UTarget",
                mesh_.time().timeName(),
                mesh_,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh_,
            dimensionedVector(dimVelocity, vector::zero)
          )
    	 );
	}

	vectorField& UTarget = UTargetPtr_();
	UTarget = vector::zero;

    const vectorField& cellCenters = mesh_.cellCentres();


    forAll(cellCenters, celli)
    {
    	bool matched = false;
     	const vector c(cellCenters[celli]);
     	scalar x = c.x();
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
            		UTarget[celli].x() = sqrt(sqr(aV*y + bV) + sqr(aU*y + bU)) + Foam::mag(value_).value();
            		UTarget[celli].y() = 0.0;
            		UTarget[celli].z() = 0.0;
			matched = true;
            		break;
        	}

   	}


	    if(!matched)
	    {
                        UTarget[celli].x() = Foam::mag(value_).value();
                        UTarget[celli].y() = 0.0;
                        UTarget[celli].z() = 0.0;

	    }

    }
 	    
    // Write out the UTarget for debugging
    if (debug && mesh_.time().writeTime())
    {
        volVectorField vUTarget
        (
            IOobject
            (
                "vUTarget",
                mesh_.time().timeName(),
                mesh_
            ),
            mesh_,
            dimVelocity,
            zeroGradientFvPatchField<vector>::typeName
        );
        vUTarget.primitiveFieldRef() = UTarget;
        vUTarget.correctBoundaryConditions();
        vUTarget.write();
    }
    
    Info<< "Finishing the damping target U setting" << nl;
    
    UTargetReady_ = true;

}

const Foam::volVectorField::Internal&
Foam::fv::seaWaveIsotropicDamping::UTarget()
{
	const bool needCalc =
  	!UTargetReady_
 	|| !UTargetPtr_.valid()
 	|| UTargetPtr_().size() != mesh_.nCells()
 	|| mesh_.topoChanging();

	if (needCalc)	
	{
    		calcUTarget();
	}

    return UTargetPtr_();
}
// ************************************************************************* //
