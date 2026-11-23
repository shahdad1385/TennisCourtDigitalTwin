// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisHUD.h"

#include "TennisGameMode.h"
#include "TennisPlayerPawn.h"
#include "TennisScoring.h"
#include "TennisTypes.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

namespace
{
	const FLinearColor PanelBackground(0.012f, 0.016f, 0.024f, 0.86f);
	const FLinearColor PanelBorder(1.0f, 1.0f, 1.0f, 0.18f);
	const FLinearColor AccentColor(0.82f, 0.90f, 0.18f, 1.0f);
	const FLinearColor TextWhite(0.96f, 0.96f, 0.94f, 1.0f);
	const FLinearColor TextDim(0.62f, 0.63f, 0.60f, 1.0f);
	const FLinearColor AmberColor(0.96f, 0.73f, 0.18f, 1.0f);
	const FLinearColor DangerColor(0.92f, 0.22f, 0.16f, 1.0f);
	const FLinearColor SuccessColor(0.34f, 0.86f, 0.38f, 1.0f);
}

ATennisHUD::ATennisHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATennisHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || Canvas->ClipY <= 1.0f || Canvas->ClipX <= 1.0f)
	{
		return;
	}

	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode || !GameMode->GetScoring())
	{
		return;
	}

	const float UiScale = FMath::Max(0.5f, Canvas->ClipY / 1080.0f);

	DrawScoreboard(UiScale);
	DrawStatusLine(UiScale);
	DrawSpeedReadout(UiScale);
	DrawPowerMeter(UiScale);
	DrawSpinIndicator(UiScale);
	DrawServePrompt(UiScale);
	DrawControlsHint(UiScale);
	DrawCrosshair(UiScale);
	DrawCenterMessage(UiScale);
}

void ATennisHUD::DrawScoreboard(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	UTennisScoringComponent* Scoring = GameMode ? GameMode->GetScoring() : nullptr;
	if (!Scoring)
	{
		return;
	}

	UFont* LabelFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	UFont* NameFont = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* NumberFont = GEngine ? GEngine->GetLargeFont() : nullptr;

	const float PanelX = 36.0f * UiScale;
	const float PanelY = 30.0f * UiScale;
	const float PanelW = 690.0f * UiScale;
	const float PanelH = 168.0f * UiScale;

	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBackground);
	DrawRect(AccentColor, PanelX, PanelY, 6.0f * UiScale, PanelH);

	const float RightSets = PanelX + 520.0f * UiScale;
	const float RightGames = PanelX + 594.0f * UiScale;
	const float RightPoints = PanelX + 664.0f * UiScale;
	const float HeaderY = PanelY + 20.0f * UiScale;

	DrawTextShadow(TEXT("SETS"), RightSets, HeaderY, LabelFont, 1.05f * UiScale, TextDim, true);
	DrawTextShadow(TEXT("GAMES"), RightGames, HeaderY, LabelFont, 1.05f * UiScale, TextDim, true);
	DrawTextShadow(TEXT("PTS"), RightPoints, HeaderY, LabelFont, 1.05f * UiScale, TextDim, true);

	const FTennisScore Score = Scoring->GetScore();
	const int32 Server = Scoring->GetServer();

	for (int32 Player = 0; Player < 2; ++Player)
	{
		const float RowY = PanelY + 54.0f * UiScale + Player * 54.0f * UiScale;

		if (Player == Server)
		{
			DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.055f),
				PanelX + 10.0f * UiScale, RowY - 7.0f * UiScale,
				PanelW - 20.0f * UiScale, 46.0f * UiScale);
			DrawRect(AccentColor,
				PanelX + 18.0f * UiScale, RowY + 8.0f * UiScale,
				9.0f * UiScale, 9.0f * UiScale);
		}

		const FLinearColor NameColor = (Player == 0) ? TextWhite : FLinearColor(0.93f, 0.82f, 0.80f, 1.0f);
		DrawTextShadow(GameMode->GetPlayerDisplayName(Player),
			PanelX + 40.0f * UiScale, RowY, NameFont, 1.75f * UiScale, NameColor);

		DrawTextShadow(FString::FromInt(Score.SetsWon[Player]),
			RightSets, RowY - 2.0f * UiScale, NumberFont, 1.65f * UiScale, TextWhite, true);
		DrawTextShadow(FString::FromInt(Score.Games[Player]),
			RightGames, RowY - 2.0f * UiScale, NumberFont, 1.65f * UiScale, TextWhite, true);
		DrawTextShadow(Scoring->GetPointDisplay(Player),
			RightPoints, RowY - 2.0f * UiScale, NumberFont, 1.90f * UiScale, AccentColor, true);
	}

	DrawLine(PanelX + 16.0f * UiScale, PanelY + 154.0f * UiScale,
		PanelX + PanelW - 16.0f * UiScale, PanelY + 154.0f * UiScale, PanelBorder, 1.0f);
}

void ATennisHUD::DrawStatusLine(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float PanelX = 36.0f * UiScale;
	const float Y = 30.0f * UiScale + 168.0f * UiScale + 10.0f * UiScale;

	const FString Status = GameMode->GetStatusLine();
	if (!Status.IsEmpty())
	{
		DrawTextShadow(Status, PanelX, Y, Font, 1.25f * UiScale, AmberColor);
	}

	if (GameMode->IsMatchComplete())
	{
		DrawTextShadow(TEXT("PRESS R TO RESTART"), PanelX, Y + 30.0f * UiScale,
			Font, 1.15f * UiScale, TextDim);
	}
}

void ATennisHUD::DrawPowerMeter(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	ATennisPlayerPawn* Human = GameMode ? GameMode->GetHumanPawn() : nullptr;
	if (!Human)
	{
		return;
	}

	const ERallyState State = GameMode->GetRallyState();
	if (State == ERallyState::MatchOver)
	{
		return;
	}

	const float Charge = FMath::Clamp(Human->GetChargeAmount(), 0.0f, 1.0f);
	const float BarW = 470.0f * UiScale;
	const float BarH = 20.0f * UiScale;
	const float BarX = (Canvas->ClipX - BarW) * 0.5f;
	const float BarY = Canvas->ClipY - 92.0f * UiScale;

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	DrawPanel(BarX - 3.0f * UiScale, BarY - 3.0f * UiScale,
		BarW + 6.0f * UiScale, BarH + 6.0f * UiScale,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));

	FLinearColor FillColor = FLinearColor::LerpUsingHSV(
		SuccessColor, FLinearColor(0.94f, 0.86f, 0.12f, 1.0f),
		FMath::Clamp(Charge * 2.0f, 0.0f, 1.0f));
	FillColor = FLinearColor::LerpUsingHSV(
		FillColor, DangerColor,
		FMath::Clamp((Charge - 0.5f) * 2.0f, 0.0f, 1.0f));

	if (Charge > 0.001f)
	{
		DrawRect(FillColor, BarX + 2.0f * UiScale, BarY + 2.0f * UiScale,
			(BarW - 4.0f * UiScale) * Charge, BarH - 4.0f * UiScale);
	}

	DrawRect(PanelBorder, BarX, BarY, BarW, BarH);

	DrawTextShadow(TEXT("POWER"), BarX, BarY - 22.0f * UiScale, Font, 1.10f * UiScale, TextDim);
	const FString Percent = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Charge * 100.0f));
	DrawTextShadow(Percent, BarX + BarW, BarY - 22.0f * UiScale, Font, 1.10f * UiScale, FillColor, true);
}

void ATennisHUD::DrawSpinIndicator(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	ATennisPlayerPawn* Human = GameMode ? GameMode->GetHumanPawn() : nullptr;
	if (!Human)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const FString SpinText = FString::Printf(TEXT("SPIN   %s"),
		*SpinToText(static_cast<uint8>(Human->GetSelectedSpin())));

	float TextW = 0.0f;
	float TextH = 0.0f;
	const float TextScale = 1.15f * UiScale;
	GetTextSize(SpinText, TextW, TextH, Font, TextScale);

	const float X = (Canvas->ClipX - TextW) * 0.5f;
	const float Y = Canvas->ClipY - 122.0f * UiScale;
	DrawTextShadow(SpinText, X, Y, Font, TextScale, AccentColor);
}

void ATennisHUD::DrawSpeedReadout(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	const float PanelW = 250.0f * UiScale;
	const float PanelH = 90.0f * UiScale;
	const float PanelX = Canvas->ClipX - PanelW - 36.0f * UiScale;
	const float PanelY = 30.0f * UiScale;

	DrawPanel(PanelX, PanelY, PanelW, PanelH, PanelBackground);
	DrawRect(AccentColor, PanelX + PanelW - 6.0f * UiScale, PanelY, 6.0f * UiScale, PanelH);

	UFont* LabelFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	UFont* ValueFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!LabelFont || !ValueFont)
	{
		return;
	}

	DrawTextShadow(TEXT("LAST SHOT"), PanelX + 18.0f * UiScale, PanelY + 12.0f * UiScale,
		LabelFont, 1.05f * UiScale, TextDim);

	const float SpeedKph = GameMode->GetLastShotSpeedKph();
	const FString Value = (SpeedKph > 1.0f)
		? FString::FromInt(FMath::RoundToInt(SpeedKph))
		: FString(TEXT("--"));

	const float ValueScale = 2.0f * UiScale;
	DrawTextShadow(Value, PanelX + 18.0f * UiScale, PanelY + 36.0f * UiScale,
		ValueFont, ValueScale, TextWhite);

	float ValueW = 0.0f;
	float ValueH = 0.0f;
	GetTextSize(Value, ValueW, ValueH, ValueFont, ValueScale);
	DrawTextShadow(TEXT("KM/H"), PanelX + 26.0f * UiScale + ValueW, PanelY + 48.0f * UiScale,
		LabelFont, 1.05f * UiScale, TextDim);
}

void ATennisHUD::DrawServePrompt(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode || !GameMode->IsHumanServing() || GameMode->GetRallyState() != ERallyState::PreServe)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font || !SmallFont)
	{
		return;
	}

	const float Time = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.0f;
	const float Blink = 0.55f + 0.45f * FMath::Sin(Time * 6.0f);

	const FString Prompt = (GameMode->GetServeNumber() >= 2)
		? TEXT("SECOND SERVE  -  HOLD LEFT MOUSE")
		: TEXT("HOLD LEFT MOUSE TO SERVE");

	const float TextScale = 1.55f * UiScale;
	float TextW = 0.0f;
	float TextH = 0.0f;
	GetTextSize(Prompt, TextW, TextH, Font, TextScale);

	const float X = (Canvas->ClipX - TextW) * 0.5f;
	const float Y = Canvas->ClipY * 0.66f;
	DrawTextShadow(Prompt, X, Y, Font, TextScale, FLinearColor(1.0f, 1.0f, 1.0f, Blink));

	const FString Hint = TEXT("SPACE FOR A QUICK SERVE");
	float HintW = 0.0f;
	float HintH = 0.0f;
	GetTextSize(Hint, HintW, HintH, SmallFont, 1.0f * UiScale);
	DrawTextShadow(Hint, (Canvas->ClipX - HintW) * 0.5f, Y + 34.0f * UiScale,
		SmallFont, 1.0f * UiScale, FLinearColor(0.8f, 0.8f, 0.78f, Blink * 0.8f));
}

void ATennisHUD::DrawCenterMessage(float UiScale)
{
	ATennisGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	const FString Message = GameMode->GetCenterMessage();
	const float Alpha = GameMode->GetCenterMessageAlpha();
	if (Message.IsEmpty() || Alpha <= 0.001f)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font)
	{
		return;
	}

	FLinearColor Color(1.0f, 1.0f, 1.0f, Alpha);
	if (Message.Contains(TEXT("OUT")) || Message.Contains(TEXT("FAULT")) || Message.Contains(TEXT("NET")))
	{
		Color = FLinearColor(DangerColor.R, DangerColor.G, DangerColor.B, Alpha);
	}
	else if (Message.Contains(TEXT("ACE")) || Message.Contains(TEXT("WINNER")) || Message.Contains(TEXT("GAME")))
	{
		Color = FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, Alpha);
	}
	else if (Message.Contains(TEXT("DEUCE")) || Message.Contains(TEXT("ADVANTAGE")) || Message.Contains(TEXT("LET")))
	{
		Color = FLinearColor(AmberColor.R, AmberColor.G, AmberColor.B, Alpha);
	}

	const float TextScale = 2.35f * UiScale;
	float TextW = 0.0f;
	float TextH = 0.0f;
	GetTextSize(Message, TextW, TextH, Font, TextScale);

	const float X = (Canvas->ClipX - TextW) * 0.5f;
	const float Y = Canvas->ClipY * 0.25f;
	DrawTextShadow(Message, X, Y, Font, TextScale, Color);

	if (GameMode->IsMatchComplete())
	{
		const FString Restart = TEXT("PRESS R FOR A NEW MATCH");
		const float RestartScale = 1.15f * UiScale;
		float RestartW = 0.0f;
		float RestartH = 0.0f;
		GetTextSize(Restart, RestartW, RestartH, Font, RestartScale);
		DrawTextShadow(Restart, (Canvas->ClipX - RestartW) * 0.5f, Y + 60.0f * UiScale,
			Font, RestartScale, FLinearColor(0.85f, 0.85f, 0.82f, Alpha));
	}
}

void ATennisHUD::DrawControlsHint(float UiScale)
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	const float X = 36.0f * UiScale;
	const float StartY = Canvas->ClipY - 108.0f * UiScale;
	const float LineHeight = 20.0f * UiScale;
	const float TextScale = 0.95f * UiScale;

	DrawTextShadow(TEXT("W/S  CLOSE / RETREAT          A/D  MOVE"), X, StartY,
		Font, TextScale, FLinearColor(0.72f, 0.73f, 0.70f, 0.9f));
	DrawTextShadow(TEXT("MOUSE  AIM          Q/E  SPIN"), X, StartY + LineHeight,
		Font, TextScale, FLinearColor(0.72f, 0.73f, 0.70f, 0.9f));
	DrawTextShadow(TEXT("LMB  CHARGE & HIT          R  RESET POINT"), X, StartY + LineHeight * 2.0f,
		Font, TextScale, FLinearColor(0.72f, 0.73f, 0.70f, 0.9f));
}

void ATennisHUD::DrawCrosshair(float UiScale)
{
	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const float Gap = 6.0f * UiScale;
	const float Arm = 9.0f * UiScale;
	const float Thickness = 1.5f * UiScale;
	const FLinearColor Color(1.0f, 1.0f, 1.0f, 0.55f);

	DrawLine(CenterX - Gap - Arm, CenterY, CenterX - Gap, CenterY, Color, Thickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + Arm, CenterY, Color, Thickness);
	DrawLine(CenterX, CenterY - Gap - Arm, CenterX, CenterY - Gap, Color, Thickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + Arm, Color, Thickness);
	DrawRect(Color, CenterX - 1.5f * UiScale, CenterY - 1.5f * UiScale,
		3.0f * UiScale, 3.0f * UiScale);
}

void ATennisHUD::DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, Width, Height);
	DrawLine(X, Y, X + Width, Y, PanelBorder, 1.0f);
	DrawLine(X + Width, Y, X + Width, Y + Height, PanelBorder, 1.0f);
	DrawLine(X + Width, Y + Height, X, Y + Height, PanelBorder, 1.0f);
	DrawLine(X, Y + Height, X, Y, PanelBorder, 1.0f);
}

void ATennisHUD::DrawTextShadow(const FString& Text, float X, float Y, UFont* Font, float Scale,
	const FLinearColor& Color, bool bRightAlign)
{
	if (Text.IsEmpty() || !Font || !Canvas)
	{
		return;
	}

	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, Font, Scale);

	const float DrawX = bRightAlign ? (X - TextWidth) : X;
	const float ShadowOffset = FMath::Max(1.0f, Scale * 1.2f);

	DrawText(Text, FLinearColor(0.0f, 0.0f, 0.0f, Color.A * 0.75f),
		DrawX + ShadowOffset, Y + ShadowOffset, Font, Scale, false);
	DrawText(Text, Color, DrawX, Y, Font, Scale, false);
}

FString ATennisHUD::SpinToText(uint8 SpinValue) const
{
	switch (static_cast<ESpinType>(SpinValue))
	{
	case ESpinType::Flat:
		return TEXT("FLAT");
	case ESpinType::Slice:
		return TEXT("SLICE");
	case ESpinType::Kick:
		return TEXT("KICK");
	default:
		return TEXT("TOPSPIN");
	}
}
