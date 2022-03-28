// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnvQueryTest_Cover.h"

#include "CoverRecastNavMesh.h"
#include "CoverSystemStatics.h"
#include "EngineUtils.h"
#include "AI/EnvQueryContext_CoverTargetQuerier.h"
#include "Components/CapsuleComponent.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if DEBUG_RENDERING
#include "DrawDebugHelpers.h"

static TAutoConsoleVariable<bool> CVarDrawEnvQueryCoverTrace(
	TEXT("r.DrawEnvQueryCoverTrace"),
	false,
	TEXT("Draw debug EQS test trace for cover detection\n"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDrawEnvQueryCoverLeanTrace(
	TEXT("r.DrawEnvQueryCoverLeanTrace"),
	false,
	TEXT("Draw debug EQS test trace for cover lean detection\n"),
	ECVF_Cheat);
#endif

UEnvQueryTest_Cover::UEnvQueryTest_Cover()
	: Super()
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	CoverTraceChannel = UEngineTypes::ConvertToTraceType(UCoverSystemStatics::CoverTraceChannel);
	TargetTraceChannel = UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility);
	TraceTargetContext = UEnvQueryContext_CoverTargetQuerier::StaticClass();
	//CoverUserContext = UEnvQueryContext_Querier::StaticClass();

	static ConstructorHelpers::FClassFinder<AActor> DefaultTraceActorClass(TEXT("/NavigationCoverSystem/BP_CoverTestActor"));
	if (DefaultTraceActorClass.Class != nullptr)
	{
		TraceActorClass = DefaultTraceActorClass.Class;
	}

	UseTraceFromContext.DefaultValue = true;
	SetWorkOnFloatValues(true);

	CoverPointMaxObjectHitDistance = 100.0f;
	CoverOutOffset.DefaultValue = 100.0f;

	//by default don't discard any values, include all
	FloatValueMin.DefaultValue = 0.0f;
	FloatValueMax.DefaultValue = 1.0f;
}

void UEnvQueryTest_Cover::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	BoolValue.BindData(QueryOwner, QueryInstance.QueryID);
	FloatValueMin.BindData(QueryOwner, QueryInstance.QueryID);
	FloatValueMax.BindData(QueryOwner, QueryInstance.QueryID);
	CoverOutOffset.BindData(QueryOwner, QueryInstance.QueryID);
	
	bool bWantsCover = BoolValue.GetValue();
	const float MinThresholdValue = FloatValueMin.GetValue();
	const float MaxThresholdValue = FloatValueMax.GetValue();
	
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

	/**
	 * initialise character variables
	 * this can be moved inside the item iterator if needed for multiple pawns
	 */
	ACharacter* QueryOwnerCharacter = Cast<ACharacter>(QueryOwner);
	if (!QueryOwnerCharacter)
		return;

	//don't need to get NavAgent interface since Pawn will always have it
	const ACoverRecastNavMesh* NavData = UCoverSystemStatics::FindNavigationData<ACoverRecastNavMesh>(*NavSys, QueryOwnerCharacter);
	if (!NavData)
		return;
	
	const ACharacter* DefaultCharacter = QueryOwnerCharacter->GetClass()->GetDefaultObject<ACharacter>();
	const float DefaultCapsuleHalfHeight = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float CharacterStandingEyeHeight = DefaultCapsuleHalfHeight + QueryOwnerCharacter->BaseEyeHeight;

	bool bTestCrouchHeight = false;
	float CharacterCrouchingEyeHeight = 0.0f;
	if (QueryOwnerCharacter->GetCharacterMovement()->CanEverCrouch())
	{
		bTestCrouchHeight = true;
		CharacterCrouchingEyeHeight = QueryOwnerCharacter->GetCharacterMovement()->CrouchedHalfHeight + QueryOwnerCharacter->CrouchedEyeHeight;
	}
	
	/**
	 * end initialise character variables
	 */
	
	UseTraceFromContext.BindData(QueryOwner, QueryInstance.QueryID);
	const float bUseTraceFromContext = UseTraceFromContext.GetValue();

	TArray<AActor*> ContextActors;
	if (bUseTraceFromContext)
	{
		QueryInstance.PrepareContext(TraceTargetContext, ContextActors);
	}
	else if (TraceActorClass)
	{
		for (TActorIterator<AActor> It(QueryInstance.World, TraceActorClass); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor)
			{
				ContextActors.Add(*It);
			}
		}
	}

	TArray<AActor*> PrimaryTargets;
	if (PrimaryTargetContext)
	{
		QueryInstance.PrepareContext(PrimaryTargetContext, PrimaryTargets);
	}
	
	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		//UEnvQueryTest_Trace
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		FCoverPointOctreeElement Element;
		if (!NavData->GetCoverPointOctreeElement(Element, ItemLocation))
		{
			continue;
		}
		
		for (const auto TestActor : ContextActors)
		{
			const bool bPrimaryTarget = PrimaryTargets.Num() > 0 && PrimaryTargets.Contains(TestActor);
			
			FVector OutLocation;
			FRotator OutRotation;
			TestActor->GetActorEyesViewPoint(OutLocation, OutRotation);

			ECoverQueryResult CoverResult = EvaluateCoverPoint(&Element, CharacterStandingEyeHeight, TestActor, OutLocation, true, World);
			if (CoverResult < ECoverQueryResult::Found_NoView && bTestCrouchHeight)
			{
				const ECoverQueryResult OldCoverResult = CoverResult;
				CoverResult = EvaluateCrouchCoverPoint(&Element, CharacterStandingEyeHeight, CharacterCrouchingEyeHeight, TestActor, OutLocation, World);

				if (CoverResult < OldCoverResult)
					CoverResult = OldCoverResult;
			}
			
			if (CoverResult == ECoverQueryResult::Found)
			{
				It.SetScore(TestPurpose, FilterType, 1.0f, MinThresholdValue, MaxThresholdValue);
			}
			//we failed the cover test, need to check if it was for a primary target, if so we discard this point
			//set score to negative one if this is not valid cover for the primary target
			else if (bPrimaryTarget)
			{
				It.SetScore(TestPurpose, FilterType, -1.0f, MinThresholdValue, MaxThresholdValue);
			}
			else //if !Success && !bPrimaryTarget
			{
				//we don't care if we can't shoot the other targets as long as we are behind cover
				//if we only failed because we couldn't target them, we still accept it as a valid cover
				It.SetScore(TestPurpose, FilterType, CoverResult > ECoverQueryResult::NotFound ? 1.0f : 0.0f, MinThresholdValue, MaxThresholdValue);
			}
		}
	}
}

FText UEnvQueryTest_Cover::GetDescriptionTitle() const
{

	FString VolumeDesc(TEXT("No Cover Test Context/Class set"));
	if (TraceTargetContext)
	{
		VolumeDesc = UEnvQueryTypes::DescribeContext(TraceTargetContext).ToString();
	}
	else if (!UseTraceFromContext.GetValue() && TraceActorClass)
	{
		VolumeDesc = TraceActorClass->GetDescription();
	}

	return FText::FromString(FString::Printf(TEXT("%s: %s"), 
		*Super::GetDescriptionTitle().ToString(), *VolumeDesc));
	
}

FText UEnvQueryTest_Cover::GetDescriptionDetails() const
{
	return GetDescriptionTitle();
}

ECoverQueryResult UEnvQueryTest_Cover::EvaluateCoverPoint(const FCoverPointOctreeElement* CoverPoint, const float CoverTestHeight,
	AActor* TestTargetActor, const FVector& TestTargetLocation, const bool bTestHitByLean, UWorld* World) const
{
	const FVector CoverLocation = CoverPoint->Data->Location;
	const FVector CoverLocationInTestHeight = FVector(CoverLocation.X, CoverLocation.Y, CoverLocation.Z - UCoverSystemStatics::CoverPointGroundOffset + CoverTestHeight);

	const FVector TestDir = (TestTargetLocation - CoverLocationInTestHeight).GetSafeNormal2D();
	const FVector CoverTestEndLocation = CoverLocationInTestHeight + TestDir * CoverPointMaxObjectHitDistance * 1.5f;
	
	
	FHitResult HitResult(1.0f);
	FCollisionShape SphereCollisionShape;
	SphereCollisionShape.SetSphere(5.0f);
	
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.TraceTag = "CoverPointFinder_EvaluateCoverPoint";
	//CollisionQueryParams.AddIgnoredActor(Character);

	//with this collision channel, we should only be hitting cover
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(CoverTraceChannel);

	// check if we can hit the enemy straight from the cover point. if we can, then the cover point is no good
	if (!World->SweepSingleByChannel(HitResult, CoverLocationInTestHeight, CoverTestEndLocation, FQuat::Identity,  CollisionChannel, SphereCollisionShape, CollisionQueryParams))
	{
#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLocationInTestHeight, CoverTestEndLocation, FColor::Blue, true, -1, 0, 10.0f);
		}
#endif

		//not hitting anything means it hit our player (shouldn't be colliding with the player from the cover channel)
		if (World->SweepSingleByChannel(HitResult, CoverLocationInTestHeight, TestTargetLocation, FQuat::Identity,  CollisionChannel, SphereCollisionShape, CollisionQueryParams))
		{
			const AActor* HitActor = HitResult.GetActor();
			if (HitActor != TestTargetActor && !HitActor->IsA<APawn>())
			{
#if DEBUG_RENDERING
				if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
				{
					DrawDebugLine(World, CoverLocationInTestHeight, TestTargetLocation, FColor::Green, true, -1, 0, 10.0f);
				}
#endif				
				return ECoverQueryResult::Obstruction;
			}
		}

#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLocationInTestHeight, TestTargetLocation, FColor::Black, true, -1, 0, 10.0f);
		}
#endif
		
		return ECoverQueryResult::NotFound;
	}
	
	//this should always be a cover object, but we will check just in case that the hit actor is not the target or a pawn
	//and we test by lean only when needed, i.e standing, if crouching we should be able to shoot over cover
	//we check that the hit actor is the cover object so that we don't accept a cover point that is blocked by a different object and not the actual cover object
	//we can also use a small CoverPointMaxObjectHitDistance too, but using the cover object is more accurate
	//#NOTE maybe remove cover object check, it could be that the cover is large and curves around, so we still need to use CoverPointMaxObjectHitDistance
	const AActor* HitActor = HitResult.GetActor();
	if (!CoverPoint->Data->bForceField && HitActor != TestTargetActor && !HitActor->IsA<APawn>()
		&& (HitActor == CoverPoint->Data->CoverObject && HitResult.Distance <= CoverPointMaxObjectHitDistance)) 
	{
#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLocationInTestHeight, CoverTestEndLocation, FColor::Red, true, -1, 0, 10.0f);
		}
#endif
		
		if (!bTestHitByLean || CheckHitByLeaning(HitResult, CoverLocationInTestHeight, TestTargetActor, TestTargetLocation, World))
		{
			return ECoverQueryResult::Found;	
		}
		
		return ECoverQueryResult::Found_NoView;
	}
	
	return ECoverQueryResult::NotFound;
}

bool UEnvQueryTest_Cover::CheckHitByLeaning(const FHitResult& CoverHitResult, const FVector& CoverLocation,
                                            AActor* TestTargetActor, const FVector& TestTargetLocation,
                                            UWorld* World) const
{
	FHitResult HitResult(1.0f);
	FCollisionShape SphereCollisionShape;
	SphereCollisionShape.SetSphere(5.0f);
	
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.TraceTag = "CoverPointFinder_CheckHitByLeaning";

	//with this collision channel, we should only be hitting cover
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(CoverTraceChannel);

	// calculate our reach for when leaning out of cover
	const FVector LeanCheckOffset = UCoverSystemStatics::GetPerpendicularVector(CoverHitResult.ImpactNormal).GetUnsafeNormal() * CoverOutOffset.GetValue();

	auto CheckHitLambda = [&](const FVector& CoverLeanStart) -> bool
	{
#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverLeanTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLocation, CoverLeanStart, FColor::Purple, true, -1, 0, 5.0f);
		}
#endif

		//move 10 units back from the lean check location and trace to see if we start in penetrating
		const FVector CheckPenetrating = CoverLeanStart + CoverHitResult.ImpactNormal * 10.0f;
		const bool bCheckPenetrating = World->SweepSingleByChannel(HitResult, CheckPenetrating, CoverLeanStart, FQuat::Identity, CollisionChannel, SphereCollisionShape, CollisionQueryParams);
		if (bCheckPenetrating || HitResult.bStartPenetrating)
		{
			return false;
		}
		
		// check if we can hit our target by leaning out of cover
		const bool bHit = World->SweepSingleByChannel(HitResult, CoverLeanStart, TestTargetLocation, FQuat::Identity, CollisionChannel, SphereCollisionShape, CollisionQueryParams);
		if ((!bHit || HitResult.GetActor() == TestTargetActor) && !HitResult.bStartPenetrating)
		{
#if DEBUG_RENDERING
			if (CVarDrawEnvQueryCoverLeanTrace.GetValueOnAnyThread())
			{
				DrawDebugLine(World, CoverLeanStart, TestTargetLocation, FColor::Red, true, -1, 0, 10.0f);
			}
#endif
			return true;
		}

#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverLeanTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLeanStart, TestTargetLocation, FColor::Blue, true, -1, 0, 10.0f);
		}
#endif
		return false;
	};
	
	return CheckHitLambda(CoverLocation + 1.0f * LeanCheckOffset) ||
		CheckHitLambda(CoverLocation - 1.0f * LeanCheckOffset);
}

ECoverQueryResult UEnvQueryTest_Cover::EvaluateCrouchCoverPoint(const FCoverPointOctreeElement* CoverPoint, const float StandingTestHeight,
												   const float CoverTestHeight, AActor* TestTargetActor, const FVector& TestTargetLocation, UWorld* World) const
{
	const ECoverQueryResult CoverResult = EvaluateCoverPoint(CoverPoint, CoverTestHeight, TestTargetActor, TestTargetLocation, false, World);

	if (CoverResult != ECoverQueryResult::Found)
		return CoverResult;
	
	//test from the standing height position that we can actually hit the target
	const FVector CoverLocation = CoverPoint->Data->Location;
	const FVector CoverLocationInTestHeight = FVector(CoverLocation.X, CoverLocation.Y, CoverLocation.Z - UCoverSystemStatics::CoverPointGroundOffset + StandingTestHeight);

	FHitResult HitResult(1.0f);
	FCollisionShape SphereCollisionShape;
	SphereCollisionShape.SetSphere(5.0f);
	
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.TraceTag = "CoverPointFinder_EvaluateCrouchCoverPoint";
	//CollisionQueryParams.AddIgnoredActor(Character);

	//with this collision channel, we should only be hitting cover
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TargetTraceChannel);

	// check if we can hit the enemy straight from the cover point. if we can, then the cover point is no good
	const bool bHit = World->SweepSingleByChannel(HitResult, CoverLocationInTestHeight, TestTargetLocation, FQuat::Identity,  CollisionChannel, SphereCollisionShape, CollisionQueryParams);
	if (bHit && HitResult.GetActor() != TestTargetActor)
	{
#if DEBUG_RENDERING
		if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
		{
			DrawDebugLine(World, CoverLocationInTestHeight, TestTargetLocation, FColor::Orange, true, -1, 0, 10.0f);
		}
#endif
		
		return ECoverQueryResult::Found_NoView;
	}

#if DEBUG_RENDERING
	if (CVarDrawEnvQueryCoverTrace.GetValueOnAnyThread())
	{
		DrawDebugLine(World, CoverLocationInTestHeight, TestTargetLocation, FColor::Yellow, true, -1, 0, 10.0f);
	}
#endif

	return ECoverQueryResult::Found;
}
