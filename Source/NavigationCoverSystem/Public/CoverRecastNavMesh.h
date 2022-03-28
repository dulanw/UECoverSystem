// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoverOctree.h"
#include "CoverOctreeController.h"
#include "NavMesh/RecastNavMesh.h"
#include "CoverRecastNavMesh.generated.h"

/**
 * 
 */
UCLASS()
class NAVIGATIONCOVERSYSTEM_API ACoverRecastNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()
	
public:
	ACoverRecastNavMesh();

	explicit ACoverRecastNavMesh(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	
	virtual void PostRegisterAllComponents() override;

	//~ Begin ANavigationData Interface
	
	/** called after regenerating tiles */
	virtual void OnNavMeshTilesUpdated(const TArray<uint32>& ChangedTiles) override;
	
	/** Event from generator that navmesh build has finished */
	virtual void OnNavMeshGenerationFinished() override;
	
	virtual void OnNavigationBoundsChanged() override;

	/** Triggers rebuild in case navigation supports it */
	virtual void RebuildAll() override;
	
	//~ End ANavigationData Interface
	
	/**
	 * Minimum distance between cover points.
	 * For the demo, this is UCoverFinderService::WeaponLeanOffset * 2
	 * Used by FNavmeshCoverPointGeneratorTask.
	 */
	float CoverPointMinDistance;
	
	// AABB used to filter out cover points on the edges of the map.
	FBox MapBounds;
	
protected:
	TSet<uint32> UpdatedTilesIntervalBuffer;

	TSet<uint32> UpdatedTilesUntilFinishedBuffer;

	/**
	 * Lock used for interval-buffered tile updates.
	 * this is probably not needed
	 */
	//FCriticalSection TileUpdateLockObject;

	static const float TileBufferInterval;

	FTimerHandle TileUpdateTimerHandle;

	void ProcessQueuedTiles();

	/**
	 * @brief start async workers to regenerate the cover points for the 
	 * @param UpdatedTiles updated tiles idx 
	 */
	void RegenerateCoverPoints(const TSet<uint32>& UpdatedTiles);
	
	/**
	 * Thread lock for CoverOctree and ElementToIDLockObject
	 */
	mutable FRWLock CoverDataLockObject;

	FCoverOctreeController CoverOctreeController;

	void ConstructCoverOctree();

	/**
	 * @brief Enlarges the supplied box to x1.5 its size 
	 * @param Box
	 * @param Multiplier 
	 * @return 
	 */
	static FBox EnlargeAABB(const FBox Box, float Multiplier = 1.0f);
	
public:
	/**
	 * @brief Adds a set of cover points to the octree in a single, thread-safe batch.
	 * @param CoverPoints 
	 */
	void AddCoverPoints(const TArray<FDataTransferObjectCoverData>& CoverPoints) const;
	
	/**
	 * @brief Removes cover points within the specified area that don't fall on the navmesh or don't have an owner anymore.
	 * Useful for trimming areas around deleted objects and dynamically placed ones.
	 * @param Area
	 * @param StaleTileIndex 
	 */
	void RemoveStaleCoverPoints(FBox Area, const TileIndexType StaleTileIndex);

	
	/**
	 * @brief combination of RemoveStaleCoverPoints then AddCoverPoints, if needed
	 * @param Area 
	 * @param StaleTileIndex 
	 * @param CoverPoints 
	 */
	void RemoveStaleAndAddCoverPoints(FBox Area, const TileIndexType StaleTileIndex, const TArray<FDataTransferObjectCoverData>& CoverPoints);

protected:

	/**
	 * @brief  Adds a set of cover points to the octree in a single, not thread-safe
	 * @param CoverPoints 
	 */
	void Internal_AddCoverPoints(const TArray<FDataTransferObjectCoverData>& CoverPoints) const;
	
	/**
	 * @brief non thread-safe remove stale cover
	 * @param Area 
	 * @param StaleTileIndex 
	 */
	void Internal_RemoveStaleCoverPoints(FBox Area, const TileIndexType StaleTileIndex);

public:
	/**
	 * @brief Thread-safe wrapper for TCoverOctree::FindCoverPoints()
	 * Finds cover points that intersect the supplied box. 
	 * @param OutCoverPoints 
	 * @param QueryBox 
	 */
	template<class T>
	void FindCoverPoints(const FBox& QueryBox, TArray<T>& OutCoverPoints) const;
	
	/**
	 * @brief Thread-safe wrapper for TCoverOctree::FindCoverPoints()
	 * Finds cover points that intersect the supplied sphere.
	 * @param OutCoverPoints 
	 * @param QuerySphere 
	 */
	template<class T>
	void FindCoverPoints(const FSphere& QuerySphere, TArray<T>& OutCoverPoints) const;

	bool GetCoverPointOctreeElement(FCoverPointOctreeElement& OutElement, const FVector& ElementLocation) const;

	/** Retrieves center of the specified polygon. Returns false on error. */
	bool GetPolyEdges(NavNodeRef PolyID, TArray<FVector>& NavMeshEdgeVerts) const;
};

template <class T>
void ACoverRecastNavMesh::FindCoverPoints(const FBox& QueryBox, TArray<T>& OutCoverPoints) const
{
	if (CoverOctreeController.IsValid())
	{
		FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_ReadOnly);
		CoverOctreeController.FindElementsInNavOctree(QueryBox, OutCoverPoints);
	}
}

template <class T>
void ACoverRecastNavMesh::FindCoverPoints(const FSphere& QuerySphere, TArray<T>& OutCoverPoints) const
{
	if (CoverOctreeController.IsValid())
	{
		FRWScopeLock CoverDataLock(CoverDataLockObject, FRWScopeLockType::SLT_ReadOnly);
		CoverOctreeController.FindElementsInNavOctree(QuerySphere, OutCoverPoints);
	}
}