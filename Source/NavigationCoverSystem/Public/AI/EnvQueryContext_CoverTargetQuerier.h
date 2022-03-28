// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_CoverTargetQuerier.generated.h"

/**
 * Querier which implements a ICoverAgentInterface, if not, it will return 
 */
UCLASS()
class NAVIGATIONCOVERSYSTEM_API UEnvQueryContext_CoverTargetQuerier : public UEnvQueryContext
{
	GENERATED_BODY()
	
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
