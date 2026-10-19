#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
class FCityTerrainHeightfield;
struct FCityServiceSnapshot;

struct CITYSIMVR_API FCityWaterPlacementRoadSample
{
	FCityCellCoord Cell;
	int32 PressurePercent = 0;
	int32 RemainingSourceOutput = 0; // Spare output on usable paths, after current allocations.
	bool bConnectedToEntrance = false;
	bool bHasActiveSourcePath = false;
	bool bHasPotentialSourcePath = false;
};

// Read-only placement guidance. Site yield and pressure from existing sources
// are separate fields; selecting a proposed source never changes this data.
struct CITYSIMVR_API FCityWaterPlacementOverlaySnapshot
{
	FCityGridDimensions Dimensions;
	int32 RulesVersion = 0;
	TArray<int32> GroundwaterYieldPercent; // XY grid, INDEX_NONE on visible water.
	TArray<FCityCellCoord> LandCells;
	TArray<FCityWaterPlacementRoadSample> Roads; // Sorted, full ground/raised addresses.
	FString Legend;
	FLinearColor GroundwaterColorAt(const FCityCellCoord& Cell) const;
	FString DescribeSite(const FCityCellCoord& Cell) const;
};

class CITYSIMVR_API FCityWaterPlacementOverlay
{
public:
	static FCityWaterPlacementOverlaySnapshot Build(const FCityPlacementModel& City,
	                                                const FCityTerrainHeightfield& Terrain,
	                                                const FCityServiceSnapshot& Services);
	// Opaque colors; the presentation layer chooses its translucent opacity.
	static FLinearColor GroundwaterColor(int32 YieldPercent);
	static FLinearColor PressureColor(int32 PressurePercent);
	static FString DescribeRoad(const FCityWaterPlacementRoadSample& Road);
};
