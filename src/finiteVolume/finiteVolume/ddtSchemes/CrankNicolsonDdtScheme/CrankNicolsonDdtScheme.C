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

#include "CrankNicolsonDdtScheme.H"
#include "surfaceInterpolate.H"
#include "fvcDiv.H"
#include "fvMatrices.H"
#include "Constant.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace fv
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
template<class GeoField>
CrankNicolsonDdtScheme<Type>::DDt0Field<GeoField>::DDt0Field
(
    const IOobject& io,
    const fvMesh& mesh
)
:
    GeoField(io, mesh),
    startTimeIndex_(-2) // This field is for a restart and thus correct so set
                        // the start time-index to correspond to a previous run
{
    // Set the time-index to the beginning of the run to ensure the field
    // is updated during the first time-step
    this->timeIndex() = mesh.time().startTimeIndex();
}


template<class Type>
template<class GeoField>
CrankNicolsonDdtScheme<Type>::DDt0Field<GeoField>::DDt0Field
(
    const IOobject& io,
    const fvMesh& mesh,
    const dimensioned<typename GeoField::value_type>& dimType
)
:
    GeoField(io, mesh, dimType),
    startTimeIndex_(mesh.time().timeIndex())
{}


template<class Type>
template<class GeoField>
label CrankNicolsonDdtScheme<Type>::DDt0Field<GeoField>::
startTimeIndex() const
{
    return startTimeIndex_;
}


template<class Type>
template<class GeoField>
GeoField& CrankNicolsonDdtScheme<Type>::DDt0Field<GeoField>::
operator()()
{
    return *this;
}


template<class Type>
template<class GeoField>
void CrankNicolsonDdtScheme<Type>::DDt0Field<GeoField>::
operator=(const GeoField& gf)
{
    GeoField::operator=(gf);
}


template<class Type>
template<class GeoField>
typename CrankNicolsonDdtScheme<Type>::template DDt0Field<GeoField>&
CrankNicolsonDdtScheme<Type>::ddt0_
(
    const word& unTypedName,
    const dimensionSet& dims
)
{
    const word name = typedName(unTypedName);

    if (!mesh().objectRegistry::template foundObject<GeoField>(name))
    {
        const Time& runTime = mesh().time();
        word startTimeName = runTime.timeName(runTime.startTime().value());

        if
        (
            (
                runTime.timeIndex() == runTime.startTimeIndex()
             || runTime.timeIndex() == runTime.startTimeIndex() + 1
            )
         && typeIOobject<DDt0Field<GeoField>>
            (
                name,
                startTimeName,
                mesh()
            ).headerOk()
        )
        {
            regIOobject::store
            (
                new DDt0Field<GeoField>
                (
                    IOobject
                    (
                        name,
                        startTimeName,
                        mesh(),
                        IOobject::MUST_READ,
                        IOobject::AUTO_WRITE
                    ),
                    mesh()
                )
            );
        }
        else
        {
            regIOobject::store
            (
                new DDt0Field<GeoField>
                (
                    IOobject
                    (
                        name,
                        mesh().time().name(),
                        mesh(),
                        IOobject::NO_READ,
                        IOobject::AUTO_WRITE
                    ),
                    mesh(),
                    dimensioned<typename GeoField::value_type>
                    (
                        "0",
                        dims/dimTime,
                        Zero
                    )
                )
            );
        }
    }

    DDt0Field<GeoField>& ddt0 = static_cast<DDt0Field<GeoField>&>
    (
        mesh().objectRegistry::template lookupObjectRef<GeoField>(name)
    );

    return ddt0;
}


template<class Type>
template<class GeoField>
bool CrankNicolsonDdtScheme<Type>::evaluate
(
    const DDt0Field<GeoField>& ddt0
) const
{
    return ddt0.timeIndex() != mesh().time().timeIndex();
}


template<class Type>
template<class GeoField>
scalar CrankNicolsonDdtScheme<Type>::coef_
(
    const DDt0Field<GeoField>& ddt0
) const
{
    if (mesh().time().timeIndex() > ddt0.startTimeIndex())
    {
        return 1 + ocCoeff();
    }
    else
    {
        return 1;
    }
}


template<class Type>
template<class GeoField>
scalar CrankNicolsonDdtScheme<Type>::coef0_
(
    const DDt0Field<GeoField>& ddt0
) const
{
    if (mesh().time().timeIndex() > ddt0.startTimeIndex() + 1)
    {
        return 1 + ocCoeff();
    }
    else
    {
        return 1;
    }
}


template<class Type>
template<class GeoField>
dimensionedScalar CrankNicolsonDdtScheme<Type>::rDtCoef_
(
    const DDt0Field<GeoField>& ddt0
) const
{
    return coef_(ddt0)/mesh().time().deltaT();
}


template<class Type>
template<class GeoField>
dimensionedScalar CrankNicolsonDdtScheme<Type>::rDtCoef0_
(
    const DDt0Field<GeoField>& ddt0
) const
{
    return coef0_(ddt0)/mesh().time().deltaT0();
}


template<class Type>
template<class GeoField>
tmp<GeoField> CrankNicolsonDdtScheme<Type>::offCentre_
(
    const GeoField& ddt0
) const
{
    if (ocCoeff() < 1)
    {
        return ocCoeff()*ddt0;
    }
    else
    {
        return ddt0;
    }
}


template<class Type>
const FieldField<volMesh::PatchField, Type>& ff
(
    const FieldField<volMesh::PatchField, Type>& bf
)
{
    return bf;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class Type>
CrankNicolsonDdtScheme<Type>::CrankNicolsonDdtScheme(const fvMesh& mesh)
:
    ddtScheme<Type>(mesh),
    ocCoeff_(new Function1s::Constant<scalar>("ocCoeff", 1))
{
    // Ensure the old-old-time cell volumes are available
    // for moving meshes
    if (mesh.moving())
    {
        mesh.V00();
    }
}


template<class Type>
CrankNicolsonDdtScheme<Type>::CrankNicolsonDdtScheme
(
    const fvMesh& mesh,
    Istream& is
)
:
    ddtScheme<Type>(mesh, is)
{
    token firstToken(is);

    if (firstToken.isNumber())
    {
        const scalar ocCoeff = firstToken.number();

        if (ocCoeff < 0 || ocCoeff > 1)
        {
            FatalIOErrorInFunction(is)
                << "Off-centreing coefficient = " << ocCoeff
                << " should be >= 0 and <= 1"
                << exit(FatalIOError);
        }

        ocCoeff_ =
            new Function1s::Constant<scalar>("ocCoeff", ocCoeff);
    }
    else
    {
        is.putBack(firstToken);
        dictionary dict(is);
        ocCoeff_ =
            Function1<scalar>::New
            (
                "ocCoeff",
                mesh.time().userUnits(),
                unitFraction,
                dict
            );
    }

    // Ensure the old-old-time cell volumes are available
    // for moving meshes
    if (mesh.moving())
    {
        mesh.V00();
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

template<class Type>
tmp<VolField<Type>>
CrankNicolsonDdtScheme<Type>::fvcDdt
(
    const dimensioned<Type>& dt
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + dt.name() + ')',
            dt.dimensions()
        );

    const word ddtName("ddt(" + dt.name() + ')');

    tmp<VolField<Type>> tdtdt
    (
        VolField<Type>::New
        (
            ddtName,
            mesh(),
            dimensioned<Type>
            (
                "0",
                dt.dimensions()/dimTime,
                Zero
            )
        )
    );

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            dimensionedScalar rDtCoef0 = rDtCoef0_(ddt0);

            ddt0.internalFieldRef() =
            (
                (rDtCoef0*dt)*(mesh().V0() - mesh().V00())
              - mesh().V00()*offCentre_(ddt0.internalField())
            )/mesh().V0();
        }

        tdtdt.ref().internalFieldRef() =
        (
            (rDtCoef*dt)*(mesh().V() - mesh().V0())
          - mesh().V0()*offCentre_(ddt0.internalField())
        )/mesh().V();
    }

    return tdtdt;
}


template<class Type>
tmp<VolField<Type>>
CrankNicolsonDdtScheme<Type>::fvcDdt
(
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + vf.name() + ')',
            vf.dimensions()
        );

    const word ddtName("ddt(" + vf.name() + ')');

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()*vf.oldTime().primitiveField()
                  - mesh().V00()*vf.oldTime().oldTime().primitiveField()
                ) - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    vf.oldTime().boundaryField()
                  - vf.oldTime().oldTime().boundaryField()
                ) - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        return VolField<Type>::New
        (
            ddtName,
            (
                rDtCoef*
                (
                    mesh().V()*vf
                  - mesh().V0()*vf.oldTime()
                ) - mesh().V0()*offCentre_(ddt0()())
            )/mesh().V(),
            rDtCoef.value()*
            (
                vf.boundaryField() - vf.oldTime().boundaryField()
            ) - offCentre_(ff(ddt0.boundaryField()))
        );
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*(vf.oldTime() - vf.oldTime().oldTime())
                 - offCentre_(ddt0());
        }

        return VolField<Type>::New
        (
            ddtName,
            rDtCoef*(vf - vf.oldTime()) - offCentre_(ddt0())
        );
    }
}


template<class Type>
tmp<VolField<Type>>
CrankNicolsonDdtScheme<Type>::fvcDdt
(
    const dimensionedScalar& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + rho.name() + ',' + vf.name() + ')',
            rho.dimensions()*vf.dimensions()
        );

    const word ddtName("ddt(" + rho.name() + ',' + vf.name() + ')');

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*rho.value()*
                (
                    mesh().V0()*vf.oldTime().primitiveField()
                  - mesh().V00()*vf.oldTime().oldTime().primitiveField()
                ) - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*rho.value()*
                (
                    vf.oldTime().boundaryField()
                  - vf.oldTime().oldTime().boundaryField()
                ) - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        return VolField<Type>::New
        (
            ddtName,
            (
                rDtCoef*rho*
                (
                    mesh().V()*vf()
                  - mesh().V0()*vf.oldTime()()
                ) - mesh().V0()*offCentre_(ddt0())()
            )/mesh().V(),
            rDtCoef.value()*rho.value()*
            (
                vf.boundaryField() - vf.oldTime().boundaryField()
            ) - offCentre_(ff(ddt0.boundaryField()))
        );
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*rho*(vf.oldTime() - vf.oldTime().oldTime())
                 - offCentre_(ddt0());
        }

        return VolField<Type>::New
        (
            ddtName,
            rDtCoef*rho*(vf - vf.oldTime()) - offCentre_(ddt0())
        );
    }
}


template<class Type>
tmp<VolField<Type>>
CrankNicolsonDdtScheme<Type>::fvcDdt
(
    const volScalarField& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + rho.name() + ',' + vf.name() + ')',
            rho.dimensions()*vf.dimensions()
        );

    const word ddtName("ddt(" + rho.name() + ',' + vf.name() + ')');

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()*rho.oldTime().primitiveField()
                   *vf.oldTime().primitiveField()
                  - mesh().V00()*rho.oldTime().oldTime().primitiveField()
                   *vf.oldTime().oldTime().primitiveField()
                ) - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    rho.oldTime().boundaryField()
                   *vf.oldTime().boundaryField()
                  - rho.oldTime().oldTime().boundaryField()
                   *vf.oldTime().oldTime().boundaryField()
                ) - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        return VolField<Type>::New
        (
            ddtName,
            (
                rDtCoef*
                (
                    mesh().V()*rho()*vf()
                  - mesh().V0()*rho.oldTime()()
                   *vf.oldTime()()
                ) - mesh().V00()*offCentre_(ddt0())()
            )/mesh().V(),
            rDtCoef.value()*
            (
                rho.boundaryField()*vf.boundaryField()
              - rho.oldTime().boundaryField()*vf.oldTime().boundaryField()
            ) - offCentre_(ff(ddt0.boundaryField()))
        );
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*
            (
                rho.oldTime()*vf.oldTime()
              - rho.oldTime().oldTime()*vf.oldTime().oldTime()
            ) - offCentre_(ddt0());
        }

        return VolField<Type>::New
        (
            ddtName,
            rDtCoef*(rho*vf - rho.oldTime()*vf.oldTime()) - offCentre_(ddt0())
        );
    }
}


template<class Type>
tmp<VolField<Type>>
CrankNicolsonDdtScheme<Type>::fvcDdt
(
    const volScalarField& alpha,
    const volScalarField& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + alpha.name() + ',' + rho.name() + ',' + vf.name() + ')',
            alpha.dimensions()*rho.dimensions()*vf.dimensions()
        );

    const word ddtName
    (
        "ddt(" + alpha.name() + ',' + rho.name() + ',' + vf.name() + ')'
    );

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()
                   *alpha.oldTime().primitiveField()
                   *rho.oldTime().primitiveField()
                   *vf.oldTime().primitiveField()

                  - mesh().V00()
                   *alpha.oldTime().oldTime().primitiveField()
                   *rho.oldTime().oldTime().primitiveField()
                   *vf.oldTime().oldTime().primitiveField()
                ) - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    alpha.oldTime().boundaryField()
                   *rho.oldTime().boundaryField()
                   *vf.oldTime().boundaryField()

                  - alpha.oldTime().oldTime().boundaryField()
                   *rho.oldTime().oldTime().boundaryField()
                   *vf.oldTime().oldTime().boundaryField()
                ) - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        return VolField<Type>::New
        (
            ddtName,
            (
                rDtCoef.value()*
                (
                    mesh().V()*alpha()*rho()*vf()
                  - mesh().V0()*alpha.oldTime()()*rho.oldTime()()*vf.oldTime()()
                ) - mesh().V00()*offCentre_(ddt0())()
            )/mesh().V(),
            rDtCoef.value()*
            (
                alpha.boundaryField()
               *rho.boundaryField()
               *vf.boundaryField()

              - alpha.oldTime().boundaryField()
               *rho.oldTime().boundaryField()
               *vf.oldTime().boundaryField()
            ) - offCentre_(ff(ddt0.boundaryField()))
        );
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*
            (
                alpha.oldTime()
               *rho.oldTime()
               *vf.oldTime()

              - alpha.oldTime().oldTime()
               *rho.oldTime().oldTime()
               *vf.oldTime().oldTime()
            ) - offCentre_(ddt0());
        }

        return VolField<Type>::New
        (
            ddtName,
            rDtCoef
           *(
                alpha*rho*vf
              - alpha.oldTime()*rho.oldTime()*vf.oldTime()
            )
          - offCentre_(ddt0())
        );
    }
}


template<class Type>
tmp<fvMatrix<Type>>
CrankNicolsonDdtScheme<Type>::fvmDdt
(
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + vf.name() + ')',
            vf.dimensions()
        );

    tmp<fvMatrix<Type>> tfvm
    (
        new fvMatrix<Type>
        (
            vf,
            vf.dimensions()*dimVolume/dimTime
        )
    );

    fvMatrix<Type>& fvm = tfvm.ref();

    const scalar rDtCoef = rDtCoef_(ddt0).value();
    fvm.diag() = rDtCoef*mesh().V();

    vf.oldTime().oldTime();

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()*vf.oldTime().primitiveField()
                  - mesh().V00()*vf.oldTime().oldTime().primitiveField()
                )
              - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    vf.oldTime().boundaryField()
                  - vf.oldTime().oldTime().boundaryField()
                )
              - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        fvm.source() =
        (
            rDtCoef*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V0();
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*(vf.oldTime() - vf.oldTime().oldTime())
                 - offCentre_(ddt0());
        }

        fvm.source() =
        (
            rDtCoef*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V();
    }

    return tfvm;
}


template<class Type>
tmp<fvMatrix<Type>>
CrankNicolsonDdtScheme<Type>::fvmDdt
(
    const dimensionedScalar& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + rho.name() + ',' + vf.name() + ')',
            rho.dimensions()*vf.dimensions()
        );

    tmp<fvMatrix<Type>> tfvm
    (
        new fvMatrix<Type>
        (
            vf,
            rho.dimensions()*vf.dimensions()*dimVolume/dimTime
        )
    );
    fvMatrix<Type>& fvm = tfvm.ref();

    const scalar rDtCoef = rDtCoef_(ddt0).value();
    fvm.diag() = rDtCoef*rho.value()*mesh().V();

    vf.oldTime().oldTime();

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*rho.value()*
                (
                    mesh().V0()*vf.oldTime().primitiveField()
                  - mesh().V00()*vf.oldTime().oldTime().primitiveField()
                )
              - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*rho.value()*
                (
                    vf.oldTime().boundaryField()
                  - vf.oldTime().oldTime().boundaryField()
                )
              - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        fvm.source() =
        (
            rDtCoef*rho.value()*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V0();
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*rho*(vf.oldTime() - vf.oldTime().oldTime())
                 - offCentre_(ddt0());
        }

        fvm.source() =
        (
            rDtCoef*rho.value()*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V();
    }

    return tfvm;
}


template<class Type>
tmp<fvMatrix<Type>>
CrankNicolsonDdtScheme<Type>::fvmDdt
(
    const volScalarField& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + rho.name() + ',' + vf.name() + ')',
            rho.dimensions()*vf.dimensions()
        );

    tmp<fvMatrix<Type>> tfvm
    (
        new fvMatrix<Type>
        (
            vf,
            rho.dimensions()*vf.dimensions()*dimVolume/dimTime
        )
    );
    fvMatrix<Type>& fvm = tfvm.ref();

    const scalar rDtCoef = rDtCoef_(ddt0).value();
    fvm.diag() = rDtCoef*rho.primitiveField()*mesh().V();

    vf.oldTime().oldTime();
    rho.oldTime().oldTime();

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()*rho.oldTime().primitiveField()
                   *vf.oldTime().primitiveField()
                  - mesh().V00()*rho.oldTime().oldTime().primitiveField()
                   *vf.oldTime().oldTime().primitiveField()
                )
              - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    rho.oldTime().boundaryField()
                   *vf.oldTime().boundaryField()
                  - rho.oldTime().oldTime().boundaryField()
                   *vf.oldTime().oldTime().boundaryField()
                )
              - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        fvm.source() =
        (
            rDtCoef*rho.oldTime().primitiveField()*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V0();
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*
            (
                rho.oldTime()*vf.oldTime()
              - rho.oldTime().oldTime()*vf.oldTime().oldTime()
            ) - offCentre_(ddt0());
        }

        fvm.source() =
        (
            rDtCoef*rho.oldTime().primitiveField()*vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V();
    }

    return tfvm;
}


template<class Type>
tmp<fvMatrix<Type>>
CrankNicolsonDdtScheme<Type>::fvmDdt
(
    const volScalarField& alpha,
    const volScalarField& rho,
    const VolField<Type>& vf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddt0(" + alpha.name() + ',' + rho.name() + ',' + vf.name() + ')',
            alpha.dimensions()*rho.dimensions()*vf.dimensions()
        );

    tmp<fvMatrix<Type>> tfvm
    (
        new fvMatrix<Type>
        (
            vf,
            alpha.dimensions()
           *rho.dimensions()
           *vf.dimensions()
           *dimVolume
           /dimTime
        )
    );
    fvMatrix<Type>& fvm = tfvm.ref();

    const scalar rDtCoef = rDtCoef_(ddt0).value();
    fvm.diag() = rDtCoef*alpha.primitiveField()*rho.primitiveField()*mesh().V();

    vf.oldTime().oldTime();
    alpha.oldTime().oldTime();
    rho.oldTime().oldTime();

    if (mesh().moving())
    {
        if (evaluate(ddt0))
        {
            const scalar rDtCoef0 = rDtCoef0_(ddt0).value();

            ddt0.primitiveFieldRef() =
            (
                rDtCoef0*
                (
                    mesh().V0()
                   *alpha.oldTime().primitiveField()
                   *rho.oldTime().primitiveField()
                   *vf.oldTime().primitiveField()

                  - mesh().V00()
                   *alpha.oldTime().oldTime().primitiveField()
                   *rho.oldTime().oldTime().primitiveField()
                   *vf.oldTime().oldTime().primitiveField()
                )
              - mesh().V00()*offCentre_(ddt0.primitiveField())
            )/mesh().V0();

            ddt0.boundaryFieldRef() =
            (
                rDtCoef0*
                (
                    alpha.oldTime().boundaryField()
                   *rho.oldTime().boundaryField()
                   *vf.oldTime().boundaryField()

                  - alpha.oldTime().oldTime().boundaryField()
                   *rho.oldTime().oldTime().boundaryField()
                   *vf.oldTime().oldTime().boundaryField()
                )
              - offCentre_(ff(ddt0.boundaryField()))
            );
        }

        fvm.source() =
        (
            rDtCoef
           *alpha.oldTime().primitiveField()
           *rho.oldTime().primitiveField()
           *vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V0();
    }
    else
    {
        if (evaluate(ddt0))
        {
            ddt0 = rDtCoef0_(ddt0)*
            (
                alpha.oldTime()
               *rho.oldTime()
               *vf.oldTime()

              - alpha.oldTime().oldTime()
               *rho.oldTime().oldTime()
               *vf.oldTime().oldTime()
            ) - offCentre_(ddt0());
        }

        fvm.source() =
        (
            rDtCoef
           *alpha.oldTime().primitiveField()
           *rho.oldTime().primitiveField()
           *vf.oldTime().primitiveField()
          + offCentre_(ddt0.primitiveField())
        )*mesh().V();
    }

    return tfvm;
}


template<class Type>
tmp<typename CrankNicolsonDdtScheme<Type>::fluxFieldType>
CrankNicolsonDdtScheme<Type>::fvcDdtUfCorr
(
    const VolField<Type>& U,
    const SurfaceField<Type>& Uf
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddtCorrDdt0(" + U.name() + ')',
            U.dimensions()
        );

    DDt0Field<SurfaceField<Type>>& dUfdt0 =
        ddt0_<SurfaceField<Type>>
        (
            "ddtCorrDdt0(" + Uf.name() + ')',
            Uf.dimensions()
        );

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (evaluate(ddt0))
    {
        ddt0 =
            rDtCoef0_(ddt0)*(U.oldTime() - U.oldTime().oldTime())
          - offCentre_(ddt0());
    }

    if (evaluate(dUfdt0))
    {
        dUfdt0 =
            rDtCoef0_(dUfdt0)*(Uf.oldTime() - Uf.oldTime().oldTime())
          - offCentre_(dUfdt0());
    }

    return fluxFieldType::New
    (
        "ddtCorr(" + U.name() + ',' + Uf.name() + ')',
        this->fvcDdtPhiCoeff(U.oldTime(), mesh().Sf() & Uf.oldTime())
       *(
            mesh().Sf()
          & (
                (rDtCoef*Uf.oldTime() + offCentre_(dUfdt0()))
              - fvc::interpolate(rDtCoef*U.oldTime() + offCentre_(ddt0()))
            )
        )
    );
}


template<class Type>
tmp<typename CrankNicolsonDdtScheme<Type>::fluxFieldType>
CrankNicolsonDdtScheme<Type>::fvcDdtPhiCorr
(
    const VolField<Type>& U,
    const fluxFieldType& phi
)
{
    DDt0Field<VolField<Type>>& ddt0 =
        ddt0_<VolField<Type>>
        (
            "ddtCorrDdt0(" + U.name() + ')',
            U.dimensions()
        );

    DDt0Field<fluxFieldType>& dphidt0 =
        ddt0_<fluxFieldType>
        (
            "ddtCorrDdt0(" + phi.name() + ')',
            phi.dimensions()
        );

    dimensionedScalar rDtCoef = rDtCoef_(ddt0);

    if (evaluate(ddt0))
    {
        ddt0 =
            rDtCoef0_(ddt0)*(U.oldTime() - U.oldTime().oldTime())
          - offCentre_(ddt0());
    }

    if (evaluate(dphidt0))
    {
        dphidt0 =
            rDtCoef0_(dphidt0)*(phi.oldTime() - phi.oldTime().oldTime())
          - offCentre_(dphidt0());
    }

    return fluxFieldType::New
    (
        "ddtCorr(" + U.name() + ',' + phi.name() + ')',
        this->fvcDdtPhiCoeff(U.oldTime(), phi.oldTime())
       *(
            (rDtCoef*phi.oldTime() + offCentre_(dphidt0()))
          - fvc::dotInterpolate
            (
                mesh().Sf(),
                rDtCoef*U.oldTime() + offCentre_(ddt0())
            )
        )
    );
}


template<class Type>
tmp<typename CrankNicolsonDdtScheme<Type>::fluxFieldType>
CrankNicolsonDdtScheme<Type>::fvcDdtUfCorr
(
    const volScalarField& rho,
    const VolField<Type>& U,
    const SurfaceField<Type>& rhoUf
)
{
    if
    (
        U.dimensions() == dimVelocity
     && rhoUf.dimensions() == rho.dimensions()*dimVelocity
    )
    {
        DDt0Field<VolField<Type>>& ddt0 =
            ddt0_<VolField<Type>>
            (
                "ddtCorrDdt0(" + rho.name() + ',' + U.name() + ')',
                rho.dimensions()*U.dimensions()
            );

        DDt0Field<SurfaceField<Type>>& drhoUfdt0 =
            ddt0_<SurfaceField<Type>>
            (
                "ddtCorrDdt0(" + rhoUf.name() + ')',
                rhoUf.dimensions()
            );

        dimensionedScalar rDtCoef = rDtCoef_(ddt0);

        VolField<Type> rhoU0
        (
            rho.oldTime()*U.oldTime()
        );

        if (evaluate(ddt0))
        {
            ddt0 =
                rDtCoef0_(ddt0)
               *(rhoU0 - rho.oldTime().oldTime()*U.oldTime().oldTime())
              - offCentre_(ddt0());
        }

        if (evaluate(drhoUfdt0))
        {
            drhoUfdt0 =
                rDtCoef0_(drhoUfdt0)
               *(rhoUf.oldTime() - rhoUf.oldTime().oldTime())
              - offCentre_(drhoUfdt0());
        }

        return fluxFieldType::New
        (
            "ddtCorr(" + rho.name() + ',' + U.name() + ',' + rhoUf.name() + ')',
            this->fvcDdtPhiCoeff
            (
                rhoU0,
                mesh().Sf() & rhoUf.oldTime(),
                rho.oldTime()
            )
           *(
                mesh().Sf()
              & (
                    (rDtCoef*rhoUf.oldTime() + offCentre_(drhoUfdt0()))
                  - fvc::interpolate(rDtCoef*rhoU0 + offCentre_(ddt0()))
                )
            )
        );
    }
    else if
    (
        U.dimensions() == rho.dimensions()*dimVelocity
     && rhoUf.dimensions() == rho.dimensions()*dimVelocity
    )
    {
        DDt0Field<VolField<Type>>& ddt0 =
            ddt0_<VolField<Type>>
            (
                "ddtCorrDdt0(" + U.name() + ')',
                U.dimensions()
            );

        DDt0Field<SurfaceField<Type>>& drhoUfdt0 =
            ddt0_<SurfaceField<Type>>
            (
                "ddtCorrDdt0(" + rhoUf.name() + ')',
                rhoUf.dimensions()
            );

        dimensionedScalar rDtCoef = rDtCoef_(ddt0);

        if (evaluate(ddt0))
        {
            ddt0 =
                rDtCoef0_(ddt0)*(U.oldTime() - U.oldTime().oldTime())
              - offCentre_(ddt0());
        }

        if (evaluate(drhoUfdt0))
        {
            drhoUfdt0 =
                rDtCoef0_(drhoUfdt0)
               *(rhoUf.oldTime() - rhoUf.oldTime().oldTime())
              - offCentre_(drhoUfdt0());
        }

        return fluxFieldType::New
        (
            "ddtCorr(" + U.name() + ',' + rhoUf.name() + ')',
            this->fvcDdtPhiCoeff
            (
                U.oldTime(),
                mesh().Sf() & rhoUf.oldTime(),
                rho.oldTime()
            )
           *(
                mesh().Sf()
              & (
                    (rDtCoef*rhoUf.oldTime() + offCentre_(drhoUfdt0()))
                  - fvc::interpolate
                    (
                        rDtCoef*U.oldTime() + offCentre_(ddt0())
                    )
                )
            )
        );
    }
    else
    {
        FatalErrorInFunction
            << "dimensions of rhoUf are not correct"
            << abort(FatalError);

        return fluxFieldType::null();
    }
}


template<class Type>
tmp<typename CrankNicolsonDdtScheme<Type>::fluxFieldType>
CrankNicolsonDdtScheme<Type>::fvcDdtPhiCorr
(
    const volScalarField& rho,
    const VolField<Type>& U,
    const fluxFieldType& phi
)
{
    if
    (
        U.dimensions() == dimVelocity
     && phi.dimensions() == rho.dimensions()*dimVolumetricFlux
    )
    {
        DDt0Field<VolField<Type>>& ddt0 =
            ddt0_<VolField<Type>>
            (
                "ddtCorrDdt0(" + rho.name() + ',' + U.name() + ')',
                rho.dimensions()*U.dimensions()
            );

        DDt0Field<fluxFieldType>& dphidt0 =
            ddt0_<fluxFieldType>
            (
                "ddtCorrDdt0(" + phi.name() + ')',
                phi.dimensions()
            );

        dimensionedScalar rDtCoef = rDtCoef_(ddt0);

        VolField<Type> rhoU0
        (
            rho.oldTime()*U.oldTime()
        );

        if (evaluate(ddt0))
        {
            ddt0 =
                rDtCoef0_(ddt0)
               *(rhoU0 - rho.oldTime().oldTime()*U.oldTime().oldTime())
              - offCentre_(ddt0());
        }

        if (evaluate(dphidt0))
        {
            dphidt0 =
                rDtCoef0_(dphidt0)
              *(phi.oldTime() - phi.oldTime().oldTime())
              - offCentre_(dphidt0());
        }

        return fluxFieldType::New
        (
            "ddtCorr(" + rho.name() + ',' + U.name() + ',' + phi.name() + ')',
            this->fvcDdtPhiCoeff(rhoU0, phi.oldTime(), rho.oldTime())
           *(
                (rDtCoef*phi.oldTime() + offCentre_(dphidt0()))
              - fvc::dotInterpolate
                (
                    mesh().Sf(),
                    rDtCoef*rhoU0 + offCentre_(ddt0())
                )
            )
        );
    }
    else if
    (
        U.dimensions() == rho.dimensions()*dimVelocity
     && phi.dimensions() == rho.dimensions()*dimVolumetricFlux
    )
    {
        DDt0Field<VolField<Type>>& ddt0 =
            ddt0_<VolField<Type>>
            (
                "ddtCorrDdt0(" + U.name() + ')',
                U.dimensions()
            );

        DDt0Field<fluxFieldType>& dphidt0 =
            ddt0_<fluxFieldType>
            (
                "ddtCorrDdt0(" + phi.name() + ')',
                phi.dimensions()
            );

        dimensionedScalar rDtCoef = rDtCoef_(ddt0);

        if (evaluate(ddt0))
        {
            ddt0 =
                rDtCoef0_(ddt0)*(U.oldTime() - U.oldTime().oldTime())
              - offCentre_(ddt0());
        }

        if (evaluate(dphidt0))
        {
            dphidt0 =
                rDtCoef0_(dphidt0)*(phi.oldTime() - phi.oldTime().oldTime())
              - offCentre_(dphidt0());
        }

        return fluxFieldType::New
        (
            "ddtCorr(" + U.name() + ',' + phi.name() + ')',
            this->fvcDdtPhiCoeff(U.oldTime(), phi.oldTime(), rho.oldTime())
           *(
                (rDtCoef*phi.oldTime() + offCentre_(dphidt0()))
              - fvc::dotInterpolate
                (
                    mesh().Sf(),
                    rDtCoef*U.oldTime() + offCentre_(ddt0())
                )
            )
        );
    }
    else
    {
        FatalErrorInFunction
            << "dimensions of phi are not correct"
            << abort(FatalError);

        return fluxFieldType::null();
    }
}


template<class Type>
tmp<surfaceScalarField> CrankNicolsonDdtScheme<Type>::meshPhi
(
    const VolField<Type>& vf
)
{
    DDt0Field<surfaceScalarField>& meshPhi0 =
        ddt0_<surfaceScalarField>("meshPhi0", dimVolume);

    if (evaluate(meshPhi0))
    {
        meshPhi0 =
            coef0_(meshPhi0)*mesh().phi().oldTime() - offCentre_(meshPhi0());
    }

    return surfaceScalarField::New
    (
        mesh().phi().name(),
        coef_(meshPhi0)*mesh().phi() - offCentre_(meshPhi0())
    );
}


template<class Type>
tmp<scalarField> CrankNicolsonDdtScheme<Type>::meshPhi
(
    const VolField<Type>& vf,
    const label patchi
)
{
    DDt0Field<surfaceScalarField>& meshPhi0 =
        ddt0_<surfaceScalarField>("meshPhi0", dimVolume);

    if (evaluate(meshPhi0))
    {
        meshPhi0 =
            coef0_(meshPhi0)*mesh().phi().oldTime() - offCentre_(meshPhi0());
    }

    return
    (
        coef_(meshPhi0)*mesh().phi().boundaryField()[patchi]
      - offCentre_
        (
            static_cast<const scalarField&>(meshPhi0().boundaryField()[patchi])
        )
    );
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fv

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
