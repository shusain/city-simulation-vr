#pragma once
#include "CoreMinimal.h"
#include "Presentation/CityScenery.h"
#include "Simulation/CityEnvironmentSimulation.h"

enum class ECitySeasonalTreeVariant : uint8
{
	Leafy,
	Autumn,
	Bare
};

// Ordinary broadleaf follows the calendar across all terrain climates. Palms
// and conifers retain evergreen identity; weather only adds conifer snow caps.
// Species, placement and simulation state are never changed by this policy.
class CITYSIMVR_API FCitySeasonalScenery
{
public:
	// Retain the existing frozen pool assignment; warmth does not bypass seasons.
	static constexpr int32 WarmBroadleafPoolThreshold = 65;
	static ECitySeasonalTreeVariant BroadleafVariant(int32 WarmthPercent, ECitySeason Season);
	static bool ConiferSnowMesh(const FCityEnvironmentSnapshot& Weather, double SnowCover, bool bPreviouslySnowy);
	static FLinearColor BroadleafColor(ECitySeasonalTreeVariant Variant);
};
