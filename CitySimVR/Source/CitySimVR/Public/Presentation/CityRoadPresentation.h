#pragma once

#include "CoreMinimal.h"

// Shared road-tile dimensions for street surfaces and pedestrian placement.
namespace CityRoadPresentation
{
	inline double SurfaceWidth(const FName DefinitionId)
	{
		return DefinitionId == TEXT("DirtRoad")  ? 0.52
		       : DefinitionId == TEXT("Avenue")  ? 0.80
		       : DefinitionId == TEXT("Highway") ? 0.98
		                                         : 0.68;
	}
	inline double SidewalkWidth(const FName DefinitionId)
	{
		return DefinitionId == TEXT("Highway") ? 0.0 : FMath::Min(0.12, 0.5 - SurfaceWidth(DefinitionId) * 0.5);
	}
	inline double SidewalkOffset(const FName DefinitionId)
	{
		return SurfaceWidth(DefinitionId) * 0.5 + SidewalkWidth(DefinitionId) * 0.5;
	}
} // namespace CityRoadPresentation
