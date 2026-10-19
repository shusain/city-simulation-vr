#pragma once

#include "CoreMinimal.h"

class FCityTerrainHeightfield;

// Structural floor rectangles in native mesh-local centimeters. Outdoor pads,
// approach paving and the logical occupancy rectangle are deliberately separate.
struct FCityBuildingSupportRegion
{
	double MinX, MinY, MaxX, MaxY, BottomZ;
	// Local rotation allows narrow supports around curved shell feet without
	// filling their open centres with a bounding-box foundation.
	double YawDegrees = 0;
};

namespace CityBuildingSupports
{
	CITYSIMVR_API TConstArrayView<FCityBuildingSupportRegion> Find(const FString& MeshPackage);
	CITYSIMVR_API void MakeTransforms(TConstArrayView<FCityBuildingSupportRegion> Regions,
	                                 const FTransform& BuildingPose, const FCityTerrainHeightfield& Terrain,
	                                 TArray<FTransform>& OutTransforms);
}
