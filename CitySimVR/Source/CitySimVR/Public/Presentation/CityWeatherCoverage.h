#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityTerrain.h"

// Static cosmetic attributes baked once into terrain UV1. They never affect
// terrain geometry, weather authority, hydrology, roads or simulation saves.
struct CITYSIMVR_API FCityWeatherCoverage
{
	// X = snow retention, Y = wet retention; each channel lies in [0,1].
	static FVector2D Sample(const FCityTerrainHeightfield& Terrain, const FVector2D& Cell, const FVector& Normal);
	static FVector2D Evaluate(int32 Seed, const FVector2D& Cell, double HeightAboveWaterCm, const FVector& Normal);
	// Mirrors the standard material graph for meaningful transition tests.
	static double SnowAmount(double SnowCover, double Retention);
	static double WetAmount(double Wetness, double Retention);
};
