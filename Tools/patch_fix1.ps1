$src = 'C:\Projects\ClayTennis\Source\ClayTennis'
function Patch([string]$file, [scriptblock]$fn) {
    $p = Join-Path $src $file
    $s = [System.IO.File]::ReadAllText($p)
    $s = & $fn $s
    [System.IO.File]::WriteAllText($p, $s)
    Write-Host "patched $file"
}

# ---------------- TennisPlayerPawn.h : local spring state type
Patch 'TennisPlayerPawn.h' {
    param($s)
    $s = $s.Replace('FFloatSpringState AimYawSpring;', 'FTennisSpringState AimYawSpring;')
    $s = $s.Replace('FFloatSpringState AimPitchSpring;', 'FTennisSpringState AimPitchSpring;')
    $struct = @'
class ATennisRacket;

/** Minimal critically damped spring state (UE 5.5 has no FFloatSpringState in Core). */
struct FTennisSpringState
{
	float PrevValue = 0.0f;
	float PrevVelocity = 0.0f;
};
'@
    return [regex]::Replace($s, '(?m)^class ATennisRacket;\s*$', $struct)
}

# ---------------- TennisPlayerPawn.cpp : implement the spring locally
Patch 'TennisPlayerPawn.cpp' {
    param($s)
    $s = $s.Replace('FMath::SpringInterp(', 'TennisSpringInterp(')
    $helper = @'
	float TennisSpringInterp(float Current, float Target, FTennisSpringState& State,
		float Stiffness, float CriticalDampingFactor, float DeltaTime)
	{
		const float Stiff = FMath::Max(0.0f, Stiffness);
		const float Damping = FMath::Max(0.0f, CriticalDampingFactor);
		const float Dt = FMath::Clamp(DeltaTime, 0.0f, 0.05f);
		const float SpringForce = (Target - Current) * Stiff;
		const float DampingForce = -State.PrevVelocity * (2.0f * FMath::Sqrt(Stiff) * Damping);
		State.PrevVelocity += (SpringForce + DampingForce) * Dt;
		State.PrevValue = Current + State.PrevVelocity * Dt;
		return State.PrevValue;
	}

	EShotType ShotTypeForSpin(ESpinType Spin)
'@
    return [regex]::Replace($s, '(?m)^\tEShotType ShotTypeForSpin\(ESpinType Spin\)\s*$', $helper.TrimEnd())
}

# ---------------- TennisPlayerController.cpp : rename locals hiding AController::Pawn
Patch 'TennisPlayerController.cpp' {
    param($s)
    $s = $s.Replace('ATennisPlayerPawn* Pawn = GetTennisPawn()', 'ATennisPlayerPawn* TennisPawn = GetTennisPawn()')
    $s = $s.Replace('!Pawn || !GameMode', '!TennisPawn || !GameMode')
    $s = $s.Replace('Pawn->', 'TennisPawn->')
    return $s
}

Write-Host '--- remaining bare Pawn locals:'
Select-String -Path (Join-Path $src 'TennisPlayerController.cpp') -Pattern '(?<![A-Za-z])Pawn(?!\.|Ap|s) ->|(?<![A-Za-z])Pawn\b' | Select-Object -First 10 LineNumber, Line
