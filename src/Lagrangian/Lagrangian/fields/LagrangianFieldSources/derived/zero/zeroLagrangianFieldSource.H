/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2025 OpenFOAM Foundation
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
    Foam::zeroLagrangianFieldSource

Description
    This source condition provides a zero value.

Usage
    Example specification:
    \verbatim
    <LagrangianModelName>
    {
        type            zero;
    }
    \endverbatim

SourceFiles
    zeroLagrangianFieldSource.C

\*---------------------------------------------------------------------------*/

#ifndef zeroLagrangianFieldSource_H
#define zeroLagrangianFieldSource_H

#include "LagrangianFieldSource.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
               Class zeroLagrangianFieldSource Declaration
\*---------------------------------------------------------------------------*/

template<class Type>
class zeroLagrangianFieldSource
:
    public LagrangianFieldSource<Type>
{
public:

    //- Runtime type information
    TypeName("zero");


    // Constructors

        //- Inherit constructors
        using LagrangianFieldSource<Type>::LagrangianFieldSource;

        //- Construct and return a clone setting the internal field reference
        virtual autoPtr<LagrangianFieldSource<Type>> clone
        (
            const regIOobject& iIo
        ) const
        {
            return autoPtr<LagrangianFieldSource<Type>>
            (
                new zeroLagrangianFieldSource<Type>(*this, iIo)
            );
        }


    //- Destructor
    virtual ~zeroLagrangianFieldSource();


    // Member Functions

        //- Return the source value
        virtual tmp<LagrangianSubField<Type>> sourceValue
        (
            const LagrangianSource&,
            const LagrangianSubMesh&
        ) const;

        //- Return the internal coefficient
        virtual tmp<LagrangianSubScalarField> internalCoeff
        (
            const LagrangianSource&,
            const LagrangianSubMesh&
        ) const;

        //- Return the value for an instantaneous injection
        virtual tmp<LagrangianSubField<Type>> value
        (
            const LagrangianInjection&,
            const LagrangianSubMesh&
        ) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifdef NoRepository
    #include "zeroLagrangianFieldSource.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
