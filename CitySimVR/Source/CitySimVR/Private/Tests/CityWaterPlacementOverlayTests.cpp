#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityWaterPlacementOverlay.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityGrowthSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterPlacementOverlayDataTest,
                                 "CitySim.Water.PlacementGuidanceSharedDataAndLayers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterPlacementOverlayDataTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.WaterRulesVersion = 1;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	auto Road = [&](FCityCellCoord A, FCityCellCoord B)
	{
		auto P = City.BeginRoadPreview(A);
		City.UpdateRoadPreviewEnd(P, B);
		return City.CommitRoad(P).IsSuccess();
	};
	TestTrue(TEXT("Entrance pipe fixture"), Road(City.GetEntranceCell(), {2, 8}));
	auto Span = City.BeginRoadPreview({2, 8});
	Span.End = {16, 8};
	Span.Profile = ECityRoadProfile::Elevated;
	if (!TestTrue(TEXT("Raised pipe fixture"), City.CommitRoad(Span).IsSuccess()))
		return false;
	TestTrue(TEXT("Exit pipe fixture"), Road({16, 8}, {20, 8}));
	TestTrue(TEXT("Independent unconnected ground underpass fixture"), Road({8, 4}, {8, 12}));
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = TEXT("Service");
	Definition.DisplayName = TEXT("Water source");
	Definition.Footprint = FCityCellFootprint(1, 1);
	auto Preview = City.BeginPreview(Definition, {0, 9});
	if (!TestTrue(TEXT("Operational source placed"), City.CommitPlacement(Preview, Definition).IsSuccess()))
		return false;
	const auto Services = FCityServiceEvaluator::BuildSnapshot(City, FCityEconomyTuning());
	const auto Revision = City.GetRevision();
	const auto Funds = City.GetFunds();
	const auto Heights = Terrain.GetQuantizedHeightsCm();
	const auto RoadPressure = Services.Water.RoadPressurePercent;
	const int32 SourceUsed = Services.Water.Sources[0].Used;
	const auto Result = FCityWaterPlacementOverlay::Build(City, Terrain, Services);
	TestEqual(TEXT("Bounded site field covers exact city grid"), Result.GroundwaterYieldPercent.Num(),
	          FCityGrid::CellCount);
	for (const auto& Cell : Result.LandCells)
		TestEqual(TEXT("Placement site value comes from the exact source-yield query"),
		          Result.GroundwaterYieldPercent[Cell.Y * FCityGrid::Width + Cell.X],
		          Terrain.GetGroundwaterYieldPercent(Cell));
	TestEqual(TEXT("Every real ground/raised road included, including dry roads"), Result.Roads.Num(),
	          City.GetRoadCells().Num());
	for (int32 I = 0; I < Result.Roads.Num(); ++I)
	{
		const auto& Sample = Result.Roads[I];
		TestEqual(TEXT("Existing pipe pressure copied exactly"), Sample.PressurePercent,
		          Services.Water.RoadPressurePercent.FindRef(Sample.Cell));
		if (I > 0)
		{
			const auto& A = Result.Roads[I - 1].Cell;
			const auto& B = Sample.Cell;
			TestTrue(TEXT("Full road addresses have deterministic row/column/level order"),
			         A.Y < B.Y || (A.Y == B.Y && (A.X < B.X || (A.X == B.X && A.RoadLevel < B.RoadLevel))));
		}
	}
	const auto* Deck = Result.Roads.FindByPredicate([](const auto& R) { return R.Cell == FCityCellCoord(8, 8, 1); });
	const auto* Ground = Result.Roads.FindByPredicate([](const auto& R) { return R.Cell == FCityCellCoord(8, 8, 0); });
	if (TestNotNull(TEXT("Raised road sample"), Deck) && TestNotNull(TEXT("Separate ground sample"), Ground))
	{
		TestTrue(TEXT("Existing raised pipe carries pressure"), Deck->PressurePercent > 0);
		TestEqual(TEXT("Unconnected ground pipe stays zero under supplied deck"), Ground->PressurePercent, 0);
		TestTrue(TEXT("Road label explicitly identifies existing raised pressure"),
		         FCityWaterPlacementOverlay::DescribeRoad(*Deck).Contains(TEXT("Existing raised pipe")));
		TestTrue(TEXT("Ground label identifies road disconnection"),
		         FCityWaterPlacementOverlay::DescribeRoad(*Ground).Contains(TEXT("disconnected")));
	}
	TestEqual(TEXT("No placement-model mutation"), City.GetRevision(), Revision);
	TestEqual(TEXT("No spending"), City.GetFunds(), Funds);
	TestTrue(TEXT("No terrain mutation"), Terrain.GetQuantizedHeightsCm() == Heights);
	TestEqual(TEXT("No source budget consumption"), Services.Water.Sources[0].Used, SourceUsed);
	TestEqual(TEXT("No pressure-address mutation"), Services.Water.RoadPressurePercent.Num(), RoadPressure.Num());
	for (const auto& Pair : RoadPressure)
		TestTrue(TEXT("No pressure mutation"), Services.Water.RoadPressurePercent.Contains(Pair.Key) &&
		                                           Services.Water.RoadPressurePercent.FindRef(Pair.Key) == Pair.Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterPlacementOverlayLegendTest,
                                 "CitySim.Water.PlacementGradientsLegacyAndWaterMask",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterPlacementOverlayLegendTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.GeneratorVersion = 2;
	Config.WaterRulesVersion = 1;
	Config.Seed = 6;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	FCityServiceSnapshot Services;
	Services.Water.RulesVersion = 1;
	const auto Result = FCityWaterPlacementOverlay::Build(City, Terrain, Services);
	int32 WaterCells = 0;
	for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
		for (int32 X = 0; X < FCityGrid::Width; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (Terrain.IsWaterCell(Cell))
			{
				++WaterCells;
				TestEqual(TEXT("Visible surface water excluded from land-source gradient"),
				          Result.GroundwaterYieldPercent[Y * FCityGrid::Width + X], INDEX_NONE);
				TestEqual(TEXT("Surface water has transparent guidance color"), Result.GroundwaterColorAt(Cell).A, 0.f);
			}
		}
	TestTrue(TEXT("Representative organic terrain has a visible water mask"), WaterCells > 0);
	TestTrue(TEXT("Legend separates site yield from existing pressure"),
	         Result.Legend.Contains(TEXT("Site groundwater yield")) &&
	             Result.Legend.Contains(TEXT("Existing pipe pressure")));
	TestTrue(TEXT("Site text reports groundwater rather than proposed supply"),
	         Result.DescribeSite(Config.GetEntranceCell()).Contains(TEXT("groundwater yield")));
	Services.Water.RulesVersion = 0;
	const auto Legacy = FCityWaterPlacementOverlay::Build(City, Terrain, Services);
	for (const auto& Cell : Legacy.LandCells)
		TestEqual(TEXT("Legacy guidance uses actual flat legacy yield even on varied terrain"),
		          Legacy.GroundwaterYieldPercent[Cell.Y * FCityGrid::Width + Cell.X], 100);
	TestTrue(TEXT("Legacy site label explains rules"),
	         Legacy.DescribeSite(Config.GetEntranceCell()).Contains(TEXT("legacy rules")));
	const auto Low = FCityWaterPlacementOverlay::GroundwaterColor(60),
	           Mid = FCityWaterPlacementOverlay::GroundwaterColor(100),
	           High = FCityWaterPlacementOverlay::GroundwaterColor(140);
	TestTrue(TEXT("Site yield gradient red/amber/cyan"),
	         Low.R > Low.G && Mid.G > Low.G && High.B > High.R && High.G > High.R);
	const auto Dry = FCityWaterPlacementOverlay::PressureColor(0),
	           Threshold = FCityWaterPlacementOverlay::PressureColor(20),
	           Full = FCityWaterPlacementOverlay::PressureColor(60);
	TestTrue(TEXT("Pressure gradient red/orange/blue"), Dry.R > Dry.B && Threshold.G > Dry.G && Full.B > Full.R);
	TestFalse(TEXT("Pressure is a gradient rather than a two-color classification"),
	          FCityWaterPlacementOverlay::PressureColor(31).Equals(FCityWaterPlacementOverlay::PressureColor(32)));
	TestEqual(TEXT("Invalid site lookup is transparent"), Result.GroundwaterColorAt({-1, 8}).A, 0.f);
	return true;
}
#endif
