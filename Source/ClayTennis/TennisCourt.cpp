// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisCourt.h"

#include "TennisPhysics.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor ClayColor(0.56f, 0.24f, 0.12f, 1.0f);
	const FLinearColor LineColor(0.92f, 0.90f, 0.84f, 1.0f);
	const FLinearColor NetColor(0.16f, 0.17f, 0.18f, 1.0f);
	const FLinearColor PostColor(0.72f, 0.72f, 0.70f, 1.0f);
	const FLinearColor MarkColor(0.34f, 0.14f, 0.07f, 1.0f);

	constexpr float HalfLengthCm = 1188.5f;
	constexpr float DoublesHalfWidthCm = 548.5f;
	constexpr float SinglesHalfWidthCm = 411.5f;
	constexpr float ServiceLineCm = 640.0f;
	constexpr float NetHalfWidthCm = 639.9f;
}

ATennisCourt::ATennisCourt()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ClaySurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClaySurface"));
	ClaySurface->SetupAttachment(SceneRoot);
	ClaySurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClaySurface->SetGenerateOverlapEvents(false);

	BounceMarkInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BounceMarks"));
	BounceMarkInstances->SetupAttachment(SceneRoot);
	BounceMarkInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BounceMarkInstances->SetGenerateOverlapEvents(false);
	BounceMarkInstances->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	if (CubeMesh)
	{
		ClaySurface->SetStaticMesh(CubeMesh);
		ClaySurface->SetRelativeLocation(FVector(0.0, 0.0, -4.0));
		ClaySurface->SetRelativeScale3D(FVector(28.0, 16.0, 0.08));
		TintedMeshes.Add(ClaySurface);
		TintedColors.Add(ClayColor);

		BuildCourtGeometry(CubeMesh, CylinderMesh);
	}

	if (CylinderMesh)
	{
		BounceMarkInstances->SetStaticMesh(CylinderMesh);
	}
	else if (CubeMesh)
	{
		// Degrade gracefully to a tiny cube mark if the cylinder is missing.
		BounceMarkInstances->SetStaticMesh(CubeMesh);
	}
}

void ATennisCourt::BuildCourtGeometry(UStaticMesh* CubeMesh, UStaticMesh* CylinderMesh)
{
	// ---- Painted lines, 5 cm wide, 4 mm above the clay ----

	// Baselines (doubles width).
	AddBox(TEXT("BaselineNear"), CubeMesh, FVector(-HalfLengthCm, 0.0, 0.4), FVector(5.0, 1097.0, 0.4), LineColor);
	AddBox(TEXT("BaselineFar"), CubeMesh, FVector(HalfLengthCm, 0.0, 0.4), FVector(5.0, 1097.0, 0.4), LineColor);

	// Doubles and singles sidelines.
	AddBox(TEXT("DoublesLineNear"), CubeMesh, FVector(0.0, -DoublesHalfWidthCm, 0.4), FVector(2377.0, 5.0, 0.4), LineColor);
	AddBox(TEXT("DoublesLineFar"), CubeMesh, FVector(0.0, DoublesHalfWidthCm, 0.4), FVector(2377.0, 5.0, 0.4), LineColor);
	AddBox(TEXT("SinglesLineNear"), CubeMesh, FVector(0.0, -SinglesHalfWidthCm, 0.4), FVector(2377.0, 5.0, 0.4), LineColor);
	AddBox(TEXT("SinglesLineFar"), CubeMesh, FVector(0.0, SinglesHalfWidthCm, 0.4), FVector(2377.0, 5.0, 0.4), LineColor);

	// Service lines and center service lines.
	AddBox(TEXT("ServiceLineNear"), CubeMesh, FVector(-ServiceLineCm, 0.0, 0.4), FVector(5.0, 823.0, 0.4), LineColor);
	AddBox(TEXT("ServiceLineFar"), CubeMesh, FVector(ServiceLineCm, 0.0, 0.4), FVector(5.0, 823.0, 0.4), LineColor);
	AddBox(TEXT("CenterServiceNear"), CubeMesh, FVector(-ServiceLineCm * 0.5f, 0.0, 0.4), FVector(ServiceLineCm, 5.0, 0.4), LineColor);
	AddBox(TEXT("CenterServiceFar"), CubeMesh, FVector(ServiceLineCm * 0.5f, 0.0, 0.4), FVector(ServiceLineCm, 5.0, 0.4), LineColor);

	// Center marks on both baselines (20 cm long, 5 cm wide).
	AddBox(TEXT("CenterMarkNear"), CubeMesh, FVector(-HalfLengthCm + 10.0f, 0.0, 0.4), FVector(20.0, 5.0, 0.4), LineColor);
	AddBox(TEXT("CenterMarkFar"), CubeMesh, FVector(HalfLengthCm - 10.0f, 0.0, 0.4), FVector(20.0, 5.0, 0.4), LineColor);

	// ---- Net ----
	// Vertical strings. A true mesh material would be cheaper but requires a
	// translucent asset, which this project deliberately avoids.
	constexpr int32 StringCount = 16;
	for (int32 Index = 0; Index <= StringCount; ++Index)
	{
		const float Y = -NetHalfWidthCm + (2.0f * NetHalfWidthCm) * (static_cast<float>(Index) / StringCount);
		AddBox(TEXT("NetString"), CubeMesh, FVector(0.0, Y, 45.7f), FVector(1.4, 1.2, 91.4), NetColor);
	}

	// Horizontal net bands.
	for (int32 Band = 0; Band < 4; ++Band)
	{
		const float Z = 18.0f + Band * 23.0f;
		AddBox(TEXT("NetBand"), CubeMesh, FVector(0.0, 0.0, Z), FVector(1.4, 1279.8, 1.4), NetColor);
	}

	// White canvas top band and center strap (the strap is the 1.07 m point).
	AddBox(TEXT("NetTopBand"), CubeMesh, FVector(0.0, 0.0, 91.4f), FVector(2.4, 1279.8, 6.0), LineColor);
	AddBox(TEXT("NetCenterStrap"), CubeMesh, FVector(0.0, 0.0, 53.5f), FVector(4.0, 5.0, 107.0), LineColor);

	// Posts: 1.07 m high, 0.914 m outside the doubles sideline.
	if (CylinderMesh)
	{
		AddBox(TEXT("NetPostNear"), CylinderMesh, FVector(0.0, -NetHalfWidthCm, 53.5f), FVector(12.0, 12.0, 107.0), PostColor);
		AddBox(TEXT("NetPostFar"), CylinderMesh, FVector(0.0, NetHalfWidthCm, 53.5f), FVector(12.0, 12.0, 107.0), PostColor);
	}

	// A darker runoff apron so the camera does not look into the void.
	AddBox(TEXT("Runoff"), CubeMesh, FVector(0.0, 0.0, -0.6), FVector(3400.0, 2200.0, 1.0), FLinearColor(0.10f, 0.20f, 0.10f, 1.0f));
}

UStaticMeshComponent* ATennisCourt::AddBox(const FString& BaseName,
	UStaticMesh* Mesh,
	const FVector& CenterCm,
	const FVector& SizeCm,
	const FLinearColor& Tint)
{
	if (!Mesh)
	{
		return nullptr;
	}

	const FName ComponentName(*FString::Printf(TEXT("%s_%d"), *BaseName, TintedMeshes.Num()));
	UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
	if (!Component)
	{
		return nullptr;
	}

	Component->SetupAttachment(SceneRoot);
	Component->SetStaticMesh(Mesh);
	Component->SetRelativeLocation(CenterCm);
	Component->SetRelativeScale3D(SizeCm / 100.0);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(true);
	Component->SetMobility(EComponentMobility::Movable);

	TintedMeshes.Add(Component);
	TintedColors.Add(Tint);
	return Component;
}

void ATennisCourt::BeginPlay()
{
	Super::BeginPlay();
	ApplyTints();
}

void ATennisCourt::ApplyTints()
{
	for (int32 Index = 0; Index < TintedMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Component = TintedMeshes[Index];
		if (!Component || !TintedColors.IsValidIndex(Index))
		{
			continue;
		}

		// The engine basic-shape material exposes a "Color" vector parameter.
		// If a different material is present the parameter is simply ignored.
		UMaterialInstanceDynamic* Dynamic = Component->CreateAndSetMaterialInstanceDynamic(0);
		if (Dynamic)
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), TintedColors[Index]);
			Dynamic->SetVectorParameterValue(TEXT("BaseColor"), TintedColors[Index]);
		}
	}

	if (BounceMarkInstances)
	{
		UMaterialInstanceDynamic* MarkMaterial = BounceMarkInstances->CreateAndSetMaterialInstanceDynamic(0);
		if (MarkMaterial)
		{
			MarkMaterial->SetVectorParameterValue(TEXT("Color"), MarkColor);
			MarkMaterial->SetVectorParameterValue(TEXT("BaseColor"), MarkColor);
		}
	}
}

float ATennisCourt::GetCourtLengthCm()
{
	return static_cast<float>(TennisSpec::CourtLengthM * 100.0);
}

float ATennisCourt::GetCourtHalfLengthCm()
{
	return static_cast<float>(TennisSpec::CourtHalfLengthM * 100.0);
}

float ATennisCourt::GetSinglesHalfWidthCm()
{
	return static_cast<float>(TennisSpec::SinglesHalfWidthM * 100.0);
}

float ATennisCourt::GetDoublesHalfWidthCm()
{
	return static_cast<float>(TennisSpec::DoublesHalfWidthM * 100.0);
}

float ATennisCourt::GetServiceLineDistanceCm()
{
	return static_cast<float>(TennisSpec::ServiceLineFromNetM * 100.0);
}

bool ATennisCourt::IsInSinglesCourt(const FVector& WorldLocation)
{
	const double Tolerance = TennisSpec::LineToleranceM * 100.0;
	return FMath::Abs(WorldLocation.X) <= TennisSpec::CourtHalfLengthM * 100.0 + Tolerance
		&& FMath::Abs(WorldLocation.Y) <= TennisSpec::SinglesHalfWidthM * 100.0 + Tolerance;
}

bool ATennisCourt::IsInDoublesCourt(const FVector& WorldLocation)
{
	const double Tolerance = TennisSpec::LineToleranceM * 100.0;
	return FMath::Abs(WorldLocation.X) <= TennisSpec::CourtHalfLengthM * 100.0 + Tolerance
		&& FMath::Abs(WorldLocation.Y) <= TennisSpec::DoublesHalfWidthM * 100.0 + Tolerance;
}

bool ATennisCourt::IsInServiceBox(const FVector& WorldLocation, ETennisSide ReceiverSide, bool bDeuceCourt)
{
	if (ReceiverSide == ETennisSide::None)
	{
		return false;
	}

	const double ReceiverSign = TennisUtil::SideSign(ReceiverSide);
	const double ExpectedYSide = bDeuceCourt ? -ReceiverSign : ReceiverSign;
	const double Tolerance = TennisSpec::LineToleranceM * 100.0;

	return (WorldLocation.X * ReceiverSign) >= -Tolerance
		&& FMath::Abs(WorldLocation.X) <= TennisSpec::ServiceLineFromNetM * 100.0 + Tolerance
		&& FMath::Abs(WorldLocation.Y) <= TennisSpec::SinglesHalfWidthM * 100.0 + Tolerance
		&& (WorldLocation.Y * ExpectedYSide) >= -Tolerance;
}

FVector ATennisCourt::GetServeTarget(ETennisSide ServerSide, bool bDeuceCourt)
{
	const double ServerSign = TennisUtil::SideSign(ServerSide);
	const double ReceiverSign = -ServerSign;
	const double ExpectedYSide = bDeuceCourt ? -ReceiverSign : ReceiverSign;

	// 3.2 m deep, 2.0 m off the center line: a safe middle-of-the-box target.
	return FVector(ReceiverSign * 320.0, ExpectedYSide * 200.0, 0.0);
}

FVector ATennisCourt::ClampToServiceBox(const FVector& WorldLocation, ETennisSide ReceiverSide, bool bDeuceCourt)
{
	if (ReceiverSide == ETennisSide::None)
	{
		return FVector::ZeroVector;
	}

	const double ReceiverSign = TennisUtil::SideSign(ReceiverSide);
	const double ExpectedYSide = bDeuceCourt ? -ReceiverSign : ReceiverSign;

	// Keep the aim at least 1.2 m past the net and 0.2 m inside the service
	// line; the Y clamp keeps it inside the singles sideline.
	const double Depth = FMath::Clamp(FMath::Abs(static_cast<double>(WorldLocation.X)), 120.0, 620.0);
	const double Width = FMath::Clamp(FMath::Abs(static_cast<double>(WorldLocation.Y)), 30.0, 390.0);

	return FVector(ReceiverSign * Depth, ExpectedYSide * Width, 0.0);
}

float ATennisCourt::GetNetHeightAtYCm(float Y)
{
	return static_cast<float>(TennisPhysics::GetNetHeightAtY(static_cast<double>(Y) / 100.0) * 100.0);
}

ETennisSide ATennisCourt::GetSideFromX(float X)
{
	return TennisUtil::SideFromX(static_cast<double>(X));
}

void ATennisCourt::AddBounceMark(const FVector& WorldLocation, bool bInBounds)
{
	if (!BounceMarkInstances)
	{
		return;
	}

	// Keep the buffer bounded; an old set of marks is not worth more memory.
	if (BounceMarks.Num() >= 140)
	{
		ClearBounceMarks();
	}

	const FTransform MarkTransform(FRotator::ZeroRotator,
		FVector(WorldLocation.X, WorldLocation.Y, 0.7),
		FVector(0.085, 0.085, 0.006));
	BounceMarkInstances->AddInstance(MarkTransform, true);

	FTennisBounceMark Mark;
	Mark.Location = FVector(WorldLocation.X, WorldLocation.Y, 0.0);
	Mark.bInBounds = bInBounds;
	Mark.TimeSeconds = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	BounceMarks.Add(Mark);
}

void ATennisCourt::ClearBounceMarks()
{
	BounceMarks.Reset();
	if (BounceMarkInstances)
	{
		BounceMarkInstances->ClearInstances();
	}
}
