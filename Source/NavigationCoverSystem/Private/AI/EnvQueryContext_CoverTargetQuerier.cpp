// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnvQueryContext_CoverTargetQuerier.h"

#include "AISystem.h"
#include "AI/CoverAgentInterface.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEnvQueryContext_CoverTargetQuerier::ProvideContext(FEnvQueryInstance& QueryInstance,
                                                        FEnvQueryContextData& ContextData) const
{
	AActor* QueryOwner = Cast<AActor>(QueryInstance.Owner.Get());
	if (GetDefault<UAISystem>()->bAllowControllersAsEQSQuerier == false && Cast<AController>(QueryOwner) != nullptr)
	{
		UE_LOG(LogEQS, Warning,
			   TEXT(
				   "UEnvQueryContext_CoverTargetQuerier::ProvideContext Using Controller as query's owner is dangerous since Controller's location is usually not what you expect it to be!"
			   ));
	}

	ICoverAgentInterface* CoverAgentQueryOwner = Cast<ICoverAgentInterface>(QueryOwner);
	if (CoverAgentQueryOwner == nullptr)
	{
		UE_LOG(LogEQS, Error,
			   TEXT(
				   "UEnvQueryContext_CoverTargetQuerier::ProvideContext %s does not implement ICoverAgentInterface and cannot be used to get relavent actors!"
			   ), *QueryOwner->GetName());
		return;
	}
	
	UEnvQueryItemType_Actor::SetContextHelper(ContextData, CoverAgentQueryOwner->Execute_GetRelevantCoverTestActors(QueryOwner));
}
