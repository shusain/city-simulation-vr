#pragma once

#include "CoreMinimal.h"

class FCityPlacementModel;
class FCityTerrainHeightfield;
class UCityBuildingCatalog;
struct FCityServiceSnapshot;
struct FCityDevelopmentSnapshot;
struct FCityEnvironmentSnapshot;

enum class ECitySeasonalDecorationKind : uint8
{
	FlowerBed,
	Pumpkin,
	Snowman,
	FestiveTree,
	Gift,
	LeafPile,
	WreathAutumn,
	WreathWinter
};

struct CITYSIMVR_API FCitySeasonalDecorationSample
{
	uint64 EntityId = 0;
	ECitySeasonalDecorationKind Kind = ECitySeasonalDecorationKind::FlowerBed;
	// Absolute logical XY, already aligned to the imported building orientation.
	FVector2D CellPosition = FVector2D::ZeroVector;
	float YawDegrees = 0.f;
	float Scale = 1.f;
	// Added to the building pivot, or terrain when bUseBuildingBase is false.
	float SurfaceOffsetCm = 0.f;
	bool bUseBuildingBase = true;
	bool bPark = false;
	bool bWallMounted = false;
	bool operator==(const FCitySeasonalDecorationSample& Other) const = default;
};

// Calendar-driven, bounded cosmetics. No actors, random-state consumption,
// simulation rewards, occupancy changes or additional persisted state.
class CITYSIMVR_API FCitySeasonalDecorations
{
public:
	static constexpr int32 MaximumSamples = 64;
	static constexpr int32 MaximumParkSamples = 32;
	static constexpr int32 MaximumCitySamples = 32;
	static void Build(const FCityPlacementModel& City, const FCityTerrainHeightfield& Terrain,
	                  const FCityServiceSnapshot& Services, const FCityDevelopmentSnapshot& Development,
	                  const UCityBuildingCatalog* Catalog, const FCityEnvironmentSnapshot& Weather, int32 Month,
	                  TArray<FCitySeasonalDecorationSample>& OutSamples);
};
