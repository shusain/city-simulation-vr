#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
	FCityBuildingDefinition MakeHome()
	{
		FCityBuildingDefinition Home;
		Home.DefinitionId = TEXT("Home");
		Home.DisplayName = TEXT("Home");
		Home.Footprint.Width = 2;
		Home.Footprint.Height = 2;
		Home.Cost = 100;
		Home.ConstructionSteps = 2;
		Home.DevelopmentDemandType = ECityDevelopmentDemandType::Residential;
		for (int32 LevelIndex = 0; LevelIndex < 3; ++LevelIndex)
		{
			FCityBuildingLevelDefinition Level;
			Level.Level = static_cast<ECityDensityLevel>(LevelIndex);
			Level.ResidentsProvided = 10 << LevelIndex;
			Level.PowerDemand = Level.ResidentsProvided;
			Level.WaterDemand = Level.ResidentsProvided;
			Level.MonthlyUpkeep = 5 << LevelIndex;
			Level.UpgradeCost = LevelIndex * 100;
			Level.MinimumOperationalSteps = LevelIndex == 1 ? 2 : 3;
			Level.MinimumDemand = 5;
			Home.DensityLevels.Add(Level);
		}
		return Home;
	}

	FCityBuildingDefinition MakeDemandFamily(const FName Id, const ECityDevelopmentDemandType DemandType)
	{
		FCityBuildingDefinition Definition = MakeHome();
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.DevelopmentDemandType = DemandType;
		for (FCityBuildingLevelDefinition& Level : Definition.DensityLevels)
		{
			if (DemandType != ECityDevelopmentDemandType::Residential)
			{
				Level.JobsProvided = Level.ResidentsProvided;
				Level.EconomicOutput = Level.ResidentsProvided;
				Level.ResidentsProvided = 0;
			}
		}
		return Definition;
	}

	uint64 PlaceHome(FCityPlacementModel& City, const FCityBuildingDefinition& Home)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Home, FCityCellCoord(10, 9), 1);
		return City.CommitPlacement(Preview, Home).EntityId;
	}

	FCityBuildingDefinition MakeUtility(const FName Id, const int64 Cost)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(2, 2);
		Definition.Cost = Cost;
		return Definition;
	}

	void PlaceAt(FCityPlacementModel& City, const FCityBuildingDefinition& Definition, const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Anchor);
		City.CommitPlacement(Preview, Definition);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDevelopmentDeterminismTest, "CitySim.Development.FixedStepDeterminism",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDevelopmentDeterminismTest::RunTest(const FString& Parameters)
{
	const FCityBuildingDefinition Home = MakeHome();
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions.Add(Home);
	FCityPlacementModel FineCity(5000);
	FCityPlacementModel CoarseCity(5000);
	const uint64 FineId = PlaceHome(FineCity, Home);
	const uint64 CoarseId = PlaceHome(CoarseCity, Home);
	FCityDevelopmentSimulation Fine;
	FCityDevelopmentSimulation Coarse;
	FCityServiceSnapshot Services;
	Services.PowerCapacity = 1000;
	Services.ServiceCapacity = 1000;
	FCityNeedsSnapshot Needs;
	Needs.ResidentialDemandBalance = 100;
	FCityEnvironmentSnapshot Environment;
	FCityEconomyTuning Tuning;
	Tuning.BaseResidentialDesirability = 100;
	Tuning.MinimumResidentialDesirability = 0;

	Fine.AdvanceToStep(0, FineCity, Catalog, Services, Needs, Environment, Tuning);
	const FCityDevelopmentState* Initial = Fine.GetSnapshot().Find(FineId);
	TestNotNull(TEXT("new residential lot is tracked"), Initial);
	TestEqual(TEXT("new lot starts under construction"), Initial->Stage, ECityDevelopmentStage::Construction);
	Fine.AdvanceToStep(0, FineCity, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("paused clock does not advance construction"), Fine.GetSnapshot().Find(FineId)->Stage,
	          ECityDevelopmentStage::Construction);

	UCityBuildingCatalog* ServiceCatalog = NewObject<UCityBuildingCatalog>();
	ServiceCatalog->Definitions.Add(Home);
	const FCityBuildingDefinition Power = MakeUtility(TEXT("Power"), 500);
	const FCityBuildingDefinition Water = MakeUtility(TEXT("Service"), 400);
	ServiceCatalog->Definitions.Add(Power);
	ServiceCatalog->Definitions.Add(Water);
	FCityPlacementModel ServiceCity(10000);
	FCityRoadPreview Road = ServiceCity.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	ServiceCity.UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
	TestTrue(TEXT("service accounting fixture road commits"), ServiceCity.CommitRoad(Road).IsSuccess());
	PlaceAt(ServiceCity, Power, FCityCellCoord(1, 9));
	PlaceAt(ServiceCity, Water, FCityCellCoord(3, 9));
	const uint64 ServiceHomeId = PlaceHome(ServiceCity, Home);
	FCityDevelopmentSimulation ServiceDevelopment;
	ServiceDevelopment.AdvanceToStep(0, ServiceCity, ServiceCatalog, Services, Needs, Environment, Tuning);
	FCityServiceSnapshot ServiceAccounting =
	    FCityServiceEvaluator::BuildSnapshot(ServiceCity, Tuning, ServiceCatalog, &ServiceDevelopment.GetSnapshot());
	TestEqual(TEXT("construction provides no residents"), ServiceAccounting.Residents, 0);
	TestEqual(TEXT("construction consumes no power"), ServiceAccounting.PowerDemand, 0);
	TestEqual(TEXT("construction consumes no water"), ServiceAccounting.ServiceDemand, 0);
	ServiceDevelopment.AdvanceToStep(2, ServiceCity, ServiceCatalog, ServiceAccounting, Needs, Environment, Tuning);
	ServiceAccounting =
	    FCityServiceEvaluator::BuildSnapshot(ServiceCity, Tuning, ServiceCatalog, &ServiceDevelopment.GetSnapshot());
	TestEqual(TEXT("completed low density provides authored residents"), ServiceAccounting.Residents, 10);
	TestEqual(TEXT("completed low density consumes authored power"), ServiceAccounting.PowerDemand, 10);
	TestEqual(TEXT("completed low density consumes authored water"), ServiceAccounting.ServiceDemand, 10);
	TestEqual(TEXT("completed low density uses authored upkeep"), ServiceAccounting.ResidentialUpkeep, int64(5));
	TestTrue(TEXT("service fixture home remains tracked"),
	         ServiceDevelopment.GetSnapshot().Find(ServiceHomeId) != nullptr);

	for (int64 Step = 1; Step <= 7; ++Step)
	{
		Fine.AdvanceToStep(Step, FineCity, Catalog, Services, Needs, Environment, Tuning);
	}
	Coarse.AdvanceToStep(7, CoarseCity, Catalog, Services, Needs, Environment, Tuning);
	const FCityDevelopmentState* FineState = Fine.GetSnapshot().Find(FineId);
	const FCityDevelopmentState* CoarseState = Coarse.GetSnapshot().Find(CoarseId);
	TestEqual(TEXT("fine simulation reaches high density"), FineState->DensityLevel, ECityDensityLevel::High);
	TestEqual(TEXT("coarse and fine density match"), CoarseState->DensityLevel, FineState->DensityLevel);
	TestEqual(TEXT("coarse and fine stage match"), CoarseState->Stage, FineState->Stage);
	TestEqual(TEXT("coarse and fine treasury match"), CoarseCity.GetFunds(), FineCity.GetFunds());

	const FCityPlacedEntity& Preserved = FineCity.GetEntities().FindChecked(FineId);
	TestEqual(TEXT("upgrade preserves entity identity"), Preserved.EntityId, FineId);
	TestEqual(TEXT("upgrade preserves anchor X"), Preserved.Anchor.X, 10);
	TestEqual(TEXT("upgrade preserves rotation"), Preserved.QuarterTurnsClockwise, 1);
	const int64 BeforeRefund = FineCity.GetFunds();
	const FCityDemolitionResult Demolition = FineCity.DemolishEntity(FineId);
	TestTrue(TEXT("upgraded lot remains demolishable"), Demolition.IsSuccess());
	TestEqual(TEXT("refund uses recorded placement cost"), Demolition.Refund,
	          Home.Cost * FCityPlacementModel::DemolitionRefundPercent / 100);
	TestEqual(TEXT("refund reaches treasury"), FineCity.GetFunds(), BeforeRefund + Demolition.Refund);
	Fine.Reconcile(FineCity, Catalog);
	TestNull(TEXT("demolition removes development state"), Fine.GetSnapshot().Find(FineId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDevelopmentInfillDemandTest, "CitySim.Development.InfillBeforeDensityUpgrade",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDevelopmentInfillDemandTest::RunTest(const FString& Parameters)
{
	const FCityBuildingDefinition Home = MakeHome();
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions.Add(Home);
	FCityPlacementModel City(5000);
	const uint64 HomeId = PlaceHome(City, Home);
	FCityDevelopmentSimulation Development;
	FCityServiceSnapshot Services;
	Services.PowerCapacity = 1000;
	Services.ServiceCapacity = 1000;
	FCityEnvironmentSnapshot Environment;
	FCityEconomyTuning Tuning;
	Tuning.BaseResidentialDesirability = 100;
	Tuning.MinimumResidentialDesirability = 0;
	FCityNeedsSnapshot Needs;
	Needs.ResidentialDemandBalance = 20;
	Needs.ResidentialEligibleFootprints = 2;
	Development.AdvanceToStep(4, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("moderate demand remains reserved for two new low-density lots"),
	          Development.GetSnapshot().States.FindChecked(HomeId).DensityLevel, ECityDensityLevel::Low);

	Needs.ResidentialDemandBalance = 35;
	Development.AdvanceToStep(5, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("demand beyond the infill reserve permits a density upgrade"),
	          Development.GetSnapshot().States.FindChecked(HomeId).DensityLevel, ECityDensityLevel::Medium);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDevelopmentDemandChannelMatrixTest, "CitySim.Development.DemandChannelMatrix",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDevelopmentDemandChannelMatrixTest::RunTest(const FString& Parameters)
{
	const TArray<TPair<ECityDevelopmentDemandType, FName>> Families = {
	    {ECityDevelopmentDemandType::Residential, TEXT("Home")},
	    {ECityDevelopmentDemandType::Commercial, TEXT("Shop")},
	    {ECityDevelopmentDemandType::Industrial, TEXT("Factory")}};
	for (const TPair<ECityDevelopmentDemandType, FName>& Family : Families)
	{
		const FCityBuildingDefinition Definition = MakeDemandFamily(Family.Value, Family.Key);
		UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
		Catalog->Definitions.Add(Definition);
		FCityPlacementModel City(5000);
		const uint64 EntityId = PlaceHome(City, Definition);
		FCityDevelopmentSimulation Development;
		FCityServiceSnapshot Services;
		Services.PowerCapacity = 1000;
		Services.ServiceCapacity = 1000;
		FCityNeedsSnapshot Needs;
		int32* Balance = Family.Key == ECityDevelopmentDemandType::Commercial   ? &Needs.CommercialDemandBalance
		                 : Family.Key == ECityDevelopmentDemandType::Industrial ? &Needs.IndustrialDemandBalance
		                                                                        : &Needs.ResidentialDemandBalance;
		int32* Eligible = Family.Key == ECityDevelopmentDemandType::Commercial   ? &Needs.CommercialEligibleFootprints
		                  : Family.Key == ECityDevelopmentDemandType::Industrial ? &Needs.IndustrialEligibleFootprints
		                                                                         : &Needs.ResidentialEligibleFootprints;
		*Balance = 20;
		*Eligible = 2;
		FCityEnvironmentSnapshot Environment;
		FCityEconomyTuning Tuning;
		Tuning.BaseResidentialDesirability = 100;
		Development.AdvanceToStep(4, City, Catalog, Services, Needs, Environment, Tuning);
		TestEqual(FString::Printf(TEXT("%s moderate demand remains low-density infill"), *Family.Value.ToString()),
		          Development.GetSnapshot().States.FindChecked(EntityId).DensityLevel, ECityDensityLevel::Low);
		*Balance = 35;
		Development.AdvanceToStep(5, City, Catalog, Services, Needs, Environment, Tuning);
		TestEqual(FString::Printf(TEXT("%s high own-channel demand permits redevelopment"), *Family.Value.ToString()),
		          Development.GetSnapshot().States.FindChecked(EntityId).DensityLevel, ECityDensityLevel::Medium);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDevelopmentLocalPressureSelectionTest,
                                 "CitySim.Development.LocalPressureSelection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDevelopmentLocalPressureSelectionTest::RunTest(const FString& Parameters)
{
	FCityBuildingDefinition Shop = MakeDemandFamily(TEXT("Shop"), ECityDevelopmentDemandType::Commercial);
	Shop.ConstructionSteps = 0;
	for (FCityBuildingLevelDefinition& Level : Shop.DensityLevels)
	{
		Level.MinimumOperationalSteps = 0;
		Level.MinimumDesirability = 0;
	}
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions.Add(Shop);
	FCityPlacementModel City(5000);
	FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
	TestTrue(TEXT("local pressure fixture road commits"), City.CommitRoad(Road).IsSuccess());
	FCityPlacementPreview DirtyPreview = City.BeginPreview(Shop, FCityCellCoord(10, 9));
	const uint64 DirtyId = City.CommitPlacement(DirtyPreview, Shop).EntityId;
	FCityPlacementPreview CleanPreview = City.BeginPreview(Shop, FCityCellCoord(14, 9));
	const uint64 CleanId = City.CommitPlacement(CleanPreview, Shop).EntityId;
	FCityEnvironmentSnapshot Environment;
	for (int32 Y = 9; Y < 11; ++Y)
	{
		for (int32 X = 10; X < 12; ++X)
		{
			Environment.Pollution[Y * FCityGrid::Width + X] = 300;
		}
	}
	FCityNeedsSnapshot Needs;
	Needs.CommercialDemandBalance = 100;
	FCityServiceSnapshot Services;
	Services.PowerCapacity = 1000;
	Services.ServiceCapacity = 1000;
	FCityEconomyTuning Tuning;
	FCityDevelopmentSimulation Development;
	Development.AdvanceToStep(1, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("clean commercial parcel wins deterministic local pressure"),
	          Development.GetSnapshot().States.FindChecked(CleanId).DensityLevel, ECityDensityLevel::Medium);
	TestEqual(TEXT("lower-pressure commercial parcel remains low"),
	          Development.GetSnapshot().States.FindChecked(DirtyId).DensityLevel, ECityDensityLevel::Low);
	const FCityDevelopmentPressure* DirtyPressure = Development.GetSnapshot().LatestPressures.Find(DirtyId);
	const FCityDevelopmentPressure* CleanPressure = Development.GetSnapshot().LatestPressures.Find(CleanId);
	TestNotNull(TEXT("dirty pressure is inspectable"), DirtyPressure);
	TestNotNull(TEXT("clean pressure is inspectable"), CleanPressure);
	if (DirtyPressure && CleanPressure)
	{
		TestTrue(TEXT("clean parcel has higher pressure score"), CleanPressure->Total > DirtyPressure->Total);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDevelopmentDensityAccountingTest, "CitySim.Development.DensityAccounting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDevelopmentDensityAccountingTest::RunTest(const FString& Parameters)
{
	FCityBuildingDefinition Shop = MakeDemandFamily(TEXT("Shop"), ECityDevelopmentDemandType::Commercial);
	FCityBuildingDefinition Factory = MakeDemandFamily(TEXT("Factory"), ECityDevelopmentDemandType::Industrial);
	Shop.ConstructionSteps = 0;
	Factory.ConstructionSteps = 0;
	for (int32 Index = 0; Index < Factory.DensityLevels.Num(); ++Index)
	{
		Factory.DensityLevels[Index].PollutionEmission = 10 << Index;
	}
	const FCityBuildingDefinition Power = MakeUtility(TEXT("Power"), 100);
	const FCityBuildingDefinition Water = MakeUtility(TEXT("Service"), 100);
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {Shop, Factory, Power, Water};
	FCityPlacementModel City(10000);
	FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
	TestTrue(TEXT("density accounting road commits"), City.CommitRoad(Road).IsSuccess());
	PlaceAt(City, Power, FCityCellCoord(1, 9));
	PlaceAt(City, Water, FCityCellCoord(3, 9));
	FCityPlacementPreview ShopPreview = City.BeginPreview(Shop, FCityCellCoord(10, 9));
	const uint64 ShopId = City.CommitPlacement(ShopPreview, Shop).EntityId;
	FCityPlacementPreview FactoryPreview = City.BeginPreview(Factory, FCityCellCoord(14, 9));
	const uint64 FactoryId = City.CommitPlacement(FactoryPreview, Factory).EntityId;
	FCityDevelopmentSnapshot SavedDevelopment;
	for (const uint64 EntityId : {ShopId, FactoryId})
	{
		FCityDevelopmentState State;
		State.EntityId = EntityId;
		State.Stage = ECityDevelopmentStage::Operational;
		State.DensityLevel = ECityDensityLevel::High;
		SavedDevelopment.States.Add(EntityId, State);
	}
	FCityDevelopmentSimulation Development;
	FString RestoreError;
	TestTrue(TEXT("high density fixture restores"),
	         Development.RestoreState(SavedDevelopment, City, Catalog, 0, RestoreError));
	FCityEconomyTuning Tuning;
	const FCityServiceSnapshot Services =
	    FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, &Development.GetSnapshot());
	TestEqual(TEXT("high commercial jobs are counted"), Services.CommercialJobs, 40);
	TestEqual(TEXT("high industrial jobs are counted"), Services.IndustrialJobs, 40);
	TestEqual(TEXT("all level jobs reach aggregate needs"), Services.Jobs, 80);
	TestEqual(TEXT("high commercial output is counted"), Services.CommercialOutput, 40);
	TestEqual(TEXT("high industrial output is counted"), Services.IndustrialOutput, 40);
	TestEqual(TEXT("high commercial upkeep is counted"), Services.CommercialUpkeep, int64(20));
	TestEqual(TEXT("high industrial upkeep is counted"), Services.IndustrialUpkeep, int64(20));
	TestEqual(TEXT("high density power demand is counted"), Services.PowerDemand, 80);
	TestEqual(TEXT("high density water demand is counted"), Services.ServiceDemand, 80);
	TestEqual(TEXT("high industrial pollution is exposed"), Services.EntityPollutionEmissions.FindRef(FactoryId), 40);
	return true;
}

#endif
