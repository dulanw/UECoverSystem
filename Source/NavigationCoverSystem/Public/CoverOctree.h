// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "Math/GenericOctreePublic.h"
#include "Math/GenericOctree.h"
#include "CoverOctree.generated.h"

/** uniform identifier type for navigation data elements may it be a polygon or graph node */
typedef int32 TileIndexType;

/**
 * DTO for FCoverPointOctreeData
 * Data Transfer Objects
 */
struct FDataTransferObjectCoverData
{
public:
	AActor* CoverObject;
	FVector Location;
	bool bForceField;
	TileIndexType TileIndex;
	NavNodeRef NodeRef;

	FDataTransferObjectCoverData()
		: CoverObject(), Location(), bForceField(), TileIndex(-1), NodeRef(INVALID_NAVNODEREF)
	{
	}

	FDataTransferObjectCoverData(AActor* InCoverObject, const FVector InLocation, const bool bInForceField, const TileIndexType InTileIndex, const NavNodeRef InNodeRef)
		: CoverObject(InCoverObject), Location(InLocation), bForceField(bInForceField), TileIndex(InTileIndex), NodeRef(InNodeRef)
	{
	}
};

struct FCoverPointOctreeData : public TSharedFromThis<FCoverPointOctreeData, ESPMode::ThreadSafe>
{
public:	
	// Location of the cover point
	const FVector Location;

	// no leaning if it's a force field wall

	// true if it's a force field, i.e. units can walk through but projectiles are blocked
	const bool bForceField;

	// Object that generated this cover point
	const TWeakObjectPtr<AActor> CoverObject;

	// Whether the cover point is taken by a unit
	bool bTaken;

	TileIndexType TileIndex;

	NavNodeRef NodeRef;

	FCoverPointOctreeData()
		: Location(), bForceField(false), CoverObject(), bTaken(false), TileIndex(-1), NodeRef(INVALID_NAVNODEREF)
	{
	}

	FCoverPointOctreeData(FDataTransferObjectCoverData CoverData)
		: Location(CoverData.Location), bForceField(CoverData.bForceField), CoverObject(CoverData.CoverObject),
		  bTaken(false), TileIndex(CoverData.TileIndex), NodeRef(CoverData.NodeRef)
	{
	}
};

USTRUCT(BlueprintType)
struct FCoverPointOctreeElement
{
	GENERATED_USTRUCT_BODY()

public:
	TSharedRef<FCoverPointOctreeData, ESPMode::ThreadSafe> Data;

	FBoxSphereBounds Bounds;

	FCoverPointOctreeElement()
		: Data(new FCoverPointOctreeData()), Bounds()
	{}

	FCoverPointOctreeElement(const FDataTransferObjectCoverData& CoverData)
		: Data(new FCoverPointOctreeData(CoverData)), Bounds(FSphere(CoverData.Location, 1.0f))
	{}

	FORCEINLINE bool IsEmpty() const
	{
		const FBox BoundingBox = Bounds.GetBox();
		return BoundingBox.IsValid == 0 || BoundingBox.GetSize().IsNearlyZero();
	}

	FORCEINLINE UObject* GetOwner() const
	{
		return Data->CoverObject.Get();
	}

	FORCEINLINE operator FVector() const
	{
		return Data->Location;
	}
};

struct FCoverPointOctreeSemantics
{
	typedef TOctree2<FCoverPointOctreeElement, FCoverPointOctreeSemantics> FOctree;
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FCoverPointOctreeElement& Element)
	{
		return Element.Bounds;
	}

	FORCEINLINE static bool AreElementsEqual(const FCoverPointOctreeElement& A, const FCoverPointOctreeElement& B)
	{
		//TODO: revisit this when introducing new properties to FCoverPointData
		return A.Bounds == B.Bounds;
	}
	
	static void SetElementId(FOctree& OctreeOwner, const FCoverPointOctreeElement& Element, FOctreeElementId2 Id);
};

/**
 * Octree for storing cover points. Not thread-safe, use UCoverSystem for manipulation.
 */
class FCoverOctree : public TOctree2<FCoverPointOctreeElement, FCoverPointOctreeSemantics>, public TSharedFromThis<FCoverOctree, ESPMode::ThreadSafe>
{
public:
	FCoverOctree();

	FCoverOctree(const FVector& Origin, float Radius);

	virtual ~FCoverOctree() {}	

	// Won't crash the game if ElementId is invalid, unlike the similarly named superclass method. This method hides the base class method as it's not virtual.
	// ReSharper disable once CppHidingFunction
	void RemoveElement(const FOctreeElementId2 ElementId);

	// Mark the cover at the supplied location as taken.
	// Returns true if the cover wasn't already taken, false if it was or an error has occurred, e.g. the cover no longer exists.
	bool HoldCover(FOctreeElementId2 ElementId);
	//bool HoldCover(FCoverPointOctreeElement Element);
	
	// Releases a cover that was taken.
	// Returns true if the cover was taken before, false if it wasn't or an error has occurred, e.g. the cover no longer exists.
	bool ReleaseCover(FOctreeElementId2 ElementId);
	//bool ReleaseCover(FCoverPointOctreeElement Element);

protected:
	friend struct FCoverPointOctreeSemantics;
	friend struct FCoverOctreeController;
	
	/**
	 * Maps cover point locations to their ids
	 * NOT THREAD-SAFE! Use the corresponding thread-safe functions instead
	 */
	TMap<const FVector, FOctreeElementId2> ElementToOctreeId;

	void SetElementIdImpl(const FVector ElementLocation, FOctreeElementId2 Id);
};

