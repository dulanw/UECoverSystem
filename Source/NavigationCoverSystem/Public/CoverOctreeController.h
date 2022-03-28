#pragma once

#include "CoverOctree.h"

struct NAVIGATIONCOVERSYSTEM_API FCoverOctreeController
{
	TSharedPtr<FCoverOctree, ESPMode::ThreadSafe> CoverOctree;
	
	/**
	 * @brief Maps cover objects to their cover point locations
	 */
	TMultiMap<TWeakObjectPtr<const AActor>, FVector> CoverObjectToLocation;

	void Reset();
	
	bool IsValid() const { return CoverOctree.IsValid(); }

	const FOctreeElementId2* GetElementNavOctreeId(const FVector& ElementLocation) const;
	
	void RemoveNavOctreeElementId(const FOctreeElementId2& ElementId) const;

	void RemoveElementNavOctreeId(const FVector& ElementLocation) const;

	/**
	 * @brief Finds cover points that intersect the supplied box. 
	 * @param Elements T
	 * @param QueryBox 
	 */
	template<class T>
	void FindElementsInNavOctree(const FBox& QueryBox, TArray<T>& Elements) const;
	
	/**
	 * #TODO use template for OutCoverPoints type
	 * @brief Finds cover points that intersect the supplied sphere. 
	 * @param Elements T
	 * @param QuerySphere 
	 */
	template<class T>
	void FindElementsInNavOctree(const FSphere& QuerySphere, TArray<T>& Elements) const;
	
	/**
	 * @brief does the octree have an element inside given query
	 * @param QueryBox 
	 * @return return true if element found
	 */
	bool HasElementInNavOctree(const FBoxCenterAndExtent& QueryBox) const;

	bool AddNode(const FCoverPointOctreeElement& Element, const float DuplicateRadius) const;
};

template <class T>
void FCoverOctreeController::FindElementsInNavOctree(const FBox& QueryBox, TArray<T>& Elements) const
{
	if (CoverOctree.IsValid())
	{
		CoverOctree->FindElementsWithBoundsTest(QueryBox, [&Elements](const FCoverPointOctreeElement& CoverPoint) { Elements.Add(CoverPoint); });
	}
}

template <class T>
void FCoverOctreeController::FindElementsInNavOctree(const FSphere& QuerySphere, TArray<T>& Elements) const
{
	if (CoverOctree.IsValid())
	{
		const FBoxCenterAndExtent& BoxFromSphere = FBoxCenterAndExtent(QuerySphere.Center, FVector(QuerySphere.W));
		CoverOctree->FindElementsWithBoundsTest(BoxFromSphere, [&Elements, &QuerySphere](const FCoverPointOctreeElement& CoverPoint)	{
			// check if cover point is inside the supplied sphere's radius, now that we've ball parked it with a box query
			if (QuerySphere.Intersects(CoverPoint.Bounds.GetSphere()))
			{
				Elements.Add(CoverPoint);
			}
		});
	}
}
