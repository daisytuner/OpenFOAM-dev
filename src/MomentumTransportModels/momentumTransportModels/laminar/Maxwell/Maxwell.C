/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     | Website:  https://openfoam.org
    \\  /    A nd           | Copyright (C) 2016-2024 OpenFOAM Foundation
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

#include "Maxwell.H"
#include "fvModels.H"
#include "fvConstraints.H"
#include "uniformDimensionedFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
namespace laminarModels
{

// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
PtrList<dimensionedScalar>
Maxwell<BasicMomentumTransportModel>::readModeCoefficients
(
    const word& name,
    const dimensionSet& dims
) const
{
    PtrList<dimensionedScalar> modeCoeffs(nModes_);

    if (modeCoefficients_.size())
    {
        if (this->coeffDict().found(name))
        {
            IOWarningInFunction(this->coeffDict())
                << "Using 'modes' list, '" << name << "' entry will be ignored."
                << endl;
        }

        forAll(modeCoefficients_, modei)
        {
            modeCoeffs.set
            (
                modei,
                new dimensioned<scalar>
                (
                    name,
                    dims,
                    modeCoefficients_[modei].lookup(name)
                )
            );
        }
    }
    else
    {
        modeCoeffs.set
        (
            0,
            new dimensioned<scalar>
            (
                name,
                dims,
                this->coeffDict().lookup(name)
            )
        );
    }

    return modeCoeffs;
}


template<class BasicMomentumTransportModel>
tmp<fvSymmTensorMatrix> Maxwell<BasicMomentumTransportModel>::sigmaSource
(
    const label modei,
    volSymmTensorField& sigma
) const
{
    return -fvm::Sp(this->alpha_*this->rho_/lambdas_[modei], sigma);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
Maxwell<BasicMomentumTransportModel>::Maxwell
(
    const alphaField& alpha,
    const rhoField& rho,
    const volVectorField& U,
    const surfaceScalarField& alphaRhoPhi,
    const surfaceScalarField& phi,
    const viscosity& viscosity,
    const word& type
)
:
    laminarModel<BasicMomentumTransportModel>
    (
        type,
        alpha,
        rho,
        U,
        alphaRhoPhi,
        phi,
        viscosity
    ),

    modeCoefficients_
    (
        this->coeffDict().found("modes")
      ? PtrList<dictionary>
        (
            this->coeffDict().lookup("modes")
        )
      : PtrList<dictionary>()
    ),

    nModes_(modeCoefficients_.size() ? modeCoefficients_.size() : 1),

    nuM_("nuM", dimKinematicViscosity, this->coeffDict().lookup("nuM")),

    lambdas_(readModeCoefficients("lambda", dimTime)),

    sigma_
    (
        IOobject
        (
            this->groupName("sigma"),
            this->runTime_.name(),
            this->mesh_,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        this->mesh_
    )
{
    if (nModes_ > 1)
    {
        sigmas_.setSize(nModes_);

        forAll(sigmas_, modei)
        {
            typeIOobject<volSymmTensorField> header
            (
                this->groupName("sigma" + name(modei)),
                this->runTime_.name(),
                this->mesh_,
                IOobject::NO_READ
            );

            // Check if mode field exists and can be read
            if (header.headerOk())
            {
                Info<< "    Reading mode stress field "
                    << header.name() << endl;

                sigmas_.set
                (
                    modei,
                    new volSymmTensorField
                    (
                        IOobject
                        (
                            header.name(),
                            this->runTime_.name(),
                            this->mesh_,
                            IOobject::MUST_READ,
                            IOobject::AUTO_WRITE
                        ),
                        this->mesh_
                    )
                );
            }
            else
            {
                sigmas_.set
                (
                    modei,
                    new volSymmTensorField
                    (
                        IOobject
                        (
                            header.name(),
                            this->runTime_.name(),
                            this->mesh_,
                            IOobject::NO_READ,
                            IOobject::AUTO_WRITE
                        ),
                        sigma_
                    )
                );
            }
        }
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class BasicMomentumTransportModel>
bool Maxwell<BasicMomentumTransportModel>::read()
{
    if (laminarModel<BasicMomentumTransportModel>::read())
    {
        if (modeCoefficients_.size())
        {
            this->coeffDict().lookup("modes") >> modeCoefficients_;
        }

        nuM_.read(this->coeffDict());

        lambdas_ = readModeCoefficients("lambda", dimTime);

        return true;
    }
    else
    {
        return false;
    }
}


template<class BasicMomentumTransportModel>
tmp<volScalarField> Maxwell<BasicMomentumTransportModel>::nuEff() const
{
    return volScalarField::New
    (
        this->groupName("nuEff"),
        this->nu()
    );
}


template<class BasicMomentumTransportModel>
tmp<scalarField> Maxwell<BasicMomentumTransportModel>::nuEff
(
    const label patchi
) const
{
    return this->nu(patchi);
}


template<class BasicMomentumTransportModel>
tmp<surfaceVectorField> Maxwell<BasicMomentumTransportModel>::devTau() const
{
    const surfaceVectorField nf(this->mesh().nf());

    return surfaceVectorField::New
    (
        this->groupName("devTau"),
        fvc::dotInterpolate
        (
            nf,
            this->alpha_*this->rho_*this->nuM_*fvc::grad(this->U_)
        )
      + fvc::dotInterpolate
        (
            nf,
            this->alpha_*this->rho_*sigma_
        )
      - fvc::dotInterpolate
        (
            nf,
            this->alpha_*this->rho_*this->nu()*dev2(T(fvc::grad(this->U_)))
        )
      - fvc::interpolate(this->alpha_*this->rho_*nu0())*fvc::snGrad(this->U_)
    );
}


template<class BasicMomentumTransportModel>
template<class RhoFieldType>
tmp<fvVectorMatrix>
Maxwell<BasicMomentumTransportModel>::DivDevTau
(
    const RhoFieldType& rho,
    volVectorField& U
) const
{
    return
    (
        fvc::div(this->alpha_*rho*this->nuM_*fvc::grad(U))
      + fvc::div(this->alpha_*rho*sigma_)
      - fvc::div(this->alpha_*rho*this->nu()*dev2(T(fvc::grad(U))))
      - fvm::laplacian(this->alpha_*rho*nu0(), U)
    );
}


template<class BasicMomentumTransportModel>
tmp<fvVectorMatrix> Maxwell<BasicMomentumTransportModel>::divDevTau
(
    volVectorField& U
) const
{
    return DivDevTau(this->rho_, U);
}


template<class BasicMomentumTransportModel>
tmp<fvVectorMatrix>
Maxwell<BasicMomentumTransportModel>::divDevTau
(
    const volScalarField& rho,
    volVectorField& U
) const
{
    return DivDevTau(rho, U);
}


template<class BasicMomentumTransportModel>
void Maxwell<BasicMomentumTransportModel>::correct()
{
    // Local references
    const alphaField& alpha = this->alpha_;
    const rhoField& rho = this->rho_;
    const surfaceScalarField& alphaRhoPhi = this->alphaRhoPhi_;
    const volVectorField& U = this->U_;
    const Foam::fvModels& fvModels(Foam::fvModels::New(this->mesh_));
    const Foam::fvConstraints& fvConstraints
    (
        Foam::fvConstraints::New(this->mesh_)
    );

    laminarModel<BasicMomentumTransportModel>::correct();

    tmp<volTensorField> tgradU(fvc::grad(U));
    const volTensorField& gradU = tgradU();

    forAll(lambdas_, modei)
    {
        volSymmTensorField& sigma = nModes_ == 1 ? sigma_ : sigmas_[modei];

        uniformDimensionedScalarField rLambda
        (
            IOobject
            (
                this->groupName
                (
                    "rLambda"
                  + (nModes_ == 1 ? word::null : name(modei))
                ),
                this->runTime_.constant(),
                this->mesh_
            ),
            1/lambdas_[modei]
        );

        // Note sigma is positive on lhs of momentum eqn
        const volSymmTensorField P
        (
            twoSymm(sigma & gradU)
          - nuM_*rLambda*twoSymm(gradU)
        );

        // Viscoelastic stress equation
        fvSymmTensorMatrix sigmaEqn
        (
            fvm::ddt(alpha, rho, sigma)
          + fvm::div
            (
                alphaRhoPhi,
                sigma,
                "div(" + alphaRhoPhi.name() + ',' + sigma_.name() + ')'
            )
         ==
            alpha*rho*P
          + sigmaSource(modei, sigma)
          + fvModels.source(alpha, rho, sigma)
        );

        sigmaEqn.relax();
        fvConstraints.constrain(sigmaEqn);
        sigmaEqn.solve("sigma");
        fvConstraints.constrain(sigma);
    }

    if (sigmas_.size())
    {
        volSymmTensorField sigmaSum("sigmaSum", sigmas_[0]);

        for (label modei = 1; modei<sigmas_.size(); modei++)
        {
            sigmaSum += sigmas_[modei];
        }

        sigma_ == sigmaSum;
    }
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace laminarModels
} // End namespace Foam

// ************************************************************************* //
