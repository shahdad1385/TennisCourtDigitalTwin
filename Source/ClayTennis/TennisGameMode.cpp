// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisGameMode.h"
#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

#include "TennisAI.h"
#include "TennisBall.h"
#include "TennisCourt.h"
#include "TennisHUD.h"
#include "TennisPlayerController.h"
#include "TennisPlayerPawn.h"
#include "TennisScoring.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	constexpr float StandHeightCm = 90.0f;
	constexpr float ServeContactHeightCm = 275.0f;
	constexpr float ServeContactInsideBaselineCm = 38.5f;
}

ATennisGameMode::ATennisGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = ATennisPlayerPawn::StaticClass();
	PlayerControllerClass = ATennisPlayerController::StaticClass();
	HUDClass = ATennisHUD::StaticClass();

	Scoring = CreateDefaultSubobject<UTennisScoringComponent>(TEXT("TennisScoring"));
}

void ATennisGameMode::BeginPlay()
{
	Super::BeginPlay();

	SpawnWorldActors();
	SetupEnvironment();

	if (Ball)
	{
		Ball->OnBallBounced.AddDynamic(this, &ATennisGameMode::HandleBallBounced);
		Ball->OnBallHit.AddDynamic(this, &ATennisGameMode::HandleBallHit);
	}

	// Give the player controller one frame to possess the pawn.
	GetWorldTimerManager().SetTimer(StartMatchTimer, this, &ATennisGameMode::StartNewMatch, 0.80f, false);

	if (FParse::Param(FCommandLine::Get(), TEXT("autoshot")))
	{
		BeginAutoShot();
	}
}

void ATennisGameMode::SpawnWorldActors()
{
	if (!GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Reuse an authored court if the level has one; otherwise build it in code.
	for (TActorIterator<ATennisCourt> It(GetWorld()); It; ++It)
	{
		Court = *It;
		break;
	}
	if (!Court)
	{
		Court = GetWorld()->SpawnActor<ATennisCourt>(ATennisCourt::StaticClass(), FTransform::Identity, SpawnParams);
	}

	for (TActorIterator<ATennisBall> It(GetWorld()); It; ++It)
	{
		Ball = *It;
		break;
	}
	if (!Ball)
	{
		Ball = GetWorld()->SpawnActor<ATennisBall>(ATennisBall::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, ServeContactHeightCm)), SpawnParams);
	}

	if (Ball)
	{
		Ball->SetBallVisible(true);
	}

	// AI opponent: configured before BeginPlay so its camera is inactive and
	// its physical side is correct from the first frame.
	const FVector AIStart(ATennisCourt::GetCourtHalfLengthCm() + 120.0f, 0.0f, StandHeightCm);
	const FTransform AITransform(FRotator(0.0f, 180.0f, 0.0f), AIStart);

	AIPawn = GetWorld()->SpawnActorDeferred<ATennisPlayerPawn>(
		ATennisPlayerPawn::StaticClass(), AITransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (AIPawn)
	{
		AIPawn->ConfigurePlayer(1, ETennisSide::Far, true, AIDisplayName);
		AIPawn->SetBall(Ball);
		AIPawn->FinishSpawning(AITransform);

		AIBrain = NewObject<UTennisAIComponent>(AIPawn, TEXT("TennisAIBrain"));
		if (AIBrain)
		{
			AIBrain->InitializeAI(AIPawn, Ball, Court, AIDifficulty);
			AIBrain->RegisterComponent();
			AIPawn->AddInstanceComponent(AIBrain);
		}
	}
}

void ATennisGameMode::SetupEnvironment()
{
	if (!bSpawnSunLight || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		// A level-authored sun already exists; do not fight it.
		return;
	}

	const FTransform SunTransform(FRotator(-42.0f, 35.0f, 0.0f), FVector(0.0f, 0.0f, 1200.0f));
	ADirectionalLight* Sun = GetWorld()->SpawnActorDeferred<ADirectionalLight>(
		ADirectionalLight::StaticClass(), SunTransform);
	if (!Sun)
	{
		return;
	}

	if (ULightComponent* LightComponent = Sun->GetLightComponent())
	{
		LightComponent->SetMobility(EComponentMobility::Movable);
		LightComponent->SetIntensity(6.5f);
		LightComponent->SetLightColor(FLinearColor(1.0f, 0.96f, 0.88f));
		LightComponent->SetCastShadows(true);
	}

	Sun->FinishSpawning(SunTransform);
}

ATennisPlayerPawn* ATennisGameMode::SpawnHumanAtStart(AController* NewPlayer)
{
	if (HumanPawn || !GetWorld())
	{
		return HumanPawn;
	}

	const FVector Start(-(ATennisCourt::GetCourtHalfLengthCm() + 120.0f), 0.0f, StandHeightCm);
	const FTransform SpawnTransform(FRotator::ZeroRotator, Start);

	HumanPawn = GetWorld()->SpawnActorDeferred<ATennisPlayerPawn>(
		ATennisPlayerPawn::StaticClass(), SpawnTransform, NewPlayer, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (HumanPawn)
	{
		HumanPawn->ConfigurePlayer(0, ETennisSide::Near, false, HumanDisplayName);
		HumanPawn->SetBall(Ball);
		HumanPawn->FinishSpawning(SpawnTransform);
	}

	return HumanPawn;
}

APawn* ATennisGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	(void)SpawnTransform;
	return SpawnHumanAtStart(NewPlayer);
}

void ATennisGameMode::EnsureHumanPawn()
{
	if (HumanPawn || !GetWorld())
	{
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (ATennisPlayerPawn* Existing = Cast<ATennisPlayerPawn>(PlayerController->GetPawn()))
	{
		HumanPawn = Existing;
		HumanPawn->SetBall(Ball);
		return;
	}

	if (ATennisPlayerPawn* Spawned = SpawnHumanAtStart(PlayerController))
	{
		PlayerController->Possess(Spawned);
	}
}

void ATennisGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MessageTimer > 0.0f)
	{
		MessageTimer = FMath::Max(0.0f, MessageTimer - DeltaSeconds);
	}

	EnsureHumanPawn();
}

void ATennisGameMode::StartNewMatch()
{
	if (!Scoring)
	{
		return;
	}

	EnsureHumanPawn();
	if (!HumanPawn || !AIPawn || !Ball)
	{
		GetWorldTimerManager().SetTimer(StartMatchTimer, this, &ATennisGameMode::StartNewMatch, 0.25f, false);
		return;
	}

	Scoring->StartMatch(BestOfSets);

	HumanPawn->ConfigurePlayer(0, ETennisSide::Near, false, HumanDisplayName);
	HumanPawn->SetBall(Ball);
	AIPawn->ConfigurePlayer(1, ETennisSide::Far, true, AIDisplayName);
	AIPawn->SetBall(Ball);

	if (Court)
	{
		Court->ClearBounceMarks();
	}

	RallyState = ERallyState::Idle;
	StartNextPoint();
}

void ATennisGameMode::StartNextPoint()
{
	if (!Scoring || !Ball || !HumanPawn || !AIPawn)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AIServeTimer);
	GetWorldTimerManager().ClearTimer(SecondServeTimer);
	GetWorldTimerManager().ClearTimer(LetTimer);

	if (Scoring->IsMatchOver())
	{
		RallyState = ERallyState::MatchOver;
		return;
	}

	const bool bSwapped = ApplyEndsSwapIfNeeded();

	ServeNumber = 1;
	ServeFaults = 0;
	bCurrentPointWasServe = false;
	RallyState = ERallyState::PreServe;

	PositionPlayersForPoint();
	Ball->ResetBall(GetServeContactPoint());
	Ball->SetSimulationEnabled(false);
	Ball->SetBallVisible(true);

	if (!bSwapped)
	{
		if (Scoring->IsDeuce())
		{
			SetCenterMessage(TEXT("DEUCE"), 1.3f);
		}
		else if (Scoring->IsAdvantage(0))
		{
			SetCenterMessage(TEXT("ADVANTAGE - ") + HumanDisplayName, 1.3f);
		}
		else if (Scoring->IsAdvantage(1))
		{
			SetCenterMessage(TEXT("ADVANTAGE - ") + AIDisplayName, 1.3f);
		}
		else
		{
			SetCenterMessage(Scoring->GetServer() == 0 ? TEXT("YOUR SERVE") : TEXT("CPU TO SERVE"), 1.3f);
		}
	}

	if (Scoring->GetServer() == 1)
	{
		GetWorldTimerManager().SetTimer(AIServeTimer, this, &ATennisGameMode::ExecuteAIServe, 1.15f, false);
	}
}

void ATennisGameMode::StartSecondServe()
{
	if (!Scoring || !Ball || Scoring->IsMatchOver())
	{
		return;
	}

	RallyState = ERallyState::PreServe;
	PositionPlayersForPoint();
	Ball->ResetBall(GetServeContactPoint());
	Ball->SetSimulationEnabled(false);
	Ball->SetBallVisible(true);
	SetCenterMessage(TEXT("SECOND SERVE"), 1.2f);

	if (Scoring->GetServer() == 1)
	{
		GetWorldTimerManager().SetTimer(AIServeTimer, this, &ATennisGameMode::ExecuteAIServe, 1.05f, false);
	}
}

void ATennisGameMode::StartLetReplay()
{
	if (!Scoring || !Ball || Scoring->IsMatchOver())
	{
		return;
	}

	RallyState = ERallyState::PreServe;
	PositionPlayersForPoint();
	Ball->ResetBall(GetServeContactPoint());
	Ball->SetSimulationEnabled(false);
	Ball->SetBallVisible(true);
	SetCenterMessage(TEXT("LET - REPLAY"), 1.2f);

	if (Scoring->GetServer() == 1)
	{
		GetWorldTimerManager().SetTimer(AIServeTimer, this, &ATennisGameMode::ExecuteAIServe, 1.0f, false);
	}
}

void ATennisGameMode::PositionPlayersForPoint()
{
	if (!Scoring)
	{
		return;
	}

	ATennisPlayerPawn* ServerPawn = GetPawnForPlayer(Scoring->GetServer());
	ATennisPlayerPawn* ReceiverPawn = GetPawnForPlayer(1 - Scoring->GetServer());
	if (!ServerPawn || !ReceiverPawn)
	{
		return;
	}

	const float HalfLength = ATennisCourt::GetCourtHalfLengthCm();
	const float ServerSign = TennisUtil::SideSign(ServerPawn->GetPhysicalSide());
	const bool bDeuce = Scoring->IsDeuceCourtServe();
	const float StanceY = (bDeuce ? -ServerSign : ServerSign) * 100.0f;

	ServerPawn->SetReadyPose(FVector(ServerSign * (HalfLength + 70.0f), StanceY, StandHeightCm));
	ReceiverPawn->SetReadyPose(FVector(-ServerSign * (HalfLength + 160.0f), 0.0f, StandHeightCm));

	// Cancel any charge left over from the previous point.
	ServerPawn->ConsumeCharge();
	ReceiverPawn->ConsumeCharge();
}

FVector ATennisGameMode::GetServeContactPoint() const
{
	if (!Scoring)
	{
		return FVector(-1150.0f, 0.0f, ServeContactHeightCm);
	}

	ATennisPlayerPawn* ServerPawn = GetPawnForPlayer(Scoring->GetServer());
	if (!ServerPawn)
	{
		return FVector(-1150.0f, 0.0f, ServeContactHeightCm);
	}

	const float Sign = TennisUtil::SideSign(ServerPawn->GetPhysicalSide());
	const bool bDeuce = Scoring->IsDeuceCourtServe();
	const float StanceY = (bDeuce ? -Sign : Sign) * 100.0f;

	return FVector(
		Sign * (ATennisCourt::GetCourtHalfLengthCm() - ServeContactInsideBaselineCm),
		StanceY,
		ServeContactHeightCm);
}

FVector ATennisGameMode::GetServeTargetForServer() const
{
	if (!Scoring)
	{
		return FVector(320.0f, 0.0f, 0.0f);
	}

	ATennisPlayerPawn* ServerPawn = GetPawnForPlayer(Scoring->GetServer());
	const ETennisSide ServerSide = ServerPawn ? ServerPawn->GetPhysicalSide() : ETennisSide::Near;

	return ATennisCourt::GetServeTarget(ServerSide, Scoring->IsDeuceCourtServe());
}

bool ATennisGameMode::ApplyEndsSwapIfNeeded()
{
	if (!Scoring || !Scoring->ConsumeEndsChangeRequest() || !HumanPawn || !AIPawn)
	{
		return false;
	}

	HumanPawn->SetPhysicalSide(TennisUtil::OppositeSide(HumanPawn->GetPhysicalSide()));
	AIPawn->SetPhysicalSide(TennisUtil::OppositeSide(AIPawn->GetPhysicalSide()));
	SetCenterMessage(TEXT("CHANGE ENDS"), 1.5f);
	return true;
}

bool ATennisGameMode::IsHumanServing() const
{
	return Scoring != nullptr
		&& Scoring->GetServer() == 0
		&& RallyState == ERallyState::PreServe;
}

void ATennisGameMode::ExecuteHumanServe(ATennisPlayerPawn* Pawn, float Charge01, const FVector& AimTargetCm)
{
	if (!Scoring || !Pawn || RallyState != ERallyState::PreServe || Scoring->GetServer() != 0)
	{
		return;
	}

	EShotType ServeType = EShotType::ServeKick;
	switch (Pawn->GetSelectedSpin())
	{
	case ESpinType::Flat:
		ServeType = EShotType::ServeFlat;
		break;
	case ESpinType::Slice:
		ServeType = EShotType::ServeSlice;
		break;
	default:
		ServeType = EShotType::ServeKick;
		break;
	}

	ExecuteServe(0, Charge01, AimTargetCm, ServeType);
}

void ATennisGameMode::ExecuteAIServe()
{
	if (!Scoring || RallyState != ERallyState::PreServe || Scoring->GetServer() != 1)
	{
		return;
	}

	FVector Target = GetServeTargetForServer();
	EShotType ServeType = EShotType::ServeKick;
	if (AIBrain)
	{
		AIBrain->ChooseServePlan(Target, ServeType, ServeFaults > 0);
	}

	const float Charge = FMath::Clamp(0.80f + 0.16f * AIDifficulty, 0.4f, 1.0f);
	ExecuteServe(1, Charge, Target, ServeType);
}

void ATennisGameMode::ExecuteServe(int32 ServerIndex, float Charge01, const FVector& TargetCm, EShotType ServeType)
{
	if (!Ball || !Scoring || RallyState != ERallyState::PreServe)
	{
		return;
	}

	ATennisPlayerPawn* ServerPawn = GetPawnForPlayer(ServerIndex);
	ATennisPlayerPawn* ReceiverPawn = GetPawnForPlayer(1 - ServerIndex);
	if (!ServerPawn || !ReceiverPawn)
	{
		return;
	}

	const bool bDeuce = Scoring->IsDeuceCourtServe();
	const ETennisSide ReceiverSide = ReceiverPawn->GetPhysicalSide();

	FVector FinalTarget = FVector(TargetCm.X, TargetCm.Y, 0.0f);

	if (ServerIndex == 0)
	{
		// The human aims with the camera; project that point into the correct
		// service box, then apply the deep-box geometry limit.
		FinalTarget = ATennisCourt::ClampToServiceBox(TargetCm, ReceiverSide, bDeuce);

		// Serve geometry: from a 2.75 m contact a flat serve must land at least
		// ~5.6 m past the net to clear the 0.914 m net. Targets are therefore
		// clamped to the deep half of the service box (5.6-6.25 m).
		const float ReceiverSign = TennisUtil::SideSign(ReceiverSide);
		const float TargetYSide = (FinalTarget.Y >= 0.0f) ? 1.0f : -1.0f;
		float Depth = static_cast<float>(FMath::Clamp(FMath::Abs(static_cast<double>(FinalTarget.X)), 560.0, 625.0));
		float Width = static_cast<float>(FMath::Clamp(FMath::Abs(static_cast<double>(FinalTarget.Y)), 60.0, 390.0));

		// Placement error grows at high charge (over-hitting) and only on the
		// long/wide side so a mishit reads as a fault, not a net dribble.
		const float ErrorCm = FMath::Max(0.0f, FMath::Clamp(Charge01, 0.0f, 1.0f) - 0.85f) * 180.0f;
		Depth = FMath::Min(Depth + FMath::FRandRange(0.0f, ErrorCm), 720.0f);
		Width = FMath::Clamp(Width + FMath::FRandRange(-ErrorCm, ErrorCm), 30.0f, 500.0f);

		FinalTarget = FVector(ReceiverSign * Depth, TargetYSide * Width, 0.0f);
	}
	else
	{
		// The AI already reasons in world space and deliberately aims long/wide
		// on rare faults; clamping here would erase those errors.
		FinalTarget = FVector(TargetCm.X, TargetCm.Y, 0.0f);
	}

	const FVector Contact = GetServeContactPoint();
	Ball->ResetBall(Contact);

	const EShotType Type = TennisUtil::IsServeShot(ServeType) ? ServeType : EShotType::ServeFlat;
	const FShotParams Params = ATennisBall::MakeShotParams(Type, Charge01);
	Ball->LaunchAtTarget(FinalTarget, Params, ServerPawn);

	RallyState = ERallyState::Serving;
	bCurrentPointWasServe = true;
	LastShotSpeedKph = Ball->GetSpeedKph();
	ServerPawn->PlayHitReaction(true);
	SetCenterMessage(TEXT(""), 0.0f);
}

void ATennisGameMode::HandleBallHit(AActor* Hitter)
{
	if (!Ball)
	{
		return;
	}

	LastShotSpeedKph = Ball->GetSpeedKph();

	if (RallyState == ERallyState::PreServe)
	{
		// This is the serve launch itself; ExecuteServe sets the state next.
		return;
	}

	if (RallyState == ERallyState::Serving)
	{
		RallyState = ERallyState::Rally;
	}

	// Any touch after the serve ends the ace possibility.
	bCurrentPointWasServe = false;
	(void)Hitter;
}

void ATennisGameMode::HandleBallBounced(FVector Location, bool bInBounds, bool bNetContact, int32 BounceIndex)
{
	if (!Ball || !Court || !Scoring)
	{
		return;
	}

	if (RallyState == ERallyState::Serving)
	{
		HandleServeBounce(Location, bNetContact);
		return;
	}

	if (RallyState != ERallyState::Rally)
	{
		return;
	}

	ATennisPlayerPawn* Hitter = Cast<ATennisPlayerPawn>(Ball->GetLastHitter());
	if (!Hitter)
	{
		return;
	}

	const int32 HitterIndex = Hitter->GetPlayerIndex();
	const float HitterSign = TennisUtil::SideSign(Hitter->GetPhysicalSide());
	const float BounceSign = (Location.X >= 0.0f) ? 1.0f : -1.0f;

	// A second bounce ends the rally in the hitter's favour regardless of where
	// it lands: the receiver failed to return a ball that had already bounced
	// in. (If the first bounce had been out, the point would already be over.)
	if (BounceIndex >= 2)
	{
		Court->AddBounceMark(Location, bInBounds);
		const bool bAce = bCurrentPointWasServe && HitterIndex == Scoring->GetServer();
		AwardPoint(HitterIndex, bAce ? TEXT("ACE") : TEXT("WINNER"));
		return;
	}

	if (!bInBounds)
	{
		Court->AddBounceMark(Location, false);
		AwardPoint(1 - HitterIndex, TEXT("OUT"));
		return;
	}

	Court->AddBounceMark(Location, true);

	if (BounceSign == HitterSign)
	{
		// The ball came back to the hitter's own side (net or a shank).
		AwardPoint(1 - HitterIndex, TEXT("NET"));
	}
}

void ATennisGameMode::HandleServeBounce(const FVector& Location, bool bNetContact)
{
	if (!Scoring || !Court)
	{
		return;
	}

	const int32 ServerIndex = Scoring->GetServer();
	const int32 ReceiverIndex = 1 - ServerIndex;
	ATennisPlayerPawn* ReceiverPawn = GetPawnForPlayer(ReceiverIndex);
	if (!ReceiverPawn)
	{
		return;
	}

	const bool bDeuce = Scoring->IsDeuceCourtServe();
	const bool bInBox = Court->IsInServiceBox(Location, ReceiverPawn->GetPhysicalSide(), bDeuce);
	Court->AddBounceMark(Location, bInBox);

	// A serve that clips the net and still lands in the correct box is a let.
	if (bInBox && bNetContact)
	{
		RallyState = ERallyState::PointOver;
		Ball->StopBall();
		SetCenterMessage(TEXT("LET"), 1.2f);
		GetWorldTimerManager().SetTimer(LetTimer, this, &ATennisGameMode::StartLetReplay, 1.3f, false);
		return;
	}

	if (bInBox)
	{
		RallyState = ERallyState::Rally;
		LastShotSpeedKph = Ball->GetSpeedKph();
		return;
	}

	// Fault.
	++ServeFaults;
	if (ServeFaults >= 2)
	{
		AwardPoint(ReceiverIndex, TEXT("DOUBLE FAULT"));
		return;
	}

	ServeNumber = 2;
	RallyState = ERallyState::PointOver;
	Ball->StopBall();
	SetCenterMessage(TEXT("FAULT"), 1.3f);
	GetWorldTimerManager().SetTimer(SecondServeTimer, this, &ATennisGameMode::StartSecondServe, 1.5f, false);
}

void ATennisGameMode::AwardPoint(int32 Winner, const FString& Reason)
{
	if (!Scoring || RallyState == ERallyState::PointOver || RallyState == ERallyState::MatchOver)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AIServeTimer);
	GetWorldTimerManager().ClearTimer(SecondServeTimer);
	GetWorldTimerManager().ClearTimer(LetTimer);

	RallyState = ERallyState::PointOver;
	if (Ball)
	{
		Ball->StopBall();
	}
	SetCenterMessage(Reason, 1.6f);

	const int32 Flags = Scoring->AwardPoint(Winner);
	const FString WinnerName = GetPlayerDisplayName(Winner);

	if ((Flags & ETennisPointFlags::Match) != 0)
	{
		RallyState = ERallyState::MatchOver;
		SetCenterMessage(WinnerName + TEXT(" WINS THE MATCH"), 6.0f);
		return;
	}

	float Delay = 1.7f;

	if ((Flags & ETennisPointFlags::Set) != 0)
	{
		SetCenterMessage(TEXT("SET - ") + WinnerName, 2.4f);
		Delay = 2.6f;
	}
	else if ((Flags & ETennisPointFlags::Game) != 0)
	{
		SetCenterMessage(TEXT("GAME - ") + WinnerName, 2.0f);
		Delay = 2.2f;
	}

	GetWorldTimerManager().SetTimer(NextPointTimer, this, &ATennisGameMode::StartNextPoint, Delay, false);
}

void ATennisGameMode::SetCenterMessage(const FString& Message, float Duration)
{
	CenterMessage = Message;
	MessageDuration = FMath::Max(Duration, 0.01f);
	MessageTimer = Duration;
}

float ATennisGameMode::GetCenterMessageAlpha() const
{
	if (MessageTimer <= 0.0f || MessageDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(MessageTimer / 0.35f, 0.0f, 1.0f);
}

ATennisPlayerPawn* ATennisGameMode::GetPawnForPlayer(int32 PlayerIndex) const
{
	return (PlayerIndex == 0) ? HumanPawn : AIPawn;
}

FString ATennisGameMode::GetPlayerDisplayName(int32 PlayerIndex) const
{
	return (PlayerIndex == 0) ? HumanDisplayName : AIDisplayName;
}

FString ATennisGameMode::GetStatusLine() const
{
	if (!Scoring)
	{
		return FString();
	}

	if (Scoring->IsTiebreak())
	{
		const FTennisScore Score = Scoring->GetScore();
		return FString::Printf(TEXT("TIEBREAK  %d - %d"), Score.TiebreakPoints[0], Score.TiebreakPoints[1]);
	}
	if (Scoring->IsDeuce())
	{
		return TEXT("DEUCE");
	}
	if (Scoring->IsAdvantage(0))
	{
		return TEXT("ADVANTAGE  ") + HumanDisplayName;
	}
	if (Scoring->IsAdvantage(1))
	{
		return TEXT("ADVANTAGE  ") + AIDisplayName;
	}
	return FString();
}

bool ATennisGameMode::IsMatchComplete() const
{
	return Scoring && Scoring->IsMatchOver();
}

int32 ATennisGameMode::GetMatchWinner() const
{
	return Scoring ? Scoring->GetMatchWinner() : -1;
}

ETennisSide ATennisGameMode::GetHumanPhysicalSide() const
{
	return HumanPawn ? HumanPawn->GetPhysicalSide() : ETennisSide::Near;
}

void ATennisGameMode::ResetMatchOrPoint()
{
	GetWorldTimerManager().ClearTimer(StartMatchTimer);
	GetWorldTimerManager().ClearTimer(NextPointTimer);
	GetWorldTimerManager().ClearTimer(AIServeTimer);
	GetWorldTimerManager().ClearTimer(SecondServeTimer);
	GetWorldTimerManager().ClearTimer(LetTimer);

	if (!Scoring)
	{
		return;
	}

	if (Scoring->IsMatchOver() || RallyState == ERallyState::Idle)
	{
		StartNewMatch();
		return;
	}

	StartNextPoint();
}

void ATennisGameMode::BeginAutoShot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector Loc(-1600.0f, -700.0f, 300.0f);
	FRotator Rot(-6.0f, 21.0f, 0.0f);
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("LookDev.txt"))))
	{
		TArray<FString> Parts;
		Text.ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 6)
		{
			Loc = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			Rot = FRotator(FCString::Atof(*Parts[3]), FCString::Atof(*Parts[4]), FCString::Atof(*Parts[5]));
		}
	}

	ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Loc, Rot);
	if (Cam)
	{
		Cam->SetActorLabel(TEXT("AutoShotCam"));
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetViewTarget(Cam);
		}
	}

	FTimerHandle ShotTimer;
	FTimerHandle QuitTimer;
	World->GetTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateLambda([this]()
	{
		FScreenshotRequest::RequestScreenshot(TEXT("AutoShot"), false, true);
		UE_LOG(LogTemp, Warning, TEXT("[AUTOSHOT] screenshot requested"));
		{
			GEngine->Exec(GetWorld(), TEXT("HighResShot 1920x1080"));
		}
	}), 22.0f, false);

	World->GetTimerManager().SetTimer(QuitTimer, FTimerDelegate::CreateLambda([]()
	{
		FPlatformMisc::RequestExit(false);
	}), 32.0f, false);
}