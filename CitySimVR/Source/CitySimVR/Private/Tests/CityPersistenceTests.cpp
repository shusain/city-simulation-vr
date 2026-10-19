#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
FCityBuildingDefinition SaveDefinition(const FName Id)
{
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = Id;
	Definition.DisplayName = Id.ToString();
	Definition.Footprint = FCityCellFootprint(2, 2);
	Definition.LogicalHeightCm = 100.0;
	return Definition;
}

TMap<FName, FCityBuildingDefinition> SaveDefinitions()
{
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const FName Id :
	     {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Power")), FName(TEXT("Service"))})
	{
		Definitions.Add(Id, SaveDefinition(Id));
	}
	return Definitions;
}

FCitySaveDocument MakeSaveFixture()
{
	FCitySaveDocument Document;
	Document.Placement.Funds = 4321;
	Document.Placement.Revision = 17;
	Document.Placement.NextEntityId = 3;
	Document.Placement.Zones.Init(ECityZoneType::None, FCityGrid::CellCount);
	for (int32 X = 0; X <= 20; ++X)
	{
		Document.Placement.RoadCells.Emplace(X, 8);
	}
	Document.Placement.RoadRecords.Add({FCityCellCoord(10, 8), TEXT("Avenue")});
	for (int32 Y = 9; Y <= 10; ++Y)
	{
		for (int32 X = 5; X <= 6; ++X)
		{
			Document.Placement.Zones[Y * FCityGrid::Width + X] = ECityZoneType::Residential;
		}
	}
	FCityPlacedEntity Power;
	Power.EntityId = 1;
	Power.DefinitionId = TEXT("Power");
	Power.Anchor = FCityCellCoord(1, 9);
	Power.FootprintSize = FIntPoint(2, 2);
	Power.RecordedPlacementCost = 500;
	Document.Placement.Entities.Add(Power);
	FCityPlacedEntity Service;
	Service.EntityId = 2;
	Service.DefinitionId = TEXT("Service");
	Service.Anchor = FCityCellCoord(3, 9);
	Service.FootprintSize = FIntPoint(2, 2);
	Service.RecordedPlacementCost = 400;
	Document.Placement.Entities.Add(Service);
	Document.Economy.FixedStepId = 24;
	Document.Economy.MonthId = 12;
	Document.Economy.LastBilledMonthId = 12;
	Document.Economy.ResidentialTaxBasisPoints = 900;
	Document.Economy.CommercialTaxBasisPoints = 1400;
	Document.Economy.LastResidentialTaxRevenue = 45;
	Document.Economy.LastCommercialTaxRevenue = 28;
	Document.Economy.CommercialOutput = 22;
	Document.Economy.CommercialUpkeep = 18;
	Document.Economy.IndustrialUpkeep = 35;
	Document.Economy.LastIncome = 73;
	Document.Economy.BuiltIndustries = 1;
	Document.Economy.OperationalIndustries = 1;
	Document.Economy.IndustrialOutput = 20;
	Document.Economy.BuiltParks = 1;
	Document.Economy.EmployedResidents = 10;
	Document.Economy.HouseholdGoodsDemand = 5;
	Document.Economy.GoodsProduced = 20;
	Document.Economy.RetailCapacity = 22;
	Document.Economy.GoodsSold = 5;
	Document.Economy.HouseholdIncome = 50;
	Document.Economy.HouseholdSpending = 25;
	Document.Economy.ParkUpkeep = 15;
	Document.EconomyAccumulatorSeconds = 0.1;
	Document.SimulationSpeed = 3;
	Document.GrowthRandomState = 42;
	Document.NextGrowthStep = 25;
	Document.Scenario.State = 1;
	Document.Scenario.ProfitableMonthStreak = 2;
	Document.Scenario.LastObservedBilledMonthId = 12;
	Document.Terrain.Seed = 9137;
	Document.Terrain.HillAmplitudeCm = 1700;
	Document.Terrain.SeaLevelCm = -920;
	Document.Environment.LastFixedStepId = 24;
	Document.Environment.Pollution[9 * FCityGrid::Width + 5] = 35;
	Document.Environment.TotalPollution = 35;
	Document.Environment.PeakPollution = 35;
	Document.Development.LastFixedStepId = 24;
	FCityDevelopmentState Development;
	Development.EntityId = 2;
	Development.Stage = ECityDevelopmentStage::Construction;
	Development.DensityLevel = ECityDensityLevel::Low;
	Development.ConstructionCompleteStep = 30;
	Document.Development.States.Add(2, Development);
	Document.Crime.LastFixedStepId = Document.Fire.LastFixedStepId = Document.Disasters.LastFixedStepId =
	    Document.Economy.FixedStepId;
	Document.Disasters.NextCheckStep = Document.Economy.FixedStepId;
	return Document;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPersistenceRoundTripTest, "CitySim.Persistence.RoundTripAndValidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPersistenceRoundTripTest::RunTest(const FString& Parameters)
{
	FCitySaveDocument Source = MakeSaveFixture();
	Source.Traffic.FixedStepId = 24;
	Source.Traffic.NetworkFingerprint = 18446744073709550001ull;
	FCityTrafficVehicleState SavedVehicle;
	SavedVehicle.Id = 9007199254740993ull;
	SavedVehicle.Route = {{0, false, 0}, {1, true, 1}};
	SavedVehicle.RouteLegIndex = 1;
	SavedVehicle.DistanceUnits = 1500;
	SavedVehicle.SpeedUnitsPerStep = 400;
	SavedVehicle.LengthUnits = 600;
	SavedVehicle.StartDistanceUnits = 300;
	SavedVehicle.EndDistanceUnits = 2500;
	SavedVehicle.VehicleType = ECityTripVehicleType::FreightTruck;
	SavedVehicle.SourceId = 4503599627370496ull;
	SavedVehicle.OriginEntityId = 1;
	SavedVehicle.DestinationEntityId = 2;
	SavedVehicle.bTransitTrip = true;
	SavedVehicle.CompletedStep = 0;
	Source.Traffic.Vehicles.Add(SavedVehicle);
	FString Json;
	FString Error;
	TestTrue(TEXT("current schema serializes"), FCityPersistence::Serialize(Source, Json, Error));
	FCitySaveDocument Loaded;
	TestTrue(TEXT("current schema deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("traffic fingerprint retains uint64 precision"), Loaded.Traffic.NetworkFingerprint,
	          Source.Traffic.NetworkFingerprint);
	TestEqual(TEXT("traffic clock round trips"), Loaded.Traffic.FixedStepId, int64(24));
	if (!TestEqual(TEXT("traffic vehicle count round trips"), Loaded.Traffic.Vehicles.Num(), 1))
		return false;
	const FCityTrafficVehicleState& LoadedVehicle = Loaded.Traffic.Vehicles[0];
	TestEqual(TEXT("traffic ID retains uint64 precision"), LoadedVehicle.Id, SavedVehicle.Id);
	TestEqual(TEXT("traffic leg index round trips"), LoadedVehicle.RouteLegIndex, 1);
	TestEqual(TEXT("traffic distance round trips"), LoadedVehicle.DistanceUnits, 1500);
	TestEqual(TEXT("traffic speed round trips"), LoadedVehicle.SpeedUnitsPerStep, 400);
	TestEqual(TEXT("traffic length round trips"), LoadedVehicle.LengthUnits, 600);
	TestFalse(TEXT("traffic completion round trips"), LoadedVehicle.bCompleted);
	TestEqual(TEXT("partial route start round trips"), LoadedVehicle.StartDistanceUnits, 300);
	TestEqual(TEXT("partial route end round trips"), LoadedVehicle.EndDistanceUnits, 2500);
	TestEqual(TEXT("vehicle mode round trips"), LoadedVehicle.VehicleType,
	          ECityTripVehicleType::FreightTruck);
	TestEqual(TEXT("source identity round trips"), LoadedVehicle.SourceId, SavedVehicle.SourceId);
	TestEqual(TEXT("trip origin round trips"), LoadedVehicle.OriginEntityId, uint64(1));
	TestEqual(TEXT("trip destination round trips"), LoadedVehicle.DestinationEntityId, uint64(2));
	TestTrue(TEXT("delivery source kind round trips"), LoadedVehicle.bTransitTrip);
	if (!TestEqual(TEXT("traffic route length round trips"), LoadedVehicle.Route.Num(), 2))
		return false;
	TestTrue(TEXT("traffic direction round trips"), LoadedVehicle.Route[1].bReverse);
	TestEqual(TEXT("traffic lane round trips"), LoadedVehicle.Route[1].LaneIndex, 1);
	TestEqual(TEXT("traffic edge round trips"), LoadedVehicle.Route[1].EdgeId, 1);
	TSharedPtr<FJsonObject> TrafficRoot;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), TrafficRoot);
	TrafficRoot->RemoveField(TEXT("traffic"));
	FString TrafficJson;
	FJsonSerializer::Serialize(TrafficRoot.ToSharedRef(), TJsonWriterFactory<>::Create(&TrafficJson));
	FCitySaveDocument LegacyTraffic;
	TestFalse(TEXT("v14 rejects missing traffic"),
	          FCityPersistence::Deserialize(TrafficJson, LegacyTraffic, Error));
	TestEqual(TEXT("funds round trip"), Loaded.Placement.Funds, Source.Placement.Funds);
	TestEqual(TEXT("next entity ID round trip"), Loaded.Placement.NextEntityId,
	          Source.Placement.NextEntityId);
	TestEqual(TEXT("zone count round trips"), Loaded.Placement.Zones.Num(), Source.Placement.Zones.Num());
	const FCityRoadCellRecord* AvenueRecord = Loaded.Placement.RoadRecords.FindByPredicate(
	    [](const FCityRoadCellRecord& Record) { return Record.Cell == FCityCellCoord(10, 8); });
	TestTrue(TEXT("road definition record round trips"),
	         AvenueRecord && AvenueRecord->DefinitionId == TEXT("Avenue"));
	TestEqual(TEXT("residential zone round trips"), Loaded.Placement.Zones[9 * FCityGrid::Width + 5],
	          ECityZoneType::Residential);
	TestEqual(TEXT("clock round trip"), Loaded.Economy.FixedStepId, Source.Economy.FixedStepId);
	TestEqual(TEXT("shortfall round trip"), Loaded.Economy.UpkeepShortfall, Source.Economy.UpkeepShortfall);
	TestEqual(TEXT("residential tax rate round trips"), Loaded.Economy.ResidentialTaxBasisPoints, 900);
	TestEqual(TEXT("commercial tax revenue round trips"), Loaded.Economy.LastCommercialTaxRevenue, int64(28));
	TestEqual(TEXT("PRNG round trip"), Loaded.GrowthRandomState, Source.GrowthRandomState);
	TestEqual(TEXT("scenario streak round trip"), Loaded.Scenario.ProfitableMonthStreak,
	          Source.Scenario.ProfitableMonthStreak);
	TestEqual(TEXT("scenario observed month round trip"), Loaded.Scenario.LastObservedBilledMonthId,
	          Source.Scenario.LastObservedBilledMonthId);
	TestEqual(TEXT("terrain seed round trips"), Loaded.Terrain.Seed, Source.Terrain.Seed);
	TestEqual(TEXT("terrain parameters round trip"), Loaded.Terrain.HillAmplitudeCm,
	          Source.Terrain.HillAmplitudeCm);
	TestEqual(TEXT("sea level round trips"), Loaded.Terrain.SeaLevelCm, Source.Terrain.SeaLevelCm);
	TestEqual(TEXT("industrial output round trips"), Loaded.Economy.IndustrialOutput, 20);
	TestEqual(TEXT("commercial output round trips"), Loaded.Economy.CommercialOutput, 22);
	TestEqual(TEXT("commercial upkeep round trips"), Loaded.Economy.CommercialUpkeep, int64(18));
	TestEqual(TEXT("industrial upkeep round trips"), Loaded.Economy.IndustrialUpkeep, int64(35));
	TestEqual(TEXT("goods sold round trips"), Loaded.Economy.GoodsSold, 5);
	TestEqual(TEXT("household spending round trips"), Loaded.Economy.HouseholdSpending, int64(25));
	TestEqual(TEXT("park upkeep round trips"), Loaded.Economy.ParkUpkeep, int64(15));
	TestEqual(TEXT("pollution clock round trips"), Loaded.Environment.LastFixedStepId, int64(24));
	TestEqual(TEXT("pollution field round trips"), Loaded.Environment.GetPollution(FCityCellCoord(5, 9)), 35);
	TestEqual(TEXT("pollution aggregate round trips"), Loaded.Environment.TotalPollution, 35);
	TestEqual(TEXT("weather random state round trips"), Loaded.Environment.WeatherRandomState,
	          Source.Environment.WeatherRandomState);
	TestEqual(TEXT("weather next change round trips"), Loaded.Environment.NextWeatherChangeStep,
	          Source.Environment.NextWeatherChangeStep);
	TestEqual(TEXT("weather wind strength round trips"), Loaded.Environment.WindStrength,
	          Source.Environment.WindStrength);
	TestEqual(TEXT("construction completion step round trips"),
	          Loaded.Development.States.FindChecked(2).ConstructionCompleteStep, int64(30));
	TestEqual(TEXT("construction stage round trips"), Loaded.Development.States.FindChecked(2).Stage,
	          ECityDevelopmentStage::Construction);
	const TMap<FName, FCityBuildingDefinition> Definitions = SaveDefinitions();
	TUniquePtr<FCityPlacementModel> First =
	    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
	TUniquePtr<FCityPlacementModel> Second =
	    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
	TestTrue(TEXT("loaded city validates into temporary model"), First.IsValid() && Second.IsValid());
	FCityGrowthSimulation FirstGrowth;
	FCityGrowthSimulation SecondGrowth;
	TestTrue(TEXT("first growth state restores"),
	         FirstGrowth.RestoreState(Loaded.GrowthRandomState, Loaded.NextGrowthStep, Error));
	TestTrue(TEXT("second growth state restores"),
	         SecondGrowth.RestoreState(Loaded.GrowthRandomState, Loaded.NextGrowthStep, Error));
	FCityEconomyTuning Tuning;
	const FCityServiceSnapshot FirstServices = FCityServiceEvaluator::BuildSnapshot(*First, Tuning);
	const FCityServiceSnapshot SecondServices = FCityServiceEvaluator::BuildSnapshot(*Second, Tuning);
	const FCityBuildingDefinition& HomeDefinition = Definitions.FindChecked(TEXT("Home"));
	const FCityBuildingDefinition& ShopDefinition = Definitions.FindChecked(TEXT("Shop"));
	const FCityNeedsSnapshot FirstNeeds = FCityNeedsEvaluator::BuildSnapshot(
	    *First, FirstServices, Tuning, &HomeDefinition, &ShopDefinition, false);
	const FCityNeedsSnapshot SecondNeeds = FCityNeedsEvaluator::BuildSnapshot(
	    *Second, SecondServices, Tuning, &HomeDefinition, &ShopDefinition, false);
	TestEqual(TEXT("derived residential demand repeats after load"), FirstNeeds.ResidentialDemand,
	          SecondNeeds.ResidentialDemand);
	TestEqual(TEXT("derived commercial demand repeats after load"), FirstNeeds.CommercialDemand,
	          SecondNeeds.CommercialDemand);
	TestEqual(TEXT("derived growth constraint repeats after load"), FirstNeeds.ResidentialConstraint,
	          SecondNeeds.ResidentialConstraint);
	const FCityGrowthResult FirstResult = FirstGrowth.AdvanceToStep(
	    25, *First, FirstServices, FirstNeeds, Tuning, &HomeDefinition, &ShopDefinition, false);
	const FCityGrowthResult SecondResult = SecondGrowth.AdvanceToStep(
	    25, *Second, SecondServices, SecondNeeds, Tuning, &HomeDefinition, &ShopDefinition, false);
	TestTrue(TEXT("restored next growth attempt succeeds"), FirstResult.IsSuccess());
	TestEqual(TEXT("restored PRNG selects same X"), FirstResult.Anchor.X, SecondResult.Anchor.X);
	TestEqual(TEXT("restored PRNG selects same Y"), FirstResult.Anchor.Y, SecondResult.Anchor.Y);
	TestEqual(TEXT("restored next entity ID is used"), FirstResult.EntityId, Source.Placement.NextEntityId);

	FCitySaveDocument Invalid = Loaded;
	Invalid.Placement.Entities[1].EntityId = Invalid.Placement.Entities[0].EntityId;
	TestFalse(TEXT("duplicate IDs are rejected"),
	          FCityPlacementModel::CreateValidatedFromState(Invalid.Placement, Definitions, Error).IsValid());
	Invalid = Loaded;
	Invalid.Placement.Entities[1].Anchor = Invalid.Placement.Entities[0].Anchor;
	TestFalse(TEXT("overlapping buildings are rejected"),
	          FCityPlacementModel::CreateValidatedFromState(Invalid.Placement, Definitions, Error).IsValid());
	Invalid = Loaded;
	Invalid.Placement.Entities[0].DefinitionId = TEXT("RemovedContent");
	TestFalse(TEXT("unknown content IDs are rejected"),
	          FCityPlacementModel::CreateValidatedFromState(Invalid.Placement, Definitions, Error).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPersistenceFailureTest, "CitySim.Persistence.CorruptionAndAtomicWrite",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPersistenceFailureTest::RunTest(const FString& Parameters)
{
	FString Json;
	FString Error;
	FCityPersistence::Serialize(MakeSaveFixture(), Json, Error);
	FCitySaveDocument Document;
	TestFalse(TEXT("truncated JSON is rejected"),
	          FCityPersistence::Deserialize(Json.Left(Json.Len() / 2), Document, Error));
	FCitySaveDocument IncompatibleDocument = MakeSaveFixture();
	IncompatibleDocument.SchemaVersion = 99;
	FString Incompatible;
	FCityPersistence::Serialize(IncompatibleDocument, Incompatible, Error);
	TestFalse(TEXT("incompatible schema is rejected"),
	          FCityPersistence::Deserialize(Incompatible, Document, Error));

	const FString Directory =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CityPersistence"));
	const FString SavePath = FPaths::Combine(Directory, TEXT("Atomic.json"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	IFileManager::Get().Delete(*SavePath, false, true, true);
	IFileManager::Get().Delete(*(SavePath + TEXT(".tmp")), false, true, true);
	IFileManager::Get().Delete(*(SavePath + TEXT(".bak")), false, true, true);
	TestTrue(TEXT("older save writes"), FCityPersistence::WriteAtomic(SavePath, TEXT("old-valid"), Error));
	TestFalse(TEXT("simulated interruption reports failure"),
	          FCityPersistence::WriteAtomic(SavePath, TEXT("new-value"), Error, true));
	FString Current;
	TestTrue(TEXT("older save remains readable after interruption"),
	         FFileHelper::LoadFileToString(Current, *SavePath));
	TestEqual(TEXT("interruption preserves older save"), Current, FString(TEXT("old-valid")));
	TestTrue(TEXT("replacement succeeds"), FCityPersistence::WriteAtomic(SavePath, TEXT("new-value"), Error));
	FString Backup;
	TestTrue(TEXT("last-known-good backup is retained"),
	         FFileHelper::LoadFileToString(Backup, *(SavePath + TEXT(".bak"))));
	TestEqual(TEXT("backup contains prior save"), Backup, FString(TEXT("old-valid")));
	IFileManager::Get().Delete(*SavePath, false, true, true);
	IFileManager::Get().Delete(*(SavePath + TEXT(".tmp")), false, true, true);
	IFileManager::Get().Delete(*(SavePath + TEXT(".bak")), false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficSaveResumeTest, "CitySim.Persistence.TrafficResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTrafficSaveResumeTest::RunTest(const FString& Parameters)
{
	FCitySaveDocument Document = MakeSaveFixture();
	FString Error;
	TUniquePtr<FCityPlacementModel> City =
	    FCityPlacementModel::CreateValidatedFromState(Document.Placement, SaveDefinitions(), Error);
	if (!TestTrue(TEXT("traffic save city restores"), City.IsValid()))
		return false;
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(*City, FCityRoadDefinition::Local(), Network);
	FCityTrafficSimulation Original;
	TestTrue(TEXT("traffic starts at saved clock"),
	         Original.AdvanceToStep(Network, Document.Economy.FixedStepId, Error));
	FCityTrafficVehicleState Vehicle;
	Vehicle.Id = 100;
	Vehicle.Route.Add({0, false, 0});
	TestTrue(TEXT("saved city's route accepts vehicle"), Original.AddVehicle(Vehicle, Network, &Error));
	Document.Traffic = Original.GetSnapshot();
	FString Json;
	TestTrue(TEXT("traffic city serializes"), FCityPersistence::Serialize(Document, Json, Error));
	FCitySaveDocument Loaded;
	TestTrue(TEXT("traffic city deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TUniquePtr<FCityPlacementModel> RestoredCity =
	    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, SaveDefinitions(), Error);
	if (!TestTrue(TEXT("road graph source restores"), RestoredCity.IsValid()))
		return false;
	FCityRoadNetworkSnapshot RestoredNetwork;
	FCityRoadNetworkBuilder::Build(*RestoredCity, FCityRoadDefinition::Local(), RestoredNetwork);
	FCityTrafficSimulation Restored;
	TestTrue(TEXT("traffic validates against rebuilt road graph"),
	         Restored.RestoreState(Loaded.Traffic, RestoredNetwork, Loaded.Economy.FixedStepId, Error));
	TestTrue(TEXT("original traffic advances"),
	         Original.AdvanceToStep(Network, Document.Economy.FixedStepId + 2, Error));
	TestTrue(TEXT("loaded traffic advances"),
	         Restored.AdvanceToStep(RestoredNetwork, Document.Economy.FixedStepId + 2, Error));
	if (!TestEqual(TEXT("loaded vehicle retained"), Restored.GetSnapshot().Vehicles.Num(), 1))
		return false;
	TestEqual(TEXT("loaded progress matches uninterrupted progress"),
	          Restored.GetSnapshot().Vehicles[0].DistanceUnits,
	          Original.GetSnapshot().Vehicles[0].DistanceUnits);
	TestEqual(TEXT("loaded speed matches uninterrupted speed"),
	          Restored.GetSnapshot().Vehicles[0].SpeedUnitsPerStep,
	          Original.GetSnapshot().Vehicles[0].SpeedUnitsPerStep);
	TestEqual(TEXT("loaded completion matches uninterrupted state"),
	          Restored.GetSnapshot().Vehicles[0].bCompleted, Original.GetSnapshot().Vehicles[0].bCompleted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDisasterPolicySaveTest, "CitySim.Disasters.PolicySaveValidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDisasterPolicySaveTest::RunTest(const FString& Parameters)
{
	FString Json, Error;
	FCitySaveDocument Loaded;
	for (const auto Frequency : {ECityDisasterFrequency::Off, ECityDisasterFrequency::Low,
	                             ECityDisasterFrequency::Medium, ECityDisasterFrequency::High})
	{
		auto Document = MakeSaveFixture();
		Document.Identity.DisasterFrequency = Frequency;
		TestTrue(TEXT("Per-city frequency serializes"), FCityPersistence::Serialize(Document, Json, Error));
		TestTrue(TEXT("Frequency reloads"), FCityPersistence::Deserialize(Json, Loaded, Error));
		TestEqual(TEXT("Setting retained exactly"), Loaded.Identity.DisasterFrequency, Frequency);
	}
	auto Current = MakeSaveFixture();
	FCityPersistence::Serialize(Current, Json, Error);
	TSharedPtr<FJsonObject> Object;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
	const auto Encode = [&]() {
		FString Result;
		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
		return Result;
	};
	Object->SetNumberField(TEXT("disasterFrequency"), 4);
	TestFalse(TEXT("Out of range policy rejected"), FCityPersistence::Deserialize(Encode(), Loaded, Error));
	Object->SetNumberField(TEXT("disasterFrequency"), 1.5);
	TestFalse(TEXT("Fractional policy rejected"), FCityPersistence::Deserialize(Encode(), Loaded, Error));
	Object->RemoveField(TEXT("disasterFrequency"));
	TestFalse(TEXT("Missing policy in schema40 rejected"),
	          FCityPersistence::Deserialize(Encode(), Loaded, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNaturalSaveValidationTest, "CitySim.Disasters.NaturalSaveValidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNaturalSaveValidationTest::RunTest(const FString& Parameters)
{
	FString Json, Error;
	FCitySaveDocument Loaded;
	auto Doc = MakeSaveFixture();
	Doc.Disasters.LastFixedStepId = Doc.Economy.FixedStepId;
	Doc.Disasters.NextCheckStep = Doc.Economy.FixedStepId + 300;
	FCityDisasterEvent E;
	E.Id = 1;
	E.WarningStep = Doc.Economy.FixedStepId - 6;
	E.StartStep = E.WarningStep + 5;
	E.EndStep = E.StartStep + 6;
	E.Center = {2, 9};
	E.bTest = true;
	E.Affected = {1};
	Doc.Disasters.TotalEvents = 1;
	Doc.Disasters.Events.Add(E);
	Doc.Fire.LastFixedStepId = Doc.Economy.FixedStepId;
	Doc.Fire.NextIgnitionStep = Doc.Economy.FixedStepId;
	Doc.Fire.DamagedBuildings.Add(1, {40, -1, 1});
	TestTrue(TEXT("Natural history and damage serialize"), FCityPersistence::Serialize(Doc, Json, Error));
	TestTrue(TEXT("Natural history and damage reload"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("Cause retained"), Loaded.Fire.DamagedBuildings.FindRef(1).Cause, 1);
	TestEqual(TEXT("Recorded affected identities retained"), Loaded.Disasters.Events.Last().Affected[0],
	          uint64(1));
	TSharedPtr<FJsonObject> O;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O);
	const auto Encode = [&]() {
		FString R;
		FJsonSerializer::Serialize(O.ToSharedRef(), TJsonWriterFactory<>::Create(&R));
		return R;
	};
	O->GetObjectField(TEXT("disasters"))->SetStringField(TEXT("lastStep"), TEXT("23"));
	TestFalse(TEXT("Mismatched natural clock rejected"),
	          FCityPersistence::Deserialize(Encode(), Loaded, Error));
	O->GetObjectField(TEXT("disasters"))
	    ->SetStringField(TEXT("lastStep"), LexToString(Doc.Economy.FixedStepId));
	O->GetObjectField(TEXT("disasters"))
	    ->GetArrayField(TEXT("events"))[0]
	    ->AsObject()
	    ->SetNumberField(TEXT("kind"), 3);
	TestFalse(TEXT("Unknown hazard rejected"), FCityPersistence::Deserialize(Encode(), Loaded, Error));
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	City.CommitRoad(Road);
	const auto Def = SaveDefinition(TEXT("Home"));
	for (int32 X : {5, 9, 13})
	{
		auto Preview = City.BeginPreview(Def, {X, 9});
		City.CommitPlacement(Preview, Def);
	}
	auto Curved = MakeSaveFixture();
	Curved.Placement = City.ExportState();
	FCityDisasterSimulation Tornado;
	FCityFireSimulation TD;
	FCityDevelopmentSnapshot Development;
	FCityIdentity Policy;
	Policy.DisasterFrequency = ECityDisasterFrequency::Off;
	Tornado.Reset(Curved.Economy.FixedStepId - 6, 1);
	Tornado.StartTest(ECityNaturalDisaster::Tornado, {5, 9}, 1);
	Tornado.AdvanceToStep(Curved.Economy.FixedStepId, City, Development, TD, Policy, 1000, 25, 1);
	Curved.Disasters = Tornado.GetSnapshot();
	Curved.Fire = TD.GetSnapshot();
	Curved.Fire.LastFixedStepId = Curved.Economy.FixedStepId;
	TestTrue(TEXT("Curved tornado saves"), FCityPersistence::Serialize(Curved, Json, Error));
	TestTrue(TEXT("Curved tornado reloads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestTrue(TEXT("Curve arrivals retained"), Loaded.Disasters.Events.Last().TravelDistances ==
	                                              Curved.Disasters.Events.Last().TravelDistances);
	TestTrue(TEXT("Curve position retained"),
	         (FCityDisasterSimulation::ContinuousPosition(Loaded.Disasters.Events.Last(),
	                                                      Curved.Economy.FixedStepId + .5) -
	          FCityDisasterSimulation::ContinuousPosition(Curved.Disasters.Events.Last(),
	                                                      Curved.Economy.FixedStepId + .5))
	                 .Size() < .000001);
	return true;
}
#endif
