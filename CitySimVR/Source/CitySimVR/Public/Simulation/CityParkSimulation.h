#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
class UCityBuildingCatalog;
struct FCityServiceSnapshot;
struct FCityEconomyTuning;
struct FCityDevelopmentSnapshot;

struct FCityParkPolicy
{
	bool bIsPark = false;
	int32 Radius = 0;
	int32 Bonus = 0;
	int64 Upkeep = 0;
};

struct FCityParkProvider
{
	uint64 EntityId = 0;
	FCityCellCoord Anchor;
	FIntPoint Footprint;
	int32 Radius = 0;
	int32 Bonus = 0;
};

class CITYSIMVR_API FCityParkEvaluator
{
public:
	static constexpr int32 MaximumBonus = 30;
	static FCityParkPolicy Policy(FName Id, const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog);
	static void Build(const FCityPlacementModel& City, FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	                  const UCityBuildingCatalog* Catalog, const FCityDevelopmentSnapshot* Development,
	                  bool bUnpaidBills = false);
	static int32 Benefit(const TArray<FCityParkProvider>& Providers, const FCityCellCoord& Anchor,
	                     const FIntPoint& Footprint);
};
