#include "Performance/CityBenchmark.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Simulation/CityBuildingDefinition.h"

namespace
{
	constexpr int32 MaxBenchmarkLots = FCityGrid::Width == 64 ? 1000 : FCityGrid::CellCount;
	constexpr int32 MaxBenchmarkRoadCells = FCityGrid::CellCount;
	constexpr int32 MaxBenchmarkVehicles = FCityGrid::CellCount;

	bool IsFootprintFree(const FCityCellCoord& Anchor, const FIntPoint& Size, const TSet<FCityCellCoord>& ReservedCells,
	                     const FCityGridDimensions Dimensions)
	{
		if (Anchor.X < 0 || Anchor.Y < 0 || Anchor.X + Size.X > Dimensions.GetWidth() ||
		    Anchor.Y + Size.Y > Dimensions.GetHeight())
		{
			return false;
		}
		for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
			{
				if (ReservedCells.Contains(FCityCellCoord(X, Y)))
				{
					return false;
				}
			}
		}
		return true;
	}
} // namespace

bool FCityBenchmark::ParseCommandLine(const TCHAR* CommandLine, FCityBenchmarkConfig& OutConfig, FString& OutError)
{
	OutConfig = FCityBenchmarkConfig();
	FParse::Value(CommandLine, TEXT("CityBenchmarkSave="), OutConfig.SavedCityPath);
	const bool bLots = FParse::Value(CommandLine, TEXT("CityBenchmarkLots="), OutConfig.RequestedLots);
	if (!bLots && OutConfig.SavedCityPath.IsEmpty())
	{
		return true;
	}
	FParse::Value(CommandLine, TEXT("CityBenchmarkRoads="), OutConfig.RequestedRoadCells);
	FParse::Value(CommandLine, TEXT("CityBenchmarkVehicles="), OutConfig.RequestedVehicles);
	FParse::Value(CommandLine, TEXT("CityBenchmarkSpeed="), OutConfig.Speed);
	FParse::Value(CommandLine, TEXT("CityBenchmarkWeather="), OutConfig.Weather);
	OutConfig.bPopulated = FParse::Param(CommandLine, TEXT("CityBenchmarkPopulated"));
	OutConfig.bLateGame = FParse::Param(CommandLine, TEXT("CityBenchmarkLateGame"));
	OutConfig.bReferenceServiceReuse = FParse::Param(CommandLine, TEXT("CityBenchmarkReferenceServiceReuse"));
	OutConfig.bReferenceCare = FParse::Param(CommandLine, TEXT("CityBenchmarkReferenceCare"));
	if (OutConfig.bReferenceCare &&
	    (!OutConfig.bPopulated || !OutConfig.SavedCityPath.IsEmpty() || OutConfig.Speed <= 0))
	{
		OutError = TEXT("Care control requires a running generated populated benchmark");
		return false;
	}
	if (OutConfig.bReferenceServiceReuse &&
	    (!OutConfig.bPopulated || !OutConfig.SavedCityPath.IsEmpty() || OutConfig.Speed <= 0))
	{
		OutError = TEXT("Service reuse control requires a running generated populated benchmark");
		return false;
	}
	if (OutConfig.bLateGame && (!OutConfig.bPopulated || !OutConfig.SavedCityPath.IsEmpty()))
	{
		OutError = TEXT("Late-game fixture requires a generated populated benchmark");
		return false;
	}
	FParse::Value(CommandLine, TEXT("CityBenchmarkCitizenRoutes="), OutConfig.CitizenRouteBudget);
	FParse::Value(CommandLine, TEXT("CityBenchmarkVisibleTraffic="), OutConfig.VisibleTraffic);
	if (OutConfig.CitizenRouteBudget < 1 || OutConfig.CitizenRouteBudget > 2048 || OutConfig.VisibleTraffic < 1 ||
	    OutConfig.VisibleTraffic > 2048)
	{
		OutError = TEXT("Citizen route budget and visible traffic must be 1-2048");
		return false;
	}
	if (!OutConfig.SavedCityPath.IsEmpty())
	{
		if (bLots || OutConfig.bPopulated ||
		    (OutConfig.Speed != 0 && OutConfig.Speed != 1 && OutConfig.Speed != 3 && OutConfig.Speed != 5) ||
		    (OutConfig.Weather != TEXT("Natural") && OutConfig.Weather != TEXT("Clear") &&
		     OutConfig.Weather != TEXT("Rain")))
		{
			OutError = TEXT("Saved-city benchmark needs no lot override, speed 0/1/3/5 and Natural/Clear/Rain weather");
			return false;
		}
		return true;
	}
	if (OutConfig.bPopulated &&
	    ((OutConfig.Speed != 0 && OutConfig.Speed != 1 && OutConfig.Speed != 3 && OutConfig.Speed != 5) ||
	     (OutConfig.Weather != TEXT("Natural") && OutConfig.Weather != TEXT("Clear") &&
	      OutConfig.Weather != TEXT("Rain"))))
	{
		OutError = TEXT("Populated benchmark needs speed 0/1/3/5 and Natural/Clear/Rain weather");
		return false;
	}
	if (OutConfig.RequestedLots <= 0 || OutConfig.RequestedLots > MaxBenchmarkLots ||
	    OutConfig.RequestedRoadCells < 0 || OutConfig.RequestedRoadCells > MaxBenchmarkRoadCells ||
	    OutConfig.RequestedVehicles < 0 || OutConfig.RequestedVehicles > MaxBenchmarkVehicles)
	{
		OutError =
		    FString::Printf(TEXT("Benchmark arguments outside supported ranges: lots 1-%d, roads 0-%d, vehicles 0-%d"),
		                    MaxBenchmarkLots, MaxBenchmarkRoadCells, MaxBenchmarkVehicles);
		return false;
	}
	return true;
}

bool FCityBenchmark::BuildScene(const FCityBenchmarkConfig& Config, const UCityBuildingCatalog& Catalog,
                                FCityPlacementModelState& OutState, FCityBenchmarkResult& OutResult, FString& OutError,
                                const FCityGridDimensions Dimensions)
{
	if (!Dimensions.IsValid())
	{
		OutError = TEXT("Invalid fixture dimensions");
		return false;
	}
	OutState = FCityPlacementModelState();
	OutState.Dimensions = Dimensions;
	OutResult = FCityBenchmarkResult();
	OutResult.Config = Config;
	OutState.Funds = MAX_int64 / 4;
	OutState.Zones.Init(ECityZoneType::None, Dimensions.GetCellCount());

	TSet<FCityCellCoord> ReservedCells;
	if (Config.bPopulated)
	{
		// Dense connected neighbourhoods, not the roadless paused renderer fixture.
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
				if (X == 0 || Y % 5 == 2 || (Config.bLateGame && X % 16 == 0))
				{
					OutState.RoadCells.Add({X, Y});
					if (Config.bLateGame)
						OutState.RoadRecords.Add(
						    {{X, Y}, X % 16 == 0 ? FName(TEXT("Avenue")) : FName(TEXT("LocalRoad"))});
					ReservedCells.Add({X, Y});
				}
	}
	const int32 EntranceY = FMath::Clamp(FCityPlacementModel::EntranceCell.Y, 0, Dimensions.GetHeight() - 1);
	for (int32 RowOffset = 0; !Config.bPopulated && RowOffset < Dimensions.GetHeight() &&
	                          OutState.RoadCells.Num() < Config.RequestedRoadCells;
	     ++RowOffset)
	{
		const int32 Y = (EntranceY + RowOffset) % Dimensions.GetHeight();
		const bool bLeftToRight = (RowOffset % 2) == 0;
		for (int32 Step = 0; Step < Dimensions.GetWidth() && OutState.RoadCells.Num() < Config.RequestedRoadCells;
		     ++Step)
		{
			const int32 X = bLeftToRight ? Step : Dimensions.GetWidth() - 1 - Step;
			const FCityCellCoord Cell(X, Y);
			OutState.RoadCells.Add(Cell);
			ReservedCells.Add(Cell);
		}
	}

	TArray<const FCityBuildingDefinition*> LotDefinitions;
	const TArray<FName> DefinitionIds =
	    Config.bLateGame    ? TArray<FName>{TEXT("Power"),         TEXT("Service"),    TEXT("Home"),
	                                        TEXT("Shop"),          TEXT("Home"),       TEXT("Factory"),
	                                        TEXT("WaterTower"),    TEXT("Home"),       TEXT("Shop"),
	                                        TEXT("Power"),         TEXT("Service"),    TEXT("Home"),
	                                        TEXT("Home"),          TEXT("Shop"),       TEXT("Factory"),
	                                        TEXT("Service"),       TEXT("Police"),     TEXT("Home"),
	                                        TEXT("Fire"),          TEXT("Home"),       TEXT("Hospital"),
	                                        TEXT("Home"),          TEXT("WasteDepot"), TEXT("Home"),
	                                        TEXT("Shop"),          TEXT("Recycling"),  TEXT("Service"),
	                                        TEXT("PrimarySchool"), TEXT("Home"),       TEXT("SecondarySchool"),
	                                        TEXT("Home"),          TEXT("Park")}
	    : Config.bPopulated ? TArray<FName>{TEXT("Home"), TEXT("Home"),  TEXT("Shop"),   TEXT("Home"),
	                                        TEXT("Shop"), TEXT("Power"), TEXT("Service")}
	                        : TArray<FName>{TEXT("Home"), TEXT("Shop"), TEXT("Power"), TEXT("Service")};
	for (const FName DefinitionId : DefinitionIds)
	{
		const FCityBuildingDefinition* Definition = Catalog.FindDefinition(DefinitionId);
		if (Definition && Definition->IsValid() &&
		    (Config.bLateGame || Definition->Footprint.GetRotatedSize(0) == FIntPoint(2, 2)))
		{
			LotDefinitions.Add(Definition);
		}
	}
	if (LotDefinitions.IsEmpty())
	{
		OutError = TEXT("Benchmark requires at least one valid 2 x 2 city definition");
		return false;
	}

	uint64 NextEntityId = 1;
	int32 DefinitionCursor = 0;
	for (int32 Row = 0; Row < Dimensions.GetHeight() && OutState.Entities.Num() < Config.RequestedLots; ++Row)
	{
		const int32 Y = Config.bPopulated ? 3 + (Row / 2) * 5 + (Row % 2) * 2 : Row * 2;
		for (int32 X = Config.bPopulated ? 1 : 0;
		     X < Dimensions.GetWidth() && OutState.Entities.Num() < Config.RequestedLots; X += 2)
		{
			FCityCellCoord Anchor(X, Y);
			const FCityBuildingDefinition* Selected =
			    LotDefinitions[Config.bLateGame ? DefinitionCursor % LotDefinitions.Num()
			                                    : OutState.Entities.Num() % LotDefinitions.Num()];
			FIntPoint Footprint = Selected->Footprint.GetRotatedSize(0);
			// Align the actual footprint with its street, including 1x1 towers.
			if (Config.bLateGame && Row % 2 != 0)
				Anchor.Y = Y + 2 - Footprint.Y;
			// Larger civic lots take a whole part of a block. If a boundary/road
			// prevents one, defer it and fill the small gap with actual housing.
			if (Config.bLateGame && !IsFootprintFree(Anchor, Footprint, ReservedCells, Dimensions))
			{
				Selected = Catalog.FindDefinition(TEXT("Home"));
				Footprint = Selected->Footprint.GetRotatedSize(0);
				Anchor.Y = Y;
			}
			if (!IsFootprintFree(Anchor, Footprint, ReservedCells, Dimensions))
			{
				continue;
			}
			const FCityBuildingDefinition& Definition = *Selected;
			if (Selected == LotDefinitions[DefinitionCursor % LotDefinitions.Num()])
				++DefinitionCursor;
			FCityPlacedEntity Entity;
			Entity.EntityId = NextEntityId++;
			Entity.DefinitionId = Definition.DefinitionId;
			Entity.Anchor = Anchor;
			Entity.QuarterTurnsClockwise = Config.bPopulated ? (Row % 2 == 0 ? 0 : 2) : (X / 2 + Y / 2) % 4;
			Entity.FootprintSize = Definition.Footprint.GetRotatedSize(Entity.QuarterTurnsClockwise);
			Entity.RecordedPlacementCost = Definition.Cost;
			OutState.Entities.Add(Entity);
			for (int32 CellY = Anchor.Y; CellY < Anchor.Y + Footprint.Y; ++CellY)
			{
				for (int32 CellX = X; CellX < X + Footprint.X; ++CellX)
				{
					ReservedCells.Add(FCityCellCoord(CellX, CellY));
				}
			}
		}
	}

	OutState.NextEntityId = NextEntityId;
	OutState.Revision = static_cast<uint64>(OutState.Entities.Num() + OutState.RoadCells.Num());
	OutResult.PlacedLots = OutState.Entities.Num();
	OutResult.PlacedRoadCells = OutState.RoadCells.Num();
	OutResult.PlacedVehicles = Config.bPopulated ? 0 : FMath::Min(Config.RequestedVehicles, OutResult.PlacedRoadCells);
	return true;
}

FCityTerrainGenerationParameters FCityBenchmark::CapacityTerrainParameters()
{
	FCityTerrainGenerationParameters Params;
	Params.GeneratorVersion = 2;
	Params.BroadElevationAmplitudeCm = Params.HillAmplitudeCm = Params.RidgeAmplitudeCm = 0;
	for (Params.Seed = 7429; Params.Seed < 8000; ++Params.Seed)
		if (Params.DescribeLandscape().Contains(TEXT("dry landscape")) && Params.GetMapRotation() == 0)
			break;
	return Params;
}
