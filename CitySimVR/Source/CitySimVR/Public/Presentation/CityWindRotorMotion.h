#pragma once
#include "CoreMinimal.h"
// Cosmetic real-time motion. Exact exponential integration avoids frame-rate-dependent ramps.
struct FCityWindRotorMotion
{
	double Speed = 0.0;
	double Angle = 0.0;
	void Advance(double Seconds, int32 WindStrength)
	{
		if (!FMath::IsFinite(Seconds) || Seconds <= 0)
			return;
		const double Target = FMath::Clamp(WindStrength, 0, 2) * 90.0;
		const double Decay = FMath::Exp(-Seconds / 2.5);
		Angle = FMath::Fmod(Angle + Target * Seconds + (Speed - Target) * 2.5 * (1.0 - Decay), 360.0);
		Speed = Target + (Speed - Target) * Decay;
	}
	FTransform Transform(const FVector& AxleCm) const
	{
		return FTransform(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(Angle)), AxleCm);
	}
};
