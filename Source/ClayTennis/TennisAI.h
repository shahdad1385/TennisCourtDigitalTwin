// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "TennisTypes.h"
#include "TennisAI.generated.h"

class ATennisPlayerPawn;
class ATennisBall;
class ATennisCourt;

/**
 * Human-like opponent brain.
 *
 * The AI predicts the real bounce with ATennisBall::PredictLanding (same
 * integrator the ball uses), moves to an intercept point with the pawn's
 * acceleration limits, waits a short reaction time, then asks the pawn to
 * execute a shot. Difficulty controls foot speed, reaction, target noise and
 * the chance of an unforced error.
 */
UCLASS(ClassGroup = (Tennis), meta = (BlueprintSpawnableComponent))
class CLAYTENNIS_API UTennisAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTennisAIComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AI")
	void InitializeAI(ATennisPlayerPawn* InPawn, ATennisBall* InBall, ATennisCourt* InCourt, float InDifficulty);

	/** Picks a serve target/type. bSecondServe makes the AI more conservative. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool ChooseServePlan(FVector& OutTargetCm, EShotType& OutServeType, bool bSecondServe);

	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetDifficulty(float InDifficulty);

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetDifficulty() const { return Difficulty; }

protected:
	UPROPERTY()
	TObjectPtr<ATennisPlayerPawn> AIPawn;

	UPROPERTY()
	TObjectPtr<ATennisBall> Ball;

	UPROPERTY()
	TObjectPtr<ATennisCourt> Court;

	UPROPERTY(EditAnywhere, Category = "AI")
	float Difficulty = 0.62f;

	float SwingCooldown = 0.0f;
	float ReactionTimer = 0.0f;
	bool bSwingCommitted = false;

	FRandomStream Rng;

	FVector ChooseShotTarget(float MySideSign, EShotType& OutShotType, float& OutCharge);
	bool TrySwing();
	float GetFootSpeedScale() const;
};
