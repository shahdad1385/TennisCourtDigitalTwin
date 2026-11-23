// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TennisHUD.generated.h"

class UFont;

/**
 * Broadcast-style Canvas HUD. No UMG, no textures, no fonts beyond the engine's
 * built-in fonts, so the whole interface works from code alone.
 */
UCLASS()
class CLAYTENNIS_API ATennisHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATennisHUD();

	virtual void DrawHUD() override;

protected:
	void DrawScoreboard(float UiScale);
	void DrawStatusLine(float UiScale);
	void DrawPowerMeter(float UiScale);
	void DrawSpinIndicator(float UiScale);
	void DrawSpeedReadout(float UiScale);
	void DrawServePrompt(float UiScale);
	void DrawCenterMessage(float UiScale);
	void DrawControlsHint(float UiScale);
	void DrawCrosshair(float UiScale);

	void DrawPanel(float X, float Y, float Width, float Height, const FLinearColor& Color);
	void DrawTextShadow(const FString& Text, float X, float Y, UFont* Font, float Scale,
		const FLinearColor& Color, bool bRightAlign = false);

	FString SpinToText(uint8 SpinValue) const;
};
