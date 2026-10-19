#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Persistence/CityPersistence.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"

namespace
{
uint64 FreightLot(FCityPlacementModel& City, const FName Id, const FCityCellCoord Cell)
{
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = Id;
	Definition.DisplayName = Id.ToString();
	Definition.Footprint = FCityCellFootprint(1, 1);
	Definition.Cost = 0;
	Definition.EntranceSides = 15;
	auto Preview = City.BeginPreview(Definition, Cell);
	const auto Result = City.CommitPlacement(Preview, Definition);
	check(Result.IsSuccess());
	return Result.EntityId;
}
void FreightRoad(FCityPlacementModel& City)
{
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	check(City.CommitRoad(Road).IsSuccess());
}
int64 AccountedGoods(const FCityPopulationSnapshot& State)
{
	int64 Total = State.DiscardedGoods;
	for (const auto& Business : State.Businesses)
		Total += Business.Inventory + Business.ExportedGoods;
	for (const auto& Trip : State.TransitTrips)
		if (!Trip.bCompleted && (Trip.Purpose == ECityTripPurpose::FreightDelivery ||
		                         Trip.Purpose == ECityTripPurpose::FreightExport))
			Total += Trip.Quantity;
	return Total;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFreightCargoTest, "CitySim.Simulation.FreightCargo",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFreightCargoTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	FreightRoad(City);
	const uint64 FactoryId = FreightLot(City, TEXT("Factory"), {18, 9});
	const uint64 ShopId = FreightLot(City, TEXT("Shop"), {4, 9});
	FCityPopulationSnapshot Initial;
	FCityBusinessLedgerRecord Factory;
	Factory.EntityId = FactoryId;
	Factory.bIndustrial = true;
	Factory.bInitialFreightReceived = true;
	Factory.Inventory = 50;
	Factory.PendingOrders = 20;
	Factory.Balance = 100;
	FCityBusinessLedgerRecord Shop;
	Shop.EntityId = ShopId;
	Shop.bInitialFreightReceived = true;
	Shop.PendingOrders = 10;
	Shop.Balance = 100;
	Initial.Businesses = {Factory, Shop};
	FCityPopulationSimulation Population;
	FString Error, Json;
	TestTrue(TEXT("cargo fixture restores"), Population.RestoreState(Initial, Error));
	Population.QueueFreight(1);
	Population.QueueFreight(1);
	TestEqual(TEXT("one outstanding truck per factory"), Population.GetSnapshot().TransitTrips.Num(), 1);
	TestEqual(TEXT("local restocking has priority"),
	          Population.GetSnapshot().TransitTrips[0].DestinationEntityId, ShopId);
	TestEqual(TEXT("dispatch reserves only shop demand"), Population.GetSnapshot().TransitTrips[0].Quantity,
	          10);
	TestEqual(TEXT("shop cannot use in-transit goods"), Population.GetSnapshot().Businesses[1].Inventory, 0);
	TestEqual(TEXT("dispatch conserves goods"), AccountedGoods(Population.GetSnapshot()), int64(50));
	Population.AdvanceTransitToStep(1000);
	TestFalse(TEXT("timer cannot deliver stock"), Population.GetSnapshot().TransitTrips[0].bCompleted);
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityTrafficSimulation Traffic;
	Traffic.UpdateTrips(City, nullptr, Population.GetSnapshot(), Network, 1);
	Traffic.AdvanceOneStep(Network, 1);
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Population = Population.GetSnapshot();
	Save.Traffic = Traffic.GetSnapshot();
	Save.Economy.FixedStepId = 1;
	Save.Disasters.LastFixedStepId = 1; // Current save authorities share the economy clock.
	if (!TestTrue(TEXT("cargo saves"), FCityPersistence::Serialize(Save, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("cargo JSON loads"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	FCityPopulationSimulation Resumed;
	FCityTrafficSimulation ResumedTraffic;
	TestTrue(TEXT("cargo resumes"), Resumed.RestoreState(Loaded.Population, Error));
	TestTrue(TEXT("cargo traffic resumes"), ResumedTraffic.RestoreState(Loaded.Traffic, Network, 1, Error));
	bool bSawExport = false;
	for (int64 Step = 2; Step <= 60; ++Step)
	{
		for (int32 Run = 0; Run < 2; ++Run)
		{
			auto& P = Run == 0 ? Population : Resumed;
			auto& T = Run == 0 ? Traffic : ResumedTraffic;
			P.PruneTransitTrips(City);
			P.QueueFreight(Step);
			T.UpdateTrips(City, nullptr, P.GetSnapshot(), Network, Step);
			T.AdvanceOneStep(Network, Step);
			for (const auto& Vehicle : T.GetSnapshot().Vehicles)
			{
				bSawExport |= Vehicle.DestinationEntityId == 0;
				if (Vehicle.bCompleted)
				{
					P.CompleteTransitTrip(Vehicle.SourceId, Step);
					P.CompleteTransitTrip(Vehicle.SourceId, Step);
				}
			}
			TestEqual(TEXT("stock plus cargo plus exports stays constant"), AccountedGoods(P.GetSnapshot()),
			          int64(50));
		}
	}
	const auto& State = Population.GetSnapshot();
	TestTrue(TEXT("export trucks travel to city entrance"), bSawExport);
	TestEqual(TEXT("retail receives ten units exactly once"), State.Businesses[1].Inventory, 10);
	TestEqual(TEXT("surplus leaves as explicit exports"), State.Businesses[0].ExportedGoods, int64(40));
	TestEqual(TEXT("retailer pays wholesale only on arrival"), State.Businesses[1].Balance, int64(80));
	TestEqual(TEXT("factory receives wholesale and external export payment"), State.Businesses[0].Balance,
	          int64(200));
	TestEqual(TEXT("resume preserves export count"), Resumed.GetSnapshot().Businesses[0].ExportedGoods,
	          State.Businesses[0].ExportedGoods);
	Save.Population = State;
	TestTrue(TEXT("export ledger saves"), FCityPersistence::Serialize(Save, Json, Error));
	TestTrue(TEXT("export ledger loads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("export totals persist"), Loaded.Population.Businesses[0].ExportedGoods, int64(40));
	TestTrue(TEXT("reset cancellation fixture"), Population.RestoreState(Initial, Error));
	Population.QueueFreight(1);
	City.DemolishEntity(ShopId);
	Population.PruneTransitTrips(City);
	Population.PruneTransitTrips(City);
	TestEqual(TEXT("demolished destination returns cargo to factory once"),
	          Population.GetSnapshot().Businesses[0].Inventory, 50);
	TestEqual(TEXT("return is not discarded"), Population.GetSnapshot().DiscardedGoods, int64(0));
	TestTrue(TEXT("reset source demolition fixture"), Population.RestoreState(Initial, Error));
	Population.QueueFreight(1);
	City.DemolishEntity(FactoryId);
	Population.PruneTransitTrips(City);
	FCityEconomyTuning Tuning;
	Population.Reconcile(City, FCityEconomySnapshot(), Tuning);
	TestEqual(TEXT("demolished stock and orphaned cargo are explicitly discarded"),
	          Population.GetSnapshot().DiscardedGoods, int64(50));
	Save.Population = Population.GetSnapshot();
	TestTrue(TEXT("discard ledger saves"), FCityPersistence::Serialize(Save, Json, Error));
	TestTrue(TEXT("discard ledger loads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("discard ledger persists"), Loaded.Population.DiscardedGoods, int64(50));
	Save.Population = Initial;
	TestTrue(TEXT("legacy fixture saves"), FCityPersistence::Serialize(Save, Json, Error));
	TestTrue(TEXT("Current freight state loads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestTrue(TEXT("Goods state restores"), Population.RestoreState(Loaded.Population, Error));
	Population.QueueFreight(1);
	FCityPopulationSnapshot Invalid = Population.GetSnapshot();
	Invalid.TransitTrips[0].Quantity = FCityPopulationSimulation::FreightLoadSize + 1;
	TestFalse(TEXT("oversized cargo is rejected"), Population.RestoreState(Invalid, Error));
	TestEqual(TEXT("invalid restore is atomic"), Population.GetSnapshot().TransitTrips[0].Quantity, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFreightIntegrationTest,
                                 "CitySim.Simulation.FreightFixedStepIntegration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFreightIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* FineWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FreightFine"), nullptr, true,
	                                        ERHIFeatureLevel::Num, &Values);
	UWorld* CoarseWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FreightCoarse"), nullptr, true,
	                                          ERHIFeatureLevel::Num, &Values);
	auto* Fine = FineWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Coarse = CoarseWorld->GetSubsystem<UCitySimulationSubsystem>();
	for (auto* Sim : {Fine, Coarse})
	{
		Sim->PlacementModel->SetTerrain(nullptr);
		FreightRoad(*Sim->PlacementModel);
		FreightLot(*Sim->PlacementModel, TEXT("Power"), {1, 9});
		FreightLot(*Sim->PlacementModel, TEXT("Service"), {2, 9});
		FreightLot(*Sim->PlacementModel, TEXT("Home"), {4, 9});
		// Fill both retail and industrial jobs so this fixture exercises surplus exports.
		FreightLot(*Sim->PlacementModel, TEXT("Home"), {6, 9});
		FreightLot(*Sim->PlacementModel, TEXT("Home"), {8, 9});
		FreightLot(*Sim->PlacementModel, TEXT("Shop"), {14, 9});
		FreightLot(*Sim->PlacementModel, TEXT("Factory"), {18, 9});
		Sim->EconomyTuning.FixedStepSeconds = 0.125;
		Sim->EconomyTuning.FixedStepsPerMonth = 4;
		Sim->EconomyTuning.MaxCatchUpSteps = 8;
		Sim->EconomyTuning.WaterRangeRoadHops = 64;
		Sim->EconomyTuning.CrimePopulationThreshold = 5;
		Sim->Scenario.Start();
		Sim->SetSimulationPaused(false);
	}
	int64 Purchases = 0, Produced = 0;
	for (int32 Group = 0; Group < 16; ++Group)
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const int64 Month = Fine->GetEconomySnapshot().LastBilledMonthId;
			const int32 ArrivalsBeforeStep = Fine->GetPopulationSnapshot().SalesThisMonth;
			Fine->Tick(0.125f);
			if (Fine->GetEconomySnapshot().LastBilledMonthId != Month)
			{
				TestEqual(TEXT("monthly sales close actual arrivals"), Fine->GetEconomySnapshot().GoodsSold,
				          ArrivalsBeforeStep);
				TestEqual(TEXT("commercial tax uses arrivals, not reservations"),
				          Fine->GetEconomySnapshot().LastCommercialTaxRevenue,
				          FCityEconomySimulation::CalculateTaxRevenue(
				              ArrivalsBeforeStep, Fine->EconomyTuning.IncomePerOperationalShop,
				              Fine->GetEconomySnapshot().CommercialTaxBasisPoints) /
				              FMath::Max(1, Fine->EconomyTuning.JobsPerOperationalShop));
				Produced += Fine->GetEconomySnapshot().GoodsProduced;
				TestTrue(TEXT("actual purchases reconcile every settlement"),
				         Fine->Population.MatchesEconomy(Fine->GetEconomySnapshot(), Fine->EconomyTuning));
			}
			Purchases = Fine->GetPopulationSnapshot().TotalArrivalPurchases;
			TestEqual(TEXT("Goods guide advances on actual arrivals, before month-end"),
			          Fine->GetGuidedProgress().CompletedIds.Contains(TEXT("goods")), Purchases > 0);
			TestEqual(TEXT("all production is stock, cargo, exports, purchases, or disposal"),
			          AccountedGoods(Fine->GetPopulationSnapshot()) + Purchases, Produced);
			int32 ExpectedOutput = 0;
			FCityEconomySnapshot CurrentCapacity = Fine->GetEconomySnapshot();
			CurrentCapacity.Residents = Fine->GetServiceSnapshot().Residents;
			CurrentCapacity.Jobs = Fine->GetServiceSnapshot().Jobs;
			CurrentCapacity.EmployedResidents = FMath::Min(CurrentCapacity.Residents, CurrentCapacity.Jobs);
			CurrentCapacity.RetailCapacity = Fine->GetServiceSnapshot().CommercialOutput;
			const int32 ExpectedSales = FCityPopulationSimulation::ForecastGoodsSales(
			    Fine->GetPlacementModel(), CurrentCapacity, Fine->EconomyTuning, Fine->GetServiceSnapshot(),
			    Fine->GetPopulationSnapshot(), nullptr, &ExpectedOutput);
			TestEqual(TEXT("needs output uses staffed production"), Fine->GetNeedsSnapshot().GoodsProduced,
			          ExpectedOutput);
			TestEqual(TEXT("needs sales use stock and household affordability"),
			          Fine->GetNeedsSnapshot().GoodsSold, ExpectedSales);
		}
		Coarse->Tick(1.0f);
		FString FineJson, CoarseJson, Error;
		TestTrue(TEXT("fine state serializes"),
		         FCityPersistence::Serialize(Fine->BuildSaveDocument(), FineJson, Error));
		TestTrue(TEXT("coarse state serializes"),
		         FCityPersistence::Serialize(Coarse->BuildSaveDocument(), CoarseJson, Error));
		TestEqual(TEXT("catch-up matches entire fine-step save"), CoarseJson, FineJson);
	}
	TestTrue(TEXT("households buy delivered goods"), Purchases > 0);
	TestTrue(TEXT("businesses produce goods"), Produced > 0);
	TestTrue(TEXT("Delivered purchases complete guided goods milestone"),
	         Fine->GetGuidedProgress().CompletedIds.Contains(TEXT("goods")));
	TestTrue(TEXT("Full catch-up fixture includes introduced crime and grace"),
	         Fine->GetCrimeSnapshot().IsIntroduced());
	TestTrue(TEXT("surplus exports complete"), Fine->GetPopulationSnapshot().Businesses.ContainsByPredicate(
	                                               [](const auto& B) { return B.ExportedGoods > 0; }));
	FineWorld->DestroyWorld(false);
	CoarseWorld->DestroyWorld(false);
	return true;
}
#endif
