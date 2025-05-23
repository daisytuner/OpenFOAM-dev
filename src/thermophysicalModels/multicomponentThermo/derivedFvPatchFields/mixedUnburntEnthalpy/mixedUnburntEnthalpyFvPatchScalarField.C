/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2011-2025 OpenFOAM Foundation
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

#include "mixedUnburntEnthalpyFvPatchScalarField.H"
#include "addToRunTimeSelectionTable.H"
#include "fieldMapper.H"
#include "psiuMulticomponentThermo.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::mixedUnburntEnthalpyFvPatchScalarField::
mixedUnburntEnthalpyFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF
)
:
    mixedFvPatchScalarField(p, iF)
{
    valueFraction() = scalar(0);
    refValue() = scalar(0);
    refGrad() = scalar(0);
}


Foam::mixedUnburntEnthalpyFvPatchScalarField::
mixedUnburntEnthalpyFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF, dict)
{}


Foam::mixedUnburntEnthalpyFvPatchScalarField::
mixedUnburntEnthalpyFvPatchScalarField
(
    const mixedUnburntEnthalpyFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const fieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper, false)
{
    map(ptf, mapper);
}


Foam::mixedUnburntEnthalpyFvPatchScalarField::
mixedUnburntEnthalpyFvPatchScalarField
(
    const mixedUnburntEnthalpyFvPatchScalarField& tppsf,
    const DimensionedField<scalar, volMesh>& iF
)
:
    mixedFvPatchScalarField(tppsf, iF)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::mixedUnburntEnthalpyFvPatchScalarField::map
(
    const mixedUnburntEnthalpyFvPatchScalarField& ptf,
    const fieldMapper& mapper
)
{
    // Unmapped faces are considered zero-gradient/adiabatic
    // until they are corrected later
    mapper(*this, ptf, [&](){ return patchInternalField(); });
    mapper(refValue(), ptf.refValue(), [&](){ return patchInternalField(); });
    mapper(refGrad(), ptf.refGrad(), scalar(0));
    mapper(valueFraction(), ptf.valueFraction(), scalar(0));
}


void Foam::mixedUnburntEnthalpyFvPatchScalarField::map
(
    const fvPatchScalarField& ptf,
    const fieldMapper& mapper
)
{
    map(refCast<const mixedUnburntEnthalpyFvPatchScalarField>(ptf), mapper);
}


void Foam::mixedUnburntEnthalpyFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const psiuMulticomponentThermo& thermo =
        db().lookupObject<psiuMulticomponentThermo>
        (
            physicalProperties::typeName
        );

    const label patchi = patch().index();

    mixedFvPatchScalarField& Tw = refCast<mixedFvPatchScalarField>
    (
        const_cast<fvPatchScalarField&>(thermo.Tu().boundaryField()[patchi])
    );

    Tw.evaluate();

    valueFraction() = Tw.valueFraction();
    refValue() = thermo.heu(Tw.refValue(), patchi);
    refGrad() = thermo.Cp(Tw, patchi)*Tw.refGrad()
      + patch().deltaCoeffs()*
        (
            thermo.heu(Tw, patchi)
          - thermo.heu(Tw, patch().faceCells())
        );

    mixedFvPatchScalarField::updateCoeffs();
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makeNullConstructablePatchTypeField
    (
        fvPatchScalarField,
        mixedUnburntEnthalpyFvPatchScalarField
    );
}

// ************************************************************************* //
