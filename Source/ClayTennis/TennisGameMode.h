// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TennisTypes.h"
#include "TennisGameMode.generated.h"

class ATennisBall;
class ATennisCourt;
class ATennisPlayerPawn;
class UTennisAIComponent;
class UTennisScoringComponent;

/**
 * Match director.
 *
 * Flow: StartNewMatch -> StartNextPoint -> PreServe -> Serving -> Rally ->
 * AwardPoint -> pause -> StartNextPoint. The game mode is the only class that
 * knows both the score engine and the physical actors; everything else talks
 * to it through the small query API below (used by the HUD and controller).
 */
UCLASS()
class CLAYTENNIS_API ATennisGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATennisGameMode();

	virtual void BeginPlay() override;

	void BeginAutoShot();
	virtual void Tick(float DeltaSeconds) override;

	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

	// ---- Human serve / reset entry points (controller) ----
	UFUNCTION(BlueprintCallable, Category = "Match")
	bool IsHumanServing() const;

	UFUNCTION(BlueprintCallable, Category = "Match")
	void ExecuteHumanServe(ATennisPlayerPawn* Pawn, float Charge01, const FVector& AimTargetCm);

	UFUNCTION(BlueprintCallable, Category = "Match")
	void ResetMatchOrPoint();

	// ---- HUD / query API ----
	UFUNCTION(BlueprintPure, Category = "Match")
	ERallyState GetRallyState() const { return RallyState; }

	UFUNCTION(BlueprintPure, Category = "Match")
	ATennisBall* GetBall() const { return Ball; }

	UFUNCTION(BlueprintPure, Category = "Match")
	ATennisCourt* GetCourt() const { return Court; }

	UFUNCTION(BlueprintPure, Category = "Match")
	UTennisScoringComponent* GetScoring() const { return Scoring; }

	UFUNCTION(BlueprintPure, Category = "Match")
	ATennisPlayerPawn* GetHumanPawn() const { return HumanPawn; }

	UFUNCTION(BlueprintPure, Category = "Match")
	ATennisPlayerPawn* GetAIPawn() const { return AIPawn; }

	UFUNCTION(BlueprintPure, Category = "Match")
	ATennisPlayerPawn* GetPawnForPlayer(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Match")
	FString GetPlayerDisplayName(int32 PlayerIndex) const;

	UFUNCTION(BlueprintPure, Category = "Match")
	FString GetStatusLine() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	FString GetCenterMessage() const { return CenterMessage; }

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetCenterMessageAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetServeNumber() const { return ServeNumber; }

	UFUNCTION(BlueprintPure, Category = "Match")
	float GetLastShotSpeedKph() const { return LastShotSpeedKph; }

	UFUNCTION(BlueprintPure, Category = "Match")
	bool IsMatchComplete() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetMatchWinner() const;

	UFUNCTION(BlueprintPure, Category = "Match")
	ETennisSide GetHumanPhysicalSide() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Match")
	TObjectPtr<UTennisScoringComponent> Scoring;

	UPROPERTY()
	TObjectPtr<ATennisCourt> Court;

	UPROPERTY()
	TObjectPtr<ATennisBall> Ball;

	UPROPERTY()
	TObjectPtr<ATennisPlayerPawn> HumanPawn;

	UPROPERTY()
	TObjectPtr<ATennisPlayerPawn> AIPawn;

	UPROPERTY()
	TObjectPtr<UTennisAIComponent> AIBrain;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 BestOfSets = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float AIDifficulty = 0.62f;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FString HumanDisplayName = TEXT("PLAYER");

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FString AIDisplayName = TEXT("CPU");

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	bool bSpawnSunLight = true;

	ERallyState RallyState = ERallyState::Idle;
	int32 ServeNumber = 1;
	int32 ServeFaults = 0;
	bool bCurrentPointWasServe = false;
	float LastShotSpeedKph = 0.0f;

	FString CenterMessage;
	float MessageTimer = 0.0f;
	float MessageDuration = 0.0f;

	FTimerHandle StartMatchTimer;
	FTimerHandle NextPointTimer;
	FTimerHandle AIServeTimer;
	FTimerHandle SecondServeTimer;
	FTimerHandle LetTimer;

	// ---- Spawning / setup ----
	void SpawnWorldActors();
	void SetupEnvironment();
	ATennisPlayerPawn* SpawnHumanAtStart(AController* NewPlayer);
	void EnsureHumanPawn();

	// ---- Match flow ----
	void StartNewMatch();
	void StartNextPoint();
	void StartSecondServe();
	void StartLetReplay();
	void ExecuteAIServe();
	void ExecuteServe(int32 ServerIndex, float Charge01, const FVector& TargetCm, EShotType ServeType);

	void PositionPlayersForPoint();
	FVector GetServeContactPoint() const;
	FVector GetServeTargetForServer() const;
	bool ApplyEndsSwapIfNeeded();

	void AwardPoint(int32 Winner, const FString& Reason);
	void HandleServeBounce(const FVector& Location, bool bNetContact);

	void SetCenterMessage(const FString& Message, float Duration);

	// ---- Ball delegates (must be UFUNCTION) ----
	UFUNCTION()
	void HandleBallBounced(FVector Location, bool bInBounds, bool bNetContact, int32 BounceIndex);

	UFUNCTION()
	void HandleBallHit(AActor* Hitter);
};
