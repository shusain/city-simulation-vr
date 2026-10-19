#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityNeedsPresentation.h"
#include "Simulation/CityFirstPlayableGuide.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
FCityBuildingDefinition PrivateFamily(const FName Id, const ECityDevelopmentDemandType Type)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Id;
	D.DisplayName = Id.ToString();
	D.Footprint = FCityCellFootprint(2, 2);
	D.Cost = 1000;
	D.DevelopmentDemandType = Type;
	D.ConstructionSteps = 2;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FCityBuildingLevelDefinition L;
		L.Level = static_cast<ECityDensityLevel>(Index);
		L.ResidentsProvided = Type == ECityDevelopmentDemandType::Residential ? 10 << Index : 0;
		L.JobsProvided = Type != ECityDevelopmentDemandType::Residential ? 10 << Index : 0;
		L.EconomicOutput = L.JobsProvided;
		L.UpgradeCost = Index * 2000;
		L.MinimumOperationalSteps = 2;
		L.MinimumDemand = 5;
		D.DensityLevels.Add(L);
	}
	return D;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPrivateDevelopmentTest,
                                 "CitySim.Development.PrivateFundingAndSavedContinuation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPrivateDevelopmentTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(20000);
	FCityFirstPlayableGuideState Guide;
	Guide.IndustryCost = 1000;
	TestTrue(
	    TEXT("industrial zoning objective explains private funding at zero treasury"),
	    FCityFirstPlayableGuide::BuildMilestone(TEXT("goods"), Guide).Hint.Contains(TEXT("privately built")));
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {PrivateFamily(TEXT("Home"), ECityDevelopmentDemandType::Residential),
	                        PrivateFamily(TEXT("Shop"), ECityDevelopmentDemandType::Commercial),
	                        PrivateFamily(TEXT("Factory"), ECityDevelopmentDemandType::Industrial)};
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& D : Catalog->Definitions)
		Definitions.Add(D.DefinitionId, D);
	const auto& Home = Catalog->Definitions[0];
	const auto& Shop = Catalog->Definitions[1];
	const auto& Industry = Catalog->Definitions[2];
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {30, 8});
	const int64 BeforeRoad = City.GetFunds();
	const auto PurchasedRoad = City.CommitRoad(Road);
	TestTrue(TEXT("public road purchased"), PurchasedRoad.IsSuccess());
	TestEqual(TEXT("public road still charges treasury"), BeforeRoad - City.GetFunds(), PurchasedRoad.Cost);
	for (const FName Id : {FName(TEXT("Power")), FName(TEXT("Service"))})
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Id;
		D.DisplayName = Id.ToString();
		D.Footprint = FCityCellFootprint(2, 2);
		D.Cost = 500;
		Definitions.Add(Id, D);
		auto P = City.BeginPreview(D, {Id == TEXT("Power") ? 0 : 2, 9});
		TestEqual(TEXT("public facility cannot use private funding"),
		          City.ValidatePlacement(P, D, ECityConstructionFunding::PrivateDeveloper).Failure,
		          ECityPlacementFailure::InvalidDefinition);
		const int64 Before = City.GetFunds();
		TestTrue(TEXT("public facility purchased"), City.CommitPlacement(P, D).IsSuccess());
		TestEqual(TEXT("public facility charges treasury"), Before - City.GetFunds(), D.Cost);
	}
	TestTrue(TEXT("fixture exhausts treasury"), City.TrySpendFunds(City.GetFunds()));
	auto Unaffordable = City.BeginPreview(Home, {24, 9});
	TestEqual(TEXT("manual purchase retains insufficient funds failure"),
	          City.CommitPlacement(Unaffordable, Home).Failure, ECityPlacementFailure::InsufficientFunds);
	auto NoRoadFunds = City.BeginRoadPreview({31, 8});
	City.UpdateRoadPreviewEnd(NoRoadFunds, {32, 8});
	TestEqual(TEXT("road purchase retains insufficient funds failure"), City.CommitRoad(NoRoadFunds).Failure,
	          ECityRoadFailure::InsufficientFunds);

	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 100;
	Tuning.PowerCapacityPerBuilding = 10000;
	Tuning.WaterCapacityPerTreatment = 10000;
	Tuning.BaseResidentialDesirability = 100;
	FCityGrowthSimulation Growth(42);
	TArray<uint64> PrivateIds;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const auto Zone = static_cast<ECityZoneType>(Index + 1);
		TestTrue(TEXT("private lot zoned"), City.SetZoneArea({5 + Index * 4, 9}, {2, 2}, Zone));
		const auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
		auto Needs =
		    FCityNeedsEvaluator::BuildSnapshot(City, Services, Tuning, &Home, &Shop, false, &Industry);
		if (Index == 0)
		{
			TestEqual(TEXT("zero treasury leaves serviced residential growth ready"),
			          Needs.ResidentialConstraint, ECityDemandConstraint::None);
			TestEqual(TEXT("zoned city construction cost is zero"), Needs.ResidentialGrowthCost, int64(0));
		}
		// Isolate each family from the separate demand-balancing/density task.
		Needs.RecommendedGrowthZone = Zone;
		const auto Result = Growth.AdvanceToStep((Index + 1) * Tuning.GrowthIntervalSteps, City, Services,
		                                         Needs, Tuning, &Home, &Shop, false, &Industry);
		if (!TestTrue(TEXT("each zoned family builds with no treasury funds"), Result.IsSuccess()))
			return false;
		PrivateIds.Add(Result.EntityId);
		TestEqual(TEXT("private construction leaves treasury at zero"), City.GetFunds(), int64(0));
		TestEqual(TEXT("private construction records no refundable city purchase"),
		          City.GetEntities().FindChecked(Result.EntityId).RecordedPlacementCost, int64(0));
	}
	const int32 BeforeOverlapCount = City.GetEntities().Num();
	auto Overlap = City.BeginPreview(Home, {5, 9});
	TestEqual(TEXT("private funding cannot bypass occupancy validation"),
	          City.CommitPlacement(Overlap, Home, ECityConstructionFunding::PrivateDeveloper).Failure,
	          ECityPlacementFailure::Occupied);
	TestEqual(TEXT("failed private placement is atomic"), City.GetEntities().Num(), BeforeOverlapCount);
	TestEqual(TEXT("failed private placement leaves treasury alone"), City.GetFunds(), int64(0));
	// A documented income flow funds one manual/debug home, representing a legacy city-paid building.
	City.ApplyMonthlyEconomy(Home.Cost, 0);
	auto Manual = City.BeginPreview(Home, {20, 9});
	const int64 BeforeManual = City.GetFunds();
	const uint64 ManualId = City.CommitPlacement(Manual, Home).EntityId;
	TestEqual(TEXT("manual building remains a public purchase"), BeforeManual - City.GetFunds(), Home.Cost);
	FCityDevelopmentSimulation Development;
	FCityServiceSnapshot Services;
	Services.PowerCapacity = 10000;
	Services.ServiceCapacity = 10000;
	FCityNeedsSnapshot Needs;
	Needs.ResidentialDemandBalance = Needs.CommercialDemandBalance = Needs.IndustrialDemandBalance = 100;
	FCityEnvironmentSnapshot Environment;
	Development.AdvanceToStep(0, City, Catalog, Services, Needs, Environment, Tuning);
	Development.AdvanceToStep(0, City, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("paused construction stays pending"), Development.GetSnapshot().Find(PrivateIds[0])->Stage,
	          ECityDevelopmentStage::Construction);
	Development.AdvanceToStep(4, City, Catalog, Services, Needs, Environment, Tuning);
	for (const uint64 Id : PrivateIds)
		TestEqual(TEXT("each private family upgrades to medium with zero funds"),
		          Development.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::Medium);
	TestEqual(TEXT("medium upgrades spend no treasury"), City.GetFunds(), int64(0));

	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy.FixedStepId = 4;
	Save.Development = Development.GetSnapshot();
	Save.bSimulationPaused = true;
	FString Json, Error;
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("private funding saves"), FCityPersistence::Serialize(Save, Json, Error)) ||
	    !TestTrue(TEXT("private funding reloads"), FCityPersistence::Deserialize(Json, Loaded, Error)))
	{
		AddError(Error);
		return false;
	}
	auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
	if (!TestTrue(TEXT("saved placement model restores"), Restored.IsValid()))
		return false;
	FCityDevelopmentSimulation Continued;
	if (!TestTrue(TEXT("saved development restores"),
	              Continued.RestoreState(Loaded.Development, *Restored, Catalog, 4, Error)))
		return false;
	TestTrue(TEXT("pause survives save"), Loaded.bSimulationPaused);
	Continued.AdvanceToStep(4, *Restored, Catalog, Services, Needs, Environment, Tuning);
	TestEqual(TEXT("saved paused step does not upgrade"),
	          Continued.GetSnapshot().Find(PrivateIds[0])->DensityLevel, ECityDensityLevel::Medium);
	Continued.AdvanceToStep(10, *Restored, Catalog, Services, Needs, Environment, Tuning);
	for (const uint64 Id : PrivateIds)
	{
		TestEqual(TEXT("each family reaches high density after saved continuation"),
		          Continued.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::High);
		TestEqual(TEXT("private demolition gives no city refund"), Restored->DemolishEntity(Id).Refund,
		          int64(0));
	}
	TestEqual(TEXT("private upgrades and demolition never create city cash"), Restored->GetFunds(), int64(0));
	TestEqual(TEXT("legacy/manual purchase cost survives private upgrades and save"),
	          Restored->GetEntities().FindChecked(ManualId).RecordedPlacementCost, Home.Cost);
	TestEqual(TEXT("legacy/manual refund remains based only on original city purchase"),
	          Restored->DemolishEntity(ManualId).Refund,
	          Home.Cost * FCityPlacementModel::DemolitionRefundPercent / 100);
	return true;
}

#endif
