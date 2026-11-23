// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TennisRacket.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/**
 * Code-built tennis racket (cylinder grip + cube frame/cross strings).
 * Purely cosmetic: the hit test lives in ATennisPlayerPawn.
 */
UCLASS()
class CLAYTENNIS_API ATennisRacket : public AActor
{
	GENERATED_BODY()

public:
	ATennisRacket();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Racket")
	void PlaySwing(bool bServe);

	UFUNCTION(BlueprintPure, Category = "Racket")
	bool IsSwinging() const { return SwingTime > 0.0f; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> Handle;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> FrameTop;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> FrameBottom;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> FrameLeft;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> FrameRight;

	UPROPERTY(VisibleAnywhere, Category = "Racket")
	TObjectPtr<UStaticMeshComponent> StringBed;

	float SwingTime = 0.0f;
	float SwingDuration = 0.30f;
	bool bServeSwing = false;
	FRotator BaseRelativeRotation = FRotator::ZeroRotator;

	void BuildRacket();
	void ApplyTints();
};
