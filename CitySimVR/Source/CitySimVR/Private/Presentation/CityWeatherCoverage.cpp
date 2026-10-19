#include "Presentation/CityWeatherCoverage.h"

namespace
{
	double UnitHash(int32 X, int32 Y, uint32 Seed)
	{
		uint32 Value = static_cast<uint32>(X) * 0x8da6b343u ^ static_cast<uint32>(Y) * 0xd8163841u ^ Seed;
		Value ^= Value >> 13;
		Value *= 0x85ebca6bu;
		Value ^= Value >> 16;
		return static_cast<double>(Value & 0xffffu) / 65535.0;
	}

	double Patch(const FVector2D& Cell, double Scale, uint32 Seed)
	{
		const double X = Cell.X / Scale, Y = Cell.Y / Scale;
		const int32 Ix = FMath::FloorToInt32(X), Iy = FMath::FloorToInt32(Y);
		const double Fx = FMath::SmoothStep(0.0, 1.0, X - Ix), Fy = FMath::SmoothStep(0.0, 1.0, Y - Iy);
		return FMath::Lerp(FMath::Lerp(UnitHash(Ix, Iy, Seed), UnitHash(Ix + 1, Iy, Seed), Fx),
		                   FMath::Lerp(UnitHash(Ix, Iy + 1, Seed), UnitHash(Ix + 1, Iy + 1, Seed), Fx), Fy);
	}
} // namespace

FVector2D FCityWeatherCoverage::Sample(const FCityTerrainHeightfield& Terrain, const FVector2D& Cell,
                                       const FVector& Normal)
{
	return Evaluate(Terrain.GetParameters().Seed, Cell,
	                Terrain.SampleHeightCm(Cell.X, Cell.Y) - Terrain.GetSeaLevelCm(), Normal);
}

FVector2D FCityWeatherCoverage::Evaluate(int32 Seed, const FVector2D& Cell, double HeightAboveWaterCm,
                                         const FVector& Normal)
{
	if (!FMath::IsFinite(Cell.X) || !FMath::IsFinite(Cell.Y) || !FMath::IsFinite(HeightAboveWaterCm) ||
	    Normal.ContainsNaN())
		return FVector2D::ZeroVector;
	const uint32 PatternSeed = static_cast<uint32>(Seed) ^ 0x65a37d19u;
	const double Coherent = Patch(Cell, 6.0, PatternSeed) * .75 + Patch(Cell, 2.0, PatternSeed ^ 0x471bcaf3u) * .25;
	const FVector UnitNormal = Normal.GetSafeNormal();
	const double Flatness = FMath::SmoothStep(.68, .98, FMath::Clamp(UnitNormal.Z, 0.0, 1.0));
	const double Altitude = FMath::SmoothStep(400.0, 3000.0, HeightAboveWaterCm);
	// Fixed north-facing exposure gives stable sheltered/cooler faces; gusts do
	// not crawl the snow pattern or require geometry updates every frame.
	const double Shelter = FMath::Clamp(.5 - UnitNormal.Y * .5, 0.0, 1.0);
	const double Snow =
	    FMath::Clamp(.16 + .56 * Coherent + .18 * Altitude + .12 * Shelter - .22 * (1.0 - Flatness), 0.0, 1.0);
	const double Lowland = 1.0 - FMath::SmoothStep(40.0, 1400.0, HeightAboveWaterCm);
	const double Wet = FMath::Clamp(.12 + .55 * (1.0 - Coherent) + .19 * Lowland + .14 * Flatness, 0.0, 1.0);
	return FVector2D(Snow, Wet);
}

double FCityWeatherCoverage::SnowAmount(double SnowCover, double Retention)
{
	if (!FMath::IsFinite(SnowCover) || !FMath::IsFinite(Retention))
		return 0.0;
	return FMath::Clamp((FMath::Clamp(SnowCover, 0.0, 1.0) * 1.25 - (1.0 - FMath::Clamp(Retention, 0.0, 1.0))) * 1.4,
	                    0.0, 1.0) *
	       .82;
}

double FCityWeatherCoverage::WetAmount(double Wetness, double Retention)
{
	if (!FMath::IsFinite(Wetness) || !FMath::IsFinite(Retention))
		return 0.0;
	return FMath::Clamp(Wetness, 0.0, 1.0) * (.35 + .65 * FMath::Clamp(Retention, 0.0, 1.0));
}
