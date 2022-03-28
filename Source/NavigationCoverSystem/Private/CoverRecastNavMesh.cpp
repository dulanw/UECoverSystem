// Fill out your copyright notice in the Description page of Project Settings.


#include "CoverRecastNavMesh.h"

#include "CoverSystemStatics.h"
#include "DrawDebugHelpers.h"
#include "NavmeshCoverPointGeneratorAsyncTask.h"
#include "Detour/DetourNavMesh.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_PathingGrid.h"
#include "NavMesh/PImplRecastNavMesh.h"
#include "NavMesh/RecastHelpers.h"


#if DEBUG_RENDERING
static TAutoConsoleVariable<bool> CVarDrawUpdatedTiles(
	TEXT("r.DrawUpdatedTiles"),
	false,
	TEXT("Draw debug solid box for updated tiles\n"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDrawCoverPointGenerator(
	TEXT("r.DrawCoverPointGenerator"),
	false,
	TEXT("Draw debug spheres for final cover points generated\n"),
	ECVF_Cheat);
#endif



DEFINE_LOG_CATEGORY_STATIC(CoverNavMesh, Log, All)

#define LOG_NAV_MESH(Verbosity, Format, ...) \
{ \
	UE_LOG(CoverNavMesh, Verbosity, Format, ##__VA_ARGS__); \
}

constexpr float ACoverRecastNavMesh::TileBufferInterval = 0.2f;

ACoverRecastNavMesh::ACoverRecastNavMesh()
	: Super()
{
}

ACoverRecastNavMesh::ACoverRecastNavMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CoverPointMinDistance = 2 * 30.0f;
}

void ACoverRecastNavMesh::OnNavMeshTilesUpdated(const TArray<uint32>& ChangedTiles)
{
	Super::OnNavMeshTilesUpdated(ChangedTiles);
	
	TSet<uint32> UpdatedTiles;
	for (const uint32& ChangedTile : ChangedTiles)
	{
		//don't call append, since it's just a for loop as well
		UpdatedTiles.Add(ChangedTile);
		UpdatedTilesIntervalBuffer.Add(ChangedTile);
		UpdatedTilesUntilFinishedBuffer.Add(ChangedTile);
	}
	
}

void ACoverRecastNavMesh::OnNavMeshGenerationFinished()
{
	Super::OnNavMeshGenerationFinished();

	//don't need scope lock, the actor will not call timers asynchronously 
	//FScopeLock TileUpdateLock(&TileUpdateLockObject);
	//NavmeshTilesUpdatedUntilFinishedDelegate.Broadcast(UpdatedTilesUntilFinishedBuffer);
	RegenerateCoverPoints(UpdatedTilesUntilFinishedBuffer);
	UpdatedTilesUntilFinishedBuffer.Empty();
}

void ACoverRecastNavMesh::OnNavigationBoundsChanged()
{
	Super::OnNavigationBoundsChanged();
	MapBounds = GetNavMeshBounds();
}

void ACoverRecastNavMesh::RebuildAll()
{
	Super::RebuildAll();

	if (NavDataGenerator.IsValid())
	{
		ConstructCoverOctree();
	}
}

void ACoverRecastNavMesh::BeginPlay()
{
	Super::BeginPlay();

	//might not need this timer, we can just do it on nav mesh generation finished instead of doing it at fixed intervals
	//GetWorld()->GetTimerManager().SetTimer(TileUpdateTimerHandle, this, &ACoverRecastNavMesh::ProcessQueuedTiles, TileBufferInterval, true);
}

void ACoverRecastNavMesh::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	MapBounds = GetNavMeshBounds();

	//construct octree here so we can get the nav area size
	if (!CoverOctreeController.IsValid())
	{
		//LOG_NAV_MESH(Warning, TEXT("ACoverRecastNavMesh::PostRegisterAllComponents Invalid CoverOctreeController, Constructing Octree"));
		ConstructCoverOctree();
	}
}

void ACoverRecastNavMesh::ProcessQueuedTiles()
{
	//don't need scope lock, the actor will not call timers asynchronously 
	//FScopeLock TileUpdateLock(&TileUpdateLockObject);
	if (UpdatedTilesIntervalBuffer.Num() > 0)
	{
		LOG_NAV_MESH(Log, TEXT("ACoverRecastNavMesh::ProcessQueuedTiles - tile count: %d"), UpdatedTilesIntervalBuffer.Num());
		RegenerateCoverPoints(UpdatedTilesIntervalBuffer);
		UpdatedTilesIntervalBuffer.Empty();
	}
}

void ACoverRecastNavMesh::RegenerateCoverPoints(const TSet<uint32>& UpdatedTiles)
{
	if (IsPendingKillPending())
		return;
		
	// regenerate cover points within the updated navmesh tiles
	for (uint32 TileIdx : UpdatedTiles)
	{
#if DEBUG_RENDERING
		if (CVarDrawUpdatedTiles.GetValueOnGameThread())
		{
			LOG_NAV_MESH(Log, TEXT("ACoverRecastNavMesh::RegenerateCoverPoints: %u"), TileIdx);
			//DrawDebugSolidBox(GetWorld(), GetNavMeshTileBounds(TileIdx),FColor::Red, FTransform::Identity, true);
		}
#endif
		
#if DEBUG_RENDERING
		// DrawDebugXXX calls may crash UE4 when not called from the main thread, so start synchronous tasks in case we're planning on drawing debug shapes
		if (CVarDrawCoverPointGenerator.GetValueOnGameThread())
			(new FAutoDeleteAsyncTask<FNavmeshCoverPointGeneratorAsyncTask>(CoverPointMinDistance, UCoverSystemStatics::SmallestAgentHeight,
			UCoverSystemStatics::CoverPointGroundOffset,MapBounds, TileIdx, this))->StartSynchronousTask();
		else
#endif
		(new FAutoDeleteAsyncTask<FNavmeshCoverPointGeneratorAsyncTask>(CoverPointMinDistance, UCoverSystemStatics::SmallestAgentHeight,
			UCoverSystemStatics::CoverPointGroundOffset,MapBounds, TileIdx, this))->StartBackgroundTask();
	}
}

void ACoverRecastNavMesh::ConstructCoverOctree()
{
	CoverOctreeController.Reset();

	const float Radius = GetNavMeshBounds().GetSize().Size();
	CoverOctreeController.CoverOctree = MakeShareable(new FCoverOctree(FVector(0, 0, 0), Radius));
}

void ACoverRecastNavMesh::AddCoverPoints(const TArray<FDataTransferObjectCoverData>& CoverPoints) const
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return;

	FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_Write);
	Internal_AddCoverPoints(CoverPoints);
}

FBox ACoverRecastNavMesh::EnlargeAABB(const FBox Box, float Multiplier)
{
	return Box.ExpandBy(FVector(
		(Box.Max.X - Box.Min.X) * (Multiplier - 1.0f),
		(Box.Max.Y - Box.Min.Y) * (Multiplier - 1.0f),
		(Box.Max.Z - Box.Min.Z) * (Multiplier - 1.0f)
	));
}

void ACoverRecastNavMesh::RemoveStaleCoverPoints(FBox Area, const TileIndexType StaleTileIndex)
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return;

	FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_Write);
	Internal_RemoveStaleCoverPoints(Area, StaleTileIndex);
}

void ACoverRecastNavMesh::RemoveStaleAndAddCoverPoints(FBox Area, const TileIndexType StaleTileIndex, const TArray<FDataTransferObjectCoverData>& CoverPoints)
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return;

	FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_Write);

	Internal_RemoveStaleCoverPoints(Area, StaleTileIndex);
	Internal_AddCoverPoints(CoverPoints);
}

void ACoverRecastNavMesh::Internal_AddCoverPoints(const TArray<FDataTransferObjectCoverData>& CoverPoints) const
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return;
	
	for (auto CoverPoint : CoverPoints)
	{
		//#TODO add to object map??
		// ReSharper disable once CppExpressionWithoutSideEffects
		const bool bInserted = CoverOctreeController.AddNode(CoverPoint, CoverPointMinDistance * 0.9f);
#if DEBUG_RENDERING
		static const auto CVarDrawCoverPoints = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DrawCoverPoints")); 
		if (CVarDrawCoverPoints->GetBool() && !bInserted)
		{
			DrawDebugSphere(GetWorld(), CoverPoint.Location + FVector(0.0f, 0.0f, 3.0f), 20.0f, 4, FColor::Black, true, -1.0f, 0, 4.0f);
		}
#endif
	}
	
	// optimize the octree
	CoverOctreeController.CoverOctree->ShrinkElements();
}

void ACoverRecastNavMesh::Internal_RemoveStaleCoverPoints(FBox Area, const TileIndexType StaleTileIndex)
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return;
	
	// enlarge the clean-up area to x1.5 its size
	// #NOTE 1. don't do this, when items moves instead of being deleted, then extra ones might be deleted see NOTE #2 below
	Area = EnlargeAABB(Area, 1.25f);

#if DEBUG_RENDERING
	// if (CVarDrawCoverPointGenerator.GetValueOnAnyThread())
	// {
	// 	DrawDebugBox(GetWorld(), Area.GetCenter(), Area.GetExtent(), FColor::Purple, true, -1.0f, 0, 5.0f);
	// }
#endif
	
	// find all the cover points in the specified area
	TArray<FCoverPointOctreeElement> CoverPoints;
	CoverOctreeController.FindElementsInNavOctree(Area, CoverPoints);

	for (FCoverPointOctreeElement CoverPoint : CoverPoints)
	{
		// NOTE 2, do not do this either, this will keep stale items as long as the actor is not deleted. if the actor moves then it will not be cleaned up
		// check if the cover point still has an owner and still falls on the exact same location on the navmesh as it did when it was generated
		// no need to check ProjectPoint, it will sometimes fail due to precision error which causes false positives and removes valid cover
		// just checking the tile index is enough for now
		// FNavLocation NavLocation;
		//if (CoverPoint.Data->TileIndex != StaleTileIndex && IsValid(CoverPoint.GetOwner()) && ProjectPoint(CoverPoint.Data->Location, NavLocation, FVector(0.1f, 0.1f, CoverPointGroundOffset)))
		if (CoverPoint.Data->TileIndex != StaleTileIndex && IsValid(CoverPoint.GetOwner()) )
		{
/*#if DEBUG_RENDERING
			LOG_NAV_MESH(Warning, TEXT("KEEP %d != %d, IsValid: %d, bProjectPoint: %d"), CoverPoint.Data->TileIndex, StaleTileIndex, IsValid(CoverPoint.GetOwner()), bProjectPoint);
			if (!bProjectPoint && CVarDrawCoverPointGenerator.GetValueOnAnyThread())
			{
				DrawDebugSphere(GetWorld(), CoverPoint.Data->Location + FVector(0.0f, 0.0f, 5.0f), 20.0f, 4, FColor::Orange, true, -1.0f, 0, 5.0f);
			}
#endif*/
			continue;
		}

		const FOctreeElementId2* Id = CoverOctreeController.GetElementNavOctreeId(CoverPoint.Data->Location);

		// remove the cover point from the octree
		if (Id && Id->IsValidId())
		{
/*#if DEBUG_RENDERING
			if (CVarDrawCoverPointGenerator.GetValueOnAnyThread())
			{
				//Draw the deleted one just a bit above
				DrawDebugSphere(GetWorld(), CoverPoint.Data->Location + FVector(0.0f, 0.0f, 10.0f), 20.0f, 4, FColor::Red, true, -1.0f, 0, 5.0f);
			}
#endif*/
			
			CoverOctreeController.RemoveNavOctreeElementId(*Id);
		}
		
		// remove the cover point from the element-to-id and object-to-location maps
		// #TODO remove object to location map??
		CoverOctreeController.RemoveElementNavOctreeId(CoverPoint.Data->Location);
		CoverOctreeController.CoverObjectToLocation.RemoveSingle(CoverPoint.Data->CoverObject, CoverPoint.Data->Location);
	}

	// optimize the octree
	CoverOctreeController.CoverOctree->ShrinkElements();
}

/** Internal. Calculates squared 2d distance of given point PT to segment P-Q. Values given in Recast coordinates */
static FORCEINLINE float PointDistToSegment2DSquared(const float* PT, const float* P, const float* Q)
{
	float pqx = Q[0] - P[0];
	float pqz = Q[2] - P[2];
	float dx = PT[0] - P[0];
	float dz = PT[2] - P[2];
	float d = pqx*pqx + pqz*pqz;
	float t = pqx*dx + pqz*dz;
	if (d != 0) t /= d;
	dx = P[0] + t*pqx - PT[0];
	dz = P[2] + t*pqz - PT[2];
	return dx*dx + dz*dz;
}

bool ACoverRecastNavMesh::GetCoverPointOctreeElement(FCoverPointOctreeElement& OutElement, const FVector& ElementLocation) const
{
	if (IsPendingKillPending() || !CoverOctreeController.IsValid())
		return false;

	FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_ReadOnly);
	const FOctreeElementId2* Element = CoverOctreeController.GetElementNavOctreeId(ElementLocation);
	if (Element)
	{
		OutElement = CoverOctreeController.CoverOctree->GetElementById(*Element);
		return true;
	}

	return false;	
}

bool ACoverRecastNavMesh::GetPolyEdges(NavNodeRef PolyID, TArray<FVector>& NavMeshEdgeVerts) const
{
	const FPImplRecastNavMesh* NavMeshImpl = GetRecastNavMeshImpl();
	if (NavMeshImpl == nullptr)
		return false;

	const dtNavMesh* DetourNavMesh = NavMeshImpl->GetRecastMesh();
	if (NavMeshImpl->DetourNavMesh == nullptr)
		return false;

	static const float thr = FMath::Square(0.01f);

	// get poly data from recast
	const dtPoly* Poly;
	const dtMeshTile* Tile;
	dtStatus Status = DetourNavMesh->getTileAndPolyByRef((dtPolyRef)PolyID, &Tile, &Poly);
	
	if (Poly->getType() != DT_POLYTYPE_GROUND)
	{
		return false;
	}

	const uint32 PolyIndex = DetourNavMesh->decodePolyIdPoly((dtPolyRef)PolyID);
	const dtPolyDetail* pd = &Tile->detailMeshes[PolyIndex];
	int32 CurNavEdgeVerts = 0;
	for (int j = 0, nj = (int)Poly->vertCount; j < nj; ++j)
	{
		bool bIsExternal = Poly->neis[j] == 0 || Poly->neis[j] & DT_EXT_LINK;
		
		if (Poly->getArea() == RECAST_NULL_AREA)
		{
			if (Poly->neis[j] && !(Poly->neis[j] & DT_EXT_LINK) &&
				Poly->neis[j] <= Tile->header->offMeshBase &&
				Tile->polys[Poly->neis[j] - 1].getArea() != RECAST_NULL_AREA)
			{
				bIsExternal = true;
			}
			else if (Poly->neis[j] == 0)
			{
				bIsExternal = true;
			}
		}
		else if (bIsExternal)
		{
			unsigned int k = Poly->firstLink;
			while (k != DT_NULL_LINK)
			{
				const dtLink& link = DetourNavMesh->getLink(Tile, k);
				k = link.next;

				if (link.edge == j)
				{
					//bIsConnected = true;
					bIsExternal = false;
					break;
				}
			}
		}
		
		if (!bIsExternal)
		{
			continue;
		}

		const float* V0 = &Tile->verts[Poly->verts[j] * 3];
		const float* V1 = &Tile->verts[Poly->verts[(j + 1) % nj] * 3];

		// Draw detail mesh edges which align with the actual poly edge.
		// This is really slow.
		for (int32 k = 0; k < pd->triCount; ++k)
		{
			const unsigned char* t = &(Tile->detailTris[(pd->triBase + k) * 4]);
			const float* tv[3];

			for (int32 m = 0; m < 3; ++m)
			{
				if (t[m] < Poly->vertCount)
				{
					tv[m] = &Tile->verts[Poly->verts[t[m]] * 3];
				}
				else
				{
					tv[m] = &Tile->detailVerts[(pd->vertBase + (t[m] - Poly->vertCount)) * 3];
				}
			}
			for (int m = 0, n = 2; m < 3; n=m++)
			{
				if (((t[3] >> (n*2)) & 0x3) == 0)
				{
					continue;	// Skip inner detail edges.
				}
				
				if (PointDistToSegment2DSquared(tv[n],V0,V1) < thr && PointDistToSegment2DSquared(tv[m],V0,V1) < thr)
				{
					int32 const AddIdx = NavMeshEdgeVerts.AddZeroed(2);
					NavMeshEdgeVerts[AddIdx] =  Recast2UnrealPoint(tv[n]);
					NavMeshEdgeVerts[AddIdx+1] = Recast2UnrealPoint(tv[m]);
					++CurNavEdgeVerts;
				}
			}
		}
	}

	return CurNavEdgeVerts > 0;
}
