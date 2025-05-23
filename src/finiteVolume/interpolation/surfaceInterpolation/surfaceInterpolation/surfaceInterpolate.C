/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2024 OpenFOAM Foundation
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

#include "surfaceInterpolate.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
Foam::tmp<Foam::surfaceInterpolationScheme<Type>>
Foam::fvc::scheme
(
    const surfaceScalarField& faceFlux,
    Istream& streamData
)
{
    return surfaceInterpolationScheme<Type>::New
    (
        faceFlux.mesh(),
        faceFlux,
        streamData
    );
}


template<class Type>
Foam::tmp<Foam::surfaceInterpolationScheme<Type>> Foam::fvc::scheme
(
    const surfaceScalarField& faceFlux,
    const word& name
)
{
    return surfaceInterpolationScheme<Type>::New
    (
        faceFlux.mesh(),
        faceFlux,
        faceFlux.mesh().schemes().interpolation(name)
    );
}


template<class Type>
Foam::tmp<Foam::surfaceInterpolationScheme<Type>> Foam::fvc::scheme
(
    const fvMesh& mesh,
    Istream& streamData
)
{
    return surfaceInterpolationScheme<Type>::New
    (
        mesh,
        streamData
    );
}


template<class Type>
Foam::tmp<Foam::surfaceInterpolationScheme<Type>> Foam::fvc::scheme
(
    const fvMesh& mesh,
    const word& name
)
{
    return surfaceInterpolationScheme<Type>::New
    (
        mesh,
        mesh.schemes().interpolation(name)
    );
}


template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf,
    const surfaceScalarField& faceFlux,
    Istream& schemeData
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << endl;
    }

    return scheme<Type>(faceFlux, schemeData)().interpolate(vf);
}


template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf,
    const surfaceScalarField& faceFlux,
    const word& name
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << " using " << name << endl;
    }

    return scheme<Type>(faceFlux, name)().interpolate(vf);
}

template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const tmp<VolField<Type>>& tvf,
    const surfaceScalarField& faceFlux,
    const word& name
)
{
    tmp<SurfaceField<Type>> tsf =
        interpolate(tvf(), faceFlux, name);

    tvf.clear();

    return tsf;
}

template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf,
    const tmp<surfaceScalarField>& tFaceFlux,
    const word& name
)
{
    tmp<SurfaceField<Type>> tsf =
        interpolate(vf, tFaceFlux(), name);

    tFaceFlux.clear();

    return tsf;
}

template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const tmp<VolField<Type>>& tvf,
    const tmp<surfaceScalarField>& tFaceFlux,
    const word& name
)
{
    tmp<SurfaceField<Type>> tsf =
        interpolate(tvf(), tFaceFlux(), name);

    tvf.clear();
    tFaceFlux.clear();

    return tsf;
}


template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf,
    Istream& schemeData
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << endl;
    }

    return scheme<Type>(vf.mesh(), schemeData)().interpolate(vf);
}

template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf,
    const word& name
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << " using " << name
            << endl;
    }

    return scheme<Type>(vf.mesh(), name)().interpolate(vf);
}

template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const tmp<VolField<Type>>& tvf,
    const word& name
)
{
    tmp<SurfaceField<Type>> tsf =
        interpolate(tvf(), name);

    tvf.clear();

    return tsf;
}


template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const VolField<Type>& vf
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << " using run-time selected scheme"
            << endl;
    }

    return interpolate(vf, "interpolate(" + vf.name() + ')');
}


template<class Type>
Foam::tmp<Foam::SurfaceField<Type>>
Foam::fvc::interpolate
(
    const tmp<VolField<Type>>& tvf
)
{
    tmp<SurfaceField<Type>> tsf =
        interpolate(tvf());
    tvf.clear();
    return tsf;
}


template<class Type>
Foam::tmp<Foam::FieldField<Foam::surfaceMesh::PatchField, Type>>
Foam::fvc::interpolate
(
    const FieldField<volMesh::PatchField, Type>& fvpff
)
{
    FieldField<surfaceMesh::PatchField, Type>* fvspffPtr
    (
        new FieldField<surfaceMesh::PatchField, Type>(fvpff.size())
    );

    forAll(*fvspffPtr, patchi)
    {
        fvspffPtr->set
        (
            patchi,
            surfaceMesh::PatchField<Type>::NewCalculatedType
            (
                fvpff[patchi].patch()
            ).ptr()
        );
        (*fvspffPtr)[patchi] = fvpff[patchi];
    }

    return tmp<FieldField<surfaceMesh::PatchField, Type>>(fvspffPtr);
}


template<class Type>
Foam::tmp<Foam::FieldField<Foam::surfaceMesh::PatchField, Type>>
Foam::fvc::interpolate
(
    const tmp<FieldField<volMesh::PatchField, Type>>& tfvpff
)
{
    tmp<FieldField<surfaceMesh::PatchField, Type>> tfvspff =
        interpolate(tfvpff());
    tfvpff.clear();
    return tfvspff;
}


template<class Type>
Foam::tmp
<
    Foam::SurfaceField<typename Foam::innerProduct<Foam::vector, Type>::type>
>
Foam::fvc::dotInterpolate
(
    const surfaceVectorField& Sf,
    const VolField<Type>& vf
)
{
    if (surfaceInterpolation::debug)
    {
        InfoInFunction
            << "interpolating VolField<Type> "
            << vf.name() << " using run-time selected scheme"
            << endl;
    }

    return scheme<Type>
    (
        vf.mesh(),
        "dotInterpolate(" + Sf.name() + ',' + vf.name() + ')'
    )().dotInterpolate(Sf, vf);
}


template<class Type>
Foam::tmp
<
    Foam::SurfaceField<typename Foam::innerProduct<Foam::vector, Type>::type>
>
Foam::fvc::dotInterpolate
(
    const surfaceVectorField& Sf,
    const tmp<VolField<Type>>& tvf
)
{
    tmp<SurfaceField<typename Foam::innerProduct<Foam::vector, Type>::type>> tsf
    (
        dotInterpolate(Sf, tvf())
    );
    tvf.clear();
    return tsf;
}


// ************************************************************************* //
