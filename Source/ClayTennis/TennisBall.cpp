// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisBall.h"

#include "TennisCourt.h"
#include "TennisPhysics.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double TwoPi = 6.28318530717958647692;
	constexpr double RpmToRadPerSec = TwoPi / 60.0;
	constexpr double BallDiameterCm = TennisSpec::BallDiameterM * 100.0;
	constexpr double FixedSimStep = 1.0 / 240.0;
}

ATennisBall::ATennisBall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	SetRootComponent(BallMesh);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetGenerateOverlapEvents(false);
	BallMesh->SetCastShadow(true);
	BallMesh->SetMobility(EComponentMobility::Movable);
	BallMesh->SetRelativeScale3D(FVector(BallDiameterCm / 100.0));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereFinder.Object);
	}
}

void ATennisBall::BeginPlay()
{
	Super::BeginPlay();

	if (BallMesh)
	{
		if (UMaterialInterface* BallMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_TennisBall.MI_TennisBall")))
		{
			BallMesh->SetMaterial(0, BallMaterial);
		}
		UMaterialInstanceDynamic* Dynamic = BallMesh->CreateAndSetMaterialInstanceDynamic(0);
		if (Dynamic)
		{
			const FLinearColor BallColor(0.92f, 0.88f, 0.16f, 1.0f);
			Dynamic->SetVectorParameterValue(TEXT("Color"), BallColor);
			Dynamic->SetVectorParameterValue(TEXT("BaseColor"), BallColor);
		}
	}

	ResetBall(GetActorLocation());
}

void ATennisBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSimulating)
	{
		UpdateVisualRotation(DeltaSeconds);
		return;
	}

	// Fixed-timestep accumulator: trajectories are reproducible at any framerate.
	StepAccumulator += FMath::Min(static_cast<double>(DeltaSeconds), 0.1);
	int32 Guard = 0;
	while (StepAccumulator >= FixedSimStep)
	{
		SimulateSubstep(FixedSimStep);
		StepAccumulator -= FixedSimStep;

		if (!bSimulating)
		{
			StepAccumulator = 0.0;
			break;
		}

		// Avoid a spiral of death if the game hitches badly.
		if (++Guard > 48)
		{
			StepAccumulator = 0.0;
			break;
		}
	}

	SyncActorToSimulation();
	UpdateVisualRotation(DeltaSeconds);
}

void ATennisBall::SimulateSubstep(double DeltaSeconds)
{
	const FVector PreviousPosition = SimState.Position;

	TennisPhysics::IntegrateRK4(SimState, DeltaSeconds);
	ResolveNetCollision(PreviousPosition);

	if (SimState.Position.Z <= TennisSpec::BallRadiusM && SimState.Velocity.Z < 0.0)
	{
		HandleBounce();
	}
}

bool ATennisBall::ResolveNetCollision(const FVector& PreviousPositionM)
{
	// Only a sign change across x = 0 can hit the net.
	if (PreviousPositionM.X * SimState.Position.X >= 0.0)
	{
		return false;
	}

	const double DeltaX = SimState.Position.X - PreviousPositionM.X;
	if (FMath::Abs(DeltaX) < 1.0e-9)
	{
		return false;
	}

	const double Alpha = FMath::Clamp(-PreviousPositionM.X / DeltaX, 0.0, 1.0);
	const FVector Crossing = FMath::Lerp(PreviousPositionM, SimState.Position, Alpha);
	const double NetTop = TennisPhysics::GetNetHeightAtY(Crossing.Y);
	const double BallBottom = Crossing.Z - TennisSpec::BallRadiusM;

	if (FMath::Abs(Crossing.Y) > TennisSpec::NetHalfWidthM
		|| BallBottom > NetTop
		|| Crossing.Z < 0.0)
	{
		return false;
	}

	bNetContactSinceHit = true;

	// Top-cord contact keeps most of the forward direction, producing a let.
	if (FMath::Abs(BallBottom - NetTop) <= TennisSpec::NetCordWindowM)
	{
		SimState.Position = Crossing;
		SimState.Velocity.X *= TennisSpec::NetCordSpeedRetain;
		SimState.Velocity.Y *= 0.90;
		SimState.Velocity.Z = FMath::Max(SimState.Velocity.Z, 0.35);
		SimState.AngularVelocity *= 0.65;
		return true;
	}

	SimState.Position = Crossing;
	SimState.Position.X = (PreviousPositionM.X >= 0.0) ? 0.02 : -0.02;
	SimState.Velocity.X = -SimState.Velocity.X * TennisSpec::NetRestitution;
	SimState.Velocity.Y *= TennisSpec::NetTangentialDamping;
	SimState.Velocity.Z *= TennisSpec::NetTangentialDamping;
	SimState.AngularVelocity *= 0.5;
	return true;
}

void ATennisBall::HandleBounce()
{
	SimState.Position.Z = TennisSpec::BallRadiusM;

	FVector ContactCm = SimState.Position * 100.0;
	ContactCm.Z = 0.0;

	const bool bInBounds = ATennisCourt::IsInSinglesCourt(ContactCm);

	const TennisPhysics::FBounceResult Bounce = TennisPhysics::ComputeBounce(
		SimState.Velocity, SimState.AngularVelocity);

	if (Bounce.bValid)
	{
		SimState.Velocity = Bounce.Velocity;
		SimState.AngularVelocity = Bounce.AngularVelocity;
	}
	else
	{
		// Degenerate contact: kill the downward component so the loop cannot
		// re-enter the court surface every substep.
		SimState.Velocity.Z = 0.0;
	}

	++BounceIndex;

	// Move the visual ball before broadcasting so listeners reading the actor
	// transform (AI, HUD) see the bounce position.
	SyncActorToSimulation();

	OnBallBounced.Broadcast(ContactCm, bInBounds, bNetContactSinceHit, BounceIndex);
}

void ATennisBall::UpdateVisualRotation(float DeltaSeconds)
{
	if (!BallMesh || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const double SpinMagnitude = SimState.AngularVelocity.Size();
	if (SpinMagnitude < 0.01)
	{
		return;
	}

	const FVector Axis = SimState.AngularVelocity / SpinMagnitude;
	BallMesh->AddWorldRotation(FQuat(Axis, SpinMagnitude * static_cast<double>(DeltaSeconds)));
}

void ATennisBall::SyncActorToSimulation()
{
	SetActorLocation(SimState.Position * 100.0);
}

void ATennisBall::LaunchAtTarget(const FVector& TargetWorldCm, const FShotParams& Params, AActor* Hitter)
{
	if (Params.SpeedMps <= 1.0f)
	{
		return;
	}

	FVector Start = SimState.Position;
	// A ball picked up off the clay is lifted to a playable contact height.
	if (Start.Z < 0.22)
	{
		Start.Z = 0.22;
	}
	SimState.Position = Start;

	FVector TargetMeters(TargetWorldCm.X / 100.0, TargetWorldCm.Y / 100.0, 0.0);
	FVector ToTarget = TargetMeters - Start;
	ToTarget.Z = 0.0;

	if (ToTarget.Size() < 0.6)
	{
		// Degenerate aim: hit straight toward the other baseline.
		ToTarget = FVector((Start.X < 0.0) ? 1.0 : -1.0, 0.0, 0.0) * 5.0;
	}
	const FVector Direction = ToTarget.GetSafeNormal();

	// Topspin axis for a ball travelling along +X is +Y (right-hand rule).
	const FVector TopspinAxis = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	const FVector SpinRadPerSec = TopspinAxis * (static_cast<double>(Params.TopspinRpm) * RpmToRadPerSec)
		+ FVector::UpVector * (static_cast<double>(Params.SideSpinRpm) * RpmToRadPerSec);

	double PitchRadians = FMath::DegreesToRadians(10.0);
	if (!TennisPhysics::SolveLaunchAngle(Start, TargetMeters, Params.SpeedMps, SpinRadPerSec, PitchRadians))
	{
		// Keep a sane fallback for very short / unreachable targets.
		PitchRadians = FMath::DegreesToRadians(6.0);
	}

	const FVector LaunchVelocity = (Direction * FMath::Cos(PitchRadians)
		+ FVector(0.0, 0.0, FMath::Sin(PitchRadians))) * static_cast<double>(Params.SpeedMps);

	LaunchWithVelocity(LaunchVelocity * 100.0, SpinRadPerSec, Hitter);
}

void ATennisBall::LaunchWithVelocity(const FVector& VelocityCmPerSecond, const FVector& AngularVelocityRadPerSecond, AActor* Hitter)
{
	SimState.Velocity = VelocityCmPerSecond / 100.0;
	SimState.AngularVelocity = AngularVelocityRadPerSecond;
	StepAccumulator = 0.0;
	BounceIndex = 0;
	bNetContactSinceHit = false;
	bSimulating = true;
	LastHitter = Hitter;

	SyncActorToSimulation();
	OnBallHit.Broadcast(Hitter);
}

void ATennisBall::ResetBall(const FVector& WorldLocationCm)
{
	SimState = TennisPhysics::FSimState();
	SimState.Position = FVector(WorldLocationCm.X / 100.0, WorldLocationCm.Y / 100.0,
		FMath::Max(WorldLocationCm.Z, 0.5) / 100.0);

	StepAccumulator = 0.0;
	bSimulating = false;
	bNetContactSinceHit = false;
	BounceIndex = 0;
	LastHitter = nullptr;

	SetBallVisible(true);
	SyncActorToSimulation();
}

void ATennisBall::StopBall()
{
	bSimulating = false;
	StepAccumulator = 0.0;
	SimState.Velocity = FVector::ZeroVector;
	SimState.AngularVelocity = FVector::ZeroVector;
}

void ATennisBall::SetSimulationEnabled(bool bEnabled)
{
	bSimulating = bEnabled;
	if (bEnabled)
	{
		StepAccumulator = 0.0;
	}
}

void ATennisBall::SetBallVisible(bool bVisible)
{
	if (BallMesh)
	{
		BallMesh->SetVisibility(bVisible, true);
	}
}

bool ATennisBall::PredictLanding(FVector& OutLocationCm, float& OutTimeSeconds) const
{
	FVector LandingMeters = FVector::ZeroVector;
	double TimeSeconds = 0.0;
	bool bNetContact = false;

	const bool bLanded = TennisPhysics::SimulateUntilLanding(SimState, 6.0, 1.0 / 120.0,
		LandingMeters, TimeSeconds, bNetContact);

	OutLocationCm = LandingMeters * 100.0;
	OutLocationCm.Z = 0.0;
	OutTimeSeconds = static_cast<float>(TimeSeconds);
	return bLanded;
}

bool ATennisBall::PredictSecondBounce(FVector& OutLocationCm, float& OutTimeSeconds) const
{
	FVector LandingMeters = FVector::ZeroVector;
	double TimeSeconds = 0.0;
	bool bNetContact = false;

	const bool bLanded = TennisPhysics::SimulateToBounceNumber(SimState, 2, 8.0, 1.0 / 120.0,
		LandingMeters, TimeSeconds, bNetContact);

	OutLocationCm = LandingMeters * 100.0;
	OutLocationCm.Z = 0.0;
	OutTimeSeconds = static_cast<float>(TimeSeconds);
	return bLanded;
}

float ATennisBall::GetSpeedKph() const
{
	return static_cast<float>(SimState.Velocity.Size() * 3.6);
}

float ATennisBall::GetSpeedMps() const
{
	return static_cast<float>(SimState.Velocity.Size());
}

FVector ATennisBall::GetAngularVelocityRpm() const
{
	return SimState.AngularVelocity * (60.0 / TwoPi);
}

FShotParams ATennisBall::MakeShotParams(EShotType Type, float Charge01)
{
	const float Charge = FMath::Clamp(Charge01, 0.0f, 1.0f);

	FShotParams Params;
	Params.ShotType = Type;
	Params.Charge01 = Charge;
	Params.bIsServe = TennisUtil::IsServeShot(Type);

	switch (Type)
	{
	case EShotType::Flat:
		Params.SpeedMps = 26.0f + 10.0f * Charge;
		Params.TopspinRpm = 250.0f + 350.0f * Charge;
		Params.SideSpinRpm = 0.0f;
		break;

	case EShotType::Topspin:
		Params.SpeedMps = 22.0f + 9.0f * Charge;
		Params.TopspinRpm = 1500.0f + 2500.0f * Charge;
		Params.SideSpinRpm = 0.0f;
		break;

	case EShotType::Slice:
		Params.SpeedMps = 17.0f + 7.0f * Charge;
		Params.TopspinRpm = -(800.0f + 600.0f * Charge);
		Params.SideSpinRpm = 220.0f;
		break;

	case EShotType::ServeFlat:
		// 34-46 m/s (122-166 km/h). Above ~47 m/s a serve from a 2.75 m
		// contact cannot land inside the 6.4 m service box at all.
		Params.SpeedMps = 34.0f + 12.0f * Charge;
		Params.TopspinRpm = 250.0f;
		Params.SideSpinRpm = 0.0f;
		break;

	case EShotType::ServeKick:
		Params.SpeedMps = 27.0f + 11.0f * Charge;
		Params.TopspinRpm = 2300.0f + 1000.0f * Charge;
		Params.SideSpinRpm = 350.0f;
		break;

	case EShotType::ServeSlice:
		Params.SpeedMps = 27.0f + 11.0f * Charge;
		Params.TopspinRpm = -400.0f;
		Params.SideSpinRpm = 1600.0f;
		break;

	default:
		break;
	}

	return Params;
}
