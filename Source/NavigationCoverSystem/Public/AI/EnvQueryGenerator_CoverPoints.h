// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_CoverPoints.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "Cover Points"))
class NAVIGATIONCOVERSYSTEM_API UEnvQueryGenerator_CoverPoints : public UEnvQueryGenerator
{
	GENERATED_BODY()	
public:
	UEnvQueryGenerator_CoverPoints();
	
	/** Max distance of path between point and context.  NOTE: Zero and negative values will never return any results if
	  * UseRadius is true.  "Within" requires Distance < Radius.  Actors ON the circle (Distance == Radius) are excluded.
	  */
	UPROPERTY(EditDefaultsOnly, Category="Generator")
	FAIDataProviderFloatValue SearchRadius;

	/** context */
	UPROPERTY(EditAnywhere, Category="Generator")
	TSubclassOf<UEnvQueryContext> SearchCenter;
	
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual void ProcessItems(FEnvQueryInstance& QueryInstance, TArray<FVector>& FoundCover) const {}

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};


