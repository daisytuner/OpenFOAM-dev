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

#include "gaussConvectionScheme.H"
#include "fvcSurfaceIntegrate.H"
#include "fvMatrices.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace fv
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
const surfaceInterpolationScheme<Type>&
gaussConvectionScheme<Type>::interpScheme() const
{
    return tinterpScheme_();
}


template<class Type>
tmp<SurfaceField<Type>>
gaussConvectionScheme<Type>::interpolate
(
    const surfaceScalarField&,
    const VolField<Type>& vf
) const
{
    return tinterpScheme_().interpolate(vf);
}


template<class Type>
tmp<SurfaceField<Type>>
gaussConvectionScheme<Type>::flux
(
    const surfaceScalarField& faceFlux,
    const VolField<Type>& vf
) const
{
    return faceFlux*interpolate(faceFlux, vf);
}


template<class Type>
tmp<fvMatrix<Type>>
gaussConvectionScheme<Type>::fvmDiv
(
    const surfaceScalarField& faceFlux,
    const VolField<Type>& vf
) const
{
    tmp<surfaceScalarField> tweights = tinterpScheme_().weights(vf);
    const surfaceScalarField& weights = tweights();

    tmp<fvMatrix<Type>> tfvm
    (
        new fvMatrix<Type>
        (
            vf,
            faceFlux.dimensions()*vf.dimensions()
        )
    );
    fvMatrix<Type>& fvm = tfvm.ref();

    fvm.lower() = -weights.primitiveField()*faceFlux.primitiveField();
    fvm.upper() = fvm.lower() + faceFlux.primitiveField();
    fvm.negSumDiag();

    forAll(vf.boundaryField(), patchi)
    {
        const fvPatchField<Type>& psf = vf.boundaryField()[patchi];
        const fvsPatchScalarField& patchFlux = faceFlux.boundaryField()[patchi];
        const fvsPatchScalarField& pw = weights.boundaryField()[patchi];

        fvm.internalCoeffs()[patchi] = patchFlux*psf.valueInternalCoeffs(pw);
        fvm.boundaryCoeffs()[patchi] = -patchFlux*psf.valueBoundaryCoeffs(pw);
    }

    if (tinterpScheme_().corrected())
    {
        tmp<SurfaceField<Type>> tfaceFluxCorrection
        (
            faceFlux*tinterpScheme_().correction(vf)
        );

        fvm += fvc::surfaceIntegrate(tfaceFluxCorrection());

        if (vf.mesh().schemes().fluxRequired(vf.name()))
        {
            fvm.faceFluxCorrectionPtr() = tfaceFluxCorrection.ptr();
        }
    }

    return tfvm;
}


template<class Type>
tmp<VolField<Type>>
gaussConvectionScheme<Type>::fvcDiv
(
    const surfaceScalarField& faceFlux,
    const VolField<Type>& vf
) const
{
    tmp<VolField<Type>> tConvection
    (
        fvc::surfaceIntegrateExtrapolate(flux(faceFlux, vf))
    );

    tConvection.ref().rename
    (
        "convection(" + faceFlux.name() + ',' + vf.name() + ')'
    );

    return tConvection;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
