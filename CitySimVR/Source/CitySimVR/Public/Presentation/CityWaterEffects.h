#pragma once
#include "CoreMinimal.h"
#include "Presentation/CityWaterMesh.h"
#include "Presentation/CityAtmosphereMotion.h"
class FCityPlacementModel;

// Cosmetic shoreline field and two batched meshes; no simulation capacity limit.
struct CITYSIMVR_API FCityWaterEffects
{
	FCityWaterMesh Foam;
	FCityWaterMesh Splashes;
	static FCityWaterEffects Build(const FCityTerrainHeightfield& Terrain, const FCityPlacementModel& City,
	                               FCityWaterMesh& Water);
	static double Strength(const FCityAtmosphereMotion& Motion)
	{
		return FMath::Clamp(Motion.WindStrength - 1.0, 0.0, 1.0);
	}
};
