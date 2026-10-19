#include "Presentation/CityWaterPlacementOverlay.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTerrain.h"

namespace
{
	int32 SiteIndex(const FCityCellCoord& Cell, const FCityGridDimensions Dimensions)
	{
		return Cell.X >= 0 && Cell.X < Dimensions.GetWidth() && Cell.Y >= 0 && Cell.Y < Dimensions.GetHeight()
		           ? Cell.Y * Dimensions.GetWidth() + Cell.X
		           : INDEX_NONE;
	}
} // namespace

FCityWaterPlacementOverlaySnapshot FCityWaterPlacementOverlay::Build(const FCityPlacementModel& City,
                                                                     const FCityTerrainHeightfield& Terrain,
                                                                     const FCityServiceSnapshot& Services)
{
	FCityWaterPlacementOverlaySnapshot Result;
	const auto Dimensions = City.GetGrid().GetDimensions();
	Result.Dimensions = Dimensions;
	if (Terrain.GetDimensions() != Dimensions || Services.Dimensions != Dimensions)
	{
		Result.Legend = TEXT("Water siting unavailable: city dimensions disagree");
		return Result;
	}
	Result.RulesVersion = Services.Water.RulesVersion;
	Result.GroundwaterYieldPercent.Init(INDEX_NONE, Dimensions.GetCellCount());
	for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
		for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (Terrain.IsWaterCell(Cell))
				continue;
			Result.LandCells.Add(Cell);
			Result.GroundwaterYieldPercent[Y * Dimensions.GetWidth() + X] =
			    Result.RulesVersion > 0 ? Terrain.GetGroundwaterYieldPercent(Cell) : 100;
		}
	TArray<FCityCellCoord> Roads = City.GetRoadCells().Array();
	Roads.Sort([](const auto& A, const auto& B)
	           { return A.Y != B.Y   ? A.Y < B.Y
		                : A.X != B.X ? A.X < B.X
		                             : A.RoadLevel < B.RoadLevel; });
	Result.Roads.Reserve(Roads.Num());
	for (const auto& Cell : Roads)
	{
		FCityWaterPlacementRoadSample Road;
		Road.Cell = Cell;
		Road.PressurePercent = Services.Water.RoadPressurePercent.FindRef(Cell);
		Road.bConnectedToEntrance = City.IsRoadConnected(Cell);
		Road.bHasActiveSourcePath = Services.Water.ActiveNetworkRoads.Contains(Cell);
		Road.bHasPotentialSourcePath = Services.Water.PotentialNetworkRoads.Contains(Cell);
		for (const auto& Source : Services.Water.Sources)
			if (const int32* Hops = Source.RoadDistances.Find(Cell);
			    Source.bOperational && Hops &&
			    (Result.RulesVersion == 0 ||
			     Source.SourcePressurePercent - *Hops * FCityWaterEvaluator::PressureLossPerRoadHop >=
			         FCityWaterEvaluator::MinimumUsablePressurePercent))
				Road.RemainingSourceOutput += FMath::Max(0, Source.Capacity - Source.Used);
		Result.Roads.Add(Road);
	}
	Result.Legend =
	    Result.RulesVersion > 0
	        ? TEXT(
	              "Site groundwater yield: red 60% / amber 100% / cyan 140%\nExisting pipe pressure: red 0% / orange 20% / blue 60%+")
	        : TEXT("Site groundwater yield: 100% under legacy rules\nExisting pipe pressure: red 0% / blue 100%");
	int32 ShortBuildings = 0;
	for (const auto& Pair : Services.Water.EntityDemand)
		ShortBuildings += Services.Water.EntityDelivered.FindRef(Pair.Key) < Pair.Value;
	Result.Legend += FString::Printf(
	    TEXT(
	        "\nBlue = pressure, not spare supply | delivered %d / demand %d\nSource output %d | %d buildings short; inspect for cause and remedy"),
	    Services.ServiceUsed, Services.ServiceDemand, Services.ServiceCapacity, ShortBuildings);
	return Result;
}

FLinearColor FCityWaterPlacementOverlay::GroundwaterColor(const int32 YieldPercent)
{
	const FLinearColor Red(1.f, .08f, .025f), Amber(1.f, .65f, .035f), Cyan(.025f, .8f, 1.f);
	return YieldPercent <= 100 ? FMath::Lerp(Red, Amber, FMath::Clamp((YieldPercent - 60) / 40.f, 0.f, 1.f))
	                           : FMath::Lerp(Amber, Cyan, FMath::Clamp((YieldPercent - 100) / 40.f, 0.f, 1.f));
}

FLinearColor FCityWaterPlacementOverlay::PressureColor(const int32 PressurePercent)
{
	const FLinearColor Red(1.f, .035f, .02f), Orange(1.f, .42f, .025f), Blue(.035f, .3f, 1.f),
	    HighBlue(.05f, .62f, 1.f);
	if (PressurePercent <= 20)
		return FMath::Lerp(Red, Orange, FMath::Clamp(PressurePercent / 20.f, 0.f, 1.f));
	if (PressurePercent <= 60)
		return FMath::Lerp(Orange, Blue, (PressurePercent - 20) / 40.f);
	return FMath::Lerp(Blue, HighBlue, FMath::Clamp((PressurePercent - 60) / 60.f, 0.f, 1.f));
}

FLinearColor FCityWaterPlacementOverlaySnapshot::GroundwaterColorAt(const FCityCellCoord& Cell) const
{
	const int32 Index = SiteIndex(Cell, Dimensions);
	return GroundwaterYieldPercent.IsValidIndex(Index) && GroundwaterYieldPercent[Index] != INDEX_NONE
	           ? FCityWaterPlacementOverlay::GroundwaterColor(GroundwaterYieldPercent[Index])
	           : FLinearColor::Transparent;
}

FString FCityWaterPlacementOverlaySnapshot::DescribeSite(const FCityCellCoord& Cell) const
{
	const int32 Index = SiteIndex(Cell, Dimensions);
	if (!GroundwaterYieldPercent.IsValidIndex(Index) || GroundwaterYieldPercent[Index] == INDEX_NONE)
		return TEXT("Point at land for local groundwater yield");
	return FString::Printf(TEXT("Site (%d,%d): groundwater yield %d%%%s"), Cell.X, Cell.Y,
	                       GroundwaterYieldPercent[Index], RulesVersion > 0 ? TEXT("") : TEXT(" (legacy rules)"));
}

FString FCityWaterPlacementOverlay::DescribeRoad(const FCityWaterPlacementRoadSample& Road)
{
	const TCHAR* State = Road.PressurePercent >= FCityWaterEvaluator::MinimumUsablePressurePercent ? TEXT("usable head")
	                     : !Road.bConnectedToEntrance   ? TEXT("road disconnected from entrance")
	                     : Road.bHasActiveSourcePath    ? TEXT("below usable head 20%")
	                     : Road.bHasPotentialSourcePath ? TEXT("water source offline")
	                                                    : TEXT("no connected water source");
	return FString::Printf(
	    TEXT("Existing %s pipe (%d,%d): pressure %d%% | %s\nConnected source output remaining for new demand: %d"),
	    Road.Cell.RoadLevel ? TEXT("raised") : TEXT("ground"), Road.Cell.X, Road.Cell.Y, Road.PressurePercent, State,
	    Road.RemainingSourceOutput);
}
