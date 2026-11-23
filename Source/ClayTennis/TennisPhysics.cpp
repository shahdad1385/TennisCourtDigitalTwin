// Copyright (c) 2026 ClayTennis. All rights reserved.

#include "TennisPhysics.h"

namespace TennisPhysics
{
	namespace
	{
		constexpr double SpinRatioEpsilon = 1.0e-6;
		constexpr double MinSpeedForAero = 1.0e-4;
		constexpr double BallInertia = 0.4 * TennisSpec::BallMassKg * TennisSpec::BallRadiusM * TennisSpec::BallRadiusM;

		FORCEINLINE double SignOrPositive(double Value)
		{
			return (Value >= 0.0) ? 1.0 : -1.0;
		}
	}

	double ComputeSpinRatio(const FVector& Velocity, const FVector& AngularVelocity)
	{
		const double Speed = Velocity.Size();
		if (Speed < MinSpeedForAero)
		{
			return 0.0;
		}
		return (TennisSpec::BallRadiusM * AngularVelocity.Size()) / Speed;
	}

	double ComputeDragCoefficient(double SpinRatio)
	{
		// Measured tennis-ball Cd rises a little with spin because the boundary
		// layer is tripped earlier; 0.55 -> ~0.62 at S >= 1.
		const double Cd = TennisSpec::DragCoefficientBase + TennisSpec::DragSpinGain * FMath::Clamp(SpinRatio, 0.0, 1.0);
		return FMath::Clamp(Cd, TennisSpec::DragCoefficientMin, TennisSpec::DragCoefficientMax);
	}

	double ComputeLiftCoefficient(double SpinRatio)
	{
		// Cl = min(0.45, 1.9 * S). Caps the lift at the measured maximum.
		return FMath::Min(TennisSpec::LiftCoefficientMax, TennisSpec::LiftCoefficientSlope * FMath::Max(0.0, SpinRatio));
	}

	FVector ComputeDragAcceleration(const FVector& Velocity, double DragCoefficient)
	{
		const double Speed = Velocity.Size();
		if (Speed < MinSpeedForAero)
		{
			return FVector::ZeroVector;
		}
		const double DynamicPressure = 0.5 * TennisSpec::AirDensityKgM3 * Speed * Speed;
		const double ForceMagnitude = DragCoefficient * TennisSpec::BallCrossSectionM2 * DynamicPressure;
		return -(Velocity / Speed) * (ForceMagnitude / TennisSpec::BallMassKg);
	}

	FVector ComputeMagnusAcceleration(const FVector& Velocity, const FVector& AngularVelocity, double LiftCoefficient)
	{
		const double Speed = Velocity.Size();
		if (Speed < MinSpeedForAero || LiftCoefficient <= 0.0)
		{
			return FVector::ZeroVector;
		}
		const FVector Cross = FVector::CrossProduct(AngularVelocity, Velocity);
		const double CrossSize = Cross.Size();
		if (CrossSize < SpinRatioEpsilon)
		{
			return FVector::ZeroVector;
		}
		const double DynamicPressure = 0.5 * TennisSpec::AirDensityKgM3 * Speed * Speed;
		const double ForceMagnitude = LiftCoefficient * TennisSpec::BallCrossSectionM2 * DynamicPressure;
		return (Cross / CrossSize) * (ForceMagnitude / TennisSpec::BallMassKg);
	}

	FVector ComputeAcceleration(const FVector& Velocity, const FVector& AngularVelocity)
	{
		const double SpinRatio = ComputeSpinRatio(Velocity, AngularVelocity);
		const double Cd = ComputeDragCoefficient(SpinRatio);
		const double Cl = ComputeLiftCoefficient(SpinRatio);

		const FVector Drag = ComputeDragAcceleration(Velocity, Cd);
		const FVector Magnus = ComputeMagnusAcceleration(Velocity, AngularVelocity, Cl);
		return Drag + Magnus + FVector(0.0, 0.0, TennisSpec::GravityMps2);
	}

	double SpinDecayFactor(double DeltaSeconds, double Rate)
	{
		return FMath::Exp(-Rate * FMath::Max(0.0, DeltaSeconds));
	}

	FVector DecaySpin(const FVector& AngularVelocity, double DeltaSeconds)
	{
		return AngularVelocity * SpinDecayFactor(DeltaSeconds);
	}

	void IntegrateRK4(FSimState& State, double DeltaSeconds)
	{
		if (DeltaSeconds <= 0.0)
		{
			return;
		}

		const FVector V0 = State.Velocity;
		const FVector W0 = State.AngularVelocity;

		const FVector A1 = ComputeAcceleration(V0, W0);
		const FVector V2 = V0 + A1 * (0.5 * DeltaSeconds);
		const FVector A2 = ComputeAcceleration(V2, W0);
		const FVector V3 = V0 + A2 * (0.5 * DeltaSeconds);
		const FVector A3 = ComputeAcceleration(V3, W0);
		const FVector V4 = V0 + A3 * DeltaSeconds;
		const FVector A4 = ComputeAcceleration(V4, W0);

		State.Position += (V0 + 2.0 * V2 + 2.0 * V3 + V4) * (DeltaSeconds / 6.0);
		State.Velocity += (A1 + 2.0 * A2 + 2.0 * A3 + A4) * (DeltaSeconds / 6.0);

		// Air spin decay is slow compared with translation; analytic update is
		// accurate and keeps the integrator stable for arbitrary spin rates.
		State.AngularVelocity = DecaySpin(State.AngularVelocity, DeltaSeconds);
	}

	FBounceResult ComputeBounce(const FVector& InVelocity,
		const FVector& InAngularVelocity,
		double Restitution,
		double Friction)
	{
		FBounceResult Result;

		if (InVelocity.Z > -0.01 || Restitution <= 0.0)
		{
			Result.Velocity = InVelocity;
			Result.AngularVelocity = InAngularVelocity;
			return Result;
		}

		const double NormalSpeed = -InVelocity.Z;
		const FVector ContactOffset(0.0, 0.0, -TennisSpec::BallRadiusM);

		// Velocity of the material point touching the court.
		const FVector Slip = FVector(InVelocity.X, InVelocity.Y, 0.0)
			+ FVector::CrossProduct(InAngularVelocity, ContactOffset);
		const double SlipSpeed = Slip.Size();

		const double NormalImpulse = TennisSpec::BallMassKg * (1.0 + Restitution) * NormalSpeed;
		const double StopImpulse = (TennisSpec::BallMassKg * SlipSpeed) / 3.5;
		const double TangentialImpulse = FMath::Min(Friction * NormalImpulse, StopImpulse);

		FVector Impulse(0.0, 0.0, NormalImpulse);
		if (SlipSpeed > SpinRatioEpsilon)
		{
			Impulse -= (Slip / SlipSpeed) * TangentialImpulse;
		}

		Result.Velocity = InVelocity + Impulse / TennisSpec::BallMassKg;
		Result.AngularVelocity = InAngularVelocity
			+ FVector::CrossProduct(ContactOffset, Impulse) / BallInertia;
		Result.NormalImpulse = NormalImpulse;
		Result.bValid = true;

		// A bounce always pushes the ball back up a little; prevents the
		// solve/predict loops from getting stuck in the surface.
		if (Result.Velocity.Z < 0.12)
		{
			Result.Velocity.Z = 0.12;
		}

		return Result;
	}

	double GetNetHeightAtY(double YMeters)
	{
		const double Normalized = FMath::Clamp(FMath::Abs(YMeters) / TennisSpec::NetHalfWidthM, 0.0, 1.0);
		// Parabolic sag matches the ITF measurement points: 0.914 m at both
		// posts and 1.07 m at the center strap.
		return TennisSpec::NetHeightPostM
			+ (TennisSpec::NetHeightCenterM - TennisSpec::NetHeightPostM) * (1.0 - Normalized * Normalized);
	}

	namespace
	{
		/**
		 * Advances one step and resolves a net-plane crossing. Returns true if
		 * the net was touched this step (set in bNetTouch).
		 */
		bool StepWithNet(FSimState& State, double Dt, bool& bNetTouch)
		{
			const FVector Previous = State.Position;
			IntegrateRK4(State, Dt);

			bNetTouch = false;
			if (Previous.X * State.Position.X >= 0.0)
			{
				return false;
			}

			const double DeltaX = State.Position.X - Previous.X;
			if (FMath::Abs(DeltaX) < 1.0e-9)
			{
				return false;
			}

			const double Alpha = FMath::Clamp(-Previous.X / DeltaX, 0.0, 1.0);
			const FVector Crossing = FMath::Lerp(Previous, State.Position, Alpha);
			const double NetTop = GetNetHeightAtY(Crossing.Y);
			const double BallTop = Crossing.Z;
			const double BallBottom = Crossing.Z - TennisSpec::BallRadiusM;

			// Misses above the net, or wide of the posts.
			if (FMath::Abs(Crossing.Y) > TennisSpec::NetHalfWidthM
				|| BallBottom > NetTop
				|| BallTop < 0.0)
			{
				return false;
			}

			bNetTouch = true;

			// Net cord: a very high contact loses most horizontal speed but can
			// still trickle over, which is what produces a serve let.
			if (FMath::Abs(BallBottom - NetTop) <= TennisSpec::NetCordWindowM && State.Velocity.X * Previous.X < 0.0)
			{
				State.Velocity.X *= TennisSpec::NetCordSpeedRetain;
				State.Velocity.Y *= 0.9;
				State.Velocity.Z = FMath::Max(State.Velocity.Z, 0.35);
				State.AngularVelocity *= 0.65;
				State.Position = Crossing;
				return true;
			}

			// Solid net contact: bounce back toward the hitter and lose energy.
			State.Position = Crossing;
			State.Position.X = SignOrPositive(Previous.X) * 0.02;
			State.Velocity.X = -State.Velocity.X * TennisSpec::NetRestitution;
			State.Velocity.Y *= TennisSpec::NetTangentialDamping;
			State.Velocity.Z *= TennisSpec::NetTangentialDamping;
			State.AngularVelocity *= 0.5;
			return true;
		}
	}

	bool SimulateToBounceNumber(const FSimState& InitialState,
		int32 BounceNumber,
		double MaxTime,
		double DeltaSeconds,
		FVector& OutLandingMeters,
		double& OutTimeSeconds,
		bool& bOutNetContact)
	{
		FSimState State = InitialState;
		bOutNetContact = false;
		OutTimeSeconds = 0.0;

		const int32 TargetBounce = FMath::Max(1, BounceNumber);
		int32 BounceCount = 0;

		const int32 MaxSteps = FMath::Max(1, static_cast<int32>(MaxTime / FMath::Max(DeltaSeconds, 1.0e-4)) + 2);
		for (int32 Step = 0; Step < MaxSteps; ++Step)
		{
			bool bTouchedNet = false;
			StepWithNet(State, DeltaSeconds, bTouchedNet);
			bOutNetContact |= bTouchedNet;
			OutTimeSeconds += DeltaSeconds;

			if (State.Position.Z <= TennisSpec::BallRadiusM && State.Velocity.Z < 0.0)
			{
				++BounceCount;
				OutLandingMeters = FVector(State.Position.X, State.Position.Y, 0.0);

				if (BounceCount >= TargetBounce)
				{
					return true;
				}

				// Continue through the clay bounce using the same model as the
				// live ball, so the second-bounce prediction is meaningful.
				const FBounceResult Bounce = ComputeBounce(State.Velocity, State.AngularVelocity);
				if (Bounce.bValid)
				{
					State.Velocity = Bounce.Velocity;
					State.AngularVelocity = Bounce.AngularVelocity;
				}
				State.Position.Z = TennisSpec::BallRadiusM;
			}

			// Ball is already far below the surface (should not happen, but be
			// defensive against bad inputs).
			if (State.Position.Z < -1.0)
			{
				OutLandingMeters = FVector(State.Position.X, State.Position.Y, 0.0);
				return false;
			}
		}

		OutLandingMeters = FVector(State.Position.X, State.Position.Y, 0.0);
		return false;
	}

	bool SimulateUntilLanding(const FSimState& InitialState,
		double MaxTime,
		double DeltaSeconds,
		FVector& OutLandingMeters,
		double& OutTimeSeconds,
		bool& bOutNetContact)
	{
		return SimulateToBounceNumber(InitialState, 1, MaxTime, DeltaSeconds,
			OutLandingMeters, OutTimeSeconds, bOutNetContact);
	}

	namespace
	{
		bool EvaluateLaunch(const FVector& Start,
			const FVector& HorizontalDirection,
			double Distance,
			double SpeedMps,
			const FVector& SpinRadPerSecond,
			double PitchRadians,
			double Dt,
			double MaxTime,
			double& OutError,
			bool& bOutNetTouch)
		{
			FSimState State;
			State.Position = Start;
			State.Velocity = (HorizontalDirection * FMath::Cos(PitchRadians)
				+ FVector(0.0, 0.0, FMath::Sin(PitchRadians))) * SpeedMps;
			State.AngularVelocity = SpinRadPerSecond;

			FVector Landing;
			double FlightTime = 0.0;
			const bool bLanded = SimulateUntilLanding(State, MaxTime, Dt, Landing, FlightTime, bOutNetTouch);

			FVector Delta = Landing - Start;
			Delta.Z = 0.0;
			const double Projected = Delta.Dot(HorizontalDirection);
			OutError = FMath::Abs(Projected - Distance);
			return bLanded;
		}
	}

	bool SolveLaunchAngle(const FVector& StartMeters,
		const FVector& TargetMeters,
		double SpeedMps,
		const FVector& SpinRadPerSecond,
		double& OutPitchRadians)
	{
		if (SpeedMps < 1.0)
		{
			return false;
		}

		FVector ToTarget = TargetMeters - StartMeters;
		ToTarget.Z = 0.0;
		const double Distance = ToTarget.Size();
		if (Distance < 0.5)
		{
			return false;
		}

		const FVector Direction = ToTarget / Distance;
		const double CoarseDt = 1.0 / 120.0;
		const double MaxTime = FMath::Clamp((Distance / SpeedMps) * 3.0 + 0.8, 1.0, 7.0);

		double BestClearPitch = FMath::DegreesToRadians(10.0);
		double BestClearError = 1.0e30;
		double BestAnyPitch = FMath::DegreesToRadians(10.0);
		double BestAnyError = 1.0e30;
		bool bFoundClear = false;
		bool bFoundAny = false;

		// Coarse sweep. Clearing trajectories are ranked separately from
		// net-clipping ones so a low-error net hit can never suppress a legal
		// shot; the net-clipping set is only a fallback for deliberate mishits.
		for (double Degrees = -22.0; Degrees <= 50.0; Degrees += 1.0)
		{
			const double Pitch = FMath::DegreesToRadians(Degrees);
			double Error = 0.0;
			bool bNetTouch = false;
			const bool bLanded = EvaluateLaunch(StartMeters, Direction, Distance, SpeedMps,
				SpinRadPerSecond, Pitch, CoarseDt, MaxTime, Error, bNetTouch);

			if (!bLanded)
			{
				continue;
			}

			if (!bNetTouch)
			{
				if (Error < BestClearError)
				{
					BestClearError = Error;
					BestClearPitch = Pitch;
					bFoundClear = true;
				}
			}
			else if (Error < BestAnyError)
			{
				BestAnyError = Error;
				BestAnyPitch = Pitch;
				bFoundAny = true;
			}
		}

		if (!bFoundClear && !bFoundAny)
		{
			OutPitchRadians = FMath::DegreesToRadians(10.0);
			return false;
		}

		const double BestPitch = bFoundClear ? BestClearPitch : BestAnyPitch;
		const double CenterDegrees = FMath::RadiansToDegrees(BestPitch);
		double FinePitch = BestPitch;
		double FineError = 1.0e30;
		bool bFineFound = false;

		for (double Degrees = CenterDegrees - 1.5; Degrees <= CenterDegrees + 1.5; Degrees += 0.1)
		{
			const double Pitch = FMath::DegreesToRadians(Degrees);
			double Error = 0.0;
			bool bNetTouch = false;
			const bool bLanded = EvaluateLaunch(StartMeters, Direction, Distance, SpeedMps,
				SpinRadPerSecond, Pitch, 1.0 / 240.0, MaxTime, Error, bNetTouch);

			if (!bLanded)
			{
				continue;
			}
			if (bFoundClear && bNetTouch)
			{
				continue;
			}
			if (Error < FineError)
			{
				FineError = Error;
				FinePitch = Pitch;
				bFineFound = true;
			}
		}

		OutPitchRadians = bFineFound ? FinePitch : BestPitch;
		return true;
	}
}
