/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2018 OpenFOAM Foundation
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

#include "localSurfaces.H"
#include "volFields.H"
#include "surfaceFields.H"
#include "ListListOps.H"
#include "stringListOps.H"

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

template<class Type>
void Foam::localSurfaces::writeSurface
(
    const Field<Type>& values,
    const label surfI,
    const word& fieldName,
    const fileName& outputDir
)
{
    const sampledSurface& s = operator[](surfI);

    // Write locally on every processor without gathering geometry or values.
    if (s.faces().size())
    {
        formatter_->write
        (
            outputDir,
            s.name(),
            s.points(),
            s.faces(),
            fieldName,
            values,
            s.interpolate()
        );
    }
}


template<class Type>
void Foam::localSurfaces::sampleAndWrite
(
    const GeometricField<Type, fvPatchField, volMesh>& vField
)
{
    // interpolator for this field
    autoPtr<interpolation<Type>> interpolatorPtr;

    const word& fieldName = vField.name();
    const fileName outputDir = outputPath_/vField.time().timeName();

    forAll(*this, surfI)
    {
        const sampledSurface& s = operator[](surfI);

        Field<Type> values;

        if (s.interpolate())
        {
            if (interpolatorPtr.empty())
            {
                interpolatorPtr = interpolation<Type>::New
                (
                    interpolationScheme_,
                    vField
                );
            }

            values = s.interpolate(interpolatorPtr());
        }
        else
        {
            values = s.sample(vField);
        }

        writeSurface<Type>(values, surfI, fieldName, outputDir);
    }
}


template<class Type>
void Foam::localSurfaces::sampleAndWrite
(
    const GeometricField<Type, fvsPatchField, surfaceMesh>& sField
)
{
    const word& fieldName   = sField.name();
    const fileName outputDir = outputPath_/sField.time().timeName();

    forAll(*this, surfI)
    {
        const sampledSurface& s = operator[](surfI);
        Field<Type> values(s.sample(sField));
        writeSurface<Type>(values, surfI, fieldName, outputDir);
    }
}


template<class GeoField>
void Foam::localSurfaces::sampleAndWrite(const IOobjectList& objects)
{
    wordList names;
    if (loadFromFiles_)
    {
        IOobjectList fieldObjects(objects.lookupClass(GeoField::typeName));
        names = fieldObjects.names();
    }
    else
    {
        names = mesh_.thisDb().names<GeoField>();
    }

    labelList nameIDs(findStrings(fieldSelection_, names));

    wordHashSet fieldNames(wordList(names, nameIDs));

    forAllConstIter(wordHashSet, fieldNames, iter)
    {
        const word& fieldName = iter.key();

        if ((Pstream::master()) && verbose_)
        {
            Pout<< "sampleAndWrite: " << fieldName << endl;
        }

        if (loadFromFiles_)
        {
            const GeoField fld
            (
                IOobject
                (
                    fieldName,
                    mesh_.time().timeName(),
                    mesh_,
                    IOobject::MUST_READ
                ),
                mesh_
            );

            sampleAndWrite(fld);
        }
        else
        {
            sampleAndWrite
            (
                mesh_.thisDb().lookupObject<GeoField>(fieldName)
            );
        }
    }
}


// ************************************************************************* //
