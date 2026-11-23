// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TennisTypes.h"
#include "TennisCourt.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;

/** One clay mark left by a bounce, kept in chronological order. */
USTRUCT(BlueprintType)
struct FTennisBounceMark
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Court")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Court")
	bool bInBounds = true;

	UPROPERTY(BlueprintReadOnly, Category = "Court")
	float TimeSeconds = 0.0f;
};

/**
 * ITF regulation court, built entirely from /Engine/BasicShapes primitives.
 *
 * Dimensions (metres, converted to cm in code):
 *   full doubles court  23.77 x 10.97
 *   singles court       23.77 x 8.23
 *   service line        6.40 from the net
 *   net height          0.914 at the posts, 1.07 at the center strap
 *
 * The actor is visual only (no collision); all gameplay queries go through
 * the static helpers so that the ball, AI and game mode agree on one source
 * of truth for line calls.
 */
UCLASS()
class CLAYTENNIS_API ATennisCourt : public AActor
{
	GENERATED_BODY()

public:
	ATennisCourt();

	virtual void BeginPlay() override;

	// ---- Court constants (cm) ----
	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetCourtLengthCm();

	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetCourtHalfLengthCm();

	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetSinglesHalfWidthCm();

	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetDoublesHalfWidthCm();

	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetServiceLineDistanceCm();

	// ---- Line calls ----
	UFUNCTION(BlueprintPure, Category = "Court")
	static bool IsInSinglesCourt(const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Court")
	static bool IsInDoublesCourt(const FVector& WorldLocation);

	/** ReceiverSide is the physical end the serve must land on. bDeuceCourt
	 *  selects the diagonal half (deuce vs advantage). */
	UFUNCTION(BlueprintPure, Category = "Court")
	static bool IsInServiceBox(const FVector& WorldLocation, ETennisSide ReceiverSide, bool bDeuceCourt);

	/** Center of the correct service box, in world cm (Z = 0). */
	UFUNCTION(BlueprintPure, Category = "Court")
	static FVector GetServeTarget(ETennisSide ServerSide, bool bDeuceCourt);

	/** Project any aim point into the legal service box. */
	UFUNCTION(BlueprintPure, Category = "Court")
	static FVector ClampToServiceBox(const FVector& WorldLocation, ETennisSide ReceiverSide, bool bDeuceCourt);

	UFUNCTION(BlueprintPure, Category = "Court")
	static float GetNetHeightAtYCm(float Y);

	UFUNCTION(BlueprintPure, Category = "Court")
	static ETennisSide GetSideFromX(float X);

	// ---- Clay marks ----
	UFUNCTION(BlueprintCallable, Category = "Court")
	void AddBounceMark(const FVector& WorldLocation, bool bInBounds);

	UFUNCTION(BlueprintCallable, Category = "Court")
	void ClearBounceMarks();

	const TArray<FTennisBounceMark>& GetBounceMarks() const { return BounceMarks; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Court")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Court")
	TObjectPtr<UStaticMeshComponent> ClaySurface;

	UPROPERTY(VisibleAnywhere, Category = "Court")
	TObjectPtr<UInstancedStaticMeshComponent> BounceMarkInstances;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TintedMeshes;

	UPROPERTY()
	TArray<FLinearColor> TintedColors;

	UPROPERTY()
	TArray<FTennisBounceMark> BounceMarks;

	/** Adds a box component in court-local cm and queues it for tinting. */
	UStaticMeshComponent* AddBox(const FString& BaseName,
		UStaticMesh* Mesh,
		const FVector& CenterCm,
		const FVector& SizeCm,
		const FLinearColor& Tint);

	void BuildCourtGeometry(UStaticMesh* CubeMesh, UStaticMesh* CylinderMesh);
	void ApplyTints();
};
