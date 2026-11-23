// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisAI.h"

#include "TennisBall.h"
#include "TennisCourt.h"
#include "TennisGameMode.h"
#include "TennisPlayerPawn.h"
#include "TennisScoring.h"

#include "Engine/World.h"

UTennisAIComponent::UTennisAIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	Rng.Initialize(0x7E331);
}

void UTennisAIComponent::InitializeAI(ATennisPlayerPawn* InPawn, ATennisBall* InBall, ATennisCourt* InCourt, float InDifficulty)
{
	AIPawn = InPawn;
	Ball = InBall;
	Court = InCourt;
	SetDifficulty(InDifficulty);
}

void UTennisAIComponent::SetDifficulty(float InDifficulty)
{
	Difficulty = FMath::Clamp(InDifficulty, 0.0f, 1.0f);
}

float UTennisAIComponent::GetFootSpeedScale() const
{
	return 0.76f + 0.30f * Difficulty;
}

void UTennisAIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!AIPawn || !Ball || DeltaTime <= 0.0f)
	{
		return;
	}

	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	SwingCooldown = FMath::Max(0.0f, SwingCooldown - DeltaTime);

	const ERallyState State = GameMode->GetRallyState();
	if (State == ERallyState::Idle || State == ERallyState::PreServe
		|| State == ERallyState::PointOver || State == ERallyState::MatchOver)
	{
		AIPawn->SetAITargetWorld(AIPawn->GetReadyWorldLocation(), 0.65f);
		bSwingCommitted = false;
		ReactionTimer = 0.0f;
		return;
	}

	const float MySign = TennisUtil::SideSign(AIPawn->GetPhysicalSide());
	const FVector BallPosition = Ball->GetActorLocation();
	const FVector PawnPosition = AIPawn->GetActorLocation();

	FVector Landing = FVector::ZeroVector;
	float LandingTime = 0.0f;
	const bool bHasLanding = Ball->PredictLanding(Landing, LandingTime);
	(void)LandingTime;

	FVector SecondBounce = FVector::ZeroVector;
	float SecondBounceTime = 0.0f;
	const bool bHasSecondBounce = Ball->PredictSecondBounce(SecondBounce, SecondBounceTime);
	(void)SecondBounceTime;

	const bool bBallPastNet = (BallPosition.X * MySign) > 0.0;
	const bool bJustHit = (Ball->GetLastHitter() == AIPawn.Get());
	const bool bLandingIn = bHasLanding && ATennisCourt::IsInSinglesCourt(Landing);

	FVector MoveTarget = AIPawn->GetReadyWorldLocation();
	if (bJustHit || !bHasLanding)
	{
		// Recover while the opponent prepares the return.
		MoveTarget.Y = FMath::Clamp(static_cast<double>(BallPosition.Y) * 0.25, -220.0, 220.0);
	}
	else if (bBallPastNet && Ball->GetBounceIndex() >= 1)
	{
		// After the bounce the ball is rising toward the AI: chase the ball.
		// This takes priority over the next-bounce prediction, which may be
		// heading out but can still be returned before it lands again.
		MoveTarget = FVector(BallPosition.X - MySign * 55.0f, BallPosition.Y, 90.0);
	}
	else if (!bLandingIn)
	{
		// The ball is going out; let it go and recover.
		MoveTarget.Y = FMath::Clamp(static_cast<double>(BallPosition.Y) * 0.25, -220.0, 220.0);
	}
	else
	{
		// Deep balls come back to the baseline by themselves; only move in for
		// a genuinely short ball whose second bounce dies inside the court.
		const double BaselineX = static_cast<double>(MySign) * static_cast<double>(ATennisCourt::GetCourtHalfLengthCm());
		const double ShortBallThreshold = 850.0;

		double DesiredX = BaselineX + static_cast<double>(MySign) * 110.0;
		if (bHasSecondBounce
			&& (SecondBounce.X * MySign) > 0.0
			&& FMath::Abs(SecondBounce.X) < ShortBallThreshold)
		{
			DesiredX = SecondBounce.X + static_cast<double>(MySign) * 70.0;
		}

		MoveTarget = FVector(DesiredX, Landing.Y, 90.0);
	}

	AIPawn->SetAITargetWorld(MoveTarget, GetFootSpeedScale());

	const double Delta2D = FVector(BallPosition.X - PawnPosition.X, BallPosition.Y - PawnPosition.Y, 0.0).Size();
	const bool bBallOnMySide = (BallPosition.X * MySign) > -40.0;
	const bool bHeightOk = BallPosition.Z > 12.0 && BallPosition.Z < 310.0;
	const bool bInReach = Delta2D <= static_cast<double>(AIPawn->GetReachCm());

	if (bBallOnMySide && bInReach && bHeightOk)
	{
		if (!bSwingCommitted)
		{
			bSwingCommitted = true;
			// A short human-like reaction; near zero at high difficulty.
			ReactionTimer = FMath::Max(0.02f, 0.10f - 0.07f * Difficulty);
		}

		ReactionTimer -= DeltaTime;
		if (ReactionTimer <= 0.0f && SwingCooldown <= 0.0f)
		{
			if (TrySwing())
			{
				SwingCooldown = 0.45f;
				ReactionTimer = 0.0f;
			}
			bSwingCommitted = false;
		}
	}
	else
	{
		bSwingCommitted = false;
		ReactionTimer = 0.0f;
	}
}

bool UTennisAIComponent::TrySwing()
{
	if (!AIPawn || !Ball)
	{
		return false;
	}

	const float MySign = TennisUtil::SideSign(AIPawn->GetPhysicalSide());
	EShotType ShotType = EShotType::Topspin;
	float Charge = 0.65f;
	const FVector Target = ChooseShotTarget(MySign, ShotType, Charge);

	return AIPawn->TryHitBall(Ball, Target, ShotType, Charge);
}

FVector UTennisAIComponent::ChooseShotTarget(float MySideSign, EShotType& OutShotType, float& OutCharge)
{
	(void)MySideSign;

	if (!AIPawn)
	{
		OutShotType = EShotType::Topspin;
		OutCharge = 0.6f;
		return FVector::ZeroVector;
	}

	const float MySign = TennisUtil::SideSign(AIPawn->GetPhysicalSide());
	const float OpponentSign = -MySign;
	const double PawnY = AIPawn->GetActorLocation().Y;
	const float MyYSide = (PawnY >= 0.0f) ? 1.0f : -1.0f;

	// Crosscourt is the percentage play; down-the-line is the change-up.
	const bool bCrosscourt = Rng.FRand() < 0.68f;
	const float TargetYSide = bCrosscourt ? -MyYSide : MyYSide;

	float TargetX = OpponentSign * Rng.FRandRange(780.0f, 1120.0f);
	float TargetY = TargetYSide * Rng.FRandRange(210.0f, 360.0f);

	const float Noise = 1.0f - Difficulty;
	TargetX += Rng.FRandRange(-1.0f, 1.0f) * Noise * 210.0f;
	TargetY += Rng.FRandRange(-1.0f, 1.0f) * Noise * 170.0f;

	const FVector BallPosition = Ball ? Ball->GetActorLocation() : FVector::ZeroVector;
	const bool bShortBall = FMath::Abs(BallPosition.X) < 520.0;
	const float Roll = Rng.FRand();

	if (BallPosition.Z < 70.0 && Roll < 0.35f)
	{
		OutShotType = EShotType::Slice;
	}
	else if (bShortBall && Roll < 0.60f)
	{
		OutShotType = EShotType::Flat;
	}
	else if (Roll < 0.18f)
	{
		OutShotType = EShotType::Flat;
	}
	else
	{
		OutShotType = EShotType::Topspin;
	}

	OutCharge = FMath::Clamp(0.55f + 0.35f * Difficulty + Rng.FRandRange(-0.10f, 0.10f), 0.35f, 1.0f);

	// Occasional unforced error: long or wide, scaled by difficulty.
	const float ErrorChance = 0.012f + 0.05f * (1.0f - Difficulty);
	if (Rng.FRand() < ErrorChance)
	{
		if (Rng.FRand() < 0.5f)
		{
			TargetX = OpponentSign * Rng.FRandRange(1260.0f, 1580.0f);
		}
		else
		{
			TargetY = TargetYSide * Rng.FRandRange(470.0f, 720.0f);
		}
	}

	return FVector(TargetX, TargetY, 0.0f);
}

bool UTennisAIComponent::ChooseServePlan(FVector& OutTargetCm, EShotType& OutServeType, bool bSecondServe)
{
	if (!AIPawn || !Court)
	{
		return false;
	}

	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	UTennisScoringComponent* Scoring = GameMode ? GameMode->GetScoring() : nullptr;
	if (!Scoring)
	{
		return false;
	}

	const ETennisSide ServerSide = AIPawn->GetPhysicalSide();
	const ETennisSide ReceiverSide = TennisUtil::OppositeSide(ServerSide);
	const float ReceiverSign = TennisUtil::SideSign(ReceiverSide);
	const bool bDeuce = Scoring->IsDeuceCourtServe();

	const FVector BoxCenter = ATennisCourt::GetServeTarget(ServerSide, bDeuce);
	const float BoxYSide = (BoxCenter.Y >= 0.0) ? 1.0f : -1.0f;

	// Deep targets: a flat serve from 2.75 m cannot land short of ~5.6 m.
	float Depth = Rng.FRandRange(bSecondServe ? 560.0f : 590.0f, bSecondServe ? 610.0f : 625.0f);
	float Width = BoxYSide * Rng.FRandRange(bSecondServe ? 170.0f : 250.0f, bSecondServe ? 320.0f : 395.0f);

	const float Noise = (1.0f - Difficulty) * 40.0f;
	Depth += Rng.FRandRange(-Noise, Noise);
	Width += Rng.FRandRange(-Noise, Noise);

	OutTargetCm = FVector(ReceiverSign * Depth, Width, 0.0f);
	OutServeType = bSecondServe
		? EShotType::ServeKick
		: ((Rng.FRand() < 0.62f) ? EShotType::ServeFlat : EShotType::ServeKick);

	// Rare serve fault. On a second serve this becomes a double fault, which is
	// exactly the pressure the AI is supposed to feel.
	const float FaultChance = 0.10f * (1.0f - Difficulty);
	if (Rng.FRand() < FaultChance)
	{
		if (Rng.FRand() < 0.5f)
		{
			OutTargetCm.X = ReceiverSign * Rng.FRandRange(700.0f, 830.0f);
		}
		else
		{
			OutTargetCm.Y = BoxYSide * Rng.FRandRange(440.0f, 560.0f);
		}
	}

	return true;
}
