// Fill out your copyright notice in the Description page of Project Settings.


#include "NavmeshCoverPointGeneratorAsyncTask.h"
#include "CoverSystemStatics.h"
#include "DrawDebugHelpers.h"
#include "CoverRecastNavMesh.h"
#include "Detour/DetourNavMesh.h"

#if DEBUG_RENDERING
static TAutoConsoleVariable<bool> CVarDrawCoverTrace(
	TEXT("r.DrawCoverTrace"),
	false,
	TEXT("Draw debug line for the initial trace used to detect cover\n"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDrawCliffTrace(
	TEXT("r.DrawCliffTrace"),
	false,
	TEXT("Draw debug line for cliff trace after the cover trace fails\n"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDrawAngleCliffTrace(
	TEXT("r.DrawAngleCliffTrace"),
	false,
	TEXT("Draw debug line for the second cliff trace if the first one hit's something, might have hit terrain\n"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDrawCoverPoints(
	TEXT("r.DrawCoverPoints"),
	false,
	TEXT("Draw final cover points after generation\n"),
	ECVF_Cheat);
#endif



#define COVER_TRACE_CHANNEL UCoverSystemStatics::CoverTraceChannel

FNavmeshCoverPointGeneratorAsyncTask::FNavmeshCoverPointGeneratorAsyncTask()
	: CoverPointMinDistance(0.0f), ScanReach(100.0f), CliffEdgeDistance(70.0f), StraightCliffErrorTolerance(100.0f),
	  NavmeshHoleCheckReach(5.0f), SmallestAgentHeight(00.0f), CoverPointGroundOffset(0.0f), NavMeshMaxZDistanceFromGround(0.0f),
	  NavmeshTileIndex(0), NavRef(nullptr)
{
}

FNavmeshCoverPointGeneratorAsyncTask::FNavmeshCoverPointGeneratorAsyncTask(const float InCoverPointMinDistance, const float InSmallestAgentHeight,
	const float InCoverPointGroundOffset, const FBox InMapBounds, const int32 InNavmeshTileIndex, ACoverRecastNavMesh* InNav)
	: CoverPointMinDistance(InCoverPointMinDistance), ScanReach(100.0f), CliffEdgeDistance(70.0f), StraightCliffErrorTolerance(100.0f),
	  NavmeshHoleCheckReach(5.0f), SmallestAgentHeight(InSmallestAgentHeight), CoverPointGroundOffset(InCoverPointGroundOffset),
	  NavMeshMaxZDistanceFromGround(InCoverPointGroundOffset * 3.0f), MapBounds(InMapBounds), NavmeshTileIndex(InNavmeshTileIndex), NavRef(InNav)
{
#if DEBUG_RENDERING
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DrawCoverPointGenerator")); 
	bDebugDraw = CVar->GetBool();;
#endif
}

FVector FNavmeshCoverPointGeneratorAsyncTask::GetEdgeDir(const FVector& EdgeStartVertex, const FVector& EdgeEndVertex) const
{
	return FVector();
}

bool FNavmeshCoverPointGeneratorAsyncTask::ScanForCoverNavMeshProjection(FDataTransferObjectCoverData& OutCoverData,
	const NavNodeRef& NodeRef, const FVector& TraceStart, const FVector& TraceDirection) const
{
	const FVector TraceEndNavMesh = TraceStart + (TraceDirection * NavmeshHoleCheckReach); // need to keep the navmesh ray cast as short as possible
	const FVector TraceEndPhysX = TraceStart + (TraceDirection * ScanReach); // the physx ray cast is longer so that it may reach slanted geometry, e.g. ramps
	const FVector SmallestAgentHeightOffset = FVector(0.0f, 0.0f, SmallestAgentHeight);

	FNavLocation Test;
	NavRef->ProjectPoint(TraceEndNavMesh, Test, FVector(80.0f));
	
	// project the point onto the navmesh: if the projection is successful then it's not a navmesh hole, so we return failure
	// if (UNavigationSystemV1::GetCurrent(World)->ProjectPointToNavigation(traceEndNavMesh, navLocation, FVector(0.1f, 0.1f, 0.1f)))
	FNavLocation NavLocation;
	if (NavRef->ProjectPoint(TraceEndNavMesh, NavLocation, FVector(0.1f)))
		return false;
	
	// to get the cover object within the hole in the navmesh we still need to do a raycast towards its general direction, at a height of SmallestAgentHeight to ensure that the cover is tall enough
	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.TraceTag = "CoverGenerator_ScanForCoverNavMeshProjection";
		
	UWorld* World = NavRef->GetWorld();
	bool bSuccess = World->LineTraceSingleByChannel(HitResult, TraceStart + SmallestAgentHeightOffset, TraceEndPhysX + SmallestAgentHeightOffset, COVER_TRACE_CHANNEL, CollisionQueryParams);
	
#if DEBUG_RENDERING
	if (bDebugDraw && CVarDrawCoverTrace.GetValueOnAnyThread())
	{
		//draw an up arrow to make it clear
		DrawDebugDirectionalArrow(World, TraceStart, TraceStart + SmallestAgentHeightOffset,
			200.0f, bSuccess ? FColor::Red : FColor::Blue, true, -1.0f, 0, 2.0f);
		DrawDebugDirectionalArrow(World, TraceStart + SmallestAgentHeightOffset, TraceEndPhysX + SmallestAgentHeightOffset,
			200.0f, bSuccess ? FColor::Red : FColor::Blue, true, -1.0f, 0, 2.0f);
	}
#endif
	
	//TODO: comment out if not needed - ledge detection logic
	if (!bSuccess)
	{
		// if we didn't hit an object with the XY-parallel ray then cast another one towards the ground from an extended location, down along the Z-axis
		// this ensures that we pick up the edges of cliffs, which are valid cover points against units below, while also discarding flat planes that tend to creep up along navmesh tile boundaries
		// if this ray doesn't hit anything then we've found a cliff's edge
		// if it hits, then cast another, similar, but slightly slanted ray to account for any non-perfectly straight cliff walls e.g. that of landscapes
		FHitResult HitResultCliff;
		const FVector CliffTraceStart = TraceEndNavMesh + (TraceDirection * CliffEdgeDistance);
		bSuccess = !World->LineTraceSingleByChannel(HitResultCliff, CliffTraceStart, CliffTraceStart - SmallestAgentHeightOffset, COVER_TRACE_CHANNEL, CollisionQueryParams);

#if DEBUG_RENDERING
		if (bDebugDraw && CVarDrawCliffTrace.GetValueOnAnyThread())
		{
			DrawDebugDirectionalArrow(World, CliffTraceStart, CliffTraceStart - SmallestAgentHeightOffset,
				200.0f, bSuccess ? FColor::Red : FColor::Blue, true, -1.0f, 0, 2.0f);
		}
#endif
		
		if (!bSuccess)
		{
			// this is the slightly slanted, 2nd ray
			bSuccess = !World->LineTraceSingleByChannel(HitResultCliff, CliffTraceStart, CliffTraceStart - SmallestAgentHeightOffset + (TraceDirection * StraightCliffErrorTolerance), COVER_TRACE_CHANNEL, CollisionQueryParams);

#if DEBUG_RENDERING
			if (bDebugDraw && CVarDrawAngleCliffTrace.GetValueOnAnyThread())
			{
				DrawDebugDirectionalArrow(World, CliffTraceStart, CliffTraceStart - SmallestAgentHeightOffset + (TraceDirection * StraightCliffErrorTolerance),
					200.0f, bSuccess ? FColor::Red : FColor::Blue, true, -1.0f, 0, 2.0f);
			}
#endif
		}
		
		if (bSuccess)
			// now that we've established that it's a cliff's edge, we need to trace into the ground to find the "cliff object"
			bSuccess = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceStart - FVector(0.0f, 0.0f, NavMeshMaxZDistanceFromGround), COVER_TRACE_CHANNEL, CollisionQueryParams);
	}

	if (!bSuccess)
		return false;

	// force fields (shields) are handled by FActorCoverPointGeneratorTask instead
	/*if (ECC_GameTraceChannel2 == HitResult.Actor->GetRootComponent()->GetCollisionObjectType())
		return false;*/
	
	OutCoverData = FDataTransferObjectCoverData(HitResult.Actor.Get(), TraceStart, false, NavmeshTileIndex, NodeRef);
	return true;
}

void FNavmeshCoverPointGeneratorAsyncTask::ProcessEdgeStep(TArray<FDataTransferObjectCoverData>& OutCoverPointsOfActors,
	const NavNodeRef& NodeRef, const FVector& EdgeStepVertex, const FVector& EdgeDir) const
{
	if (!MapBounds.IsInside(EdgeStepVertex))
		return;
	
	//no need for a loop here, it just 
	//for (int DirectionMultiplier = 1; DirectionMultiplier > -2; DirectionMultiplier -= 2)

#if DEBUG_RENDERING
	/*if (bDebugDraw)
	{
		DrawDebugDirectionalArrow(NavRef->GetWorld(), EdgeStepVertex,
		                          EdgeStepVertex + GetPerpendicularVector(EdgeDir) * ScanReach, 200.0f, FColor::Blue,
		                          true, -1.0f, 0, 2.0f);
		DrawDebugDirectionalArrow(NavRef->GetWorld(), EdgeStepVertex,
		                          EdgeStepVertex + GetPerpendicularVector(EdgeDir) * ScanReach * -1.0f, 200.0f, FColor::Blue,
		                          true, -1.0f, 0, 2.0f);
	}*/
#endif
		
	FDataTransferObjectCoverData CoverData;
	// check if we're at the edge of the map
	if (ScanForCoverNavMeshProjection(CoverData, NodeRef, EdgeStepVertex, UCoverSystemStatics::GetPerpendicularVector(EdgeDir)))
	{
		OutCoverPointsOfActors.Add(CoverData);
		return;
	}

	if (ScanForCoverNavMeshProjection(CoverData, NodeRef, EdgeStepVertex, UCoverSystemStatics::GetPerpendicularVector(EdgeDir) * -1.0f))
	{
		OutCoverPointsOfActors.Add(CoverData);
		// ReSharper disable once CppRedundantControlFlowJump
		return;
	}
}

FBox FNavmeshCoverPointGeneratorAsyncTask::GenerateCoverInBounds(TArray<FDataTransferObjectCoverData>& OutCoverPoints) const
{
	// profiling
	SCOPE_CYCLE_COUNTER(STAT_GenerateCoverInBounds);
	INC_DWORD_STAT(STAT_GenerateCoverHistoricalCount);
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateCoverAverageTime);

	NavRef->BeginBatchQuery();
	TArray<FNavPoly> Polys;
	NavRef->GetPolysInTile(NavmeshTileIndex, Polys);
	NavRef->FinishBatchQuery();
	
	for (const auto& Poly : Polys)
	{
		NavRef->BeginBatchQuery();
		TArray<FVector> Vertices;
		NavRef->GetPolyEdges(Poly.Ref, Vertices);
		NavRef->FinishBatchQuery();

		// process the navmesh vertices (called nav mesh edges for some occult reason)
		const int nVertices = Vertices.Num();
		if (nVertices > 1)
		{
			//const FVector FirstEdgeDir = GetEdgeDir(Vertices[0], Vertices[1]);
			//const FVector LastEdgeDir = GetEdgeDir(Vertices[nVertices - 2], Vertices[nVertices - 1]);
			// ReSharper disable once CppUE4CodingStandardNamingViolationWarning
			for (int iVertex = 0; iVertex < nVertices; iVertex += 2)
			{
				const FVector EdgeStartVertex = Vertices[iVertex];
				const FVector EdgeEndVertex = Vertices[iVertex + 1];
				const FVector Edge = EdgeEndVertex - EdgeStartVertex;
				const FVector EdgeDir = Edge.GetUnsafeNormal();

	#if DEBUG_RENDERING
				if (bDebugDraw)
				{
					DrawDebugDirectionalArrow(NavRef->GetWorld(), EdgeStartVertex, EdgeEndVertex, 200.0f, FColor::Purple, true, -1.0f, 0, 2.0f);
				}
	#endif

				// step through the edge in CoverPointMinDistance increments
				const int nEdgeSteps = Edge.Size() / CoverPointMinDistance;
				for (int iEdgeStep = 0; iEdgeStep <= nEdgeSteps; iEdgeStep++)
				{
					// check to the left and to optionally, to the right of the vertex's location for any blocking geometry
					// if geometry blocks the ray cast then the vertex is marked as a cover point
					ProcessEdgeStep(OutCoverPoints, Poly.Ref, EdgeStartVertex + (iEdgeStep * CoverPointMinDistance * EdgeDir) + FVector(0.0f, 0.0f, CoverPointGroundOffset), EdgeDir);
				}
					
				// process the first step if the edge was shorter than CoverPointMinDistance
				if (nEdgeSteps == 0)
				{
					ProcessEdgeStep(OutCoverPoints, Poly.Ref, EdgeStartVertex + FVector(0.0f, 0.0f, CoverPointGroundOffset), EdgeDir);
				}

				// process the end vertex; 99% of the time it's left out by the above for-loop, and in that 1% of cases we will just process the same vertex twice (likely to never happen because of floating-point division)
				ProcessEdgeStep(OutCoverPoints, Poly.Ref, EdgeEndVertex + FVector(0.0f, 0.0f, CoverPointGroundOffset), EdgeDir);

				// process the end vertex again, this time with its edge direction rotated by 45 degrees
				ProcessEdgeStep(OutCoverPoints, Poly.Ref, EdgeEndVertex + FVector(0.0f, 0.0f, CoverPointGroundOffset), FVector(FVector2D(EdgeDir).GetRotated(45.0f), EdgeDir.Z));
			}
		}
	}
	

	// return the AABB of the navmesh tile that's been processed, expanded by minimum tile height on the Z-axis
	FBox NavMeshBounds = NavRef->GetNavMeshTileBounds(NavmeshTileIndex);
	const float NavMeshTileHeight = NavRef->GetRecastMesh()->getParams()->tileHeight;
	if (NavMeshTileHeight > 0)
	{
		NavMeshBounds = NavMeshBounds.ExpandBy(FVector(0.0f, 0.0f, NavMeshTileHeight * 0.5f));
	}

	return NavMeshBounds;
}

void FNavmeshCoverPointGeneratorAsyncTask::DoWork() const
{
	// profiling
	SCOPE_CYCLE_COUNTER(STAT_GenerateCover);
	INC_DWORD_STAT(STAT_GenerateCoverHistoricalCount);
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateCoverAverageTime);
	INC_DWORD_STAT(STAT_TaskCount);

	// generate cover points
	TArray<FDataTransferObjectCoverData> CoverPoints;
	const FBox NavMeshTileArea = GenerateCoverInBounds(CoverPoints);

	// remove any cover points that don't fall on the navmesh anymore
	// happens when a newly placed cover object is placed on top of previously generated cover points
	if (!IsValid(NavRef))
		return;
	
	//TODO: consider deleting this - a few more cover points might be left over upon object removal but at the expense of fewer cover points per object. most apparent near ledges. not a big deal either way, though.
	NavRef->RemoveStaleCoverPoints(NavMeshTileArea, NavmeshTileIndex);
	 
	// add the generated cover points to the octree in a single batch
	if (!IsValid(NavRef))
		return;
		
	NavRef->AddCoverPoints(CoverPoints);

#if DEBUG_RENDERING
	if (CVarDrawCoverPoints.GetValueOnAnyThread())
	{
		for (FDataTransferObjectCoverData CoverPoint : CoverPoints)
		{
			DrawDebugSphere(NavRef->GetWorld(), CoverPoint.Location, 20.0f, 4, FColor::Purple, true, -1.0f, 0, 3.0f);
		}			
	}
#endif

	DEC_DWORD_STAT(STAT_TaskCount);
}
