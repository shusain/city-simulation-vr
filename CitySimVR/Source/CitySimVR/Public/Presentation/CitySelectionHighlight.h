#pragma once
#include "Commands/CityPlacementModel.h"
struct FCitySelectionHighlightPlan
{
	TArray<FCityCellCoord> Cells, RetainedCells;
	uint64 EntityId = FCityGrid::EmptyEntityId;
	bool bRoad = false;
};
namespace CitySelectionHighlight
{
	CITYSIMVR_API FCitySelectionHighlightPlan Build(const FCityPlacementModel& City, const FCityCellCoord& Cell,
	                                                bool bDemolish);
}
