#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Persistence/CityPersistence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEnvironmentLegacyDigestTest, "CitySim.Dimensions.LegacyEnvironmentDigest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityEnvironmentLegacyDigestTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City;
	FCityEnvironmentSnapshot State;
	for (int32 Y = 0; Y < 64; ++Y)
		for (int32 X = 0; X < 64; ++X)
		{
			const int32 Value = (X * 31 + Y * 7) % 801;
			State.Pollution[Y * 64 + X] = Value;
			State.TotalPollution += Value;
			State.PeakPollution = FMath::Max(State.PeakPollution, Value);
		}
	FCityEnvironmentSimulation Simulation;
	FString Error;
	if (!TestTrue(TEXT("reference pollution field restored"), Simulation.RestoreState(State, 0, Error)))
		return false;
	Simulation.AdvanceToStep(100, City, FCityServiceSnapshot(), FCityEconomyTuning());
	const auto& Result = Simulation.GetSnapshot();
	uint64 Digest = 14695981039346656037ull;
	auto Add = [&](int64 Value)
	{
		for (int32 Shift = 0; Shift < 64; Shift += 8)
			Digest = (Digest ^ static_cast<uint8>(static_cast<uint64>(Value) >> Shift)) * 1099511628211ull;
	};
	for (int32 Value : Result.Pollution)
		Add(Value);
	Add(Result.LastFixedStepId);
	Add(Result.TotalPollution);
	Add(Result.PeakPollution);
	Add(Result.WeatherRandomState);
	Add(Result.NextWeatherChangeStep);
	Add(static_cast<int32>(Result.Season));
	Add(static_cast<int32>(Result.Temperature));
	Add(static_cast<int32>(Result.WindDirection));
	Add(Result.WindStrength);
	Add(Result.bPrecipitation);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_ENVIRONMENT_DIGEST Digest=%llu"), Digest);
	TestEqual(TEXT("pre-migration 100-day pollution/weather state retained"), Digest, uint64(1535293212214686546ull));
	return TestTrue(TEXT("reference environment remains valid"), Result.IsValid());
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityExpandedEnvironmentTest, "CitySim.Dimensions.ExpandedPollutionAndRestore",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityExpandedEnvironmentTest::RunTest(const FString& Parameters)
{
	const FCityGridDimensions Dimensions(80, 80);
	FCityPlacementModel City(10000, Dimensions), Legacy;
	FCityBuildingDefinition Factory;
	Factory.DefinitionId = TEXT("Factory");
	Factory.DisplayName = TEXT("Factory");
	auto Preview = City.BeginPreview(Factory, {79, 60});
	const auto Placed = City.CommitPlacement(Preview, Factory);
	if (!TestTrue(TEXT("expanded industrial source placed"), Placed.IsSuccess()))
		return false;
	FCityServiceSnapshot Services;
	Services.Dimensions = Dimensions;
	Services.EntityPollutionEmissions.Add(Placed.EntityId, 900);
	FCityEnvironmentSimulation Simulation(Dimensions), LegacySimulation;
	const FCityEconomyTuning Tuning;
	Simulation.AdvanceToStep(1, City, Services, Tuning);
	TestEqual(TEXT("expanded source emits at its own index"), Simulation.GetSnapshot().GetPollution({79, 60}), 900);
	TestEqual(TEXT("former64 row alias remains clean"), Simulation.GetSnapshot().GetPollution({15, 61}), 0);
	Simulation.AdvanceToStep(2, City, Services, Tuning);
	TestTrue(TEXT("pollution diffuses across expanded cells"), Simulation.GetSnapshot().GetPollution({79, 61}) > 0);
	TestTrue(TEXT("expanded boundary diffusion remains valid"), Simulation.GetSnapshot().IsValid());
	FString Error;
	const auto State = Simulation.GetSnapshot();
	FCityEnvironmentSimulation Restored(Dimensions);
	TestTrue(TEXT("matching environment restores"), Restored.RestoreState(State, 2, Error));
	Simulation.AdvanceToStep(100, City, Services, Tuning);
	Restored.AdvanceToStep(100, City, Services, Tuning);
	TestTrue(TEXT("resumed pollution is exact"),
	         Simulation.GetSnapshot().Pollution == Restored.GetSnapshot().Pollution);
	TestEqual(TEXT("resumed weather random state is exact"), Simulation.GetSnapshot().WeatherRandomState,
	          Restored.GetSnapshot().WeatherRandomState);
	TestFalse(TEXT("cross-size environment restore rejected"), LegacySimulation.RestoreState(State, 2, Error));
	TestEqual(TEXT("rejected restore preserves legacy clock"), LegacySimulation.GetSnapshot().LastFixedStepId,
	          int64(0));
	for (int32 Case = 0; Case < 3; ++Case)
	{
		auto Corrupt = State;
		if (Case == 0)
			Corrupt.Pollution.Pop();
		if (Case == 1)
			Corrupt.Dimensions = FCityGridDimensions(MAX_int32, MAX_int32);
		if (Case == 2)
			Corrupt.TotalPollution++;
		TestFalse(TEXT("corrupt environment restore rejected"), Restored.RestoreState(Corrupt, 100, Error));
		TestEqual(TEXT("corrupt restore preserves current clock"), Restored.GetSnapshot().LastFixedStepId, int64(100));
	}
	Simulation.Reset();
	TestTrue(TEXT("reset retains owning dimensions"), Simulation.GetSnapshot().Dimensions == Dimensions);
	TestEqual(TEXT("reset retains expanded field size"), Simulation.GetSnapshot().Pollution.Num(), 6400);
	TestEqual(TEXT("other city's field stays legacy size"), LegacySimulation.GetSnapshot().Pollution.Num(), 4096);
	FCitySaveDocument Document;
	Document.Environment = State;
	FString Json = TEXT("sentinel");
	TestFalse(TEXT("expanded field cannot be exported under64 metadata"),
	          FCityPersistence::Serialize(Document, Json, Error));
	TestEqual(TEXT("rejected export leaves output intact"), Json, FString(TEXT("sentinel")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityExpandedServicesTest, "CitySim.Dimensions.ExpandedGrowthAndServiceFields",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityExpandedServicesTest::RunTest(const FString& Parameters)
{
	const FCityGridDimensions Dimensions(80, 80);
	FCityPlacementModel City(100000, Dimensions);
	auto Road = City.BeginRoadPreview({0, 8});
	City.UpdateRoadPreviewEnd(Road, {79, 8});
	if (!TestTrue(TEXT("expanded connected road placed"), City.CommitRoad(Road).IsSuccess()))
		return false;
	FCityBuildingDefinition Home;
	Home.DefinitionId = TEXT("Home");
	Home.DisplayName = TEXT("Home");
	Home.Footprint = {2, 2};
	TestTrue(TEXT("only growth zone is beyond64"), City.SetZoneArea({77, 9}, {2, 2}, ECityZoneType::Residential));
	FCityServiceSnapshot Services;
	Services.Dimensions = Dimensions;
	Services.PowerCapacity = Services.ServiceCapacity = 1000;
	Services.ServiceReach.RoadHopDistances.Add({77, 8}, 0);
	Services.ServiceReach.RoadHopDistances.Add({78, 8}, 0);
	const FCityEconomyTuning Tuning;
	auto Needs = FCityNeedsEvaluator::BuildSnapshot(City, Services, Tuning, &Home, nullptr, false);
	TestEqual(TEXT("needs scan sees expanded zoned footprint"), Needs.ResidentialZonedFootprints, 1);
	TestEqual(TEXT("needs scan sees expanded serviced footprint"), Needs.ResidentialEligibleFootprints, 1);
	Needs.RecommendedGrowthZone = ECityZoneType::Residential;
	FCityGrowthSimulation Growth;
	const auto Result =
	    Growth.AdvanceToStep(Tuning.GrowthIntervalSteps, City, Services, Needs, Tuning, &Home, nullptr, false);
	if (!TestTrue(TEXT("natural growth reaches expanded lot"), Result.Failure == ECityGrowthFailure::None))
		return false;
	TestEqual(TEXT("grown home uses expanded anchor"), Result.Anchor, FCityCellCoord(77, 9));
	uint64 HomeId = City.GetGrid().GetOccupant({77, 9});
	Services.Residents = 10;
	Services.EntityResidents.Add(HomeId, 10);
	for (const auto Id : {TEXT("Park"), TEXT("School")})
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id;
		auto Preview = City.BeginPreview(Definition, {FName(Id) == TEXT("Park") ? 79 : 76, 9});
		TestTrue(TEXT("expanded civic facility placed"), City.CommitPlacement(Preview, Definition).IsSuccess());
	}
	FCityDevelopmentSnapshot Development;
	FCityParkEvaluator::Build(City, Services, Tuning, nullptr, &Development);
	FCityEducationEvaluator::Build(City, Services, Tuning, nullptr, &Development);
	TestEqual(TEXT("park field allocated for expanded city"), Services.ParkCoverage.Num(), 6400);
	TestEqual(TEXT("school field allocated for expanded city"), Services.EducationCoverage.Num(), 6400);
	TestTrue(TEXT("park covers expanded home"), Services.ParkCoverage[9 * 80 + 78] > 0);
	TestEqual(TEXT("remote former index alias is uncovered"), Services.ParkCoverage[10 * 80 + 14], 0);
	TestTrue(TEXT("school covers expanded home"), Services.EducationCoverage[9 * 80 + 78] > 0);
	FString Error;
	FCityCrimeSnapshot CrimeState;
	CrimeState.Dimensions = Dimensions;
	CrimeState.ActivationStep = CrimeState.GraceEndStep = 0;
	FCityCrimeSimulation Crime;
	TestTrue(TEXT("introduced crime state restores"), Crime.RestoreState(CrimeState, 0, Error));
	Crime.Refresh(City, Services, Development, Tuning, false);
	Services.CrimeRisk = Crime.GetSnapshot().CellRisk;
	TestEqual(TEXT("crime field allocated for expanded city"), Services.CrimeRisk.Num(), 6400);
	TestTrue(TEXT("crime lookup reaches expanded home"), Crime.GetSnapshot().GetRisk({77, 9}) > 0);
	TestEqual(TEXT("crime does not write to former64 alias"), Crime.GetSnapshot().GetRisk({13, 10}), 0);
	FCityFireSnapshot FireState;
	FireState.Dimensions = Dimensions;
	FireState.ActivationStep = FireState.GraceEndStep = 0;
	FCityFireSimulation Fire;
	TestTrue(TEXT("introduced fire state restores"), Fire.RestoreState(FireState, 0, Error));
	FCityEnvironmentSnapshot Weather(Dimensions);
	Fire.Refresh(City, Services, Development, Weather, Tuning, false);
	TestEqual(TEXT("fire field allocated for expanded city"), Fire.GetSnapshot().CellRisk.Num(), 6400);
	TestTrue(TEXT("fire risk reaches expanded home"), Fire.GetSnapshot().CellRisk[9 * 80 + 77] > 0);
	TestEqual(TEXT("fire does not write to former64 alias"), Fire.GetSnapshot().CellRisk[10 * 80 + 13], 0);
	FCityFireSnapshot Damage;
	Damage.Dimensions = Dimensions;
	FCityWasteSnapshot Waste;
	Waste.Dimensions = Dimensions;
	for (uint64 Id = 1; Id <= 4097; ++Id)
	{
		Damage.DamagedBuildings.Add(Id, {1, -1, 1});
		Waste.Sites.Add({Id});
	}
	TestTrue(TEXT("structural cap follows expanded dimensions"), FCityFireSimulation::ValidateState(Damage, 0, Error));
	TestTrue(TEXT("waste cap follows expanded dimensions"), FCityWasteSimulation::ValidateState(Waste, 0, Error));
	Damage.Dimensions = Waste.Dimensions = FCityGridDimensions(64, 64);
	TestFalse(TEXT("legacy structural cap retained"), FCityFireSimulation::ValidateState(Damage, 0, Error));
	TestFalse(TEXT("legacy waste cap retained"), FCityWasteSimulation::ValidateState(Waste, 0, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityExpandedDisasterTest, "CitySim.Dimensions.ExpandedDisasterBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityExpandedDisasterTest::RunTest(const FString& Parameters)
{
	const FCityGridDimensions Dimensions(80, 80);
	FCityDisasterSnapshot State;
	State.Dimensions = Dimensions;
	State.TotalEvents = 1;
	FCityDisasterEvent Event;
	Event.Dimensions = Dimensions;
	Event.Id = 1;
	Event.Kind = ECityNaturalDisaster::Tornado;
	Event.Center = {70, 78};
	Event.Direction = 1;
	Event.Profile = 1;
	Event.bTest = true;
	Event.EndStep = 17;
	State.Events.Add(Event);
	FString Error;
	TestTrue(TEXT("expanded legacy-profile event validates"), FCityDisasterSimulation::ValidateState(State, 0, Error));
	TestEqual(TEXT("linear disaster clamps to its own boundary"), FCityDisasterSimulation::Position(Event, 16),
	          FCityCellCoord(79, 78));
	Event.Profile = 4;
	Event.Center = {78, 78};
	Event.StartStep = 1;
	Event.EndStep = 31;
	Event.InitialBuildings = 20;
	Event.SeverityPercent = 5;
	Event.Path = {{78, 78}};
	Event.PlannedTargets = {1};
	Event.TravelDistances = {0};
	State.Events[0] = Event;
	TestTrue(TEXT("expanded curved event validates"), FCityDisasterSimulation::ValidateState(State, 0, Error));
	const auto Position = FCityDisasterSimulation::ContinuousPosition(Event, 9);
	TestTrue(TEXT("curved motion remains in expanded area"),
	         Position.X > 63 && Position.Y > 63 && Position.X <= 79 && Position.Y <= 79);
	FCityDisasterSimulation Restored;
	TestTrue(TEXT("expanded disaster state restores"), Restored.RestoreState(State, 0, Error));
	auto Corrupt = State;
	Corrupt.Events[0].Path[0] = {80, 78};
	TestFalse(TEXT("out-of-bounds disaster restore rejected"), Restored.RestoreState(Corrupt, 0, Error));
	TestEqual(TEXT("rejected restore preserves existing path"), Restored.GetSnapshot().Events[0].Path[0],
	          FCityCellCoord(78, 78));
	Corrupt = State;
	Corrupt.Events[0].Dimensions = FCityGridDimensions(64, 64);
	TestFalse(TEXT("mislabeled event dimensions rejected"), FCityDisasterSimulation::ValidateState(Corrupt, 0, Error));
	FCityDisasterSimulation Expanded, Legacy;
	Expanded.Reset(0, 1, Dimensions);
	TestTrue(TEXT("expanded manual warning accepts expanded center"),
	         Expanded.StartTest(ECityNaturalDisaster::Tornado, {79, 78}, 1));
	TestFalse(TEXT("legacy warning still rejects expanded center"),
	          Legacy.StartTest(ECityNaturalDisaster::Tornado, {79, 78}, 1));
	return true;
}

#endif
