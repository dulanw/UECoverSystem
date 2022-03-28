// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnvQueryGenerator_CoverPoints.h"

#include "CoverRecastNavMesh.h"
#include "CoverSystemStatics.h"
#include "NavigationSystem.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_CoverPoints::UEnvQueryGenerator_CoverPoints()
	: Super()
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	SearchRadius.DefaultValue = 1000.0f;
	SearchCenter = UEnvQueryContext_Querier::StaticClass();
}

void UEnvQueryGenerator_CoverPoints::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	//do not call super - checkNoEntry
	//Super::GenerateItems(QueryInstance);

	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		return;
	}
	
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryInstance.World);
	if (NavSys == nullptr || QueryOwner == nullptr)
	{
		return;
	}

	SearchRadius.BindData(QueryOwner, QueryInstance.QueryID);
	const float RadiusValue = SearchRadius.GetValue();
	
	TArray<AActor*> ContextActors;
	QueryInstance.PrepareContext(SearchCenter, ContextActors);

	TArray<FVector> MatchingCoverPoints;
	for (int32 ContextIndex = 0; ContextIndex < ContextActors.Num(); ++ContextIndex)
	{
		INavAgentInterface* NavAgent = Cast<INavAgentInterface>(ContextActors[ContextIndex]);
		if (NavAgent == nullptr)
		{
			UE_LOG(LogEQS, Error, TEXT("UEnvQueryGenerator_CoverPoints::GenerateItems ContextActor does not implement INavAgentInterface and cannot be used to query navigation system!"));
			continue;
		}
	
		if (const ACoverRecastNavMesh* NavData = UCoverSystemStatics::FindNavigationData<ACoverRecastNavMesh>(*NavSys, NavAgent))
		{
			FSphere QuerySphere(ContextActors[ContextIndex]->GetActorLocation(), RadiusValue);
			NavData->FindCoverPoints(QuerySphere, MatchingCoverPoints);
		}
	}
	
	ProcessItems(QueryInstance, MatchingCoverPoints);
	QueryInstance.AddItemData<UEnvQueryItemType_Point>(MatchingCoverPoints);
}

FText UEnvQueryGenerator_CoverPoints::GetDescriptionTitle() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("DescriptionTitle"), Super::GetDescriptionTitle());

	/*if (!GenerateOnlyActorsInRadius.IsDynamic() && !GenerateOnlyActorsInRadius.GetValue())
	{
		return FText::Format(LOCTEXT("DescriptionGenerateActors", "{DescriptionTitle}: generate set of actors of {ActorsClass}"), Args);
	}*/

	Args.Add(TEXT("DescribeContext"), UEnvQueryTypes::DescribeContext(SearchCenter));
	return FText::Format(LOCTEXT("DescriptionGenerateActorsAroundContext", "{DescriptionTitle}: generate set of cover points around {DescribeContext}"), Args);
}

FText UEnvQueryGenerator_CoverPoints::GetDescriptionDetails() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Radius"), FText::FromString(SearchRadius.ToString()));
	
	return FText::Format(LOCTEXT("ActorsOfClassDescription", "radius: {Radius}"), Args);;
}

#undef LOCTEXT_NAMESPACE