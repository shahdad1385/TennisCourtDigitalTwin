// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TennisTypes.generated.h"

/** Groundstroke and serve archetypes. Serve variants are kept separate so the
 *  HUD and the AI can reason about first/second serves without extra data. */
UENUM(BlueprintType)
enum class EShotType : uint8
{
	Flat		UMETA(DisplayName = "Flat"),
	Topspin		UMETA(DisplayName = "Topspin"),
	Slice		UMETA(DisplayName = "Slice"),
	ServeFlat	UMETA(DisplayName = "Flat Serve"),
	ServeKick	UMETA(DisplayName = "Kick Serve"),
	ServeSlice	UMETA(DisplayName = "Slice Serve")
};

/** Player-facing spin selection (Q/E on the keyboard). */
UENUM(BlueprintType)
enum class ESpinType : uint8
{
	Flat		UMETA(DisplayName = "Flat"),
	Topspin		UMETA(DisplayName = "Topspin"),
	Slice		UMETA(DisplayName = "Slice"),
	Kick		UMETA(DisplayName = "Kick")
};

/** Physical end of the court. Near is the -X baseline, Far is the +X baseline.
 *  "None" is used while the ball crosses the net plane. */
UENUM(BlueprintType)
enum class ETennisSide : uint8
{
	None	UMETA(DisplayName = "None"),
	Near	UMETA(DisplayName = "Near"),
	Far		UMETA(DisplayName = "Far")
};

/** Authoritative rally state machine, owned by ATennisGameMode. */
UENUM(BlueprintType)
enum class ERallyState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	PreServe	UMETA(DisplayName = "Pre Serve"),
	Serving		UMETA(DisplayName = "Serving"),
	Rally		UMETA(DisplayName = "Rally"),
	PointOver	UMETA(DisplayName = "Point Over"),
	MatchOver	UMETA(DisplayName = "Match Over")
};

/**
 * Complete ITF score state.
 *
 * Points[] stores raw point counts (0,1,2,3 = 0/15/30/40, 4 = advantage),
 * which makes deuce/advantage fall out naturally from the difference rule.
 * Games[] counts games in the current set (0..7) and SetsWon[] counts
 * completed sets. During a tiebreak bTiebreak is true and TiebreakPoints[]
 * holds the raw 0,1,2,... counts.
 *
 * Static arrays are used (instead of TArray) because the struct is fixed size
 * and copied every frame by the HUD.
 */
USTRUCT(BlueprintType)
struct FTennisScore
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Points[2];

	UPROPERTY()
	int32 Games[2];

	UPROPERTY()
	int32 SetsWon[2];

	UPROPERTY()
	int32 TiebreakPoints[2];

	UPROPERTY()
	bool bTiebreak = false;

	/** Player index (0/1) who serves the next point. */
	UPROPERTY()
	int32 Server = 0;

	FTennisScore()
	{
		Reset();
	}

	void Reset()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			Points[Index] = 0;
			Games[Index] = 0;
			SetsWon[Index] = 0;
			TiebreakPoints[Index] = 0;
		}
		bTiebreak = false;
		Server = 0;
	}

	int32 GetPoints(int32 Player) const
	{
		return (Player == 0) ? Points[0] : Points[1];
	}

	int32 GetGames(int32 Player) const
	{
		return (Player == 0) ? Games[0] : Games[1];
	}

	int32 GetSets(int32 Player) const
	{
		return (Player == 0) ? SetsWon[0] : SetsWon[1];
	}

	int32 GetTiebreakPoints(int32 Player) const
	{
		return (Player == 0) ? TiebreakPoints[0] : TiebreakPoints[1];
	}
};

/**
 * One shot request. The launch pitch is intentionally absent: ATennisBall
 * solves the pitch numerically against the real drag/Magnus trajectory so a
 * shot aimed at a target actually lands there.
 */
USTRUCT(BlueprintType)
struct FShotParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	EShotType ShotType = EShotType::Topspin;

	/** Initial speed in m/s at contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float SpeedMps = 25.0f;

	/** Topspin (+ forward roll) or slice (- backspin) in revolutions/minute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float TopspinRpm = 1800.0f;

	/** Side spin applied around world up. Curve direction depends on the sign. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float SideSpinRpm = 0.0f;

	/** 0..1 charge used for speed, error and HUD feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	float Charge01 = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot")
	bool bIsServe = false;

	FORCEINLINE bool IsServe() const
	{
		return bIsServe || ShotType == EShotType::ServeFlat
			|| ShotType == EShotType::ServeKick
			|| ShotType == EShotType::ServeSlice;
	}
};

/** Small helpers shared by gameplay classes. */
namespace TennisUtil
{
	FORCEINLINE ETennisSide OppositeSide(ETennisSide InSide)
	{
		if (InSide == ETennisSide::Near) { return ETennisSide::Far; }
		if (InSide == ETennisSide::Far) { return ETennisSide::Near; }
		return ETennisSide::None;
	}

	/** -1 for the Near baseline (-X), +1 for the Far baseline (+X). */
	FORCEINLINE float SideSign(ETennisSide InSide)
	{
		return (InSide == ETennisSide::Near) ? -1.0f : 1.0f;
	}

	FORCEINLINE ETennisSide SideFromX(double X)
	{
		if (X < 0.0) { return ETennisSide::Near; }
		if (X > 0.0) { return ETennisSide::Far; }
		return ETennisSide::None;
	}

	FORCEINLINE bool IsServeShot(EShotType Type)
	{
		return Type == EShotType::ServeFlat
			|| Type == EShotType::ServeKick
			|| Type == EShotType::ServeSlice;
	}
}
