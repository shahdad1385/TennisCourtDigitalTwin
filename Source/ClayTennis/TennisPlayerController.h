// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TennisPlayerController.generated.h"

class ATennisPlayerPawn;
class ATennisGameMode;

/**
 * Legacy-input controller (BindAxis/BindAction against Config/DefaultInput.ini).
 * The controller is intentionally thin: it translates input into pawn calls
 * and lets ATennisGameMode own the rally rules.
 */
UCLASS()
class CLAYTENNIS_API ATennisPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATennisPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	void InputMoveRight(float Value);
	void InputMoveForward(float Value);
	void InputAimX(float Value);
	void InputAimY(float Value);
	void InputCameraZoom(float Value);

	void OnChargePressed();
	void OnChargeReleased();
	void OnSwitchSpin();
	void OnSwitchSpinPrev();
	void OnServePressed();
	void OnResetRally();

	ATennisPlayerPawn* GetTennisPawn() const;
	ATennisGameMode* GetTennisGameMode() const;

private:
	float CachedRightAxis = 0.0f;
	float CachedForwardAxis = 0.0f;
};
