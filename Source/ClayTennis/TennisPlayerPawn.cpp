// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisPlayerPawn.h"

#include "TennisBall.h"
#include "TennisCourt.h"
#include "TennisRacket.h"

#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float StandHeightCm = 90.0f;
	constexpr float BaseCameraPitch = -14.0f;
	constexpr float MaxChargeSeconds = 1.05f;
	constexpr float CourtRunoffY = 820.0f;
	constexpr float CourtRunoffBehindBaseline = 320.0f;

	float TennisSpringInterp(float Current, float Target, FTennisSpringState& State,
		float Stiffness, float CriticalDampingFactor, float DeltaTime)
	{
		const float Stiff = FMath::Max(0.0f, Stiffness);
		const float Damping = FMath::Max(0.0f, CriticalDampingFactor);
		const float Dt = FMath::Clamp(DeltaTime, 0.0f, 0.05f);
		const float SpringForce = (Target - Current) * Stiff;
		const float DampingForce = -State.PrevVelocity * (2.0f * FMath::Sqrt(Stiff) * Damping);
		State.PrevVelocity += (SpringForce + DampingForce) * Dt;
		State.PrevValue = Current + State.PrevVelocity * Dt;
		return State.PrevValue;
	}

	EShotType ShotTypeForSpin(ESpinType Spin)
	{
		switch (Spin)
		{
		case ESpinType::Flat:
			return EShotType::Flat;
		case ESpinType::Slice:
			return EShotType::Slice;
		default:
			return EShotType::Topspin;
		}
	}
}

ATennisPlayerPawn::ATennisPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	CollisionRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionRoot"));
	SetRootComponent(CollisionRoot);
	CollisionRoot->InitSphereRadius(42.0f);
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionRoot->SetGenerateOverlapEvents(false);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionRoot);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);
	BodyMesh->SetRelativeScale3D(FVector(0.36, 0.36, 0.90));
	BodyMesh->SetCastShadow(true);

	// Skeletal mannequin (used when the copied Mannequin assets exist);
	// animated in single-node mode from code - no authored animation assets.
	PlayerMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PlayerMesh"));
	PlayerMesh->SetupAttachment(CollisionRoot);
	PlayerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerMesh->SetGenerateOverlapEvents(false);
	PlayerMesh->SetCastShadow(true);
	PlayerMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	PlayerMesh->SetRelativeLocation(FVector(0.0, 0.0, -StandHeightCm));
	PlayerMesh->SetVisibility(false);  // enabled only if the mesh loads

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderFinder.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CollisionRoot);
	CameraBoom->SetRelativeLocation(FVector(0.0, 25.0, 70.0));
	CameraBoom->SetRelativeRotation(FRotator(BaseCameraPitch, 0.0, 0.0));
	CameraBoom->TargetArmLength = CameraZoom;
	CameraBoom->SocketOffset = FVector(0.0, 30.0, 15.0);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 16.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 78.0f;

	RacketSlot = CreateDefaultSubobject<UChildActorComponent>(TEXT("RacketSlot"));
	RacketSlot->SetupAttachment(CollisionRoot);
	// Actor-relative (NOT the hand bone): the mannequin's bone orientation is
	// unpredictable and can bury the racket - the twin project's fix. Their
	// (12, -32, 88) assumes a feet-origin player; our root rides at the hip
	// (90 cm), so the racket ends up at ~88 cm world: waist/hand height.
	RacketSlot->SetRelativeLocation(FVector(12.0, -32.0, -2.0));
	RacketSlot->SetRelativeRotation(FRotator(0.0, -18.0, 0.0));
	RacketSlot->SetChildActorClass(ATennisRacket::StaticClass());
}

void ATennisPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	if (RacketSlot)
	{
		Racket = Cast<ATennisRacket>(RacketSlot->GetChildActor());
	}

	ApplyBodyVisual();

	FaceNet();

	if (FollowCamera)
	{
		FollowCamera->SetActive(!bIsAI);
	}
	if (CameraBoom && bIsAI)
	{
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
	}

	// Snap the initial boom orientation so the first frame is not a spring
	// interpolation from the wrong side of the court.
	if (CameraBoom)
	{
		const float BaseYaw = (PhysicalSide == ETennisSide::Near) ? 0.0f : 180.0f;
		CameraBoom->SetRelativeRotation(FRotator(BaseCameraPitch, BaseYaw, 0.0f));
	}
}

void ATennisPlayerPawn::ApplyBodyVisual()
{
	if (!BodyMesh)
	{
		return;
	}

	// 1) Preferred: the copied Mannequin skeletal mesh, animated from code.
	if (USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny")))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PAWN] mannequin loaded, applying"));
		PlayerMesh->SetSkeletalMesh(Manny);
		PlayerMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);

		// Normalize to 180 cm; the mannequin root sits at its feet, and the
		// pawn root rides at hip height, so shift down by the stance height.
		const FBoxSphereBounds B = Manny->GetBounds();
		const float HeightCm = FMath::Max(2.0f * B.BoxExtent.Z, 1.0f);
		const float S = 180.0f / HeightCm;
		PlayerMesh->SetRelativeScale3D(FVector(S));
		PlayerMesh->SetRelativeLocation(FVector(0.0, 0.0, -StandHeightCm));

		// Start idle; UpdateLocomotionAnim swaps Walk/Run from the speed.
		if (UAnimationAsset* Idle = LoadObject<UAnimationAsset>(nullptr,
			TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle")))
		{
			PlayerMesh->SetAnimation(Idle);
			PlayerMesh->PlayAnimation(Idle, true);
		}

		// glTF-era copies of Manny face +Y; yaw -90 maps forward onto +X
		// (toward the net) so Walk/Run run along the court, not into the walls.
		PlayerMesh->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));

		// Team look: player 0 wears the _02 outfit, CPU the dark _01.
		UMaterialInterface* Outfit = LoadObject<UMaterialInterface>(nullptr,
			(PlayerIndex == 0)
				? TEXT("/Game/Characters/Mannequins/Materials/Instances/Manny/MI_Manny_02.MI_Manny_02")
				: TEXT("/Game/Characters/Mannequins/Materials/Instances/Manny/MI_Manny_01.MI_Manny_01"));
		if (Outfit)
		{
			PlayerMesh->SetMaterial(0, Outfit);
		}

		PlayerMesh->SetVisibility(true);
		LocomotionAnimState = 0;

		// Hide the static fallbacks beneath the real model.
		BodyMesh->SetVisibility(false);
		BodyMesh->SetCastShadow(false);
		return;
	}

	// 2) Fallback: the imported static character parts.
	UE_LOG(LogTemp, Warning, TEXT("[PAWN] mannequin NOT loaded - using static fallback"));
	TArray<UStaticMesh*> Parts;
	if (UStaticMesh* Single = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sprites/SM_PlayerMale.SM_PlayerMale")))
	{
		Parts.Add(Single);
	}
	else
	{
		for (int32 Idx = 0; Idx < 32; ++Idx)
		{
			UStaticMesh* Part = LoadObject<UStaticMesh>(
				nullptr, *FString::Printf(TEXT("/Game/Sprites/SM_PlayerPart_%02d.SM_PlayerPart_%02d"), Idx, Idx));
			if (!Part)
			{
				break;
			}
			Parts.Add(Part);
		}
	}

	if (Parts.Num() > 0)
	{
		// Combined bounds across all parts (unscaled, component space).
		float MinZ = TNumericLimits<float>::Max(), MaxZ = TNumericLimits<float>::Lowest();
		for (UStaticMesh* Part : Parts)
		{
			const FBoxSphereBounds B = Part->GetBounds();
			MinZ = FMath::Min(MinZ, B.Origin.Z - B.BoxExtent.Z);
			MaxZ = FMath::Max(MaxZ, B.Origin.Z + B.BoxExtent.Z);
		}
		const float HeightCm = FMath::Max(MaxZ - MinZ, 1.0f);
		const float S = 180.0f / HeightCm;

		for (int32 Idx = 0; Idx < Parts.Num(); ++Idx)
		{
			FName Name = MakeUniqueObjectName(GetRootComponent(), UStaticMeshComponent::StaticClass(),
				*FString::Printf(TEXT("PlayerPartMesh%d"), Idx));
			UStaticMeshComponent* PartMesh = NewObject<UStaticMeshComponent>(this, Name);
			PartMesh->SetupAttachment(CollisionRoot);
			PartMesh->SetStaticMesh(Parts[Idx]);
			PartMesh->SetRelativeLocation(FVector(0.0, 0.0, -StandHeightCm - S * MinZ));
			PartMesh->SetRelativeScale3D(FVector(S));
			PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PartMesh->SetGenerateOverlapEvents(false);
			PartMesh->SetMobility(EComponentMobility::Movable);
			PartMesh->RegisterComponent();
		}

		// Hide the fallback cylinder beneath the real model.
		BodyMesh->SetVisibility(false);
		BodyMesh->SetCastShadow(false);
		return;
	}

	UMaterialInstanceDynamic* Dynamic = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (!Dynamic)
	{
		return;
	}

	const FLinearColor Color = (PlayerIndex == 0)
		? FLinearColor(0.12f, 0.32f, 0.70f, 1.0f)
		: FLinearColor(0.72f, 0.16f, 0.14f, 1.0f);
	Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
	Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
}

void ATennisPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMovement(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateLocomotionAnim();
	UpdateSwingAnim(DeltaSeconds);
}

void ATennisPlayerPawn::UpdateSwingAnim(float DeltaSeconds)
{
	if (!PlayerMesh || !PlayerMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	if (SwingAnimTime > 0.0f)
	{
		SwingAnimTime = FMath::Max(0.0f, SwingAnimTime - DeltaSeconds);
		const float Alpha = 1.0f - (SwingAnimTime / SwingAnimDuration);
		const float Envelope = FMath::Sin(Alpha * PI);
		// Base yaw -90 (the mesh's imported orientation) composed with the
		// swing: groundstrokes twist about yaw, serves chop overhead.
		const float BaseYaw = -90.0f;
		const FRotator Swing = (SwingStyle == 1)
			? FRotator(-45.0f * Envelope, BaseYaw - 20.0f * Envelope, 0.0f)
			: FRotator(0.0f, BaseYaw - 55.0f * Envelope, 0.0f);
		PlayerMesh->SetRelativeRotation(Swing);
	}
	else if (!PlayerMesh->GetRelativeRotation().IsNearlyZero(0.1f))
	{
		PlayerMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
}

void ATennisPlayerPawn::UpdateLocomotionAnim()
{
	if (!PlayerMesh || !PlayerMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	const float Speed = MoveVelocity.Size();
	const int32 State = (Speed > 320.0f) ? 2 : ((Speed > 40.0f) ? 1 : 0);
	if (State == LocomotionAnimState)
	{
		return;
	}
	LocomotionAnimState = State;

	const TCHAR* AnimPath = (State == 2)
		? TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Run_Fwd.MM_Run_Fwd")
		: (State == 1)
			? TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Walk_Fwd.MM_Walk_Fwd")
			: TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle");

	if (UAnimationAsset* Anim = LoadObject<UAnimationAsset>(nullptr, AnimPath))
	{
		PlayerMesh->PlayAnimation(Anim, true);
	}
}

void ATennisPlayerPawn::UpdateMovement(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float ForwardSign = (PhysicalSide == ETennisSide::Near) ? 1.0f : -1.0f;

	FVector DesiredDirection = FVector::ZeroVector;
	float SpeedScale = 1.0f;

	if (bAITargetValid)
	{
		FVector ToTarget = AITarget - GetActorLocation();
		ToTarget.Z = 0.0f;
		const float Distance = ToTarget.Size();
		if (Distance > 18.0f)
		{
			DesiredDirection = ToTarget / Distance;
		}
		SpeedScale = AISpeedScale;
	}
	else
	{
		DesiredDirection = FVector(
			MoveInput.Y * ForwardSign,
			MoveInput.X * ForwardSign,
			0.0f);
	}

	if (DesiredDirection.SizeSquared() > 1.0f)
	{
		DesiredDirection.Normalize();
	}

	const FVector DesiredVelocity = DesiredDirection * MoveSpeed * SpeedScale;
	FVector VelocityDelta = DesiredVelocity - MoveVelocity;
	const float MaxDelta = MoveAcceleration * DeltaSeconds;
	if (VelocityDelta.SizeSquared() > MaxDelta * MaxDelta)
	{
		VelocityDelta = VelocityDelta.GetSafeNormal() * MaxDelta;
	}
	MoveVelocity += VelocityDelta;
	if (MoveVelocity.SizeSquared() < 1.0f)
	{
		MoveVelocity = FVector::ZeroVector;
	}

	FVector NewLocation = GetActorLocation() + MoveVelocity * DeltaSeconds;
	NewLocation.Z = StandHeightCm;

	const float HalfLength = ATennisCourt::GetCourtHalfLengthCm();
	const float MinX = (PhysicalSide == ETennisSide::Near) ? -(HalfLength + CourtRunoffBehindBaseline) : 70.0f;
	const float MaxX = (PhysicalSide == ETennisSide::Near) ? -70.0f : (HalfLength + CourtRunoffBehindBaseline);
	NewLocation.X = FMath::Clamp(static_cast<double>(NewLocation.X), static_cast<double>(MinX), static_cast<double>(MaxX));
	NewLocation.Y = FMath::Clamp(static_cast<double>(NewLocation.Y), -static_cast<double>(CourtRunoffY), static_cast<double>(CourtRunoffY));

	SetActorLocation(NewLocation, false);
}

void ATennisPlayerPawn::UpdateCamera(float DeltaSeconds)
{
	if (bIsAI || !CameraBoom || !FollowCamera)
	{
		return;
	}

	const float Dt = FMath::Clamp(DeltaSeconds, 0.0001f, 0.05f);
	SmoothedAimYaw = TennisSpringInterp(SmoothedAimYaw, AimYaw, AimYawSpring, 14.0f, 1.0f, Dt);
	SmoothedAimPitch = TennisSpringInterp(SmoothedAimPitch, AimPitch, AimPitchSpring, 14.0f, 1.0f, Dt);

	const float BaseYaw = (PhysicalSide == ETennisSide::Near) ? 0.0f : 180.0f;
	CameraBoom->SetRelativeRotation(FRotator(BaseCameraPitch + SmoothedAimPitch, BaseYaw + SmoothedAimYaw, 0.0f));

	CameraZoom = FMath::FInterpTo(CameraZoom, TargetCameraZoom, Dt, 8.0f);
	CameraBoom->TargetArmLength = CameraZoom;

	if (ShakeTime > 0.0f)
	{
		ShakeTime = FMath::Max(0.0f, ShakeTime - DeltaSeconds);
		const float Alpha = (ShakeDuration > 0.0f) ? FMath::Clamp(ShakeTime / ShakeDuration, 0.0f, 1.0f) : 0.0f;
		const float Amplitude = ShakeAmplitude * Alpha;
		FollowCamera->SetRelativeLocation(FVector(0.0f,
			FMath::FRandRange(-Amplitude, Amplitude),
			FMath::FRandRange(-Amplitude, Amplitude)));
	}
	else if (!FollowCamera->GetRelativeLocation().IsNearlyZero())
	{
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
	}
}

void ATennisPlayerPawn::SetMoveInput(float RightAxis, float ForwardAxis)
{
	MoveInput.X = FMath::Clamp(RightAxis, -1.0f, 1.0f);
	MoveInput.Y = FMath::Clamp(ForwardAxis, -1.0f, 1.0f);
	if (!MoveInput.IsNearlyZero())
	{
		bAITargetValid = false;
	}
}

void ATennisPlayerPawn::AddAimInput(float YawDeltaDegrees, float PitchDeltaDegrees)
{
	AimYaw = FMath::Clamp(AimYaw + YawDeltaDegrees, -38.0f, 38.0f);
	AimPitch = FMath::Clamp(AimPitch + PitchDeltaDegrees, -6.0f, 18.0f);
}

void ATennisPlayerPawn::AddCameraZoom(float Delta)
{
	TargetCameraZoom = FMath::Clamp(TargetCameraZoom + Delta * 90.0f, 520.0f, 1450.0f);
}

void ATennisPlayerPawn::SetAITargetWorld(const FVector& TargetWorldCm, float SpeedScale)
{
	bAITargetValid = true;
	AITarget = FVector(TargetWorldCm.X, TargetWorldCm.Y, StandHeightCm);
	AISpeedScale = FMath::Clamp(SpeedScale, 0.2f, 1.2f);
}

void ATennisPlayerPawn::ClearAIMovement()
{
	bAITargetValid = false;
}

FVector ATennisPlayerPawn::ClampHumanAim(const FVector& Target) const
{
	const float ForwardSign = (PhysicalSide == ETennisSide::Near) ? 1.0f : -1.0f;
	const float MinX = (ForwardSign > 0.0f) ? 120.0f : -1900.0f;
	const float MaxX = (ForwardSign > 0.0f) ? 1900.0f : -120.0f;
	return FVector(
		FMath::Clamp(static_cast<double>(Target.X), static_cast<double>(MinX), static_cast<double>(MaxX)),
		FMath::Clamp(static_cast<double>(Target.Y), -1200.0, 1200.0),
		0.0f);
}

FVector ATennisPlayerPawn::GetAimTargetWorld() const
{
	const float ForwardSign = (PhysicalSide == ETennisSide::Near) ? 1.0f : -1.0f;

	if (!FollowCamera)
	{
		return GetActorLocation() + FVector(ForwardSign * 900.0f, 0.0f, -StandHeightCm);
	}

	FVector RayOrigin = FollowCamera->GetComponentLocation();
	FVector RayDirection = FollowCamera->GetForwardVector();
	if (RayDirection.Z > -0.02f)
	{
		RayDirection.Z = -0.02f;
		RayDirection.Normalize();
	}

	if (FMath::Abs(RayDirection.Z) < 1.0e-4f)
	{
		return ClampHumanAim(RayOrigin + FVector(ForwardSign * 900.0f, 0.0f, -RayOrigin.Z));
	}

	const double DistanceToGround = -static_cast<double>(RayOrigin.Z) / static_cast<double>(RayDirection.Z);
	const FVector Target = RayOrigin + RayDirection * FMath::Clamp(DistanceToGround, 100.0, 12000.0);
	return ClampHumanAim(FVector(Target.X, Target.Y, 0.0f));
}

FVector ATennisPlayerPawn::GetReadyWorldLocation() const
{
	const float Sign = TennisUtil::SideSign(PhysicalSide);
	const float X = Sign * (ATennisCourt::GetCourtHalfLengthCm() + 120.0f);
	return FVector(X, 0.0f, StandHeightCm);
}

void ATennisPlayerPawn::BeginCharge()
{
	if (bCharging)
	{
		return;
	}
	bCharging = true;
	ChargeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastCharge01 = 0.0f;
}

void ATennisPlayerPawn::EndCharge()
{
	if (!bCharging)
	{
		return;
	}
	LastCharge01 = GetChargeAmount();
	bCharging = false;
}

float ATennisPlayerPawn::GetChargeAmount() const
{
	if (!bCharging)
	{
		return LastCharge01;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : ChargeStartTime;
	const float Elapsed = static_cast<float>(Now - ChargeStartTime);
	return FMath::Clamp(Elapsed / MaxChargeSeconds, 0.0f, 1.0f);
}

float ATennisPlayerPawn::ConsumeCharge()
{
	EndCharge();
	const float Charge = LastCharge01;
	LastCharge01 = 0.0f;
	return Charge;
}

void ATennisPlayerPawn::CycleSpin(int32 Direction)
{
	static const ESpinType Cycle[] = { ESpinType::Flat, ESpinType::Topspin, ESpinType::Slice };
	constexpr int32 CycleCount = 3;

	int32 Index = 1;
	for (int32 Candidate = 0; Candidate < CycleCount; ++Candidate)
	{
		if (Cycle[Candidate] == SelectedSpin)
		{
			Index = Candidate;
			break;
		}
	}

	Index = (Index + (Direction >= 0 ? 1 : -1) + CycleCount) % CycleCount;
	SelectedSpin = Cycle[Index];
}

bool ATennisPlayerPawn::TryHitBall(ATennisBall* InBall, const FVector& TargetWorldCm, EShotType ShotType, float Charge01)
{
	if (!InBall || !InBall->IsSimulating())
	{
		return false;
	}

	const FVector BallLocation = InBall->GetActorLocation();
	FVector ToBall = BallLocation - GetActorLocation();
	ToBall.Z = 0.0f;

	if (ToBall.Size() > ReachCm)
	{
		return false;
	}
	if (BallLocation.Z < 10.0f || BallLocation.Z > 320.0f)
	{
		return false;
	}

	const ETennisSide BallSide = TennisUtil::SideFromX(BallLocation.X);
	if (BallSide != PhysicalSide && FMath::Abs(BallLocation.X) > 180.0f)
	{
		return false;
	}

	// No double hits before the ball has bounced.
	if (InBall->GetLastHitter() == this && InBall->GetBounceIndex() == 0)
	{
		return false;
	}

	const float Charge = FMath::Clamp(Charge01, 0.0f, 1.0f);
	FVector EffectiveTarget(TargetWorldCm.X, TargetWorldCm.Y, 0.0f);

	if (!bIsAI)
	{
		EffectiveTarget = ClampHumanAim(EffectiveTarget);

		// Human error: small at full charge, noticeable when scrambling.
		const FVector Origin = GetActorLocation();
		FVector Offset = EffectiveTarget - Origin;
		Offset.Z = 0.0f;

		const float DistanceError = (1.0f - Charge) * 0.10f + 0.015f;
		const float DistanceScale = 1.0f + FMath::FRandRange(-DistanceError, DistanceError);
		Offset *= DistanceScale;

		const float YawError = FMath::FRandRange(-1.0f, 1.0f) * ((1.0f - Charge) * 3.5f + 0.4f);
		Offset = Offset.RotateAngleAxis(YawError, FVector::UpVector);

		EffectiveTarget = Origin + Offset;
		EffectiveTarget.Z = 0.0f;
		EffectiveTarget.X = FMath::Clamp(static_cast<double>(EffectiveTarget.X), -1900.0, 1900.0);
		EffectiveTarget.Y = FMath::Clamp(static_cast<double>(EffectiveTarget.Y), -1200.0, 1200.0);
	}

	FShotParams Params = ATennisBall::MakeShotParams(ShotType, Charge);
	InBall->LaunchAtTarget(EffectiveTarget, Params, this);

	PlayHitReaction(TennisUtil::IsServeShot(ShotType));
	return true;
}

bool ATennisPlayerPawn::ReleaseGroundstroke()
{
	const float Charge = ConsumeCharge();

	if (!BallRef || !BallRef->IsSimulating())
	{
		return false;
	}

	return TryHitBall(BallRef, GetAimTargetWorld(), ShotTypeForSpin(SelectedSpin), Charge);
}

void ATennisPlayerPawn::PlayHitReaction(bool bServe)
{
	ShakeDuration = bServe ? 0.26f : 0.20f;
	ShakeTime = ShakeDuration;
	ShakeAmplitude = bServe ? 3.2f : 2.0f;

	// Procedural swing: torso twist + racket arc together read as a stroke.
	SwingStyle = bServe ? 1 : 0;
	SwingAnimDuration = bServe ? 0.38f : 0.26f;
	SwingAnimTime = SwingAnimDuration;

	if (Racket)
	{
		Racket->PlaySwing(bServe);
	}
}

void ATennisPlayerPawn::ConfigurePlayer(int32 InPlayerIndex, ETennisSide InSide, bool bInAI, const FString& InName)
{
	PlayerIndex = InPlayerIndex;
	PhysicalSide = InSide;
	bIsAI = bInAI;
	PlayerName = InName;

	MoveSpeed = bIsAI ? 570.0f : 640.0f;
	MoveAcceleration = bIsAI ? 2800.0f : 3400.0f;
	ReachCm = bIsAI ? 205.0f : 250.0f;

	// Start a fresh match/point with a neutral view and no leftover motion.
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	SmoothedAimYaw = 0.0f;
	SmoothedAimPitch = 0.0f;
	AimYawSpring.PrevValue = 0.0f;
	AimYawSpring.PrevVelocity = 0.0f;
	AimPitchSpring.PrevValue = 0.0f;
	AimPitchSpring.PrevVelocity = 0.0f;
	MoveVelocity = FVector::ZeroVector;
	MoveInput = FVector2D::ZeroVector;
	bAITargetValid = false;

	FaceNet();

	if (FollowCamera)
	{
		FollowCamera->SetActive(!bIsAI);
	}
	if (CameraBoom)
	{
		const float BaseYaw = (PhysicalSide == ETennisSide::Near) ? 0.0f : 180.0f;
		CameraBoom->SetRelativeRotation(FRotator(BaseCameraPitch, BaseYaw, 0.0f));
	}
}

void ATennisPlayerPawn::SetBall(ATennisBall* InBall)
{
	BallRef = InBall;
}

void ATennisPlayerPawn::SetPhysicalSide(ETennisSide NewSide)
{
	if (NewSide == ETennisSide::None || NewSide == PhysicalSide)
	{
		return;
	}

	PhysicalSide = NewSide;
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	SmoothedAimYaw = 0.0f;
	SmoothedAimPitch = 0.0f;
	AimYawSpring.PrevValue = 0.0f;
	AimYawSpring.PrevVelocity = 0.0f;
	AimPitchSpring.PrevValue = 0.0f;
	AimPitchSpring.PrevVelocity = 0.0f;
	MoveVelocity = FVector::ZeroVector;
	bAITargetValid = false;

	FaceNet();

	if (CameraBoom)
	{
		const float BaseYaw = (PhysicalSide == ETennisSide::Near) ? 0.0f : 180.0f;
		CameraBoom->SetRelativeRotation(FRotator(BaseCameraPitch, BaseYaw, 0.0f));
	}
}

void ATennisPlayerPawn::PlaceAtWorld(const FVector& WorldLocationCm)
{
	SetActorLocation(FVector(WorldLocationCm.X, WorldLocationCm.Y, StandHeightCm), false);
	MoveVelocity = FVector::ZeroVector;
	bAITargetValid = false;
	MoveInput = FVector2D::ZeroVector;
}

void ATennisPlayerPawn::SetReadyPose(const FVector& WorldLocationCm)
{
	PlaceAtWorld(WorldLocationCm);
	FaceNet();
}

void ATennisPlayerPawn::FaceNet()
{
	const float Yaw = (PhysicalSide == ETennisSide::Near) ? 0.0f : 180.0f;
	SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
}
