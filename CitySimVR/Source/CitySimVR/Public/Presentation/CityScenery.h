#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityTerrain.h"

enum class ECitySceneryKind : uint8
{
	Broadleaf,
	Conifer,
	Rock,
	Palm
};

struct CITYSIMVR_API FCityScenerySample
{
	int32 WarmthPercent = 0;
	int32 MountainAffinityPercent = 0;
	int32 ShoreDistanceCells = FCityGrid::Width + FCityGrid::Height;
	int32 ElevationCm = 0;
	ECityTerrainSlope Slope = ECityTerrainSlope::Flat;
};

struct CITYSIMVR_API FCitySceneryCandidate
{
	FCityCellCoord Cell;
	uint32 Hash = 0;
	ECitySceneryKind Kind = ECitySceneryKind::Broadleaf;
	bool operator==(const FCitySceneryCandidate& Other) const = default;
};

// Frozen presentation profiles from initial terrain, independent of calendar,
// development and weather. Budget selection precedes development clearance.
class CITYSIMVR_API FCitySceneryField
{
public:
	void Build(const FCityTerrainHeightfield& Terrain);
	const FCityScenerySample& GetSample(const FCityCellCoord& Cell) const;
	static int32 ClimateWarmthPercent(int32 Seed);
	static FString ClimateDescription(int32 Seed);
	static uint32 Hash(uint32 X, uint32 Y, uint32 Seed);
	static ECitySceneryKind SelectKind(const FCityScenerySample& Sample, uint32 Choice);
	static TArray<FCitySceneryCandidate> SelectCandidates(const FCityTerrainHeightfield& Terrain,
	                                                      const FCityCellCoord& Entrance, int32 MaxTrees,
	                                                      int32 MaxRocks);

private:
	FCityGridDimensions Dimensions;
	TArray<FCityScenerySample> Samples;
};
