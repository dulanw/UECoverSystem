// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CoverAgentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UCoverAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class NAVIGATIONCOVERSYSTEM_API ICoverAgentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	 * Get a list of actors to test the cover position against
	 * calls GetActorEyesViewPoint to get the location to test against
	 */
	// ReSharper disable once CppMemberFunctionMayBeStatic
	// BlueprintNativeEvent might be better suited for c++
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)	// ReSharper disable once CppHiddenFunction
	TArray<AActor*> GetRelevantCoverTestActors();
	virtual TArray<AActor*> GetRelevantCoverTestActors_Implementation() PURE_VIRTUAL(GetRelevantCoverTestActor::GetRelevantCoverTestActor_Implementation, return TArray<AActor*>(); );
};
