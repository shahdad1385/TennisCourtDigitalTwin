// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisPlayerPawn.h"

#include "TennisBall.h"
#include "TennisCourt.h"
#include "TennisRacket.h"

#include "Camera/CameraComponent.h"
#include "Components/ChildActorComponent.h"
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
	BodyMesh->SetRelativeScale3D(FVector(0.42, 0.42, 1.8));
	BodyMesh->SetCastShadow(true);

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
	RacketSlot->SetRelativeLocation(FVector(30.0, 28.0, 0.0));
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

	ApplyBodyTint();
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

void ATennisPlayerPawn::ApplyBodyTint()
{
	if (!BodyMesh)
	{
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
