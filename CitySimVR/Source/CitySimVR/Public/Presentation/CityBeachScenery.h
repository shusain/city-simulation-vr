#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityTerrain.h"

class FCityPlacementModel;
struct FCitySceneryCandidate;

enum class ECityBeachKind : uint8
{
	FanShell,
	SpiralShell,
	Starfish,
	Driftwood,
	DuneGrass,
	Seaweed,
	PebbleSmall,
	PebbleMedium,
	PebbleLarge,
	Umbrella,
	Lounger,
	Chair,
	Mat,
	Cooler,
	Tote,
	CurledKelp
};

struct CITYSIMVR_API FCityBeachCandidate
{
	FCityCellCoord Cell;
	ECityBeachKind Kind = ECityBeachKind::FanShell;
	FTransform Transform;
	uint32 ClusterHash = 0;
	int32 WaterSurfaceCm = 0;
};

// Cosmetic, seeded source-sized props. Select once per terrain; development only
// removes candidates, so clearing a shore never grows replacement patches.
class CITYSIMVR_API FCityBeachScenery
{
public:
	static constexpr int32 KindCount = 16;
	static constexpr bool IsRecreation(ECityBeachKind Kind)
	{
		return Kind >= ECityBeachKind::Umbrella && Kind <= ECityBeachKind::Tote;
	}
	static constexpr bool IsSeaweed(ECityBeachKind Kind)
	{
		return Kind == ECityBeachKind::Seaweed || Kind == ECityBeachKind::CurledKelp;
	}
	static constexpr bool IsPebble(ECityBeachKind Kind)
	{
		return Kind >= ECityBeachKind::PebbleSmall && Kind <= ECityBeachKind::PebbleLarge;
	}
	static double ClearanceRadiusCm(ECityBeachKind Kind);
	// Match the rendered A-C-B / B-C-D terrain facets, including their diagonal.
	static double SampleGround(const FCityTerrainHeightfield& Terrain, double X, double Y, FVector& Normal);
	static TArray<FCityBeachCandidate> SelectCandidates(const FCityTerrainHeightfield& Terrain,
	                                                    TConstArrayView<FCitySceneryCandidate> Scenery = {});
	static bool IsClear(const FCityPlacementModel& City, const FCityCellCoord& Cell);
};
