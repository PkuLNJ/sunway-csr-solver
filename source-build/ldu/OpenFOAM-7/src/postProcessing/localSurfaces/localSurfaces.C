/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2019 OpenFOAM Foundation
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
#include "PatchTools.H"
#include "mapPolyMesh.H"
#include "addToRunTimeSelectionTable.H"
#include "volFields.H"
#include "interpolation.H"
#include "OFstream.H"
#include "HashSet.H"
#include "DynamicList.H"
#include "stringListOps.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(localSurfaces, 0);

    addToRunTimeSelectionTable
    (
        functionObject,
        localSurfaces,
        dictionary
    );
}

bool Foam::localSurfaces::verbose_ = false;
Foam::scalar Foam::localSurfaces::mergeTol_ = 1e-10;


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::localSurfaces::writeGeometry() const
{
    // Write to time directory under outputPath_
    // Skip surface without faces (eg, a failed cut-plane)

    const fileName outputDir = outputPath_/mesh_.time().timeName();

    forAll(*this, surfI)
    {
        const sampledSurface& s = operator[](surfI);

        if (s.faces().size())
        {
            formatter_->write
            (
                outputDir,
                s.name(),
                s.points(),
                s.faces()
            );
        }
    }
}


Foam::tmp<Foam::scalarField> Foam::localSurfaces::sampleField
(
    const sampledSurface& s,
    const volScalarField& vField
) const
{
    if (s.interpolate())
    {
        autoPtr<interpolation<scalar>> interpolatorPtr =
            interpolation<scalar>::New(interpolationScheme_, vField);

        return s.interpolate(interpolatorPtr());
    }

    return s.sample(vField);
}


Foam::tmp<Foam::vectorField> Foam::localSurfaces::sampleField
(
    const sampledSurface& s,
    const volVectorField& vField
) const
{
    if (s.interpolate())
    {
        autoPtr<interpolation<vector>> interpolatorPtr =
            interpolation<vector>::New(interpolationScheme_, vField);

        return s.interpolate(interpolatorPtr());
    }

    return s.sample(vField);
}


void Foam::localSurfaces::writePointCsv() const
{
    enum csvFieldType
    {
        csvScalar = 0,
        csvVector = 1
    };

    wordList scalarNames;
    wordList vectorNames;

    if (loadFromFiles_)
    {
        IOobjectList objects(mesh_, mesh_.time().timeName());
        scalarNames =
            IOobjectList(objects.lookupClass(volScalarField::typeName)).names();
        vectorNames =
            IOobjectList(objects.lookupClass(volVectorField::typeName)).names();
    }
    else
    {
        scalarNames = mesh_.thisDb().names<volScalarField>();
        vectorNames = mesh_.thisDb().names<volVectorField>();
    }

    DynamicList<word> selectedNames;
    DynamicList<label> selectedTypes;
    wordHashSet selectedScalars;
    wordHashSet selectedVectors;

    forAll(fieldSelection_, selectI)
    {
        labelList scalarIDs(findStrings(fieldSelection_[selectI], scalarNames));
        forAll(scalarIDs, i)
        {
            const word& fieldName = scalarNames[scalarIDs[i]];

            if (selectedScalars.insert(fieldName))
            {
                selectedNames.append(fieldName);
                selectedTypes.append(csvScalar);
            }
        }

        labelList vectorIDs(findStrings(fieldSelection_[selectI], vectorNames));
        forAll(vectorIDs, i)
        {
            const word& fieldName = vectorNames[vectorIDs[i]];

            if (selectedVectors.insert(fieldName))
            {
                selectedNames.append(fieldName);
                selectedTypes.append(csvVector);
            }
        }
    }

    const fileName outputDir = outputPath_/mesh_.time().timeName();

    forAll(*this, surfI)
    {
        const sampledSurface& s = operator[](surfI);
        const pointField& points = s.points();

        if (!points.size())
        {
            continue;
        }

        DynamicList<word> fieldNames;
        DynamicList<label> fieldTypes;
        DynamicList<label> fieldIndices;
        PtrList<scalarField> scalarValues;
        PtrList<vectorField> vectorValues;

        forAll(selectedNames, fieldI)
        {
            const word& fieldName = selectedNames[fieldI];

            if (selectedTypes[fieldI] == csvScalar)
            {
                tmp<scalarField> tValues
                (
                    loadFromFiles_
                  ? sampleField
                    (
                        s,
                        volScalarField
                        (
                            IOobject
                            (
                                fieldName,
                                mesh_.time().timeName(),
                                mesh_,
                                IOobject::MUST_READ
                            ),
                            mesh_
                        )
                    )
                  : sampleField
                    (
                        s,
                        mesh_.thisDb().lookupObject<volScalarField>(fieldName)
                    )
                );

                if (tValues().size() == points.size())
                {
                    scalarValues.append(new scalarField(tValues()));
                    fieldNames.append(fieldName);
                    fieldTypes.append(csvScalar);
                    fieldIndices.append(scalarValues.size() - 1);
                }
                else
                {
                    WarningInFunction
                        << "Skipping field " << fieldName
                        << " on surface " << s.name()
                        << " because it has " << tValues().size()
                        << " values for " << points.size()
                        << " points. Use interpolate true for point CSV."
                        << endl;
                }
            }
            else if (selectedTypes[fieldI] == csvVector)
            {
                tmp<vectorField> tValues
                (
                    loadFromFiles_
                  ? sampleField
                    (
                        s,
                        volVectorField
                        (
                            IOobject
                            (
                                fieldName,
                                mesh_.time().timeName(),
                                mesh_,
                                IOobject::MUST_READ
                            ),
                            mesh_
                        )
                    )
                  : sampleField
                    (
                        s,
                        mesh_.thisDb().lookupObject<volVectorField>(fieldName)
                    )
                );

                if (tValues().size() == points.size())
                {
                    vectorValues.append(new vectorField(tValues()));
                    fieldNames.append(fieldName);
                    fieldTypes.append(csvVector);
                    fieldIndices.append(vectorValues.size() - 1);
                }
                else
                {
                    WarningInFunction
                        << "Skipping field " << fieldName
                        << " on surface " << s.name()
                        << " because it has " << tValues().size()
                        << " values for " << points.size()
                        << " points. Use interpolate true for point CSV."
                        << endl;
                }
            }
        }

        fileName csvName(s.name() + ".csv");
        if (csvFileName_.size())
        {
            csvName = size() == 1
              ? csvFileName_
              : fileName(s.name() + "_" + csvFileName_);
        }

        OFstream os
        (
            outputDir/csvName,
            IOstream::ASCII,
            IOstream::currentVersion,
            IOstream::UNCOMPRESSED
        );
        os.precision(writePrecision_);

        os  << "x,y,z";
        forAll(fieldNames, fieldI)
        {
            const word& fieldName = fieldNames[fieldI];

            if (fieldTypes[fieldI] == csvScalar)
            {
                os  << ',' << fieldName;
            }
            else if (fieldTypes[fieldI] == csvVector)
            {
                os  << ','
                    << fieldName << "_x,"
                    << fieldName << "_y,"
                    << fieldName << "_z";
            }
        }
        os  << nl;

        forAll(points, pointI)
        {
            const point& pt = points[pointI];

            os  << pt.x() << ',' << pt.y() << ',' << pt.z();

            forAll(fieldNames, fieldI)
            {
                const label valueI = fieldIndices[fieldI];

                if (fieldTypes[fieldI] == csvScalar)
                {
                    os  << ',' << scalarValues[valueI][pointI];
                }
                else if (fieldTypes[fieldI] == csvVector)
                {
                    const vector& v = vectorValues[valueI][pointI];
                    os  << ',' << v.x() << ',' << v.y() << ',' << v.z();
                }
            }

            os  << nl;
        }
    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::localSurfaces::localSurfaces
(
    const word& name,
    const Time& t,
    const dictionary& dict
)
:
    functionObject(name),
    PtrList<sampledSurface>(),
    mesh_
    (
        refCast<const fvMesh>
        (
            t.lookupObject<objectRegistry>
            (
                dict.lookupOrDefault("region", polyMesh::defaultRegion)
            )
        )
    ),
    loadFromFiles_(false),
    outputPath_(fileName::null),
    fieldSelection_(),
    interpolationScheme_(word::null),
    pointCsv_(false),
    writePrecision_(IOstream::defaultPrecision()),
    csvFileName_(fileName::null),
    mergeList_(),
    formatter_(nullptr)
{
    outputPath_ = mesh_.time().path()/"postProcessing"/name;
    outputPath_.clean();

    read(dict);
}


Foam::localSurfaces::localSurfaces
(
    const word& name,
    const objectRegistry& obr,
    const dictionary& dict,
    const bool loadFromFiles
)
:
    functionObject(name),
    PtrList<sampledSurface>(),
    mesh_(refCast<const fvMesh>(obr)),
    loadFromFiles_(loadFromFiles),
    outputPath_(fileName::null),
    fieldSelection_(),
    interpolationScheme_(word::null),
    pointCsv_(false),
    writePrecision_(IOstream::defaultPrecision()),
    csvFileName_(fileName::null),
    mergeList_(),
    formatter_(nullptr)
{
    outputPath_ = mesh_.time().path()/"postProcessing"/name;
    outputPath_.clean();

    read(dict);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::localSurfaces::~localSurfaces()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::localSurfaces::verbose(const bool verbosity)
{
    verbose_ = verbosity;
}


bool Foam::localSurfaces::execute()
{
    return true;
}


bool Foam::localSurfaces::write()
{
    if (size())
    {
        // Finalize surfaces, merge points etc.
        update();

        const label nFields = classifyFields();

        if (debug)
        {
            Pout<< "Creating directory "
                << outputPath_/mesh_.time().timeName() << nl << endl;
        }
        mkDir(outputPath_/mesh_.time().timeName());

        if (pointCsv_)
        {
            writePointCsv();
        }
        else
        {
            // Write geometry first if required,
            // or when no fields would otherwise be written
            if (nFields == 0 || formatter_->separateGeometry())
            {
                writeGeometry();
            }

            const IOobjectList objects(mesh_, mesh_.time().timeName());

            sampleAndWrite<volScalarField>(objects);
            sampleAndWrite<volVectorField>(objects);
            sampleAndWrite<volSphericalTensorField>(objects);
            sampleAndWrite<volSymmTensorField>(objects);
            sampleAndWrite<volTensorField>(objects);

            sampleAndWrite<surfaceScalarField>(objects);
            sampleAndWrite<surfaceVectorField>(objects);
            sampleAndWrite<surfaceSphericalTensorField>(objects);
            sampleAndWrite<surfaceSymmTensorField>(objects);
            sampleAndWrite<surfaceTensorField>(objects);
        }
    }

    return true;
}


bool Foam::localSurfaces::read(const dictionary& dict)
{
    bool surfacesFound = dict.found("surfaces");

    if (surfacesFound)
    {
        dict.lookup("fields") >> fieldSelection_;

        dict.lookup("interpolationScheme") >> interpolationScheme_;
        const word writeType(dict.lookup("surfaceFormat"));

        pointCsv_ = dict.lookupOrDefault("pointCsv", false);
        writePrecision_ =
            dict.lookupOrDefault<label>
            (
                "writePrecision",
                IOstream::defaultPrecision()
            );
        csvFileName_ =
            dict.lookupOrDefault<fileName>("csvFileName", fileName::null);

        // Define the surface formatter
        // Optionally defined extra controls for the output formats
        formatter_ = surfaceWriter::New
        (
            writeType,
            dict.subOrEmptyDict("formatOptions").subOrEmptyDict(writeType)
        );

        PtrList<sampledSurface> newList
        (
            dict.lookup("surfaces"),
            sampledSurface::iNew(mesh_)
        );
        transfer(newList);

        if (Pstream::parRun())
        {
            mergeList_.setSize(size());
        }

        // Ensure all surfaces and merge information are expired
        expire();

        if (this->size())
        {
            Info<< "Reading surface description:" << nl;
            forAll(*this, surfI)
            {
                Info<< "    " << operator[](surfI).name() << nl;
            }
            Info<< endl;
        }
    }

    if (Pstream::master() && debug)
    {
        Pout<< "sample fields:" << fieldSelection_ << nl
            << "sample surfaces:" << nl << "(" << nl;

        forAll(*this, surfI)
        {
            Pout<< "  " << operator[](surfI) << endl;
        }
        Pout<< ")" << endl;
    }

    return true;
}


void Foam::localSurfaces::updateMesh(const mapPolyMesh& mpm)
{
    if (&mpm.mesh() == &mesh_)
    {
        expire();
    }

    // pointMesh and interpolation will have been reset in mesh.update
}


void Foam::localSurfaces::movePoints(const polyMesh& mesh)
{
    if (&mesh == &mesh_)
    {
        expire();
    }
}


void Foam::localSurfaces::readUpdate(const polyMesh::readUpdateState state)
{
    if (state != polyMesh::UNCHANGED)
    {
        expire();
    }
}


bool Foam::localSurfaces::needsUpdate() const
{
    forAll(*this, surfI)
    {
        if (operator[](surfI).needsUpdate())
        {
            return true;
        }
    }

    return false;
}


bool Foam::localSurfaces::expire()
{
    bool justExpired = false;

    forAll(*this, surfI)
    {
        if (operator[](surfI).expire())
        {
            justExpired = true;
        }

        // Clear merge information
        if (Pstream::parRun())
        {
            mergeList_[surfI].clear();
        }
    }

    // true if any surfaces just expired
    return justExpired;
}


bool Foam::localSurfaces::update()
{
    bool updated = false;

    if (!needsUpdate())
    {
        return updated;
    }

    forAll(*this, surfI)
    {
        if (operator[](surfI).update())
        {
            updated = true;
        }
    }

    return updated;
}


// ************************************************************************* //
