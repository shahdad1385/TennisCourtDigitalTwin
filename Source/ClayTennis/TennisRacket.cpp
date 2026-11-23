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

	// User-provided sprite model wins over the primitive racket; it is attached
	// to SceneRoot so PlaySwing rotates it exactly like the primitives did.
	if (UStaticMesh* Sprite = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Sprites/SM_Racket.SM_Racket")))
	{
		UStaticMeshComponent* SpriteMesh = NewObject<UStaticMeshComponent>(this, TEXT("SpriteRacketMesh"));
		SpriteMesh->SetupAttachment(SceneRoot);
		SpriteMesh->SetStaticMesh(Sprite);
		// Normalize so the longest bounds axis equals a 68 cm racket.
		const FBoxSphereBounds Bounds = Sprite->GetBounds();
		const float Longest = FMath::Max3(Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z);
		const FRotator Upright(0.0, 0.0, 90.0);
		float S = 1.0f;
		if (Longest > 0.5f)
		{
			S = 68.0f / (2.0f * Longest);
			SpriteMesh->SetRelativeScale3D(FVector(S));
		}
		// The FBX lies flat (long axis ~Y): roll upright so the head is up.
		// Its pivot is ~40 cm away from the geometry (bounds origin), so the
		// component is offset by -Rot*S*Origin to re-center on the slot.
		SpriteMesh->SetRelativeRotation(Upright);
		SpriteMesh->SetRelativeLocation(Upright.RotateVector(-Bounds.Origin * S));
		SpriteMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SpriteMesh->SetGenerateOverlapEvents(false);
		SpriteMesh->SetMobility(EComponentMobility::Movable);
		SpriteMesh->RegisterComponent();

		if (Handle) { Handle->SetVisibility(false); }
		FrameTop->SetVisibility(false);
		FrameBottom->SetVisibility(false);
		FrameLeft->SetVisibility(false);
		FrameRight->SetVisibility(false);
		StringBed->SetVisibility(false);
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
