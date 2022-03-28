// Fill out your copyright notice in the Description page of Project Settings.


#include "CoverSystemStatics.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

// PROFILER INTEGRATION //
DEFINE_STAT(STAT_GenerateCover);
DEFINE_STAT(STAT_GenerateCoverInBounds);
DEFINE_STAT(STAT_FindCover);

TEnumAsByte<enum ECollisionChannel> UCoverSystemStatics::CoverTraceChannel(ECollisionChannel::ECC_GameTraceChannel4);

float UCoverSystemStatics::CoverPointGroundOffset(10.0f);
float UCoverSystemStatics::SmallestAgentHeight(2 * 58.0f);

FVector UCoverSystemStatics::GetPerpendicularVector(const FVector& Vector)
{
	//return FVector(Vector.Y, -Vector.X, Vector.Z);

	//sloped angles will give a wrong perpendicular vector when the above is used
	return Vector.ToOrientationQuat().GetRightVector();
}

void UCoverSystemStatics::K2_FlushPersistentDebugLines()
{
	FlushPersistentDebugLines(GWorld);
}

void UCoverSystemStatics::K2_RebuildNavigationData()
{
	FlushPersistentDebugLines(GWorld);
	for (TActorIterator<ANavigationData> It(GWorld); It; ++It)
	{
		ANavigationData* NavData = (*It);
		if (NavData)
		{
			NavData->RebuildAll();
		}
	}
}
