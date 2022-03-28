// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Cover.generated.h"

UENUM()
enum class ECoverQueryResult : uint8
{
	NotFound		= 0,	//cover not found
	Obstruction		= 1,	//cover not found but view is obstructed by other obstacle
	Found_NoView	= 2,	//cover found but doesn't have view of the target
	Found			= 3		//cover found
};

/**
 * Test the cover point against a list of actors, to check visibility
 * will only use the QueryInstance.Owner, to check if the cover is valid (i.e. the character can crouch/stand behind it)
 * //#TODO if the cover position is behind a different cover, it will return false, so it counts as the actor not being behind cover
 * //use an enum CloseCover/
 */
UCLASS()
class NAVIGATIONCOVERSYSTEM_API UEnvQueryTest_Cover : public UEnvQueryTest
{
	GENERATED_BODY()

public:

	UEnvQueryTest_Cover();

	/**
	 * cover trace channel
	 * this should be a trace channel that is only used for cover objects and not pawns
	 */
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	TEnumAsByte<enum ETraceTypeQuery> CoverTraceChannel;

	/**
	 * target trace channel, used to check if target can be reached from crouch cover position
	 */
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	TEnumAsByte<enum ETraceTypeQuery> TargetTraceChannel;
	
	/**
	 * Trace context, the actor(s) which we should check the cover points against
	 * uses the actor->GetActorEyesViewPoint to get the exact location to trace from
	 * the actors which we are taking cover from
	 */
	UPROPERTY(EditAnywhere, Category="Trace")
	TSubclassOf<UEnvQueryContext> TraceTargetContext;

	/**
	 * The character which will be taking cover, this is used to get the character standing and crouch height to see if
	 * it is compatible with it, this should ideally be the UEnvQueryContext_Querier
	 */
	//UPROPERTY(EditAnywhere, Category="Trace")
	//TSubclassOf<UEnvQueryContext> CoverUserContext;

	/**  Use the above context, if false it will get all actors of class (specified below) */
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	FAIDataProviderBoolValue UseTraceFromContext;

	/**
	 * Use this for testing, checks each cover position against all actors of this class found on the level
	 */
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	TSubclassOf<AActor> TraceActorClass;

	/**
	 * context used to get the primary target, if a cover point is not valid for this actor, then it is discarded.
	 * if it is valid then the cover point will be further scored against the rest of the targets
	 */
	UPROPERTY(EditAnywhere, Category="Trace")
	TSubclassOf<UEnvQueryContext> PrimaryTargetContext;

	/**
	 * How close must the actual cover object be to a cover point. This is to avoid picking a cover point that doesn't provide meaningful cover.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	float CoverPointMaxObjectHitDistance;

	/**
	 * how far the ai can lean outward to fire from the cover position, this could be moving fully out of cover but retaining the lock on the cover pos
	 **/
	UPROPERTY(EditDefaultsOnly, Category="Trace")
	FAIDataProviderFloatValue CoverOutOffset;
	
	/** Function that does the actual work */
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	
	virtual FText GetDescriptionTitle() const override;
	
	virtual FText GetDescriptionDetails() const override;

	ECoverQueryResult EvaluateCoverPoint(const struct FCoverPointOctreeElement* CoverPoint, const float CoverTestHeight,
	                        AActor* TestTargetActor, const FVector& TestTargetLocation, const bool bTestHitByLean, UWorld* World) const;

	bool CheckHitByLeaning(const FHitResult& CoverHitResult, const FVector& CoverLocation, AActor* TestTargetActor,
	                       const FVector& TestTargetLocation, UWorld* World) const;

	ECoverQueryResult EvaluateCrouchCoverPoint(const struct FCoverPointOctreeElement* CoverPoint, const float StandingTestHeight, const float CoverTestHeight,
						AActor* TestTargetActor, const FVector& TestTargetLocation, UWorld* World) const;
};
