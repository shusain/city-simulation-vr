#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Simulation/CitySimulationSubsystem.h"

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Performance/CityBenchmark.h"
#include "Simulation/CityBuildingDefinition.h"

namespace
{
	FCityBuildingDefinition BenchmarkDefinition(const FName Id)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(2, 2);
		Definition.Cost = 100;
		Definition.LogicalHeightCm = 800.0;
		return Definition;
	}

	UCityBuildingCatalog* BenchmarkCatalog()
	{
		UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
		Catalog->Definitions = {BenchmarkDefinition(TEXT("Home")), BenchmarkDefinition(TEXT("Shop")),
		                        BenchmarkDefinition(TEXT("Power")), BenchmarkDefinition(TEXT("Service"))};
		return Catalog;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBenchmarkTiersTest, "CitySim.Performance.DeterministicBenchmarkTiers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityBenchmarkTiersTest::RunTest(const FString& Parameters)
{
	UCityBuildingCatalog* Catalog = BenchmarkCatalog();
	for (const int32 RequestedLots : {100, 500, 1000})
	{
		FCityBenchmarkConfig Config;
		Config.RequestedLots = RequestedLots;
		Config.RequestedRoadCells = 64;
		Config.RequestedVehicles = 32;
		FCityPlacementModelState FirstState;
		FCityBenchmarkResult FirstResult;
		FString Error;
		TestTrue(FString::Printf(TEXT("%d-lot scene builds"), RequestedLots),
		         FCityBenchmark::BuildScene(Config, *Catalog, FirstState, FirstResult, Error));
		TestEqual(TEXT("road target is met"), FirstResult.PlacedRoadCells, 64);
		TestEqual(TEXT("vehicle target is bounded by roads"), FirstResult.PlacedVehicles, 32);
		TestEqual(TEXT("requested lots place until deterministic capacity"), FirstResult.PlacedLots,
		          FMath::Min(RequestedLots, 992));

		TMap<FName, FCityBuildingDefinition> Definitions;
		for (const FCityBuildingDefinition& Definition : Catalog->Definitions)
		{
			Definitions.Add(Definition.DefinitionId, Definition);
		}
		TUniquePtr<FCityPlacementModel> Model =
		    FCityPlacementModel::CreateValidatedFromState(FirstState, Definitions, Error);
		TestNotNull(TEXT("generated state passes authoritative validation"), Model.Get());

		FCityPlacementModelState SecondState;
		FCityBenchmarkResult SecondResult;
		TestTrue(TEXT("repeat scene builds"),
		         FCityBenchmark::BuildScene(Config, *Catalog, SecondState, SecondResult, Error));
		TestEqual(TEXT("repeat entity count matches"), SecondState.Entities.Num(), FirstState.Entities.Num());
		TestEqual(TEXT("repeat road count matches"), SecondState.RoadCells.Num(), FirstState.RoadCells.Num());
		for (int32 Index = 0; Index < FirstState.Entities.Num(); ++Index)
		{
			TestEqual(TEXT("entity ID is deterministic"), SecondState.Entities[Index].EntityId,
			          FirstState.Entities[Index].EntityId);
			TestEqual(TEXT("entity anchor is deterministic"), SecondState.Entities[Index].Anchor,
			          FirstState.Entities[Index].Anchor);
			TestEqual(TEXT("entity content is deterministic"), SecondState.Entities[Index].DefinitionId,
			          FirstState.Entities[Index].DefinitionId);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBenchmarkArgumentsTest, "CitySim.Performance.BenchmarkArguments",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityBenchmarkArgumentsTest::RunTest(const FString& Parameters)
{
	FCityBenchmarkConfig Config;
	FString Error;
	TestTrue(TEXT("no benchmark argument leaves normal play enabled"),
	         FCityBenchmark::ParseCommandLine(TEXT("-log"), Config, Error));
	TestFalse(TEXT("normal play does not enable benchmark"), Config.IsEnabled());
	TestTrue(TEXT("Raw reuse control leaves normal play enabled"),
	         FCityBenchmark::ParseCommandLine(TEXT("-CityBenchmarkReferenceServiceReuse"), Config, Error));
	TestFalse(TEXT("Raw control cannot affect normal gameplay"), Config.bReferenceServiceReuse);
	TestTrue(
	    TEXT("Running generated reuse control parses"),
	    FCityBenchmark::ParseCommandLine(
	        TEXT(
	            "-CityBenchmarkLots=100 -CityBenchmarkPopulated -CityBenchmarkSpeed=1 -CityBenchmarkReferenceServiceReuse"),
	        Config, Error));
	TestTrue(TEXT("Generated control is recorded"), Config.bReferenceServiceReuse);
	TestTrue(TEXT("Raw care control leaves normal play enabled"),
	         FCityBenchmark::ParseCommandLine(TEXT("-CityBenchmarkReferenceCare"), Config, Error));
	TestFalse(TEXT("Raw care control cannot affect normal play"), Config.bReferenceCare);
	TestTrue(
	    TEXT("Running generated care control parses"),
	    FCityBenchmark::ParseCommandLine(
	        TEXT("-CityBenchmarkLots=100 -CityBenchmarkPopulated -CityBenchmarkSpeed=5 -CityBenchmarkReferenceCare"),
	        Config, Error));
	TestTrue(TEXT("Generated care control is recorded"), Config.bReferenceCare);
	TestTrue(TEXT("all benchmark arguments parse"),
	         FCityBenchmark::ParseCommandLine(
	             TEXT("-CityBenchmarkLots=500 -CityBenchmarkRoads=80 -CityBenchmarkVehicles=40"), Config, Error));
	TestEqual(TEXT("lot argument"), Config.RequestedLots, 500);
	TestEqual(TEXT("road argument"), Config.RequestedRoadCells, 80);
	TestEqual(TEXT("vehicle argument"), Config.RequestedVehicles, 40);
	TestFalse(TEXT("unsupported lot target is rejected"),
	          FCityBenchmark::ParseCommandLine(TEXT("-CityBenchmarkLots=1001"), Config, Error));
	TestTrue(
	    TEXT("Saved-city profile parses independently of synthetic lots"),
	    FCityBenchmark::ParseCommandLine(
	        TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkSpeed=5 -CityBenchmarkWeather=Rain"), Config, Error));
	TestTrue(TEXT("Saved city enables benchmark"),
	         Config.IsEnabled() && Config.RequestedLots == 0 && Config.Speed == 5);
	for (const TCHAR* Invalid :
	     {TEXT("-CityBenchmarkLots=100 -CityBenchmarkPopulated -CityBenchmarkSpeed=2"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkCitizenRoutes=2049"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkVisibleTraffic=0"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkPopulated"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkLots=100"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkSpeed=2"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkWeather=Unknown"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkReferenceServiceReuse"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkReferenceServiceReuse"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkPopulated -CityBenchmarkReferenceServiceReuse"),
	      TEXT("-CityBenchmarkSave=fixture.json -CityBenchmarkReferenceCare"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkReferenceCare"),
	      TEXT("-CityBenchmarkLots=100 -CityBenchmarkPopulated -CityBenchmarkReferenceCare")})
		TestFalse(TEXT("Ambiguous or unsupported saved-city settings are rejected"),
		          FCityBenchmark::ParseCommandLine(Invalid, Config, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySavedBenchmarkTest, "CitySim.Performance.SavedCityIsolationClockAndTiming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySavedBenchmarkTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* SourceWorld =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* BenchWorld =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Source = SourceWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Bench = BenchWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Source->RegisterBuildingCatalog(Catalog);
	Bench->RegisterBuildingCatalog(Catalog);
	FCityIdentity Identity;
	Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Identity.Name = TEXT("Saved benchmark regression");
	Identity.DisasterFrequency = ECityDisasterFrequency::Off;
	FCityTerrainGenerationParameters Terrain;
	Terrain.Seed = 7429;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/DesktopSmoke-SavedBenchmark") / Identity.Id;
	FString Error;
	TestTrue(TEXT("Create isolated source city"), Source->CreateCity(Identity, Terrain, Directory, Error));
	auto& City = Source->GetPlacementModel();
	auto Road = City.BeginRoadPreview({0, 8});
	Road.End = {8, 8};
	TestTrue(TEXT("Source has connected road"), City.CommitRoad(Road).IsSuccess());
	struct FSourceLot
	{
		FName Id;
		FCityCellCoord Cell;
	};
	for (const auto& Lot : {FSourceLot{FName(TEXT("Power")), {0, 9}}, FSourceLot{FName(TEXT("Service")), {4, 9}},
	                        FSourceLot{FName(TEXT("Home")), {2, 9}}, FSourceLot{FName(TEXT("Shop")), {2, 6}}})
	{
		const auto* Definition = Catalog->FindDefinition(Lot.Id);
		auto Preview = City.BeginRoadFacingPreview(*Definition, Lot.Cell);
		TestTrue(TEXT("Source uses real catalog placement"), City.CommitPlacement(Preview, *Definition).IsSuccess());
	}
	Source->SetSimulationPaused(false);
	for (int32 Month = 0; Month < 4; ++Month)
		Source->Tick(60);
	Source->SetSimulationPaused(true);
	const FString Path = Directory / TEXT("InputCity.json");
	TestTrue(TEXT("Source city saved through authoritative persistence"), Source->SaveCityToPath(Path, Error));
	FString Before, After;
	FFileHelper::LoadFileToString(Before, *Path);
	FCityBenchmarkConfig Config;
	Config.SavedCityPath = Path;
	Config.Speed = 5;
	Config.Weather = TEXT("Rain");
	TestTrue(TEXT("Saved city is validated and loaded for benchmark"), Bench->InitializeSavedBenchmark(Config, Error));
	TestEqual(TEXT("All original lots are retained"), Bench->GetPlacementModel().GetEntities().Num(),
	          City.GetEntities().Num());
	TestEqual(TEXT("Source population is retained on load"), Bench->GetPopulationSnapshot().Citizens.Num(),
	          Source->GetPopulationSnapshot().Citizens.Num());
	TestTrue(TEXT("Populated city exercises real service loop"), Bench->GetPopulationSnapshot().Citizens.Num() > 0);
	TestTrue(TEXT("Source stays paused while benchmark runs"),
	         Source->IsSimulationPaused() && !Bench->IsSimulationPaused());
	Bench->ResetBenchmarkTimings();
	const int64 MonthBefore = Bench->GetEconomySnapshot().LastBilledMonthId;
	Bench->Tick(13);
	TestTrue(TEXT("Running benchmark reaches a real monthly settlement"),
	         Bench->GetEconomySnapshot().LastBilledMonthId > MonthBefore);
	TestTrue(TEXT("Declared rain holds over a settlement"),
	         Bench->GetEnvironmentSnapshot().bPrecipitation &&
	             Bench->GetEnvironmentSnapshot().Temperature == ECityTemperatureBand::Mild);
	const auto* Timings = Bench->GetBenchmarkTimings();
	TestTrue(TEXT("Tick and monthly-containing tick timing are recorded"),
	         Timings && Timings->Ticks.Count == 1 && Timings->MonthlyTicks.Count == 1 && Timings->Ticks.SumMs > 0);
	FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("Benchmark never rewrites input save"), After, Before);
	const int64 Funds = Bench->GetPlacementModel().GetFunds();
	Config.SavedCityPath += TEXT(".missing");
	TestFalse(TEXT("Missing benchmark save fails without fallback"), Bench->InitializeSavedBenchmark(Config, Error));
	TestEqual(TEXT("Failed load preserves active authority"), Bench->GetPlacementModel().GetFunds(), Funds);
	SourceWorld->DestroyWorld(false);
	BenchWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCapacityLayoutTest, "CitySim.Performance.ConnectedPopulatedCapacityLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCapacityLayoutTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	FCityBenchmarkConfig Config;
	Config.RequestedLots = 1000;
	Config.bPopulated = true;
	FCityPlacementModelState State;
	FCityBenchmarkResult Result;
	FString Error;
	TestTrue(TEXT("Dense neighbourhood builds with real catalog"),
	         FCityBenchmark::BuildScene(Config, *Catalog, State, Result, Error));
	TestEqual(TEXT("Connected roads bound true fixture capacity"), Result.PlacedLots, 744);
	TestEqual(TEXT("Real traffic gets no placeholder benchmark vehicles"), Result.PlacedVehicles, 0);
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& Definition : Catalog->Definitions)
		Definitions.Add(Definition.DefinitionId, Definition);
	auto Model = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	TestNotNull(TEXT("Capacity fixture passes authoritative bounds/overlap validation"), Model.Get());
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("Existing dry terrain generator supports fixture"),
	         Terrain.Generate(FCityBenchmark::CapacityTerrainParameters(), &Error));
	if (Model)
	{
		Model->SetTerrain(&Terrain);
		for (const auto& Pair : Model->GetEntities())
		{
			TestTrue(TEXT("Each lot driveway reaches connected roads"), Model->HasRoadAccess(Pair.Key));
			for (int32 Y = 0; Y < 2; ++Y)
				for (int32 X = 0; X < 2; ++X)
					TestTrue(TEXT("Operational lots use buildable land"),
					         Terrain.IsCellBuildable({Pair.Value.Anchor.X + X, Pair.Value.Anchor.Y + Y}));
		}
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLateGameLayoutTest, "CitySim.Performance.DenseMixedLateGameLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLateGameLayoutTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	FCityBenchmarkConfig Config;
	Config.RequestedLots = 1000;
	Config.bPopulated = Config.bLateGame = true;
	FCityPlacementModelState State;
	FCityBenchmarkResult Result;
	FString Error;
	TestTrue(TEXT("Mixed late-game layout builds"), FCityBenchmark::BuildScene(Config, *Catalog, State, Result, Error));
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& Definition : Catalog->Definitions)
		Definitions.Add(Definition.DefinitionId, Definition);
	auto Model = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	TestNotNull(TEXT("Mixed footprints pass authority validation"), Model.Get());
	FCityTerrainHeightfield Terrain;
	auto TerrainParameters = FCityBenchmark::CapacityTerrainParameters();
	TerrainParameters.WaterRulesVersion = 1;
	TestTrue(TEXT("Dense current-water fixture terrain generates"), Terrain.Generate(TerrainParameters, &Error));
	TSet<FName> Types;
	int32 Developed = State.RoadCells.Num();
	if (Model)
	{
		Model->SetTerrain(&Terrain);
		for (const auto& Pair : Model->GetEntities())
		{
			Types.Add(Pair.Value.DefinitionId);
			Developed += Pair.Value.FootprintSize.X * Pair.Value.FootprintSize.Y;
			TestTrue(TEXT("Mixed lot entrance connects"), Model->HasRoadAccess(Pair.Key));
		}
	}
	for (FName Type : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory")), FName(TEXT("Power")),
	                   FName(TEXT("Service")), FName(TEXT("WaterTower")), FName(TEXT("Police")), FName(TEXT("Fire")),
	                   FName(TEXT("Hospital")), FName(TEXT("WasteDepot")), FName(TEXT("Recycling")),
	                   FName(TEXT("PrimarySchool")), FName(TEXT("Park"))})
		TestTrue(TEXT("Required late-game building family present"), Types.Contains(Type));
	TestTrue(TEXT("Roads and lots cover at least 75 percent of play area"), Developed >= FCityGrid::CellCount * .75);
	return true;
}
#endif
