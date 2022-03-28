// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CoverOctree.h"

/**
 * 
 */
class FNavmeshCoverPointGeneratorAsyncTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FNavmeshCoverPointGeneratorAsyncTask>;
	
	FNavmeshCoverPointGeneratorAsyncTask();
	
	FNavmeshCoverPointGeneratorAsyncTask(float InCoverPointMinDistance,	float InSmallestAgentHeight, float InCoverPointGroundOffset,
		FBox InMapBounds, int32 InNavmeshTileIndex, class ACoverRecastNavMesh* InNav);
	
private:
	// Minimum distance between cover points.
	float CoverPointMinDistance;

	// How far to trace from a navmesh edge towards the inside of the navmesh "hole".
	const float ScanReach;

	// Distance between an outermost edge of the navmesh and a cliff, i.e. no navmesh.
	const float CliffEdgeDistance;

	// Offset that gets added to the cliff edge trace. Useful for detecting not perfectly straight cliffs e.g. that of landscapes.
	const float StraightCliffErrorTolerance;

	// Length of the ray cast for checking if there's a navmesh hole to one of the sides of a navmesh edge.
	const float NavmeshHoleCheckReach;

	// Height of the smallest actor that will ever fit under an overhanging cover. Should normally be the CROUCHED height of the smallest actor in the game. Not counting bunnies. Bunnies are useless.
	const float SmallestAgentHeight;

	// A small Z-axis offset applied to each cover point. This is to prevent small irregularities in the navmesh from registering as cover.
	const float CoverPointGroundOffset;

	// Distance of the navmesh from the ground (UCoverSystem::CoverPointGroundOffset), plus a little extra to make sure we do hit the ground with the trace.
	// Used for finding cliff edges.
	const float NavMeshMaxZDistanceFromGround;

	// AABB used to filter out cover points on the edges of the map.
	const FBox MapBounds;

	// The bounding box to generate cover points in.
	const int32 NavmeshTileIndex;

	// The nav mesh that called this.
	class ACoverRecastNavMesh* NavRef;

#if DEBUG_RENDERING
	bool bDebugDraw = false;
#endif

	FVector GetEdgeDir(const FVector& EdgeStartVertex, const FVector& EdgeEndVertex) const;

	// Uses navmesh ray casts to scan for cover from TraceStart to TraceEnd.
	// Builds an FDataTransferObjectCoverData for transferring the results over to the cover octree.
	// Returns true if cover was found, false if not.
	
	/**
	 * @brief Uses navmesh ray casts to scan for cover from TraceStart to TraceEnd.
	 * Builds an FDataTransferObjectCoverData for transferring the results over to the cover octree.
	 * @param OutCoverData
	 * @param NodeRef the node ref of the current poly we are testing 
	 * @param TraceStart 
	 * @param TraceDirection 
	 * @return Returns true if cover was found, false if not.
	 */
	bool ScanForCoverNavMeshProjection(FDataTransferObjectCoverData& OutCoverData, const NavNodeRef& NodeRef, const FVector& TraceStart, const FVector& TraceDirection) const;

	/**
	 * @brief check for cover on either side of the edge
	 * @param OutCoverPointsOfActors
	 * @param NodeRef the node ref of the current poly we are testing 
	 * @param EdgeStepVertex 
	 * @param EdgeDir 
	 */
	void ProcessEdgeStep(TArray<FDataTransferObjectCoverData>& OutCoverPointsOfActors, const NavNodeRef& NodeRef, const FVector& EdgeStepVertex, const FVector& EdgeDir) const;
	
	/**
	 * @briefGenerates cover points inside the specified bounding box via navmesh edge-walking.
	 * @param OutCoverPoints 
	 * @return Returns the AABB of the navmesh tile that corresponds to NavmeshTileIndex.
	 */
	FBox GenerateCoverInBounds(TArray<FDataTransferObjectCoverData>& OutCoverPoints) const;
	
	/**
	 * @brief Find cover points in the supplied bounding box and store them in the cover system.
	 * Remove any cover points within the supplied bounding box, first.
	 */
	void DoWork() const;

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FNavmeshCoverPointGeneratorAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
