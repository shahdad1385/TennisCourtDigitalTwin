// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TennisPhysics.h"
#include "TennisTypes.h"
#include "TennisBall.generated.h"

class UStaticMeshComponent;
class ATennisCourt;

/** Broadcast for every ground contact. Location is the mark on the clay (cm). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTennisBallBouncedSignature,
	FVector, Location, bool, bInBounds, bool, bNetContact, int32, BounceIndex);

/** Broadcast whenever a player/AI launches the ball (including serves). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTennisBallHitSignature, AActor*, Hitter);

/**
 * Custom-integrated tennis ball.
 *
 * Why not UProjectileMovement/Chaos: quadratic drag, Magnus lift and the
 * spin-dependent clay bounce are the core of the simulation and must be
 * deterministic. The ball therefore advances with a fixed 240 Hz RK4
 * accumulator and never uses engine physics.
 *
 * Actor transforms are in centimeters; the internal TennisPhysics state is in
 * SI units, so the trajectory is identical regardless of frame rate.
 */
UCLASS()
class CLAYTENNIS_API ATennisBall : public AActor
{
	GENERATED_BODY()

public:
	ATennisBall();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintAssignable, Category = "Ball")
	FTennisBallBouncedSignature OnBallBounced;

	UPROPERTY(BlueprintAssignable, Category = "Ball")
	FTennisBallHitSignature OnBallHit;

	/** Solve pitch against the real trajectory, then launch toward TargetCm. */
	UFUNCTION(BlueprintCallable, Category = "Ball")
	void LaunchAtTarget(const FVector& TargetWorldCm, const FShotParams& Params, AActor* Hitter);

	UFUNCTION(BlueprintCallable, Category = "Ball")
	void LaunchWithVelocity(const FVector& VelocityCmPerSecond, const FVector& AngularVelocityRadPerSecond, AActor* Hitter);

	UFUNCTION(BlueprintCallable, Category = "Ball")
	void StopBall();

	UFUNCTION(BlueprintCallable, Category = "Ball")
	void ResetBall(const FVector& WorldLocationCm);

	UFUNCTION(BlueprintCallable, Category = "Ball")
	void SetSimulationEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Ball")
	void SetBallVisible(bool bVisible);

	/** First ground contact of the current flight, in world cm. */
	UFUNCTION(BlueprintCallable, Category = "Ball")
	bool PredictLanding(FVector& OutLocationCm, float& OutTimeSeconds) const;

	/** Second ground contact (after the first clay bounce), in world cm.
	 *  This is the "last chance" point the receiver must reach. */
	UFUNCTION(BlueprintCallable, Category = "Ball")
	bool PredictSecondBounce(FVector& OutLocationCm, float& OutTimeSeconds) const;

	UFUNCTION(BlueprintPure, Category = "Ball")
	float GetSpeedKph() const;

	UFUNCTION(BlueprintPure, Category = "Ball")
	float GetSpeedMps() const;

	UFUNCTION(BlueprintPure, Category = "Ball")
	FVector GetAngularVelocityRpm() const;

	UFUNCTION(BlueprintPure, Category = "Ball")
	bool IsSimulating() const { return bSimulating; }

	UFUNCTION(BlueprintPure, Category = "Ball")
	bool HadNetContactSinceHit() const { return bNetContactSinceHit; }

	UFUNCTION(BlueprintPure, Category = "Ball")
	int32 GetBounceIndex() const { return BounceIndex; }

	UFUNCTION(BlueprintPure, Category = "Ball")
	AActor* GetLastHitter() const { return LastHitter; }

	/** Shot table shared by the human pawn and the AI. Charge is 0..1. */
	UFUNCTION(BlueprintPure, Category = "Shot")
	static FShotParams MakeShotParams(EShotType Type, float Charge01);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ball")
	TObjectPtr<UStaticMeshComponent> BallMesh;

	UPROPERTY()
	TObjectPtr<AActor> LastHitter;

	TennisPhysics::FSimState SimState;
	double StepAccumulator = 0.0;
	bool bSimulating = false;
	bool bNetContactSinceHit = false;
	int32 BounceIndex = 0;

	void SimulateSubstep(double DeltaSeconds);
	bool ResolveNetCollision(const FVector& PreviousPositionM);
	void HandleBounce();
	void UpdateVisualRotation(float DeltaSeconds);
	void SyncActorToSimulation();
};
