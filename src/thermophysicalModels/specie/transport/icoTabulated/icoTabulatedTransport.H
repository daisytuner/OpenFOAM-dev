/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2020-2025 OpenFOAM Foundation
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
    Foam::icoTabulatedTransport

Description
    Transport properties package using non-uniformly-spaced tabulated data for
    viscosity and thermal conductivity vs temperature.

Usage
    \table
        Property        | Description
        mu              | Dynamic viscosity vs temperature table
        kappa           | Thermal conductivity vs temperature table
    \endtable

    Example of the specification of the transport properties:
    \verbatim
    transport
    {
        mu
        {
            values
            (
                (200 1.82e-05)
                (350 2.61e-05)
                (400 3.93e-05)
            );
        }

        kappa
        {
            values
            (
                (200 2.56e-5)
                (350 3.33e-5)
                (400 4.72e-5)
            );
        }
    }
    \endverbatim

SourceFiles
    icoTabulatedTransportI.H
    icoTabulatedTransport.C

See also
    Foam::Function1s::NonUniformTable

\*---------------------------------------------------------------------------*/

#ifndef icoTabulatedTransport_H
#define icoTabulatedTransport_H

#include "NonUniformTable1.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// Forward declaration of friend functions and operators

template<class Thermo> class icoTabulatedTransport;

template<class Thermo>
Ostream& operator<<(Ostream&, const icoTabulatedTransport<Thermo>&);


/*---------------------------------------------------------------------------*\
                     Class icoTabulatedTransport Declaration
\*---------------------------------------------------------------------------*/

template<class Thermo>
class icoTabulatedTransport
:
    public Thermo
{
    // Private Typedefs

        //- Table type
        typedef Function1s::NonUniformTable<scalar> nonUniformTable;


    // Private Data

        //- Dynamic viscosity table [Pa.s]
        nonUniformTable mu_;

        //- Thermal conductivity table [W/m/K]
        nonUniformTable kappa_;


public:

    // Constructors

        //- Construct from components
        inline icoTabulatedTransport
        (
            const Thermo& t,
            const nonUniformTable& mu,
            const nonUniformTable& kappa
        );

        //- Construct as named copy
        inline icoTabulatedTransport(const word&, const icoTabulatedTransport&);

        //- Construct from name and dictionary
        icoTabulatedTransport(const word& name, const dictionary& dict);

        //- Construct and return a clone
        inline autoPtr<icoTabulatedTransport> clone() const;


    // Member Functions

        //- Return the instantiated type name
        static word typeName()
        {
            return "icoTabulated<" + Thermo::typeName() + '>';
        }

        //- Dynamic viscosity [kg/m/s]
        inline scalar mu(const scalar p, const scalar T) const;

        //- Thermal conductivity [W/m/K]
        inline scalar kappa(const scalar p, const scalar T) const;

        //- Write to Ostream
        void write(Ostream& os) const;


    // Ostream Operator

        friend Ostream& operator<< <Thermo>
        (
            Ostream&,
            const icoTabulatedTransport&
        );
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#include "icoTabulatedTransportI.H"

#ifdef NoRepository
    #include "icoTabulatedTransport.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
