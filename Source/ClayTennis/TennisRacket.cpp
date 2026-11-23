// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisRacket.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATennisRacket::ATennisRacket()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	FrameTop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameTop"));
	FrameBottom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameBottom"));
	FrameLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameLeft"));
	FrameRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameRight"));
	StringBed = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StringBed"));

	BuildRacket();
}

void ATennisRacket::BuildRacket()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMesh* CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;
	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	// The racket is authored in centimeters, in a YZ plane, origin at the grip.
	if (CylinderMesh && Handle)
	{
		Handle->SetupAttachment(SceneRoot);
		Handle->SetStaticMesh(CylinderMesh);
		Handle->SetRelativeLocation(FVector(0.0, 0.0, -12.0));
		Handle->SetRelativeScale3D(FVector(0.028, 0.028, 0.24));
		Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Handle->SetMobility(EComponentMobility::Movable);
	}

	const FVector FrameSideScale(0.02, 0.02, 0.34);

	if (CubeMesh && FrameTop && FrameBottom && FrameLeft && FrameRight && StringBed)
	{
		UStaticMeshComponent* Bars[5] = { FrameTop, FrameBottom, FrameLeft, FrameRight, StringBed };
		for (UStaticMeshComponent* Bar : Bars)
		{
			Bar->SetupAttachment(SceneRoot);
			Bar->SetStaticMesh(CubeMesh);
			Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bar->SetGenerateOverlapEvents(false);
			Bar->SetMobility(EComponentMobility::Movable);
		}

		FrameTop->SetRelativeLocation(FVector(0.0, 0.0, 38.0));
		FrameTop->SetRelativeScale3D(FVector(0.02, 0.26, 0.02));

		FrameBottom->SetRelativeLocation(FVector(0.0, 0.0, 6.0));
		FrameBottom->SetRelativeScale3D(FVector(0.02, 0.26, 0.02));

		FrameLeft->SetRelativeLocation(FVector(0.0, -13.0, 22.0));
		FrameLeft->SetRelativeScale3D(FrameSideScale);

		FrameRight->SetRelativeLocation(FVector(0.0, 13.0, 22.0));
		FrameRight->SetRelativeScale3D(FrameSideScale);

		// A thin plate stands in for the string bed; the default material is
		// opaque, so a real string pattern would need an authored material.
		StringBed->SetRelativeLocation(FVector(0.0, 0.0, 22.0));
		StringBed->SetRelativeScale3D(FVector(0.008, 0.24, 0.30));
	}
}

void ATennisRacket::BeginPlay()
{
	Super::BeginPlay();

	// 1) Preferred: the textured tennis_racquet.glb from the twin project.
	TArray<UStaticMesh*> Parts;
	for (int32 Idx = 0; Idx < 8; ++Idx)
	{
		const TCHAR* Path = (Idx == 0)
			? TEXT("/Game/Sprites/SM_RacketReal.SM_RacketReal")
			: *FString::Printf(TEXT("/Game/Sprites/SM_RacketReal_%d.SM_RacketReal_%d"), Idx, Idx);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, Path))
		{
			Parts.Add(M);
		}
		else if (Idx == 0)
		{
			break;  // no real racket at all
		}
		else
		{
			break;
		}
	}

	// 2) Fallback: every part of the racket.zip FBX (the frame, strings and
	// handle are separate meshes sharing one local frame; using just the
	// renamed part rendered a lone slab - the string bed).
	if (Parts.Num() == 0)
	{
		if (UStaticMesh* Single = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sprites/SM_Racket.SM_Racket")))
		{
			Parts.Add(Single);
		}
		for (int32 Idx = 0; Idx < 16; ++Idx)
		{
			if (UStaticMesh* M = LoadObject<UStaticMesh>(
				nullptr, *FString::Printf(TEXT("/Game/Sprites/Arch27_002_%02d.Arch27_002_%02d"), Idx, Idx)))
			{
				Parts.Add(M);
			}
		}
	}

	if (Parts.Num() > 0)
	{
		BuildSpriteRacket(Parts);
		return;
	}

	if (UMaterialInterface* Carbon = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/MI_Carbon.MI_Carbon")))
	{
		Handle->SetMaterial(0, Carbon);
		FrameTop->SetMaterial(0, Carbon);
		FrameBottom->SetMaterial(0, Carbon);
		FrameLeft->SetMaterial(0, Carbon);
		FrameRight->SetMaterial(0, Carbon);
	}
	if (UMaterialInterface* Strings = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_RacketStrings.M_RacketStrings")))
	{
		StringBed->SetMaterial(0, Strings);
	}

	ApplyTints();
}

void ATennisRacket::ApplyTints()
{
	const FLinearColor FrameColor(0.06f, 0.06f, 0.07f, 1.0f);
	const FLinearColor StringColor(0.85f, 0.85f, 0.82f, 1.0f);
	const FLinearColor GripColor(0.12f, 0.12f, 0.14f, 1.0f);

	auto TintComponent = [](UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		if (!Component)
		{
			return;
		}
		if (UMaterialInstanceDynamic* Dynamic = Component->CreateAndSetMaterialInstanceDynamic(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
			Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
		}
	};

	TintComponent(Handle, GripColor);
	TintComponent(FrameTop, FrameColor);
	TintComponent(FrameBottom, FrameColor);
	TintComponent(FrameLeft, FrameColor);
	TintComponent(FrameRight, FrameColor);
	TintComponent(StringBed, StringColor);
}

void ATennisRacket::BuildSpriteRacket(const TArray<UStaticMesh*>& Parts)
{
	// Parts share one local frame (the FBX/GLB node space). Compute the
	// combined bounds, normalize the assembly to 68 cm, roll it upright
	// (the source lies flat, long axis ~Y), and keep inter-part offsets.
	const float Big = TNumericLimits<float>::Max();
	FVector Min(Big, Big, Big), Max(-Big, -Big, -Big);
	for (UStaticMesh* Part : Parts)
	{
		const FBoxSphereBounds B = Part->GetBounds();
		Min.X = FMath::Min(Min.X, B.Origin.X - B.BoxExtent.X);
		Min.Y = FMath::Min(Min.Y, B.Origin.Y - B.BoxExtent.Y);
		Min.Z = FMath::Min(Min.Z, B.Origin.Z - B.BoxExtent.Z);
		Max.X = FMath::Max(Max.X, B.Origin.X + B.BoxExtent.X);
		Max.Y = FMath::Max(Max.Y, B.Origin.Y + B.BoxExtent.Y);
		Max.Z = FMath::Max(Max.Z, B.Origin.Z + B.BoxExtent.Z);
	}
	const FVector Center = (Min + Max) * 0.5f;
	const float Longest = FMath::Max3(Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z);
	const float S = (Longest > 0.5f) ? (68.0f / Longest) : 1.0f;
	const FRotator Upright(0.0, 0.0, 90.0);  // +Y long axis -> +Z (head up)

	for (int32 Idx = 0; Idx < Parts.Num(); ++Idx)
	{
		FName Name = MakeUniqueObjectName(GetRootComponent(), UStaticMeshComponent::StaticClass(),
			*FString::Printf(TEXT("RacketPartMesh%d"), Idx));
		UStaticMeshComponent* PartMesh = NewObject<UStaticMeshComponent>(this, Name);
		PartMesh->SetupAttachment(SceneRoot);
		PartMesh->SetStaticMesh(Parts[Idx]);
		PartMesh->SetRelativeRotation(Upright);
		PartMesh->SetRelativeScale3D(FVector(S));
		// Keep the shared-frame arrangement; the grip end lands on the slot
		// (the assembly's bottom tip = -34 cm from the centered position).
		PartMesh->SetRelativeLocation(Upright.RotateVector(S * (Center - Parts[Idx]->GetBounds().Origin))
			+ FVector(0.0, 0.0, 34.0f));
		PartMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartMesh->SetGenerateOverlapEvents(false);
		PartMesh->SetMobility(EComponentMobility::Movable);
		PartMesh->RegisterComponent();
	}

	Handle->SetVisibility(false);
	FrameTop->SetVisibility(false);
	FrameBottom->SetVisibility(false);
	FrameLeft->SetVisibility(false);
	FrameRight->SetVisibility(false);
	StringBed->SetVisibility(false);
}

void ATennisRacket::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (SwingTime <= 0.0f)
	{
		return;
	}

	SwingTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(SwingTime / SwingDuration, 0.0f, 1.0f);
	const float Envelope = FMath::Sin(Alpha * PI);

	if (bServeSwing)
	{
		const FRotator ServeArc(-70.0f * Envelope, -40.0f * Envelope, 150.0f * Envelope);
		SetActorRelativeRotation(BaseRelativeRotation + ServeArc);
	}
	else
	{
		const FRotator GroundstrokeArc(15.0f * Envelope, -95.0f * Envelope, 35.0f * Envelope);
		SetActorRelativeRotation(BaseRelativeRotation + GroundstrokeArc);
	}

	if (Alpha >= 1.0f)
	{
		SetActorRelativeRotation(BaseRelativeRotation);
		SwingTime = 0.0f;
	}
}

void ATennisRacket::PlaySwing(bool bServe)
{
	BaseRelativeRotation = GetRootComponent() ? GetRootComponent()->GetRelativeRotation() : FRotator::ZeroRotator;
	SwingTime = 0.001f;
	SwingDuration = bServe ? 0.38f : 0.28f;
	bServeSwing = bServe;
	UE_LOG(LogTemp, Warning, TEXT("[SWING] started serve=%d rot=%s"), bServe ? 1 : 0, *BaseRelativeRotation.ToString());
}
