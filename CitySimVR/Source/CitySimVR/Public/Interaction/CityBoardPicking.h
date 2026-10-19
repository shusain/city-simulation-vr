#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityTerrainHeightfield;

namespace CityBoardPicking
{
	inline constexpr double LogicalCellSizeCm = 800.0;
	inline constexpr double LogicalBoardSizeCm = FCityGrid::Width * LogicalCellSizeCm;
	CITYSIMVR_API FVector2D GetLogicalBoardSize(FCityGridDimensions Dimensions);
	CITYSIMVR_API bool PickCellFromWorldRay(const FTransform& BoardTransform, FCityGridDimensions Dimensions,
	                                        const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
	                                        FCityCellCoord& OutCell, FVector* OutWorldHit = nullptr);

	CITYSIMVR_API bool PickCellFromWorldRay(const FTransform& BoardTransform, const FVector& WorldRayOrigin,
	                                        const FVector& WorldRayDirection, FCityCellCoord& OutCell,
	                                        FVector* OutWorldHit = nullptr);

	CITYSIMVR_API bool PickCellFromWorldRay(const FTransform& BoardTransform, const FCityTerrainHeightfield& Terrain,
	                                        const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
	                                        FCityCellCoord& OutCell, FVector* OutWorldHit = nullptr);

	CITYSIMVR_API FVector GetCellCenterLocal(const FCityCellCoord& Cell);
} // namespace CityBoardPicking
