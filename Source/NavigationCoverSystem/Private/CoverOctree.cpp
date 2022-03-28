// Fill out your copyright notice in the Description page of Project Settings.

#include "CoverOctree.h"

void FCoverPointOctreeSemantics::SetElementId(FOctree& OctreeOwner, const FCoverPointOctreeElement& Element, FOctreeElementId2 Id)
{
	static_cast<FCoverOctree&>(OctreeOwner).SetElementIdImpl(Element.Data->Location, Id);
}

FCoverOctree::FCoverOctree()
	: TOctree2<FCoverPointOctreeElement, FCoverPointOctreeSemantics>()
{
}

FCoverOctree::FCoverOctree(const FVector& Origin, float Radius)
	: TOctree2<FCoverPointOctreeElement, FCoverPointOctreeSemantics>(Origin, Radius)
{
}

void FCoverOctree::RemoveElement(const FOctreeElementId2 ElementId)
{
	if (!ElementId.IsValidId())
		return;

	static_cast<TOctree2*>(this)->RemoveElement(ElementId);
}

void FCoverOctree::SetElementIdImpl(const FVector ElementLocation, FOctreeElementId2 Id)
{
	ElementToOctreeId.Add(ElementLocation, Id);
}

