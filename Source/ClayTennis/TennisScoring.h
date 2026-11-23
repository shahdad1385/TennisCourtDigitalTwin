// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TennisTypes.h"
#include "TennisScoring.generated.h"

/** Bit flags returned by AwardPoint so the game mode can pick messages/delays. */
namespace ETennisPointFlags
{
	enum Type : int32
	{
		None = 0,
		Game = 1 << 0,
		Set = 1 << 1,
		Match = 1 << 2
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTennisPointScoredSignature, int32, Winner, int32, Flags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTennisGameWonSignature, int32, Winner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTennisSetWonSignature, int32, Winner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTennisMatchWonSignature, int32, Winner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTennisEndsChangedSignature);

/**
 * Complete ITF scoring engine.
 *
 * Rules implemented:
 *  - 0/15/30/40, deuce at 3-3, advantage at 4-3 (raw point counts).
 *  - Game to 4 points with a 2-point margin.
 *  - Set to 6 games with a 2-game margin (6-4, 7-5).
 *  - Tiebreak at 6-6, first to 7 with a 2-point margin.
 *  - Best of 3 or best of 5.
 *  - Serve rotation every game; proper tiebreak serve rotation (1, then 2).
 *  - Change ends after odd-numbered games (and every 6 tiebreak points).
 *
 * The component knows nothing about actors: it only deals in player indices
 * (0 = human, 1 = AI). Positions on court are the game mode's concern.
 */
UCLASS(ClassGroup = (Tennis), meta = (BlueprintSpawnableComponent))
class CLAYTENNIS_API UTennisScoringComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTennisScoringComponent();

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	void StartMatch(int32 InBestOfSets);

	UFUNCTION(BlueprintCallable, Category = "Scoring")
	void ResetMatch();

	/** Awards the point and returns a combination of ETennisPointFlags. */
	UFUNCTION(BlueprintCallable, Category = "Scoring")
	int32 AwardPoint(int32 Winner);

	UFUNCTION(BlueprintPure, Category = "Scoring")
	FTennisScore GetScore() const { return Score; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	FString GetPointDisplay(int32 Player) const;

	UFUNCTION(BlueprintPure, Category = "Scoring")
	FString GetScoreLine() const;

	UFUNCTION(BlueprintPure, Category = "Scoring")
	bool IsDeuce() const;

	UFUNCTION(BlueprintPure, Category = "Scoring")
	bool IsAdvantage(int32 Player) const;

	UFUNCTION(BlueprintPure, Category = "Scoring")
	bool IsTiebreak() const { return Score.bTiebreak; }

	/** Even total points in the current game = deuce-court serve. */
	UFUNCTION(BlueprintPure, Category = "Scoring")
	bool IsDeuceCourtServe() const;

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetServer() const { return Score.Server; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetReceiver() const { return 1 - Score.Server; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetBestOfSets() const { return BestOfSets; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	bool IsMatchOver() const { return bMatchOver; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetMatchWinner() const { return MatchWinner; }

	UFUNCTION(BlueprintPure, Category = "Scoring")
	int32 GetTotalGamesPlayed() const { return TotalGames; }

	/** One-shot read of the pending change-ends request. */
	UFUNCTION(BlueprintCallable, Category = "Scoring")
	bool ConsumeEndsChangeRequest();

	UPROPERTY(BlueprintAssignable, Category = "Scoring")
	FTennisPointScoredSignature OnPointScored;

	UPROPERTY(BlueprintAssignable, Category = "Scoring")
	FTennisGameWonSignature OnGameWon;

	UPROPERTY(BlueprintAssignable, Category = "Scoring")
	FTennisSetWonSignature OnSetWon;

	UPROPERTY(BlueprintAssignable, Category = "Scoring")
	FTennisMatchWonSignature OnMatchWon;

	UPROPERTY(BlueprintAssignable, Category = "Scoring")
	FTennisEndsChangedSignature OnEndsChanged;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Scoring")
	FTennisScore Score;

	UPROPERTY()
	int32 BestOfSets = 3;

	UPROPERTY()
	int32 MatchWinner = -1;

	UPROPERTY()
	bool bMatchOver = false;

	UPROPERTY()
	bool bMatchStarted = false;

	UPROPERTY()
	int32 TotalGames = 0;

	UPROPERTY()
	int32 TiebreakFirstServer = 0;

	UPROPERTY()
	bool bEndsChangePending = false;

	int32 WinGame(int32 Winner, bool bWasTiebreak);
	int32 WinSet(int32 Winner);
	void StartTiebreak();
	void RequestEndsChange();
};
