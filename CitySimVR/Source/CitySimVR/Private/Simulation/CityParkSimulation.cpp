#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"

namespace
{
	int32 ParkDistance(const FCityParkProvider& Park, const FCityCellCoord& Anchor, const FIntPoint& Footprint)
	{
		const int32 GapX = FMath::Max(0, FMath::Max(Park.Anchor.X - (Anchor.X + Footprint.X - 1),
		                                            Anchor.X - (Park.Anchor.X + Park.Footprint.X - 1)));
		const int32 GapY = FMath::Max(0, FMath::Max(Park.Anchor.Y - (Anchor.Y + Footprint.Y - 1),
		                                            Anchor.Y - (Park.Anchor.Y + Park.Footprint.Y - 1)));
		return GapX + GapY;
	}
} // namespace

FCityParkPolicy FCityParkEvaluator::Policy(const FName Id, const FCityEconomyTuning& Tuning,
                                           const UCityBuildingCatalog* Catalog)
{
	const auto* Definition = Catalog ? Catalog->FindDefinition(Id) : nullptr;
	const bool bLegacy = Id == Tuning.ParkDefinitionId;
	FCityParkPolicy Result;
	Result.bIsPark = bLegacy || (Definition && Definition->ParkDesirabilityBonus > 0);
	if (!Result.bIsPark)
		return Result;
	Result.Radius = Definition && Definition->ParkDesirabilityBonus > 0 ? Definition->ParkRadiusCells
	                                                                    : Tuning.ParkDesirabilityRadiusCells;
	Result.Bonus = FMath::Clamp(Definition && Definition->ParkDesirabilityBonus > 0 ? Definition->ParkDesirabilityBonus
	                                                                                : Tuning.ParkDesirabilityBonus,
	                            0, MaximumBonus);
	Result.Upkeep = bLegacy ? Tuning.ParkUpkeep : Definition->MonthlyUpkeep;
	return Result;
}

int32 FCityParkEvaluator::Benefit(const TArray<FCityParkProvider>& Providers, const FCityCellCoord& Anchor,
                                  const FIntPoint& Footprint)
{
	int32 Best = 0;
	for (const auto& Park : Providers)
	{
		if (ParkDistance(Park, Anchor, Footprint) <= Park.Radius)
			Best = FMath::Max(Best, Park.Bonus);
	}
	return FMath::Clamp(Best, 0, MaximumBonus);
}

void FCityParkEvaluator::Build(const FCityPlacementModel& City, FCityServiceSnapshot& Services,
                               const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog,
                               const FCityDevelopmentSnapshot* Development, const bool bUnpaidBills)
{
	Services.Dimensions = City.GetGrid().GetDimensions();
	Services.ParkProviders.Reset();
	Services.ParkCoverage.Init(0, City.GetGrid().GetDimensions().GetCellCount());
	Services.OperationalParks = Services.OperationalLargeParks = 0;
	if (bUnpaidBills)
		return;
	TArray<uint64> Ids;
	City.GetEntities().GetKeys(Ids);
	Ids.Sort();
	for (const uint64 Id : Ids)
	{
		const auto& Entity = City.GetEntities().FindChecked(Id);
		const auto Park = Policy(Entity.DefinitionId, Tuning, Catalog);
		if (!Park.bIsPark || Services.GetFailure(Id) != ECityOperationFailure::None ||
		    (Development && !Development->IsOperational(Id)))
			continue;
		Services.ParkProviders.Add({Id, Entity.Anchor, Entity.FootprintSize, Park.Radius, Park.Bonus});
		++Services.OperationalParks;
		if (Entity.DefinitionId != Tuning.ParkDefinitionId)
			++Services.OperationalLargeParks;
		// Merge the bounded footprint diamond using max; overlapping parks cannot stack.
		for (int32 Y = FMath::Max(0, Entity.Anchor.Y - Park.Radius);
		     Y < FMath::Min(City.GetGrid().GetDimensions().GetHeight(),
		                    Entity.Anchor.Y + Entity.FootprintSize.Y + Park.Radius);
		     ++Y)
			for (int32 X = FMath::Max(0, Entity.Anchor.X - Park.Radius);
			     X < FMath::Min(City.GetGrid().GetDimensions().GetWidth(),
			                    Entity.Anchor.X + Entity.FootprintSize.X + Park.Radius);
			     ++X)
			{
				const int32 Value =
				    ParkDistance(Services.ParkProviders.Last(), {X, Y}, {1, 1}) <= Park.Radius ? Park.Bonus : 0;
				int32& Cell = Services.ParkCoverage[Y * City.GetGrid().GetDimensions().GetWidth() + X];
				Cell = FMath::Max(Cell, Value);
			}
	}
}
