#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

struct CITYSIMVR_API FCityAreaSelection
{
	FCityCellCoord Anchor;
	FIntPoint Size = FIntPoint::ZeroValue;

	int32 GetCellCount() const
	{
		return Size.X * Size.Y;
	}
};

namespace CityAreaSelection
{
	CITYSIMVR_API FCityAreaSelection MakeInclusiveRectangle(const FCityCellCoord& FirstCorner,
	                                                        const FCityCellCoord& SecondCorner);
	CITYSIMVR_API void BuildCells(const FCityAreaSelection& Selection, TArray<FCityCellCoord>& OutCells);
} // namespace CityAreaSelection
