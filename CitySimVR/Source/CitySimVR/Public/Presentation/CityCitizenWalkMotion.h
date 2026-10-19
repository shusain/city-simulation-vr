#pragma once
#include "CoreMinimal.h"

// Cosmetic state only. The board samples authoritative trips; this never
// changes their position, routing, clock, arrival or saved identity.
namespace CityCitizenWalk
{
inline constexpr double MeshScale = .811;
inline constexpr double StrideCm = 90.3225806451613 * MeshScale;
inline constexpr int32 DesktopCap = 32;
inline constexpr int32 QuestCap = 8;
inline constexpr double PeriodSeconds = 28.0 / 24.0;
}

struct FCityStreetWalkerSample
{
	uint64 Id = 0, Identity = 0, Trip = 0;
	int32 Variant = 0;
	FTransform Transform;
};

struct FCityCitizenWalkMotion
{
	FVector LastPosition = FVector::ZeroVector;
	double LastClock = 0, Phase = 0;
	float WalkWeight = 0;
	uint64 Trip = 0;
	bool bInitialized = false;
	void Sample(uint64 Identity, uint64 InTrip, const FVector& Position, double Clock, bool bPaused)
	{
		if (!bInitialized || Trip != InTrip || Clock < LastClock || (Position-LastPosition).Size() > 1600)
		{
			Phase = static_cast<double>((Identity * 37) % 101) / 101;
			WalkWeight = 0;
			bInitialized = true;
		}
		else if (!bPaused && Clock > LastClock)
		{
			const double Distance = (Position-LastPosition).Size();
			Phase = FMath::Fmod(Phase + Distance / CityCitizenWalk::StrideCm, 1.0);
			const float Target = Distance > .001 ? 1.f : 0.f;
			WalkWeight = FMath::FInterpConstantTo(WalkWeight, Target, Clock-LastClock, 6.f);
		}
		LastPosition = Position; LastClock = Clock; Trip = InTrip;
	}
};
