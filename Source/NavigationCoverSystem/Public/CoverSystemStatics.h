// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoverSystemStatics.generated.h"

// PROFILER INTEGRATION //

DECLARE_STATS_GROUP(TEXT("CoverSystem"), STATGROUP_CoverSystem, STATCAT_Cover);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Generate Cover / Full"), STAT_GenerateCover, STATGROUP_CoverSystem, NAVIGATIONCOVERSYSTEM_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Generate Cover / GenerateCoverInBounds"), STAT_GenerateCoverInBounds, STATGROUP_CoverSystem, NAVIGATIONCOVERSYSTEM_API);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Cover - Historical Count"), STAT_GenerateCoverHistoricalCount, STATGROUP_CoverSystem);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Generate Cover - Total Time Spent"), STAT_GenerateCoverAverageTime, STATGROUP_CoverSystem);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Cover - Active Tasks"), STAT_TaskCount, STATGROUP_CoverSystem);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Find Cover"), STAT_FindCover, STATGROUP_CoverSystem, NAVIGATIONCOVERSYSTEM_API);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Find Cover - Historical Count"), STAT_FindCoverHistoricalCount, STATGROUP_CoverSystem);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Find Cover - Total Time Spent"), STAT_FindCoverTotalTimeSpent, STATGROUP_CoverSystem);

/**
 * 
 */
UCLASS()
class NAVIGATIONCOVERSYSTEM_API UCoverSystemStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static TEnumAsByte<enum ECollisionChannel> CoverTraceChannel;

	/**
	 * A small Z-axis offset applied to each cover point. This is to prevent small irregularities in the navmesh from registering as cover.
	 */
	static float CoverPointGroundOffset;

	/**
	 * Height of the smallest actor that will ever fit under an overhanging cover.
	 * Should normally be the CROUCHED height of the smallest actor in the game.
	 * full capsule height, not half height (2 * Character->Crouched Half Height)
	 */
	static float SmallestAgentHeight;

	/**
	 * @brief 
	 * @param Vector direction vector
	 * @return perpendicular vector, left of the vector
	 */
	static FVector GetPerpendicularVector(const FVector& Vector);
	
	template<class T>
	static T* FindNavigationData(class UNavigationSystemV1& NavSys, INavAgentInterface* NavAgent);
	
#if WITH_EDITORONLY_DATA
	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta=(DisplayName="FlushPersistentDebugLines"))
	static void K2_FlushPersistentDebugLines();

	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta=(DisplayName="RebuildNavigationData"))
	static void K2_RebuildNavigationData();
#endif 
	
};

template <class T>
T* UCoverSystemStatics::FindNavigationData(UNavigationSystemV1& NavSys, INavAgentInterface* NavAgent)
{
	//INavAgentInterface* NavAgent = Cast<INavAgentInterface>(NavAgent);
	if (NavAgent)
	{
		return Cast<T>(NavSys.GetNavDataForProps(NavAgent->GetNavAgentPropertiesRef(), NavAgent->GetNavAgentLocation()));
	}

	return Cast<T>(NavSys.GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
}