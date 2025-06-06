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

#include "decompositionMethod.H"
#include "Time.H"
#include "globalIndex.H"
#include "syncTools.H"
#include "Tuple2.H"
#include "faceSet.H"
#include "regionSplit.H"
#include "localPointRegion.H"
#include "minData.H"
#include "FaceCellWave.H"

#include "preserveBafflesConstraint.H"
#include "preservePatchesConstraint.H"
#include "preserveFaceZonesConstraint.H"
#include "singleProcessorFaceSetsConstraint.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(decompositionMethod, 0);
    defineRunTimeSelectionTable(decompositionMethod, decomposer);
    defineRunTimeSelectionTable(decompositionMethod, distributor);
}


// * * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * //

Foam::label Foam::decompositionMethod::nWeights
(
    const pointField& points,
    const scalarField& pointWeights
) const
{
    const label localnWeights =
        points.size() ? pointWeights.size()/points.size() : 0;

    const label nWeights = returnReduce(localnWeights, maxOp<label>());

    if (localnWeights && localnWeights != nWeights)
    {
        FatalErrorInFunction
            << "Number of weights on this processor " << localnWeights
            << " does not equal the maximum number of weights " << nWeights
            << exit(FatalError);
    }

    return nWeights;
}


Foam::label Foam::decompositionMethod::checkWeights
(
    const pointField& points,
    const scalarField& pointWeights
) const
{
    const label nWeights = this->nWeights(points, pointWeights);

    if (nWeights > 1)
    {
        FatalErrorInFunction
            << "decompositionMethod " << type()
            << " does not support multiple constraints"
            << exit(FatalError);
    }

    return nWeights;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::decompositionMethod::decompositionMethod
(
    const dictionary& decompositionDict
)
:
    nProcessors_(decompositionDict.lookup<label>("numberOfSubdomains"))
{
    // Read any constraints
    wordList constraintTypes_;

    if (decompositionDict.found("constraints"))
    {
        const dictionary& constraintsList = decompositionDict.subDict
        (
            "constraints"
        );
        forAllConstIter(dictionary, constraintsList, iter)
        {
            const dictionary& dict = iter().dict();

            constraintTypes_.append(dict.lookup("type"));

            constraints_.append
            (
                decompositionConstraint::New
                (
                    dict,
                    constraintTypes_.last()
                )
            );
        }
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::autoPtr<Foam::decompositionMethod>
Foam::decompositionMethod::NewDecomposer
(
    const dictionary& decompositionDict
)
{
    const word methodType
    (
        decompositionDict.lookupBackwardsCompatible<word>
        (
            {"decomposer", "method"}
        )
    );

    Info<< "Selecting decomposer " << methodType << endl;

    libs.open
    (
        decompositionDict,
        "libs",
        decomposerConstructorTablePtr_
    );

    decomposerConstructorTable::iterator cstrIter =
        decomposerConstructorTablePtr_->find(methodType);

    if (cstrIter == decomposerConstructorTablePtr_->end())
    {
        FatalErrorInFunction
            << "Unknown decomposer "
            << methodType << nl << nl
            << "Valid decomposers are : " << endl
            << decomposerConstructorTablePtr_->sortedToc()
            << exit(FatalError);
    }

    Info<< incrIndent;

    autoPtr<decompositionMethod> methodPtr
    (
        cstrIter()
        (
            decompositionDict,
            decompositionDict.subOrEmptyDict(methodType + "Coeffs")
        )
    );

    Info<< decrIndent;

    return methodPtr;
}


Foam::autoPtr<Foam::decompositionMethod>
Foam::decompositionMethod::NewDistributor
(
    const dictionary& distributionDict
)
{
    const word methodType
    (
        distributionDict.lookupBackwardsCompatible<word>
        (
            {"distributor", "method"}
        )
    );

    Info<< "Selecting distributor " << methodType << endl;

    libs.open
    (
        distributionDict,
        "libs",
        distributorConstructorTablePtr_
    );

    distributorConstructorTable::iterator cstrIter =
        distributorConstructorTablePtr_->find(methodType);

    if (cstrIter == distributorConstructorTablePtr_->end())
    {
        FatalErrorInFunction
            << "Unknown distributor "
            << methodType << nl << nl
            << "Valid distributors are : " << endl
            << distributorConstructorTablePtr_->sortedToc()
            << exit(FatalError);
    }

    Info<< incrIndent;

    autoPtr<decompositionMethod> methodPtr
    (
        cstrIter()
        (
            distributionDict,
            distributionDict.subOrEmptyDict(methodType + "Coeffs")
        )
    );

    Info<< decrIndent;

    return methodPtr;
}


Foam::IOdictionary Foam::decompositionMethod::decomposeParDict(const Time& time)
{
    return IOdictionary
    (
        IOobject
        (
            "decomposeParDict",
            time.system(),
            time,
            IOobject::MUST_READ,
            IOobject::NO_WRITE,
            false
        )
    );
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const polyMesh& mesh,
    const pointField& points
)
{
    scalarField weights(points.size(), 1.0);

    return decompose(mesh, points, weights);
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const polyMesh& mesh,
    const labelList& fineToCoarse,
    const pointField& coarsePoints,
    const scalarField& coarseWeights
)
{
    CompactListList<label> coarseCellCells;
    calcCellCells
    (
        mesh,
        fineToCoarse,
        coarsePoints.size(),
        true,                       // use global cell labels
        coarseCellCells
    );

    // Decompose based on agglomerated points
    labelList coarseDistribution
    (
        decompose
        (
            coarseCellCells.list(),
            coarsePoints,
            coarseWeights
        )
    );

    // Rework back into decomposition for original mesh_
    labelList fineDistribution(fineToCoarse.size());

    forAll(fineDistribution, i)
    {
        fineDistribution[i] = coarseDistribution[fineToCoarse[i]];
    }

    return fineDistribution;
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const polyMesh& mesh,
    const labelList& fineToCoarse,
    const pointField& coarsePoints
)
{
    scalarField cellWeights(coarsePoints.size(), 1.0);

    return decompose
    (
        mesh,
        fineToCoarse,
        coarsePoints,
        cellWeights
    );
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const labelListList& globalCellCells,
    const pointField& cellCentres
)
{
    scalarField cellWeights(cellCentres.size(), 1.0);

    return decompose(globalCellCells, cellCentres, cellWeights);
}


Foam::labelList Foam::decompositionMethod::scaleWeights
(
    const scalarField& weights,
    label& nWeights,
    const bool distributed
)
{
    labelList intWeights;

    if (nWeights > 0)
    {
        // Calculate the scalar -> integer scaling factor
        const scalar sumWeights
        (
            distributed
          ? gSum(weights)
          : sum(weights)
        );

        // Hack for scotch which does not accept 64bit label range for weights
        const scalar scale = INT32_MAX/(2*sumWeights);
        // const scalar scale = labelMax/(2*sumWeights);


        // Convert weights to integer
        intWeights.setSize(weights.size());
        forAll(intWeights, i)
        {
            intWeights[i] = ceil(scale*weights[i]);
        }

        /*
        // Alternatively calculate a separate scaling factor
        // for each weight, it is not clear from the parMETIS or Scotch manuals
        // which method should be used.
        //
        // Calculate the scalar -> integer scaling factors
        scalarList sumWeights(nWeights, 0.0);

        forAll(weights, i)
        {
            sumWeights[i % nWeights] += weights[i];
        }

        if (distributed)
        {
            reduce(sumWeights, ListOp<sumOp<scalar>>());
        }

        scalarList scale(nWeights, 0.0);
        forAll(scale, i)
        {
            if (sumWeights[i] > small)
            {
                scale[i] = labelMax/(2*sumWeights[i]);
            }
        }

        // Convert weights to integer
        intWeights.setSize(weights.size());
        forAll(intWeights, i)
        {
            intWeights[i] = ceil(scale[i % nWeights]*weights[i]);
        }
        */

        // Calculate the sum over all processors of each weight
        labelList sumIntWeights(nWeights, 0);
        forAll(weights, i)
        {
            sumIntWeights[i % nWeights] += intWeights[i];
        }

        if (distributed)
        {
            reduce(sumIntWeights, ListOp<sumOp<label>>());
        }

        // Check that the sum of each weight is non-zero
        boolList nonZeroWeights(nWeights, false);
        label nNonZeroWeights = 0;
        forAll(sumIntWeights, i)
        {
            if (sumIntWeights[i] != 0)
            {
                nonZeroWeights[i] = true;
                nNonZeroWeights++;
            }
        }

        // If there are zero weights remove them from the weights list
        if (nNonZeroWeights != nWeights)
        {
            label j = 0;
            forAll(intWeights, i)
            {
                if (nonZeroWeights[i % nWeights])
                {
                    intWeights[j++] = intWeights[i];
                }
            }

            // Resize the weights list
            intWeights.setSize(nNonZeroWeights*(intWeights.size()/nWeights));

            // Reset the number of weight to the number of non-zero weights
            nWeights = nNonZeroWeights;
        }
    }

    return intWeights;
}


void Foam::decompositionMethod::calcCellCells
(
    const polyMesh& mesh,
    const labelList& agglom,
    const label nLocalCoarse,
    const bool parallel,
    CompactListList<label>& cellCells
)
{
    const labelList& faceOwner = mesh.faceOwner();
    const labelList& faceNeighbour = mesh.faceNeighbour();
    const polyBoundaryMesh& patches = mesh.boundaryMesh();


    // Create global cell numbers
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~

    globalIndex globalAgglom
    (
        nLocalCoarse,
        Pstream::msgType(),
        Pstream::worldComm,
        parallel
    );


    // Get agglomerate owner on other side of coupled faces
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    labelList globalNeighbour(mesh.nFaces() - mesh.nInternalFaces());

    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label facei = pp.start();
            label bFacei = pp.start() - mesh.nInternalFaces();

            forAll(pp, i)
            {
                globalNeighbour[bFacei] = globalAgglom.toGlobal
                (
                    agglom[faceOwner[facei]]
                );

                bFacei++;
                facei++;
            }
        }
    }

    // Get the cell on the other side of coupled patches
    syncTools::swapBoundaryFaceList(mesh, globalNeighbour);


    // Count number of faces (internal + coupled)
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Number of faces per coarse cell
    labelList nFacesPerCell(nLocalCoarse, 0);

    for (label facei = 0; facei < mesh.nInternalFaces(); facei++)
    {
        label own = agglom[faceOwner[facei]];
        label nei = agglom[faceNeighbour[facei]];

        nFacesPerCell[own]++;
        nFacesPerCell[nei]++;
    }

    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label facei = pp.start();
            label bFacei = pp.start() - mesh.nInternalFaces();

            forAll(pp, i)
            {
                label own = agglom[faceOwner[facei]];

                label globalNei = globalNeighbour[bFacei];
                if
                (
                   !globalAgglom.isLocal(globalNei)
                 || globalAgglom.toLocal(globalNei) != own
                )
                {
                    nFacesPerCell[own]++;
                }

                facei++;
                bFacei++;
            }
        }
    }


    // Fill in offset and data
    // ~~~~~~~~~~~~~~~~~~~~~~~

    cellCells.setSize(nFacesPerCell);

    nFacesPerCell = 0;

    labelUList& m = cellCells.m();
    const labelList& offsets = cellCells.offsets();

    // For internal faces is just offsetted owner and neighbour
    for (label facei = 0; facei < mesh.nInternalFaces(); facei++)
    {
        label own = agglom[faceOwner[facei]];
        label nei = agglom[faceNeighbour[facei]];

        m[offsets[own] + nFacesPerCell[own]++] = globalAgglom.toGlobal(nei);
        m[offsets[nei] + nFacesPerCell[nei]++] = globalAgglom.toGlobal(own);
    }

    // For boundary faces is offsetted coupled neighbour
    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label facei = pp.start();
            label bFacei = pp.start() - mesh.nInternalFaces();

            forAll(pp, i)
            {
                label own = agglom[faceOwner[facei]];

                label globalNei = globalNeighbour[bFacei];

                if
                (
                   !globalAgglom.isLocal(globalNei)
                 || globalAgglom.toLocal(globalNei) != own
                )
                {
                    m[offsets[own] + nFacesPerCell[own]++] = globalNei;
                }

                facei++;
                bFacei++;
            }
        }
    }


    // Check for duplicates connections between cells
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Done as postprocessing step since we now have cellCells.
    label newIndex = 0;
    labelHashSet nbrCells;


    if (cellCells.size() == 0)
    {
        return;
    }

    label startIndex = cellCells.offsets()[0];

    forAll(cellCells, celli)
    {
        nbrCells.clear();
        nbrCells.insert(globalAgglom.toGlobal(celli));

        label endIndex = cellCells.offsets()[celli+1];

        for (label i = startIndex; i < endIndex; i++)
        {
            if (nbrCells.insert(cellCells.m()[i]))
            {
                cellCells.m()[newIndex++] = cellCells.m()[i];
            }
        }
        startIndex = endIndex;
        cellCells.offsets()[celli+1] = newIndex;
    }

    cellCells.setSize(cellCells.size(), newIndex);
}


void Foam::decompositionMethod::calcCellCells
(
    const polyMesh& mesh,
    const labelList& agglom,
    const label nLocalCoarse,
    const bool parallel,
    CompactListList<label>& cellCells,
    CompactListList<scalar>& cellCellWeights
)
{
    const labelList& faceOwner = mesh.faceOwner();
    const labelList& faceNeighbour = mesh.faceNeighbour();
    const polyBoundaryMesh& patches = mesh.boundaryMesh();


    // Create global cell numbers
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~

    globalIndex globalAgglom
    (
        nLocalCoarse,
        Pstream::msgType(),
        Pstream::worldComm,
        parallel
    );


    // Get agglomerate owner on other side of coupled faces
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    labelList globalNeighbour(mesh.nFaces() - mesh.nInternalFaces());

    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label faceI = pp.start();
            label bFaceI = pp.start() - mesh.nInternalFaces();

            forAll(pp, i)
            {
                globalNeighbour[bFaceI] = globalAgglom.toGlobal
                (
                    agglom[faceOwner[faceI]]
                );

                bFaceI++;
                faceI++;
            }
        }
    }

    // Get the cell on the other side of coupled patches
    syncTools::swapBoundaryFaceList(mesh, globalNeighbour);


    // Count number of faces (internal + coupled)
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Number of faces per coarse cell
    labelList nFacesPerCell(nLocalCoarse, 0);

    for (label faceI = 0; faceI < mesh.nInternalFaces(); faceI++)
    {
        label own = agglom[faceOwner[faceI]];
        label nei = agglom[faceNeighbour[faceI]];

        nFacesPerCell[own]++;
        nFacesPerCell[nei]++;
    }

    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label faceI = pp.start();
            label bFaceI = pp.start()-mesh.nInternalFaces();

            forAll(pp, i)
            {
                label own = agglom[faceOwner[faceI]];

                label globalNei = globalNeighbour[bFaceI];
                if
                (
                   !globalAgglom.isLocal(globalNei)
                 || globalAgglom.toLocal(globalNei) != own
                )
                {
                    nFacesPerCell[own]++;
                }

                faceI++;
                bFaceI++;
            }
        }
    }


    // Fill in offset and data
    // ~~~~~~~~~~~~~~~~~~~~~~~

    cellCells.setSize(nFacesPerCell);
    cellCellWeights.setSize(nFacesPerCell);

    nFacesPerCell = 0;

    labelUList& m = cellCells.m();
    scalarUList& w = cellCellWeights.m();
    const labelList& offsets = cellCells.offsets();

    // For internal faces is just offsetted owner and neighbour
    for (label faceI = 0; faceI < mesh.nInternalFaces(); faceI++)
    {
        label own = agglom[faceOwner[faceI]];
        label nei = agglom[faceNeighbour[faceI]];

        label ownIndex = offsets[own] + nFacesPerCell[own]++;
        label neiIndex = offsets[nei] + nFacesPerCell[nei]++;

        m[ownIndex] = globalAgglom.toGlobal(nei);
        w[ownIndex] = mesh.magFaceAreas()[faceI];
        m[neiIndex] = globalAgglom.toGlobal(own);
        w[ownIndex] = mesh.magFaceAreas()[faceI];
    }

    // For boundary faces is offsetted coupled neighbour
    forAll(patches, patchi)
    {
        const polyPatch& pp = patches[patchi];

        if (pp.coupled() && (parallel || !isA<processorPolyPatch>(pp)))
        {
            label faceI = pp.start();
            label bFaceI = pp.start()-mesh.nInternalFaces();

            forAll(pp, i)
            {
                label own = agglom[faceOwner[faceI]];

                label globalNei = globalNeighbour[bFaceI];

                if
                (
                   !globalAgglom.isLocal(globalNei)
                 || globalAgglom.toLocal(globalNei) != own
                )
                {
                    label ownIndex = offsets[own] + nFacesPerCell[own]++;
                    m[ownIndex] = globalNei;
                    w[ownIndex] = mesh.magFaceAreas()[faceI];
                }

                faceI++;
                bFaceI++;
            }
        }
    }


    // Check for duplicates connections between cells
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Done as postprocessing step since we now have cellCells.
    label newIndex = 0;
    labelHashSet nbrCells;


    if (cellCells.size() == 0)
    {
        return;
    }

    label startIndex = cellCells.offsets()[0];

    forAll(cellCells, cellI)
    {
        nbrCells.clear();
        nbrCells.insert(globalAgglom.toGlobal(cellI));

        label endIndex = cellCells.offsets()[cellI+1];

        for (label i = startIndex; i < endIndex; i++)
        {
            if (nbrCells.insert(cellCells.m()[i]))
            {
                cellCells.m()[newIndex] = cellCells.m()[i];
                cellCellWeights.m()[newIndex] = cellCellWeights.m()[i];
                newIndex++;
            }
        }
        startIndex = endIndex;
        cellCells.offsets()[cellI+1] = newIndex;
        cellCellWeights.offsets()[cellI+1] = newIndex;
    }

    cellCells.setSize(cellCells.size(), newIndex);
    cellCellWeights.setSize(cellCells.size(), newIndex);
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const polyMesh& mesh,
    const scalarField& cellWeights,
    const boolList& blockedFace,
    const PtrList<labelList>& specifiedProcessorFaces,
    const labelList& specifiedProcessor,
    const List<labelPair>& explicitConnections
)
{
    // Any weights specified?
    const bool hasWeights =
        returnReduce(cellWeights.size(), sumOp<label>()) > 0;

    // Any processor sets?
    label nProcSets = 0;
    forAll(specifiedProcessorFaces, setI)
    {
        nProcSets += specifiedProcessorFaces[setI].size();
    }
    reduce(nProcSets, sumOp<label>());

    // Any non-mesh connections?
    label nConnections = returnReduce
    (
        explicitConnections.size(),
        sumOp<label>()
    );

    // Any faces not blocked?
    label nUnblocked = 0;
    forAll(blockedFace, facei)
    {
        if (!blockedFace[facei])
        {
            nUnblocked++;
        }
    }
    reduce(nUnblocked, sumOp<label>());


    // Either do decomposition on cell centres or on agglomeration

    labelList finalDecomp;


    if (nProcSets+nConnections+nUnblocked == 0)
    {
        // No constraints, possibly weights

        if (hasWeights)
        {
            finalDecomp = decompose
            (
                mesh,
                mesh.cellCentres(),
                cellWeights
            );
        }
        else
        {
            finalDecomp = decompose(mesh, mesh.cellCentres());
        }
    }
    else
    {
        if (debug)
        {
            Info<< "Constrained decomposition:" << endl
                << "    faces with same owner and neighbour processor : "
                << nUnblocked << endl
                << "    baffle faces with same owner processor        : "
                << nConnections << endl
                << "    faces all on same processor                   : "
                << nProcSets << endl << endl;
        }

        // Determine local regions, separated by blockedFaces
        regionSplit localRegion(mesh, blockedFace, explicitConnections, false);


        if (debug)
        {
            Info<< "Constrained decomposition:" << endl
                << "    split into " << localRegion.nLocalRegions()
                << " regions."
                << endl;
        }

        // Determine region cell centres
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // This just takes the first cell in the region. Otherwise the problem
        // is with cyclics - if we'd average the region centre might be
        // somewhere in the middle of the domain which might not be anywhere
        // near any of the cells.

        pointField regionCentres(localRegion.nLocalRegions(), point::max);

        forAll(localRegion, celli)
        {
            label regionI = localRegion[celli];

            if (regionCentres[regionI] == point::max)
            {
                regionCentres[regionI] = mesh.cellCentres()[celli];
            }
        }

        // Do decomposition on agglomeration
        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        scalarField regionWeights(localRegion.nLocalRegions(), 0);

        if (hasWeights)
        {
            forAll(localRegion, celli)
            {
                const label regionI = localRegion[celli];

                regionWeights[regionI] += cellWeights[celli];
            }
        }
        else
        {
            forAll(localRegion, celli)
            {
                label regionI = localRegion[celli];

                regionWeights[regionI] += 1.0;
            }
        }

        finalDecomp = decompose
        (
            mesh,
            localRegion,
            regionCentres,
            regionWeights
        );



        // Implement the explicitConnections since above decompose
        // does not know about them
        forAll(explicitConnections, i)
        {
            const labelPair& baffle = explicitConnections[i];
            label f0 = baffle.first();
            label f1 = baffle.second();

            if (!blockedFace[f0] && !blockedFace[f1])
            {
                // Note: what if internal faces and owner and neighbour on
                // different processor? So for now just push owner side
                // proc

                const label proci = finalDecomp[mesh.faceOwner()[f0]];

                finalDecomp[mesh.faceOwner()[f1]] = proci;
                if (mesh.isInternalFace(f1))
                {
                    finalDecomp[mesh.faceNeighbour()[f1]] = proci;
                }
            }
            else if (blockedFace[f0] != blockedFace[f1])
            {
                FatalErrorInFunction
                    << "On explicit connection between faces " << f0
                    << " and " << f1
                    << " the two blockedFace status are not equal : "
                    << blockedFace[f0] << " and " << blockedFace[f1]
                    << exit(FatalError);
            }
        }


        // blockedFaces corresponding to processor faces need to be handled
        // separately since not handled by local regionSplit. We need to
        // walk now across coupled faces and make sure to move a whole
        // global region across
        if (Pstream::parRun())
        {
            // Re-do regionSplit

            // Field on cells and faces.
            List<minData> cellData(mesh.nCells());
            List<minData> faceData(mesh.nFaces());

            // Take over blockedFaces by seeding a negative number
            // (so is always less than the decomposition)
            label nUnblocked = 0;
            forAll(blockedFace, facei)
            {
                if (blockedFace[facei])
                {
                    faceData[facei] = minData(-123);
                }
                else
                {
                    nUnblocked++;
                }
            }

            // Seed unblocked faces with destination processor
            labelList seedFaces(nUnblocked);
            List<minData> seedData(nUnblocked);
            nUnblocked = 0;

            forAll(blockedFace, facei)
            {
                if (!blockedFace[facei])
                {
                    label own = mesh.faceOwner()[facei];
                    seedFaces[nUnblocked] = facei;
                    seedData[nUnblocked] = minData(finalDecomp[own]);
                    nUnblocked++;
                }
            }


            // Propagate information inwards
            FaceCellWave<minData> deltaCalc
            (
                mesh,
                seedFaces,
                seedData,
                faceData,
                cellData,
                mesh.globalData().nTotalCells()+1
            );

            // And extract
            forAll(finalDecomp, celli)
            {
                if (cellData[celli].valid(deltaCalc.data()))
                {
                    finalDecomp[celli] = cellData[celli].data();
                }
            }
        }


        // For specifiedProcessorFaces rework the cellToProc to enforce
        // all on one processor since we can't guarantee that the input
        // to regionSplit was a single region.
        // E.g. faceSet 'a' with the cells split into two regions
        // by a notch formed by two walls
        //
        //          \   /
        //           \ /
        //    ---a----+-----a-----
        //
        //
        // Note that reworking the cellToProc might make the decomposition
        // unbalanced.
        forAll(specifiedProcessorFaces, setI)
        {
            const labelList& set = specifiedProcessorFaces[setI];

            label proci = specifiedProcessor[setI];
            if (proci == -1)
            {
                // If no processor specified use the one from the
                // 0th element
                proci = finalDecomp[mesh.faceOwner()[set[0]]];
            }

            forAll(set, fI)
            {
                const face& f = mesh.faces()[set[fI]];
                forAll(f, fp)
                {
                    const labelList& pFaces = mesh.pointFaces()[f[fp]];
                    forAll(pFaces, i)
                    {
                        label facei = pFaces[i];

                        finalDecomp[mesh.faceOwner()[facei]] = proci;
                        if (mesh.isInternalFace(facei))
                        {
                            finalDecomp[mesh.faceNeighbour()[facei]] = proci;
                        }
                    }
                }
            }
        }


        if (debug && Pstream::parRun())
        {
            labelList nbrDecomp;
            syncTools::swapBoundaryCellList(mesh, finalDecomp, nbrDecomp);

            const polyBoundaryMesh& patches = mesh.boundaryMesh();
            forAll(patches, patchi)
            {
                const polyPatch& pp = patches[patchi];
                if (pp.coupled())
                {
                    forAll(pp, i)
                    {
                        label facei = pp.start()+i;
                        label own = mesh.faceOwner()[facei];
                        label bFacei = facei-mesh.nInternalFaces();

                        if (!blockedFace[facei])
                        {
                            label ownProc = finalDecomp[own];
                            label nbrProc = nbrDecomp[bFacei];
                            if (ownProc != nbrProc)
                            {
                                FatalErrorInFunction
                                    << "patch:" << pp.name()
                                    << " face:" << facei
                                    << " at:" << mesh.faceCentres()[facei]
                                    << " ownProc:" << ownProc
                                    << " nbrProc:" << nbrProc
                                    << exit(FatalError);
                            }
                        }
                    }
                }
            }
        }
    }

    return finalDecomp;
}


void Foam::decompositionMethod::setConstraints
(
    const polyMesh& mesh,
    boolList& blockedFace,
    PtrList<labelList>& specifiedProcessorFaces,
    labelList& specifiedProcessor,
    List<labelPair>& explicitConnections
)
{
    blockedFace.setSize(mesh.nFaces());
    blockedFace = true;

    specifiedProcessorFaces.clear();
    explicitConnections.clear();

    forAll(constraints_, constraintI)
    {
        constraints_[constraintI].add
        (
            mesh,
            blockedFace,
            specifiedProcessorFaces,
            specifiedProcessor,
            explicitConnections
        );
    }
}


void Foam::decompositionMethod::applyConstraints
(
    const polyMesh& mesh,
    const boolList& blockedFace,
    const PtrList<labelList>& specifiedProcessorFaces,
    const labelList& specifiedProcessor,
    const List<labelPair>& explicitConnections,
    labelList& decomposition
)
{
    forAll(constraints_, constraintI)
    {
        constraints_[constraintI].apply
        (
            mesh,
            blockedFace,
            specifiedProcessorFaces,
            specifiedProcessor,
            explicitConnections,
            decomposition
        );
    }
}


Foam::labelList Foam::decompositionMethod::decompose
(
    const polyMesh& mesh,
    const scalarField& cellWeights
)
{
    // Collect all constraints

    boolList blockedFace;
    PtrList<labelList> specifiedProcessorFaces;
    labelList specifiedProcessor;
    List<labelPair> explicitConnections;
    setConstraints
    (
        mesh,
        blockedFace,
        specifiedProcessorFaces,
        specifiedProcessor,
        explicitConnections
    );


    // Construct decomposition method and either do decomposition on
    // cell centres or on agglomeration

    labelList finalDecomp = decompose
    (
        mesh,
        cellWeights,            // optional weights
        blockedFace,            // any cells to be combined
        specifiedProcessorFaces,// any whole cluster of cells to be kept
        specifiedProcessor,
        explicitConnections     // baffles
    );


    // Give any constraint the option of modifying the decomposition

    applyConstraints
    (
        mesh,
        blockedFace,
        specifiedProcessorFaces,
        specifiedProcessor,
        explicitConnections,
        finalDecomp
    );

    return finalDecomp;
}


// ************************************************************************* //
