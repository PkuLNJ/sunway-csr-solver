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

Class
    Foam::fv::seaWaveIsotropicDamping

Description

    This fvOption applies an implicit damping force to all components of the
    vector field to relax the field towards a specified uniform value.  Its
    intended purpose is to damp the motions of an interface in the region
    approaching an outlet so that no reflections are generated.

    The damping force coefficient \f$\lambda\f$ should be set based on the
    desired level of damping and the residence time of a perturbation through
    the damping zone.  For example, if waves moving at 2 [m/s] are travelling
    through a damping zone 8 [m] in length, then the residence time is 4 [s]. If
    it is deemed necessary to damp for 5 time-scales, then \f$\lambda\f$ should
    be set to equal 5/(4 [s]) = 1.2 [1/s].

See also
    Foam::fv::damping
    Foam::fv::verticalDamping

SourceFiles
    seaWaveIsotropicDamping.C

\*---------------------------------------------------------------------------*/

#ifndef seaWaveIsotropicDamping_H
#define seaWaveIsotropicDamping_H

#include "damping.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace fv
{

/*---------------------------------------------------------------------------*\
                   Class seaWaveIsotropicDamping Declaration
\*---------------------------------------------------------------------------*/

class seaWaveIsotropicDamping
:
    public damping
{
    // Private Data

        //- Reference value
	dimensionedVector value_;

	autoPtr<volVectorField::Internal> UTargetPtr_;
	
	bool UTargetReady_;
	
	List<List<scalar> > UOceanCoeffs_;
    // Private Member Functions

        //- Source term to momentum equation
        void add
        (
            const volScalarField::Internal& forceCoeff,
            fvMatrix<vector>& eqn
        );


public:

    //- Runtime type information
    TypeName("seaWaveIsotropicDamping");


    // Constructors

        //- Construct from components
        seaWaveIsotropicDamping
        (
            const word& name,
            const word& modelType,
            const dictionary& dict,
            const fvMesh& mesh
        );


    //- Destructor
    virtual ~seaWaveIsotropicDamping()
    {}


    // Member Functions

         // Add explicit and implicit contributions

            //- Source term to momentum equation
            virtual void addSup
            (
                fvMatrix<vector>& eqn,
                const label fieldi
            );

            //- Source term to compressible momentum equation
            virtual void addSup
            (
                const volScalarField& rho,
                fvMatrix<vector>& eqn,
                const label fieldi
            );

            //- Source term to phase momentum equation
            virtual void addSup
            (
                const volScalarField& alpha,
                const volScalarField& rho,
                fvMatrix<vector>& eqn,
                const label fieldi
            );


        // IO

            //- Read dictionary
            virtual bool read(const dictionary& dict);
        
	    //- target velocity
        
	    void calcUTarget();
	    
	    const volVectorField::Internal& UTarget();
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv
} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
