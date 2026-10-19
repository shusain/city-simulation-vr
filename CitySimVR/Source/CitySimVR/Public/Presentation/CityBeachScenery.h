#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityTerrain.h"

class FCityPlacementModel;

enum class ECityBeachKind : uint8
{
	FanShell,
	SpiralShell,
	Starfish,
	Driftwood,
	DuneGrass
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
	static constexpr int32 KindCount = 5;
	static constexpr int32 MaxClusters = 32;
	static constexpr int32 MaxProps = MaxClusters * KindCount;
	// Match the rendered A-C-B / B-C-D terrain facets, including their diagonal.
	static double SampleGround(const FCityTerrainHeightfield& Terrain, double X, double Y, FVector& Normal);
	static TArray<FCityBeachCandidate> SelectCandidates(const FCityTerrainHeightfield& Terrain);
	static bool IsClear(const FCityPlacementModel& City, const FCityCellCoord& Cell);
};
