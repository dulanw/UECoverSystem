#include "CoverOctreeController.h"

void FCoverOctreeController::Reset()
{
	if (CoverOctree.IsValid())
	{
		CoverOctree->Destroy();
		CoverOctree = nullptr;
	}
}

const FOctreeElementId2* FCoverOctreeController::GetElementNavOctreeId(const FVector& ElementLocation) const
{
	return CoverOctree.IsValid() ? CoverOctree->ElementToOctreeId.Find(ElementLocation) : nullptr;
}

void FCoverOctreeController::RemoveNavOctreeElementId(const FOctreeElementId2& ElementId) const
{
	if (CoverOctree.IsValid())
	{
		CoverOctree->RemoveElement(ElementId);
	}
}

void FCoverOctreeController::RemoveElementNavOctreeId(const FVector& ElementLocation) const
{
	if (CoverOctree.IsValid())
	{
		CoverOctree->ElementToOctreeId.Remove(ElementLocation);
	}
}

bool FCoverOctreeController::HasElementInNavOctree(const FBoxCenterAndExtent& QueryBox) const
{
	bool bResult = false;
	if (CoverOctree.IsValid())
	{
		CoverOctree->FindFirstElementWithBoundsTest(QueryBox, [&bResult](const FCoverPointOctreeElement& CoverPoint)
		{
			bResult = true;
			return true;
		});
	}
	
	return bResult;
}

bool FCoverOctreeController::AddNode(const FCoverPointOctreeElement& Element, const float DuplicateRadius) const
{
	if (CoverOctree.IsValid())
	{
		// check if any cover points are close enough - if so, abort
		if (HasElementInNavOctree(FBoxCenterAndExtent(Element.Data->Location, FVector(DuplicateRadius))))
			return false;

		CoverOctree->AddElement(Element);
		return true;
	}

	return false;
}
