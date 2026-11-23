// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisPlayerController.h"

#include "TennisGameMode.h"
#include "TennisPlayerPawn.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"

ATennisPlayerController::ATennisPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ATennisPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void ATennisPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	InputComponent->BindAxis(TEXT("MoveRight"), this, &ATennisPlayerController::InputMoveRight);
	InputComponent->BindAxis(TEXT("MoveForward"), this, &ATennisPlayerController::InputMoveForward);
	InputComponent->BindAxis(TEXT("AimX"), this, &ATennisPlayerController::InputAimX);
	InputComponent->BindAxis(TEXT("AimY"), this, &ATennisPlayerController::InputAimY);
	InputComponent->BindAxis(TEXT("CameraZoom"), this, &ATennisPlayerController::InputCameraZoom);

	InputComponent->BindAction(TEXT("ChargeShot"), IE_Pressed, this, &ATennisPlayerController::OnChargePressed);
	InputComponent->BindAction(TEXT("ChargeShot"), IE_Released, this, &ATennisPlayerController::OnChargeReleased);
	InputComponent->BindAction(TEXT("SwitchSpin"), IE_Pressed, this, &ATennisPlayerController::OnSwitchSpin);
	InputComponent->BindAction(TEXT("SwitchSpinPrev"), IE_Pressed, this, &ATennisPlayerController::OnSwitchSpinPrev);
	InputComponent->BindAction(TEXT("Serve"), IE_Pressed, this, &ATennisPlayerController::OnServePressed);
	InputComponent->BindAction(TEXT("ResetRally"), IE_Pressed, this, &ATennisPlayerController::OnResetRally);
}

ATennisPlayerPawn* ATennisPlayerController::GetTennisPawn() const
{
	return Cast<ATennisPlayerPawn>(GetPawn());
}

ATennisGameMode* ATennisPlayerController::GetTennisGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATennisGameMode>() : nullptr;
}

void ATennisPlayerController::InputMoveRight(float Value)
{
	CachedRightAxis = Value;
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->SetMoveInput(CachedRightAxis, CachedForwardAxis);
	}
}

void ATennisPlayerController::InputMoveForward(float Value)
{
	CachedForwardAxis = Value;
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->SetMoveInput(CachedRightAxis, CachedForwardAxis);
	}
}

void ATennisPlayerController::InputAimX(float Value)
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->AddAimInput(Value * 0.42f, 0.0f);
	}
}

void ATennisPlayerController::InputAimY(float Value)
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->AddAimInput(0.0f, Value * 0.30f);
	}
}

void ATennisPlayerController::InputCameraZoom(float Value)
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->AddCameraZoom(Value);
	}
}

void ATennisPlayerController::OnChargePressed()
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->BeginCharge();
	}
}

void ATennisPlayerController::OnChargeReleased()
{
	ATennisPlayerPawn* TennisPawn = GetTennisPawn();
	ATennisGameMode* GameMode = GetTennisGameMode();
	if (!TennisPawn || !GameMode)
	{
		return;
	}

	if (GameMode->IsHumanServing() && GameMode->GetRallyState() == ERallyState::PreServe)
	{
		// The serve release also ends the charge and carries its power.
		const float Charge = TennisPawn->ConsumeCharge();
		GameMode->ExecuteHumanServe(TennisPawn, FMath::Max(Charge, 0.12f), TennisPawn->GetAimTargetWorld());
		return;
	}

	TennisPawn->ReleaseGroundstroke();
}

void ATennisPlayerController::OnSwitchSpin()
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->CycleSpin(1);
	}
}

void ATennisPlayerController::OnSwitchSpinPrev()
{
	if (ATennisPlayerPawn* TennisPawn = GetTennisPawn())
	{
		TennisPawn->CycleSpin(-1);
	}
}

void ATennisPlayerController::OnServePressed()
{
	ATennisPlayerPawn* TennisPawn = GetTennisPawn();
	ATennisGameMode* GameMode = GetTennisGameMode();
	if (!TennisPawn || !GameMode)
	{
		return;
	}

	if (GameMode->IsHumanServing() && GameMode->GetRallyState() == ERallyState::PreServe)
	{
		// Space bar is a convenience quick serve at a safe power level.
		const float Charge = FMath::Max(TennisPawn->ConsumeCharge(), 0.55f);
		GameMode->ExecuteHumanServe(TennisPawn, Charge, TennisPawn->GetAimTargetWorld());
	}
}

void ATennisPlayerController::OnResetRally()
{
	if (ATennisGameMode* GameMode = GetTennisGameMode())
	{
		GameMode->ResetMatchOrPoint();
	}
}
