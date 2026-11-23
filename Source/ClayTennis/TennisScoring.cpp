// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisScoring.h"

UTennisScoringComponent::UTennisScoringComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTennisScoringComponent::StartMatch(int32 InBestOfSets)
{
	BestOfSets = (InBestOfSets >= 5) ? 5 : 3;
	ResetMatch();
	bMatchStarted = true;
	Score.Server = 0;
}

void UTennisScoringComponent::ResetMatch()
{
	Score.Reset();
	MatchWinner = -1;
	bMatchOver = false;
	bMatchStarted = false;
	TotalGames = 0;
	TiebreakFirstServer = 0;
	bEndsChangePending = false;
}

int32 UTennisScoringComponent::AwardPoint(int32 Winner)
{
	if (!bMatchStarted || bMatchOver)
	{
		return ETennisPointFlags::None;
	}

	Winner = FMath::Clamp(Winner, 0, 1);
	const int32 Loser = 1 - Winner;
	int32 Flags = ETennisPointFlags::None;

	if (Score.bTiebreak)
	{
		Score.TiebreakPoints[Winner]++;
		const int32 WinnerPoints = Score.TiebreakPoints[Winner];
		const int32 LoserPoints = Score.TiebreakPoints[Loser];

		if (WinnerPoints >= 7 && (WinnerPoints - LoserPoints) >= 2)
		{
			Flags |= WinGame(Winner, true);
		}
		else
		{
			const int32 Total = WinnerPoints + LoserPoints;

			// ITF tiebreak serving: one point, then two points each.
			if (Total % 2 == 1)
			{
				Score.Server = 1 - Score.Server;
			}

			// Change ends every six points in a tiebreak.
			if (Total > 0 && Total % 6 == 0)
			{
				RequestEndsChange();
			}
		}
	}
	else
	{
		Score.Points[Winner]++;
		const int32 WinnerPoints = Score.Points[Winner];
		const int32 LoserPoints = Score.Points[Loser];

		if (WinnerPoints >= 4 && (WinnerPoints - LoserPoints) >= 2)
		{
			Flags |= WinGame(Winner, false);
		}
	}

	OnPointScored.Broadcast(Winner, Flags);
	return Flags;
}

int32 UTennisScoringComponent::WinGame(int32 Winner, bool bWasTiebreak)
{
	int32 Flags = ETennisPointFlags::Game;
	const int32 Loser = 1 - Winner;

	Score.Games[Winner]++;
	Score.Points[0] = 0;
	Score.Points[1] = 0;
	Score.TiebreakPoints[0] = 0;
	Score.TiebreakPoints[1] = 0;
	Score.bTiebreak = false;

	++TotalGames;

	if (bWasTiebreak)
	{
		// The tiebreak receiver serves first in the next set.
		Score.Server = 1 - TiebreakFirstServer;
	}
	else
	{
		Score.Server = 1 - Score.Server;
	}

	// Change ends after odd-numbered games.
	if (TotalGames % 2 == 1)
	{
		RequestEndsChange();
	}

	OnGameWon.Broadcast(Winner);

	const int32 WinnerGames = Score.Games[Winner];
	const int32 LoserGames = Score.Games[Loser];

	if (bWasTiebreak || (WinnerGames >= 6 && (WinnerGames - LoserGames) >= 2))
	{
		Flags |= WinSet(Winner);
	}
	else if (Score.Games[0] == 6 && Score.Games[1] == 6)
	{
		StartTiebreak();
	}

	return Flags;
}

int32 UTennisScoringComponent::WinSet(int32 Winner)
{
	int32 Flags = ETennisPointFlags::Set;

	Score.SetsWon[Winner]++;
	Score.Games[0] = 0;
	Score.Games[1] = 0;

	// End changes follow the cumulative odd-game rule. This reproduces the ITF
	// nuance: a set ending on an even number of games (e.g. 6-4) changes ends
	// after the first game of the next set, while 7-6 changes immediately.

	if (Score.SetsWon[Winner] > BestOfSets / 2)
	{
		bMatchOver = true;
		MatchWinner = Winner;
		Flags |= ETennisPointFlags::Match;
		OnMatchWon.Broadcast(Winner);
	}

	OnSetWon.Broadcast(Winner);
	return Flags;
}

void UTennisScoringComponent::StartTiebreak()
{
	Score.bTiebreak = true;
	Score.TiebreakPoints[0] = 0;
	Score.TiebreakPoints[1] = 0;
	TiebreakFirstServer = Score.Server;
}

void UTennisScoringComponent::RequestEndsChange()
{
	bEndsChangePending = true;
	OnEndsChanged.Broadcast();
}

bool UTennisScoringComponent::ConsumeEndsChangeRequest()
{
	const bool bWasPending = bEndsChangePending;
	bEndsChangePending = false;
	return bWasPending;
}

FString UTennisScoringComponent::GetPointDisplay(int32 Player) const
{
	Player = FMath::Clamp(Player, 0, 1);

	if (Score.bTiebreak)
	{
		return FString::FromInt(Score.TiebreakPoints[Player]);
	}

	const int32 Points = Score.Points[Player];
	const int32 OpponentPoints = Score.Points[1 - Player];

	if (Points >= 3 && OpponentPoints >= 3)
	{
		if (Points == OpponentPoints)
		{
			return TEXT("40");
		}
		return (Points > OpponentPoints) ? TEXT("AD") : TEXT("40");
	}

	static const TCHAR* Labels[] = { TEXT("0"), TEXT("15"), TEXT("30"), TEXT("40") };
	return FString(Labels[FMath::Clamp(Points, 0, 3)]);
}

FString UTennisScoringComponent::GetScoreLine() const
{
	return FString::Printf(TEXT("Sets %d-%d   Games %d-%d   Points %s-%s"),
		Score.SetsWon[0], Score.SetsWon[1],
		Score.Games[0], Score.Games[1],
		*GetPointDisplay(0), *GetPointDisplay(1));
}

bool UTennisScoringComponent::IsDeuce() const
{
	if (Score.bTiebreak)
	{
		return false;
	}
	return Score.Points[0] >= 3
		&& Score.Points[0] == Score.Points[1];
}

bool UTennisScoringComponent::IsAdvantage(int32 Player) const
{
	if (Score.bTiebreak)
	{
		return false;
	}
	Player = FMath::Clamp(Player, 0, 1);
	const int32 Opponent = 1 - Player;
	return Score.Points[Player] >= 4 && Score.Points[Player] > Score.Points[Opponent];
}

bool UTennisScoringComponent::IsDeuceCourtServe() const
{
	const int32 Total = Score.bTiebreak
		? (Score.TiebreakPoints[0] + Score.TiebreakPoints[1])
		: (Score.Points[0] + Score.Points[1]);

	return (Total % 2) == 0;
}
