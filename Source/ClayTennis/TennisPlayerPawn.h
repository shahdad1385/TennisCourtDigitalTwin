// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/UnrealMathUtility.h"
#include "TennisTypes.h"
#include "TennisPlayerPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class USphereComponent;
class UStaticMeshComponent;
class UChildActorComponent;
class ATennisBall;
class ATennisRacket;

/** Minimal critically damped spring state (UE 5.5 has no FFloatSpringState in Core). */
struct FTennisSpringState
{
	float PrevValue = 0.0f;
	float PrevVelocity = 0.0f;
};
/**
 * Human and AI player avatar.
 *
 * The pawn owns no CharacterMovementComponent: clay-court footwork is a simple
 * accelerated 2D model, which is easier to keep human-like (and network-free)
 * than a full character controller. A/D slides along the baseline, W/S
 * approaches the net. The human camera is a spring arm behind the shoulder and
 * the mouse drives a yaw/pitch aim offset; the aim ray is intersected with the
 * clay plane to produce a world-space target.
 */
UCLASS()
class CLAYTENNIS_API ATennisPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	ATennisPlayerPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- Setup ----
	void ConfigurePlayer(int32 InPlayerIndex, ETennisSide InSide, bool bInAI, const FString& InName);
	void SetBall(ATennisBall* InBall);
	void SetPhysicalSide(ETennisSide NewSide);
	void PlaceAtWorld(const FVector& WorldLocationCm);
	void SetReadyPose(const FVector& WorldLocationCm);

	// ---- Input / movement ----
	void SetMoveInput(float RightAxis, float ForwardAxis);
	void AddAimInput(float YawDeltaDegrees, float PitchDeltaDegrees);
	void AddCameraZoom(float Delta);
	void SetAITargetWorld(const FVector& TargetWorldCm, float SpeedScale);
	void ClearAIMovement();

	// ---- Shot flow ----
	void BeginCharge();
	void EndCharge();
	float ConsumeCharge();
	void CycleSpin(int32 Direction);
	bool ReleaseGroundstroke();
	bool TryHitBall(ATennisBall* InBall, const FVector& TargetWorldCm, EShotType ShotType, float Charge01);
	void PlayHitReaction(bool bServe);

	// ---- Queries ----
	UFUNCTION(BlueprintPure, Category = "Tennis")
	FVector GetAimTargetWorld() const;

	UFUNCTION(BlueprintPure, Category = "Tennis")
	FVector GetReadyWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Tennis")
	ETennisSide GetPhysicalSide() const { return PhysicalSide; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	int32 GetPlayerIndex() const { return PlayerIndex; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	bool IsAI() const { return bIsAI; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	FString GetPlayerName() const { return PlayerName; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	ESpinType GetSelectedSpin() const { return SelectedSpin; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	float GetChargeAmount() const;

	UFUNCTION(BlueprintPure, Category = "Tennis")
	bool IsCharging() const { return bCharging; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	float GetReachCm() const { return ReachCm; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	ATennisRacket* GetRacket() const { return Racket; }

	UFUNCTION(BlueprintPure, Category = "Tennis")
	FVector GetMoveVelocity() const { return MoveVelocity; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<USphereComponent> CollisionRoot;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "Tennis")
	TObjectPtr<UChildActorComponent> RacketSlot;

	UPROPERTY()
	TObjectPtr<ATennisRacket> Racket;

	UPROPERTY()
	TObjectPtr<ATennisBall> BallRef;

	UPROPERTY()
	int32 PlayerIndex = 0;

	UPROPERTY()
	ETennisSide PhysicalSide = ETennisSide::Near;

	UPROPERTY()
	bool bIsAI = false;

	UPROPERTY()
	FString PlayerName = TEXT("PLAYER");

	UPROPERTY()
	ESpinType SelectedSpin = ESpinType::Topspin;

	UPROPERTY(EditAnywhere, Category = "Tennis")
	float MoveSpeed = 640.0f;

	UPROPERTY(EditAnywhere, Category = "Tennis")
	float MoveAcceleration = 3400.0f;

	UPROPERTY(EditAnywhere, Category = "Tennis")
	float ReachCm = 250.0f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector MoveVelocity = FVector::ZeroVector;

	bool bAITargetValid = false;
	FVector AITarget = FVector::ZeroVector;
	float AISpeedScale = 1.0f;

	float AimYaw = 0.0f;
	float AimPitch = 0.0f;
	float SmoothedAimYaw = 0.0f;
	float SmoothedAimPitch = 0.0f;
	FTennisSpringState AimYawSpring;
	FTennisSpringState AimPitchSpring;

	float CameraZoom = 880.0f;
	float TargetCameraZoom = 880.0f;

	bool bCharging = false;
	double ChargeStartTime = 0.0;
	float LastCharge01 = 0.0f;

	float ShakeTime = 0.0f;
	float ShakeDuration = 0.22f;
	float ShakeAmplitude = 2.0f;

	void UpdateMovement(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void ApplyBodyTint();
	void FaceNet();
	FVector ClampHumanAim(const FVector& Target) const;
};
