#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
FCityBuildingDefinition WaterDefinition(FName Name)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Name;
	D.DisplayName = Name.ToString();
	D.Footprint = FCityCellFootprint(1, 1);
	D.LogicalHeightCm = 100;
	return D;
}
uint64 WaterPlace(FCityPlacementModel& City, FName Name, FCityCellCoord Cell)
{
	const auto D = WaterDefinition(Name);
	auto P = City.BeginPreview(D, Cell);
	return City.CommitPlacement(P, D).EntityId;
}
void WaterRoad(FCityPlacementModel& City, FCityCellCoord A, FCityCellCoord B)
{
	auto P = City.BeginRoadPreview(A);
	City.UpdateRoadPreviewEnd(P, B);
	City.CommitRoad(P);
}
FCityWaterSource WaterSource(uint64 Id, int32 Capacity = 200, int32 Pressure = 100)
{
	FCityWaterSource S;
	S.EntityId = Id;
	S.Capacity = Capacity;
	S.SourcePressurePercent = Pressure;
	S.bOperational = true;
	return S;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterPressureTest, "CitySim.Water.PressureThresholdAndDetours",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterPressureTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	WaterRoad(City, City.GetEntranceCell(), {63, 8});
	const uint64 Id = WaterPlace(City, TEXT("Service"), {0, 9});
	FCityWaterSnapshot W;
	W.RulesVersion = 1;
	W.Sources.Add(WaterSource(Id, 1000));
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	for (int32 X = 0; X <= 50; ++X)
		TestEqual(TEXT("Monotonic loss follows exact connected-road distance"),
		          W.RoadPressurePercent.FindRef({X, 8}), 100 - X * 2);
	TestEqual(TEXT("Healthy boundary retains full delivery"),
	          FCityWaterEvaluator::Allocate(City, W, 100, {20, 9}, {1, 1}, 60), 60);
	TestEqual(TEXT("Low usable pressure delivers partial demand"),
	          FCityWaterEvaluator::Allocate(City, W, 101, {30, 9}, {1, 1}, 60), 40);
	TestEqual(TEXT("Partial shortage identifies pressure"), W.EntityCauses.FindRef(101),
	          ECityWaterFailure::LowPressure);
	TestEqual(TEXT("Threshold20 is inclusive"),
	          FCityWaterEvaluator::Allocate(City, W, 102, {40, 9}, {1, 1}, 60), 20);
	TestEqual(TEXT("One hop beyond threshold gives no usable water"),
	          FCityWaterEvaluator::Allocate(City, W, 103, {41, 9}, {1, 1}, 60), 0);
	TestEqual(TEXT("Far connected branch reports pressure rather than disconnect"),
	          W.EntityCauses.FindRef(103), ECityWaterFailure::LowPressure);
	TestEqual(TEXT("Beyond zero-pressure frontier still diagnosed as pressure"),
	          FCityWaterEvaluator::Allocate(City, W, 104, {62, 9}, {1, 1}, 60), 0);
	TestEqual(TEXT("Beyond frontier diagnostic"), W.EntityCauses.FindRef(104),
	          ECityWaterFailure::LowPressure);
	FCityPlacementModel Detour(100000);
	WaterRoad(Detour, Detour.GetEntranceCell(), {25, 8});
	WaterRoad(Detour, {25, 8}, {25, 12});
	WaterRoad(Detour, {25, 12}, {1, 12});
	FCityWaterSnapshot D;
	D.RulesVersion = 1;
	D.Sources.Add(WaterSource(WaterPlace(Detour, TEXT("Service"), {0, 9})));
	FCityWaterEvaluator::BuildNetwork(Detour, D, 32);
	TestEqual(TEXT("Nearby geometric site cannot bypass long road detour"),
	          FCityWaterEvaluator::GetPressure(Detour, D, {1, 13}, {1, 1}), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterConservationTest,
                                 "CitySim.Water.MultipleSourcesLoopsAndConservation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterConservationTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	WaterRoad(City, City.GetEntranceCell(), {30, 8});
	WaterRoad(City, {5, 8}, {5, 12});
	WaterRoad(City, {5, 12}, {15, 12});
	WaterRoad(City, {15, 8}, {15, 12});
	const uint64 A = WaterPlace(City, TEXT("Service"), {0, 9}),
	             B = WaterPlace(City, TEXT("WaterTower"), {20, 9});
	FCityWaterSnapshot W;
	W.RulesVersion = 1;
	W.Sources.Add(WaterSource(A, 40));
	W.Sources.Add(WaterSource(B, 50, 120));
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	TestEqual(TEXT("Loop retains shortest source path"), W.Sources[0].RoadDistances.FindRef({10, 12}), 14);
	TestEqual(TEXT("Best source selects maximum pressure without adding pressures"),
	          FCityWaterEvaluator::GetPressure(City, W, {10, 9}, {1, 1}), 100);
	TestEqual(TEXT("Demand can draw finite capacities of both reachable sources"),
	          FCityWaterEvaluator::Allocate(City, W, 100, {10, 9}, {1, 1}, 70), 70);
	const int32 FirstSourceUsed = W.Sources[0].Used, SecondSourceUsed = W.Sources[1].Used;
	TestEqual(TEXT("Remaining demand receives only remaining20"),
	          FCityWaterEvaluator::Allocate(City, W, 101, {10, 9}, {1, 1}, 30), 20);
	TestEqual(TEXT("Capacity shortage is distinct from pressure"), W.EntityCauses.FindRef(101),
	          ECityWaterFailure::SourceCapacity);
	TestEqual(TEXT("Sources consumed exactly90, loops create no supply"),
	          W.Sources[0].Used + W.Sources[1].Used, 90);
	TestEqual(TEXT("Exhausted source cannot give duplicate supply"),
	          FCityWaterEvaluator::Allocate(City, W, 102, {10, 9}, {1, 1}, 30), 0);
	TestEqual(TEXT("Capacity-starved building displays the same physical head as its pipe"),
	          W.EntityPressurePercent.FindRef(102), W.RoadPressurePercent.FindRef({10, 8}));
	TestEqual(TEXT("Capacity-starved building still reports capacity rather than lost pressure"),
	          W.EntityCauses.FindRef(102), ECityWaterFailure::SourceCapacity);
	FCityWaterSnapshot Reverse;
	Reverse.RulesVersion = 1;
	Reverse.Sources.Add(WaterSource(B, 50, 120));
	Reverse.Sources.Add(WaterSource(A, 40));
	FCityWaterEvaluator::BuildNetwork(City, Reverse, 32);
	TestEqual(TEXT("Source array order cannot alter equal-distance delivery"),
	          FCityWaterEvaluator::Allocate(City, Reverse, 100, {10, 9}, {1, 1}, 70), 70);
	TestEqual(TEXT("Source ID order stays deterministic for the same first request"), Reverse.Sources[0].Used, SecondSourceUsed);
	TestEqual(TEXT("Both source budgets match for the same first request"), Reverse.Sources[1].Used, FirstSourceUsed);
	FCityWaterSnapshot Fallback;
	Fallback.RulesVersion = 1;
	Fallback.Sources.Add(WaterSource(A, 1));
	Fallback.Sources.Add(WaterSource(B, 100, 20));
	FCityWaterEvaluator::BuildNetwork(City, Fallback, 32);
	TestEqual(TEXT("Strong pressure cannot let a weak source ignore its own flow limit"),
	          FCityWaterEvaluator::Allocate(City, Fallback, 300, {20, 9}, {1, 1}, 60), 21);
	TestEqual(TEXT("Exhausted strong source no longer claims available high pressure"),
	          FCityWaterEvaluator::GetPressure(City, Fallback, {20, 9}, {1, 1}), 20);
	TestEqual(TEXT("Fallback retains its own partial delivery"),
	          FCityWaterEvaluator::Allocate(City, Fallback, 301, {20, 9}, {1, 1}, 60), 20);
	City.RemoveRoad({8, 8});
	City.RemoveRoad({8, 12});
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	TestFalse(TEXT("Disconnected source capacity cannot flow to other branch"),
	          W.Sources[1].RoadDistances.Contains({0, 8}));
	TestEqual(TEXT("Outage is identifiable on remaining connected provider"),
	          FCityWaterEvaluator::Allocate(City, W, 200, {0, 9}, {1, 1}, 60), 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterSourceGatesTest,
                                 "CitySim.Water.SourceGatesCriticalPriorityAndLegacy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterSourceGatesTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	WaterRoad(City, City.GetEntranceCell(), {63, 8});
	WaterPlace(City, TEXT("Power"), {2, 9});
	const uint64 Water = WaterPlace(City, TEXT("Service"), {0, 9});
	const uint64 Home = WaterPlace(City, TEXT("Home"), {4, 9}),
	             Hospital = WaterPlace(City, TEXT("Hospital"), {6, 9});
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	for (FName Name :
	     {FName(TEXT("Service")), FName(TEXT("Power")), FName(TEXT("Home")), FName(TEXT("Hospital"))})
	{
		auto D = WaterDefinition(Name);
		if (Name == TEXT("Hospital"))
		{
			D.WaterDemand = 10;
			D.PowerDemand = 0;
		}
		Catalog->Definitions.Add(D);
	}
	FCityEconomyTuning T;
	T.WaterCapacityPerTreatment = 15;
	T.ResidentsPerOccupiedHome = 10;
	auto S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr, nullptr,
	                                              nullptr, 1);
	TestEqual(TEXT("Critical service receives water before earlier home ID"),
	          S.Water.EntityDelivered.FindRef(Hospital), 10);
	TestEqual(TEXT("Home receives only remaining supply"), S.Water.EntityDelivered.FindRef(Home), 5);
	TestEqual(TEXT("Total delivery exactly equals finite supply"), S.ServiceUsed, 15);
	TestEqual(TEXT("Exhausted nearby source blocks new growth by capacity, not distance"),
	          FCityServiceEvaluator::EvaluateCandidate(City, S, T, TEXT("Home"), {10, 9}, {1, 1}, Catalog),
	          ECityOperationFailure::ServiceCapacity);
	TestEqual(TEXT("Candidate diagnosis does not mutate source allocation"), S.Water.Sources[0].Used, 15);
	TestEqual(TEXT("Beyond threshold candidate identifies road-distance pressure"),
	          FCityServiceEvaluator::EvaluateCandidate(City, S, T, TEXT("Home"), {45, 9}, {1, 1}, Catalog),
	          ECityOperationFailure::LowWaterPressure);
	FCityFireSnapshot Fire;
	Fire.DamagedBuildings.Add(Water, {100, -1, 1});
	S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr, &Fire, nullptr, 1);
	TestEqual(TEXT("Damage removes all source capacity"), S.ServiceCapacity, 0);
	TestEqual(TEXT("Offline source explains outage"), S.Water.EntityCauses.FindRef(Home),
	          ECityWaterFailure::SourceOutage);
	Fire.DamagedBuildings.Reset();
	TestEqual(
	    TEXT("Recovery restores capacity"),
	    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr, &Fire, nullptr, 1)
	        .ServiceCapacity,
	    15);
	const uint64 Far = WaterPlace(City, TEXT("Home"), {35, 9});
	T.WaterCapacityPerTreatment = 1000;
	T.WaterRangeRoadHops = 32;
	const auto Legacy = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr,
	                                                         nullptr, nullptr, 0);
	const auto Refined = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, nullptr, nullptr, nullptr,
	                                                          nullptr, nullptr, 1);
	TestEqual(TEXT("Legacy range remains exact"), Legacy.Water.EntityDelivered.FindRef(Far), 0);
	TestTrue(TEXT("New pressure rules allow some beyond legacy hop32"),
	         Refined.Water.EntityDelivered.FindRef(Far) > 0);
	TestEqual(TEXT("Legacy does not acquire groundwater modifier"), Legacy.Water.Sources[0].YieldPercent,
	          100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterRaisedNetworkTest, "CitySim.Water.RaisedPipeContinuity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterRaisedNetworkTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	WaterRoad(City, City.GetEntranceCell(), {2, 8});
	auto Span = City.BeginRoadPreview({2, 8});
	Span.End = {16, 8};
	Span.Profile = ECityRoadProfile::Elevated;
	if (!TestTrue(TEXT("Raised water corridor fixture commits"), City.CommitRoad(Span).IsSuccess()))
		return false;
	WaterRoad(City, {16, 8}, {20, 8});
	const uint64 Source = WaterPlace(City, TEXT("Service"), {0, 9});
	FCityWaterSnapshot W;
	W.RulesVersion = 1;
	W.Sources.Add(WaterSource(Source));
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	TestTrue(TEXT("Water passes from ground endpoint onto deck"), W.RoadPressurePercent.Contains({8, 8, 1}));
	TestFalse(TEXT("Deck never invents a ground pipe underneath"), W.RoadPressurePercent.Contains({8, 8, 0}));
	TestEqual(TEXT("Bridge distance counts path cells without added pressure"),
	          W.RoadPressurePercent.FindRef({20, 8}), 60);
	TestEqual(TEXT("Far ground building supplied through bridge"),
	          FCityWaterEvaluator::Allocate(City, W, 100, {20, 9}, {1, 1}, 10), 10);
	WaterRoad(City, {8, 4}, {8, 12});
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	TestFalse(TEXT("Unconnected ground crossing does not acquire pressure from deck"),
	          W.RoadPressurePercent.Contains({8, 8, 0}));
	WaterRoad(City, {0, 8}, {0, 4});
	WaterRoad(City, {0, 4}, {8, 4});
	FCityWaterEvaluator::BuildNetwork(City, W, 32);
	TestEqual(TEXT("Connected underpass pipe travels its own ground route"),
	          W.Sources[0].RoadDistances.FindRef({8, 8, 0}), 16);
	TestEqual(TEXT("Independent deck pipe keeps its original distance"),
	          W.Sources[0].RoadDistances.FindRef({8, 8, 1}), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterStaffPowerUpgradeTest,
                                 "CitySim.Water.QualifiedPayrollPowerAndUpgradeGates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterStaffPowerUpgradeTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	WaterRoad(City, City.GetEntranceCell(), {20, 8});
	const uint64 Source = WaterPlace(City, TEXT("Service"), {0, 9}),
	             Power = WaterPlace(City, TEXT("Power"), {5, 9});
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	auto D = WaterDefinition(TEXT("Service"));
	D.JobsProvided = 2;
	D.MinimumStaffEducation = 1;
	D.bPlayerPurchasedLevels = true;
	for (int32 Stage = 0; Stage < 3; ++Stage)
	{
		FCityBuildingLevelDefinition L;
		L.Level = static_cast<ECityDensityLevel>(Stage);
		L.JobsProvided = 2 * (Stage + 1);
		L.UtilitySupplyPercent = 100 * (Stage + 1);
		L.PowerDemand = 7;
		L.UpgradeCost = Stage * 100;
		L.CivicConstructionSteps = 5;
		D.DensityLevels.Add(L);
	}
	Catalog->Definitions.Add(D);
	Catalog->Definitions.Add(WaterDefinition(TEXT("Power")));
	FCityDevelopmentSnapshot Development;
	auto& State = Development.States.Add(Source);
	State.Stage = ECityDevelopmentStage::Operational;
	FCityPopulationSnapshot Population;
	Population.PublicEmployers.Add({Source, 2, 20, 20});
	for (int32 I = 0; I < 2; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I + 1;
		C.WorkplaceEntityId = Source;
		C.Education = ECityEducationLevel::Basic;
		Population.Citizens.Add(C);
	}
	FCityFireSnapshot Fire;
	FCityEconomyTuning T;
	auto Build = [&]() {
		return FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Development, nullptr, &Population,
		                                            &Fire, nullptr, 1);
	};
	TestEqual(TEXT("Qualified paid starter supplies full capacity"), Build().ServiceCapacity, 200);
	TestEqual(TEXT("Source power is reserved once"), Build().PowerUsed, 7);
	Population.PublicEmployers[0].PayrollPaid = 10;
	TestEqual(TEXT("Half-paid payroll preserves existing half-output floor"), Build().ServiceCapacity, 100);
	Population.PublicEmployers[0].PayrollPaid = 20;
	for (auto& Citizen : Population.Citizens)
		Citizen.Education = ECityEducationLevel::None;
	TestEqual(TEXT("Unqualified workers cannot boost source past bootstrap floor"), Build().ServiceCapacity,
	          100);
	for (auto& Citizen : Population.Citizens)
		Citizen.Education = ECityEducationLevel::Basic;
	State.DensityLevel = ECityDensityLevel::Medium;
	TestEqual(TEXT("Completed upgrade capacity still scales required staffing"), Build().ServiceCapacity,
	          200);
	for (int32 I = 2; I < 4; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I + 1;
		C.WorkplaceEntityId = Source;
		C.Education = ECityEducationLevel::Basic;
		Population.Citizens.Add(C);
	}
	Population.PublicEmployers[0].JobCapacity = 4;
	Population.PublicEmployers[0].PayrollDue = 40;
	Population.PublicEmployers[0].PayrollPaid = 40;
	TestEqual(TEXT("Fully staffed completed upgrade doubles source budget"), Build().ServiceCapacity, 400);
	State.Stage = ECityDevelopmentStage::Construction;
	TestEqual(TEXT("Construction cannot supply water"), Build().ServiceCapacity, 0);
	State.Stage = ECityDevelopmentStage::Operational;
	Fire.DamagedBuildings.Add(Power, {100, -1, 1});
	const auto Outage = Build();
	TestEqual(TEXT("Power outage disables powered extraction"), Outage.ServiceCapacity, 0);
	TestEqual(TEXT("Source explains power outage"), Outage.GetFailure(Source),
	          ECityOperationFailure::NoPower);
	Fire.DamagedBuildings.Reset();
	TestEqual(TEXT("Restored power restores upgraded supply"), Build().ServiceCapacity, 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterNativeFixtureTest, "CitySim.Water.NativePressurePreviewFixture",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterNativeFixtureTest::RunTest(const FString& Parameters)
{
	const auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Native catalog"), Catalog))
		return false;
	FCityTerrainGenerationParameters Config;
	Config.WaterRulesVersion = 1;
	Config.SceneryVersion = 1;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	FString Error;
	if (!TestTrue(TEXT("Flat native pressure terrain"), Terrain.Generate(Config, &Error)))
	{
		AddError(Error);
		return false;
	}
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	WaterRoad(City, City.GetEntranceCell(), {63, 8});
	WaterRoad(City, {30, 30}, {40, 30});
	auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(P, *D).EntityId;
	};
	const uint64 Source = Place(TEXT("Service"), {0, 9}), Power = Place(TEXT("Power"), {8, 9});
	const uint64 Near = Place(TEXT("Home"), {4, 9}), Medium = Place(TEXT("Home"), {30, 9}),
	             Far = Place(TEXT("Home"), {46, 9}), Island = Place(TEXT("Home"), {34, 31});
	if (!TestTrue(TEXT("Native sources/full/partial/low-pressure/disconnected parcels placed"),
	              Source && Power && Near && Medium && Far && Island))
		return false;
	FCityDevelopmentSimulation Development;
	Development.InitializeExistingAsOperational(City, Catalog, 0);
	FCityPopulationSnapshot Population;
	Population.Households.Add({Near, Near, 1000});
	for (uint64 Id : {Source, Power})
	{
		const auto* Definition = Catalog->FindDefinition(City.GetEntities().FindChecked(Id).DefinitionId);
		const auto* Level = Definition->FindDensityLevel(ECityDensityLevel::Low);
		const int32 Jobs = Level ? Level->JobsProvided : 2;
		Population.PublicEmployers.Add({Id, Jobs, Jobs * 10, Jobs * 10});
		for (int32 I = 0; I < Jobs; ++I)
		{
			FCityCitizenRecord C;
			C.Id = Population.NextCitizenId++;
			C.HouseholdId = Near;
			C.HomeEntityId = Near;
			C.CurrentLocationEntityId = Near;
			C.WorkplaceEntityId = Id;
			C.Education = ECityEducationLevel::Basic;
			C.AvailableMoney = 100;
			Population.Citizens.Add(C);
		}
	}
	FCityEconomyTuning T;
	const auto S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &Development.GetSnapshot(), nullptr,
	                                                    &Population);
	TestEqual(TEXT("Native nearby parcel fully supplied"), S.Water.EntityDelivered.FindRef(Near),
	          S.Water.EntityDemand.FindRef(Near));
	TestTrue(TEXT("Native medium parcel partially supplied"),
	         S.Water.EntityDelivered.FindRef(Medium) > 0 &&
	             S.Water.EntityDelivered.FindRef(Medium) < S.Water.EntityDemand.FindRef(Medium));
	TestEqual(TEXT("Native far parcel lacks usable pressure"), S.Water.EntityDelivered.FindRef(Far), 0);
	TestEqual(TEXT("Native island lacks connected supply"), S.Water.EntityDelivered.FindRef(Island), 0);
	FCitySaveDocument Save;
	Save.Identity.Name = TEXT("Water pressure review");
	Save.Identity.DisasterFrequency = ECityDisasterFrequency::Off;
	Save.Terrain = Config;
	Save.Placement = City.ExportState();
	Save.Development = Development.GetSnapshot();
	Save.Population = Population;
	Save.bSimulationPaused = true;
	Save.NextGrowthStep = 100;
	FString Json;
	if (!TestTrue(TEXT("Native pressure preview document serializes"),
	              FCityPersistence::Serialize(Save, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	TestTrue(TEXT("Native fixture saved"),
	         FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Folder, TEXT("Water-Pressure.json"))));
	return true;
}
#endif
