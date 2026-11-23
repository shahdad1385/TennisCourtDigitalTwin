// Copyright (c) 2026 ClayTennis. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ITF and aerodynamic constants for a regulation tennis ball on a clay court.
 *
 * Sources / values:
 *  - ITF Rules of Tennis, Appendix I: mass 56.0-59.4 g, diameter 6.54-6.86 cm.
 *    We use the middle of the legal window: 57 g, 67 mm.
 *  - ITF Technical Centre bounce tests on clay: coefficient of restitution
 *    measured at impact speeds of 10-25 m/s falls in 0.75-0.81. 0.78 is the
 *    value used here. Tangential (friction) response on clay is high: the
 *    measured tangential coefficient is ~0.5-0.6, giving 10-16% horizontal
 *    speed loss for typical rally balls and a pronounced forward kick for
 *    heavy topspin.
 *  - Aerodynamics (Cross, Lindsey; Mehta; Cohen et al.): Cd ~0.5-0.62 for a
 *    tennis ball in the 15-60 m/s range, Cl up to ~0.45 for high spin ratios.
 *
 * Internal physics units are SI (meters, m/s, radians/s) and are converted to
 * Unreal centimeters only at the actor boundary. This keeps the integrator and
 * the trajectory solver numerically clean and reproducible.
 */
namespace TennisSpec
{
	// ---- Ball ----
	constexpr double BallMassKg = 0.057;
	constexpr double BallRadiusM = 0.0335;
	constexpr double BallDiameterM = 0.067;
	constexpr double BallCrossSectionM2 = 3.14159265358979323846 * BallRadiusM * BallRadiusM;

	// ---- Air ----
	constexpr double AirDensityKgM3 = 1.225;
	constexpr double AirDynamicViscosity = 1.81e-5;
	constexpr double GravityMps2 = -9.81;

	// ---- Aerodynamic coefficients ----
	constexpr double DragCoefficientBase = 0.55;
	constexpr double DragCoefficientMin = 0.50;
	constexpr double DragCoefficientMax = 0.62;
	constexpr double DragSpinGain = 0.07;
	constexpr double LiftCoefficientMax = 0.45;
	constexpr double LiftCoefficientSlope = 1.9;
	constexpr double SpinDecayRatePerSec = 1.5; // d(omega)/dt = -k * omega, spec value

	// ---- Clay surface ----
	constexpr double ClayRestitution = 0.78;
	constexpr double ClayFriction = 0.55;
	constexpr double HardRestitution = 0.81; // kept for future surface variants
	constexpr double HardFriction = 0.42;

	// ---- Net ----
	constexpr double NetHeightPostM = 0.914;
	constexpr double NetHeightCenterM = 1.07;
	constexpr double NetHalfWidthM = 5.485 + 0.914; // doubles sideline + post offset
	constexpr double NetRestitution = 0.35;
	constexpr double NetTangentialDamping = 0.82;
	constexpr double NetCordSpeedRetain = 0.55;
	constexpr double NetCordWindowM = 0.06;

	// ---- Court (ITF) ----
	constexpr double CourtLengthM = 23.77;
	constexpr double CourtHalfLengthM = 11.885;
	constexpr double DoublesWidthM = 10.97;
	constexpr double DoublesHalfWidthM = 5.485;
	constexpr double SinglesWidthM = 8.23;
	constexpr double SinglesHalfWidthM = 4.115;
	constexpr double ServiceLineFromNetM = 6.40;
	constexpr double ServiceBoxHalfWidthM = 4.115;
	constexpr double NetPostOffsetM = 0.914;
	constexpr double LineWidthM = 0.05;
	constexpr double LineToleranceM = 0.02;
}

namespace TennisPhysics
{
	/** Air state in SI units. Angular velocity is a world-space vector in rad/s. */
	struct FSimState
	{
		FVector Position = FVector::ZeroVector;		// meters
		FVector Velocity = FVector::ZeroVector;		// m/s
		FVector AngularVelocity = FVector::ZeroVector;	// rad/s
	};

	struct FBounceResult
	{
		FVector Velocity = FVector::ZeroVector;
		FVector AngularVelocity = FVector::ZeroVector;
		double NormalImpulse = 0.0;
		bool bValid = false;
	};

	/** S = r * |omega| / |v|. */
	double ComputeSpinRatio(const FVector& Velocity, const FVector& AngularVelocity);

	/** Cd rises slightly with spin: 0.55 base, clamped to [0.50, 0.62]. */
	double ComputeDragCoefficient(double SpinRatio);

	/** Cl = min(0.45, 1.9 * S), the standard low-spin-ratio linear fit. */
	double ComputeLiftCoefficient(double SpinRatio);

	/** Quadratic drag acceleration: -0.5 * rho * Cd * A * |v| * v / m. */
	FVector ComputeDragAcceleration(const FVector& Velocity, double DragCoefficient);

	/** Magnus acceleration along normalize(omega x v). */
	FVector ComputeMagnusAcceleration(const FVector& Velocity, const FVector& AngularVelocity, double LiftCoefficient);

	/** Sum of gravity, drag and Magnus, in m/s^2. */
	FVector ComputeAcceleration(const FVector& Velocity, const FVector& AngularVelocity);

	/** exp(-k * dt), k = 1.5 1/s by default. */
	double SpinDecayFactor(double DeltaSeconds, double Rate = TennisSpec::SpinDecayRatePerSec);

	FVector DecaySpin(const FVector& AngularVelocity, double DeltaSeconds);

	/**
	 * Classic RK4 for translation + analytic exponential spin decay.
	 * Deterministic for a fixed dt; used at 240 Hz by ATennisBall.
	 */
	void IntegrateRK4(FSimState& State, double DeltaSeconds);

	/**
	 * Rigid-sphere bounce with Coulomb friction.
	 *
	 * Normal impulse: Jn = m(1+e) * (-vz). Tangential impulse stops the contact
	 * point slip, limited by mu * Jn. For a sphere the effective tangential mass
	 * is m/3.5 (translation + 2/5 m r^2 rotation), so Jt_stop = m|slip|/3.5.
	 *
	 * This single model reproduces both clay behaviours:
	 *  - heavy topspin: contact point slips backwards -> friction pushes the ball
	 *    forward (forward kick) while reducing topspin;
	 *  - slice: contact point slips forwards -> friction drags the ball back
	 *    (skid) and reduces/reverses the backspin.
	 */
	FBounceResult ComputeBounce(const FVector& InVelocity,
		const FVector& InAngularVelocity,
		double Restitution = TennisSpec::ClayRestitution,
		double Friction = TennisSpec::ClayFriction);

	/** Net sag: 0.914 m at the posts, 1.07 m at the center strap. */
	double GetNetHeightAtY(double YMeters);

	/**
	 * Forward integration until the ball center reaches z = radius.
	 * Net collisions are included (reflection) and reported through
	 * bOutNetContact. Returns false if no landing occurred within MaxTime.
	 */
	bool SimulateUntilLanding(const FSimState& InitialState,
		double MaxTime,
		double DeltaSeconds,
		FVector& OutLandingMeters,
		double& OutTimeSeconds,
		bool& bOutNetContact);

	/**
	 * Same integrator, but continues through (BounceNumber - 1) clay bounces
	 * using the same rigid-sphere bounce model. Used by the AI to find the
	 * second-bounce "last chance" point of an incoming ball.
	 */
	bool SimulateToBounceNumber(const FSimState& InitialState,
		int32 BounceNumber,
		double MaxTime,
		double DeltaSeconds,
		FVector& OutLandingMeters,
		double& OutTimeSeconds,
		bool& bOutNetContact);

	/**
	 * Numerically solve the launch pitch that lands the ball at TargetMeters
	 * for a fixed speed and spin vector. A coarse sweep rejects net-clipping
	 * trajectories, then a fine sweep refines the best solution. Returns false
	 * only for degenerate inputs (zero distance or speed).
	 */
	bool SolveLaunchAngle(const FVector& StartMeters,
		const FVector& TargetMeters,
		double SpeedMps,
		const FVector& SpinRadPerSecond,
		double& OutPitchRadians);
}
