#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FCityBuildingDefinition PopulationDefinition(const FName Id)
{
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = Id;
	Definition.DisplayName = Id.ToString();
	Definition.Footprint = FCityCellFootprint(1, 1);
	Definition.Cost = 0;
	Definition.LogicalHeightCm = 100.0;
	return Definition;
}

uint64 PlacePopulation(FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
                       const FCityCellCoord Cell)
{
	FCityPlacementPreview Preview = City.BeginPreview(Definition, Cell);
	const FCityPlacementResult Result = City.CommitPlacement(Preview, Definition);
	check(Result.IsSuccess());
	return Result.EntityId;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPopulationRecordsTest, "CitySim.Simulation.PersistentPopulationRecords",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPopulationRecordsTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	const FCityBuildingDefinition Home = PopulationDefinition(TEXT("Home"));
	const FCityBuildingDefinition Shop = PopulationDefinition(TEXT("Shop"));
	const FCityBuildingDefinition Factory = PopulationDefinition(TEXT("Factory"));
	PlacePopulation(City, Home, FCityCellCoord(1, 1));
	PlacePopulation(City, Shop, FCityCellCoord(2, 1));
	PlacePopulation(City, Factory, FCityCellCoord(3, 1));
	PlacePopulation(City, Shop, FCityCellCoord(4, 1));
	PlacePopulation(City, Factory, FCityCellCoord(5, 1));
	FCityEconomyTuning Tuning;
	FCityEconomySnapshot Economy;
	Economy.Residents = 10;
	Economy.Jobs = 10;
	Economy.EmployedResidents = 10;
	Economy.HouseholdGoodsDemand = 5;
	Economy.RetailCapacity = 10;
	Economy.GoodsProduced = 20;
	Economy.GoodsSold = 5;
	Economy.HouseholdIncome = 50;
	Economy.HouseholdSpending = 25;
	Economy.LastBilledMonthId = 1;
	FCityPopulationSimulation First;
	First.AdvanceToMonth(City, Economy, Tuning);
	const FCityPopulationSnapshot& FirstSnapshot = First.GetSnapshot();
	TestEqual(TEXT("one operational home has ten persistent citizens"), FirstSnapshot.Citizens.Num(), 10);
	TestEqual(TEXT("one home owns one persistent household"), FirstSnapshot.Households.Num(), 1);
	TestEqual(TEXT("each shop and factory owns a ledger"), FirstSnapshot.Businesses.Num(), 4);
	TestTrue(TEXT("workers keep an explicit workplace"), FirstSnapshot.Citizens[0].WorkplaceEntityId != 0);
	TestEqual(TEXT("affordable orders create exact completed purchases"),
	          First.BuildAggregate(Tuning).GoodsSold, 5);
	TestEqual(TEXT("purchases create authoritative household spending"),
	          First.BuildAggregate(Tuning).HouseholdSpending, int64(25));
	TestEqual(TEXT("purchases emit route-independent trip intents"), FirstSnapshot.Citizens[0].TripPurpose,
	          ECityTripPurpose::Purchase);
	TestTrue(TEXT("record sums reconcile with the aggregate economy"), First.MatchesEconomy(Economy, Tuning));
	TestEqual(TEXT("multiple shops do not duplicate city-wide retail capacity"),
	          First.BuildAggregate(Tuning).RetailCapacity, Economy.RetailCapacity);
	TestEqual(TEXT("multiple factories do not duplicate city-wide goods output"),
	          First.BuildAggregate(Tuning).GoodsProduced, Economy.GoodsProduced);

	FCityPopulationSimulation Restored;
	FString Error;
	TestTrue(TEXT("population records validate for persistence"),
	         Restored.RestoreState(FirstSnapshot, Error));
	Restored.AdvanceToMonth(City, Economy, Tuning);
	TestEqual(TEXT("reconcile preserves stable citizen ID"), Restored.GetSnapshot().Citizens[0].Id,
	          FirstSnapshot.Citizens[0].Id);
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		Restored.AdvanceToMonth(City, Economy, Tuning);
		TestTrue(TEXT("same-month refresh preserves settled economy after load"),
		         Restored.MatchesEconomy(Economy, Tuning));
		TestEqual(TEXT("same-month refresh preserves completed purchases"),
		          Restored.GetSnapshot().Households[0].CompletedPurchases, 5);
		TestEqual(TEXT("same-month refresh preserves consumption orders"),
		          Restored.GetSnapshot().Households[0].ConsumptionOrders, 5);
		TestEqual(TEXT("same-month refresh does not pay wages or charge purchases again"),
		          Restored.GetSnapshot().Households[0].Balance, FirstSnapshot.Households[0].Balance);
		for (int32 Index = 0; Index < FirstSnapshot.Businesses.Num(); ++Index)
		{
			TestEqual(TEXT("same-month refresh preserves business balance"),
			          Restored.GetSnapshot().Businesses[Index].Balance,
			          FirstSnapshot.Businesses[Index].Balance);
			TestEqual(TEXT("same-month refresh does not produce inventory again"),
			          Restored.GetSnapshot().Businesses[Index].Inventory,
			          FirstSnapshot.Businesses[Index].Inventory);
		}
	}
	// Unemployment leaves previous trip intents alive; those are not new purchases.
	Economy.LastBilledMonthId = 2;
	Economy.Jobs = 0;
	Economy.EmployedResidents = 0;
	Economy.HouseholdIncome = 0;
	Economy.GoodsSold = 0;
	Economy.HouseholdSpending = 0;
	Restored.AdvanceToMonth(City, Economy, Tuning);
	TestTrue(TEXT("next month's zero-sales ledger reconciles"), Restored.MatchesEconomy(Economy, Tuning));
	TestEqual(TEXT("old shopping intent does not count as this month's purchase"),
	          Restored.GetSnapshot().Households[0].CompletedPurchases, 0);
	TestEqual(TEXT("zero-income zero-sales month preserves household money"),
	          Restored.GetSnapshot().Households[0].Balance, FirstSnapshot.Households[0].Balance);

	FCityPlacementModel LargeCity(10000);
	for (int32 Index = 0; Index < 100; ++Index)
	{
		PlacePopulation(LargeCity, Home, FCityCellCoord(Index % 50, Index / 50));
	}
	FCityEconomySnapshot LargeEconomy;
	LargeEconomy.Residents = 1000;
	FCityPopulationSimulation Large;
	Large.Reconcile(LargeCity, LargeEconomy, Tuning);
	TestEqual(TEXT("one thousand residents stay in packed records, not actors"),
	          Large.GetSnapshot().Citizens.Num(), 1000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPopulationDensityTest,
                                 "CitySim.Simulation.PopulationDensityAndOperation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPopulationDensityTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	UCityBuildingCatalog* Catalog = NewObject<UCityBuildingCatalog>();
	for (const FName Id : {FName(TEXT("Home")), FName(TEXT("Shop")), FName(TEXT("Factory"))})
	{
		FCityBuildingDefinition Definition = PopulationDefinition(Id);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FCityBuildingLevelDefinition Level;
			Level.Level = static_cast<ECityDensityLevel>(Index);
			Level.ResidentsProvided = Id == TEXT("Home") ? (10 << Index) + 1 : 0;
			Level.JobsProvided = Id == TEXT("Home") ? 0 : 20 << Index;
			Level.EconomicOutput = Id == TEXT("Home") ? 0 : 20 << Index;
			Definition.DensityLevels.Add(Level);
		}
		Catalog->Definitions.Add(Definition);
	}
	FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
	TestTrue(TEXT("density fixture road commits"), City.CommitRoad(Road).IsSuccess());
	PlacePopulation(City, PopulationDefinition(TEXT("Power")), FCityCellCoord(1, 9));
	PlacePopulation(City, PopulationDefinition(TEXT("Service")), FCityCellCoord(2, 9));
	FCityDevelopmentSnapshot Development;
	auto Place = [&](const FName Id, const FCityCellCoord Cell, const ECityDensityLevel Density,
	                 const ECityDevelopmentStage Stage) {
		const uint64 EntityId = PlacePopulation(City, *Catalog->FindDefinition(Id), Cell);
		FCityDevelopmentState State;
		State.EntityId = EntityId;
		State.DensityLevel = Density;
		State.Stage = Stage;
		Development.States.Add(EntityId, State);
		return EntityId;
	};
	const uint64 ConstructionHome =
	    Place(TEXT("Home"), {3, 9}, ECityDensityLevel::High, ECityDevelopmentStage::Construction);
	const uint64 DisconnectedHome =
	    Place(TEXT("Home"), {30, 30}, ECityDensityLevel::High, ECityDevelopmentStage::Operational);
	const uint64 MediumHome =
	    Place(TEXT("Home"), {4, 9}, ECityDensityLevel::Medium, ECityDevelopmentStage::Operational);
	const uint64 HighHome =
	    Place(TEXT("Home"), {5, 9}, ECityDensityLevel::High, ECityDevelopmentStage::Operational);
	const uint64 ConstructionShop =
	    Place(TEXT("Shop"), {6, 9}, ECityDensityLevel::High, ECityDevelopmentStage::Construction);
	const uint64 LowShop =
	    Place(TEXT("Shop"), {7, 9}, ECityDensityLevel::Low, ECityDevelopmentStage::Operational);
	const uint64 HighShop =
	    Place(TEXT("Shop"), {8, 9}, ECityDensityLevel::High, ECityDevelopmentStage::Operational);
	Place(TEXT("Factory"), {9, 9}, ECityDensityLevel::High, ECityDevelopmentStage::Operational);
	FCityEconomyTuning Tuning;
	Tuning.FixedStepSeconds = 0.1;
	Tuning.FixedStepsPerMonth = 1;
	FCityEconomySimulation Economy;
	Economy.SetPaused(false);
	FCityPopulationSimulation Population;
	auto AdvanceMonth = [&]() {
		Economy.Advance(0.1, City, Tuning, Catalog, &Development);
		const FCityServiceSnapshot Services =
		    FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, &Development);
		Population.AdvanceToMonth(City, Economy.GetSnapshot(), Tuning, &Services);
		TestTrue(TEXT("density records match actual economy totals"),
		         Population.MatchesEconomy(Economy.GetSnapshot(), Tuning));
		return Services;
	};
	FCityServiceSnapshot Services = AdvanceMonth();
	TestEqual(TEXT("medium and high homes contribute authored capacity"),
	          Population.GetSnapshot().Citizens.Num(), 62);
	TestEqual(TEXT("demand rounds once citywide across odd home capacities"),
	          Population.BuildAggregate(Tuning).HouseholdGoodsDemand, 31);
	for (const FCityCitizenRecord& Citizen : Population.GetSnapshot().Citizens)
	{
		TestTrue(TEXT("only operational connected homes receive residents"),
		         Citizen.HomeEntityId == MediumHome || Citizen.HomeEntityId == HighHome);
		TestTrue(TEXT("construction shop receives no workers"),
		         Citizen.WorkplaceEntityId != ConstructionShop);
	}
	TestEqual(TEXT("construction home has no operational capacity"),
	          Services.EntityResidents.FindRef(ConstructionHome), 0);
	TestEqual(TEXT("disconnected home has no operational capacity"),
	          Services.EntityResidents.FindRef(DisconnectedHome), 0);
	for (const FCityBusinessLedgerRecord& Business : Population.GetSnapshot().Businesses)
	{
		TestEqual(TEXT("each business gets its own operational jobs"), Business.JobCapacity,
		          Services.EntityJobs.FindRef(Business.EntityId));
		TestEqual(TEXT("each business gets its own output"), Business.PendingOrders,
		          Services.EntityOutput.FindRef(Business.EntityId));
		if (Business.EntityId == ConstructionShop)
			TestEqual(TEXT("construction shop makes no sales"), Business.CompletedSales, 0);
		if (Business.EntityId == LowShop)
			TestEqual(TEXT("first shop sales stop at its capacity"), Business.CompletedSales, 20);
		if (Business.EntityId == HighShop)
			TestEqual(TEXT("remaining sales use the next operational shop"), Business.CompletedSales, 11);
	}
	const FCityPopulationSnapshot BeforeUpgrade = Population.GetSnapshot();
	// This fixture isolates authored density/capacity against the legacy
	// aggregate-only economy. Fund its second payroll explicitly; insolvency
	// and the population-aware forecast are covered by PayrollSolvency.
	FCityPopulationSnapshot Funded = BeforeUpgrade;
	for (FCityBusinessLedgerRecord& Business : Funded.Businesses)
		Business.Balance += 10000;
	FString FundingError;
	if (!TestTrue(TEXT("density fixture funding restores"), Population.RestoreState(Funded, FundingError)))
		return false;
	Development.States.FindChecked(MediumHome).DensityLevel = ECityDensityLevel::High;
	Services = AdvanceMonth();
	TestEqual(TEXT("upgrade expands resident records"), Population.GetSnapshot().Citizens.Num(), 82);
	for (const FCityCitizenRecord& Previous : BeforeUpgrade.Citizens)
	{
		TestTrue(
		    TEXT("upgrade retains existing citizen identity and home"),
		    Population.GetSnapshot().Citizens.ContainsByPredicate([&](const FCityCitizenRecord& Current) {
			    return Current.Id == Previous.Id && Current.HomeEntityId == Previous.HomeEntityId;
		    }));
	}
	FCityPopulationSimulation Restored;
	FString Error;
	TestTrue(TEXT("dense population restores"), Restored.RestoreState(Population.GetSnapshot(), Error));
	Restored.AdvanceToMonth(City, Economy.GetSnapshot(), Tuning, &Services);
	TestTrue(TEXT("dense records reconcile after restore without resettlement"),
	         Restored.MatchesEconomy(Economy.GetSnapshot(), Tuning));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBusinessOpeningTest, "CitySim.Simulation.BusinessOpeningFreight",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityBusinessOpeningTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	TestTrue(TEXT("opening road commits"), City.CommitRoad(Road).IsSuccess());
	PlacePopulation(City, PopulationDefinition(TEXT("Power")), {1, 9});
	PlacePopulation(City, PopulationDefinition(TEXT("Service")), {2, 9});
	const uint64 Shop = PlacePopulation(City, PopulationDefinition(TEXT("Shop")), {16, 9});
	const uint64 Factory = PlacePopulation(City, PopulationDefinition(TEXT("Factory")), {18, 9});
	const uint64 Blocked = PlacePopulation(City, PopulationDefinition(TEXT("Shop")), {30, 30});
	FCityDevelopmentSnapshot Development;
	FCityDevelopmentState Construction;
	Construction.EntityId = Factory;
	Construction.Stage = ECityDevelopmentStage::Construction;
	Development.States.Add(Factory, Construction);
	FCityPopulationSimulation Population;
	FCityEconomySimulation Economy;
	FCityEconomyTuning Tuning;
	Tuning.FixedStepSeconds = 0.1;
	Tuning.FixedStepsPerMonth = 1;
	Tuning.WaterRangeRoadHops = 64;
	Economy.SetPaused(false);
	auto Settle = [&]() {
		Economy.Advance(0.1, City, Tuning, nullptr, &Development, &Population.GetSnapshot());
		const FCityServiceSnapshot Services = FCityServiceEvaluator::BuildSnapshot(
		    City, Tuning, nullptr, &Development, nullptr, &Population.GetSnapshot());
		Population.AdvanceToMonth(City, Economy.GetSnapshot(), Tuning, &Services, true);
		TestTrue(TEXT("gated totals reconcile"), Population.MatchesEconomy(Economy.GetSnapshot(), Tuning));
		return Services;
	};
	auto Services = Settle();
	TestEqual(TEXT("Unopened shop has no jobs; utilities retain four civic slots"),
	          Economy.GetSnapshot().Jobs, 4);
	TestEqual(TEXT("Unopened business provides no commercial jobs"), Services.CommercialJobs, 0);
	TestEqual(TEXT("opening status is inspectable"), Services.GetFailure(Shop),
	          ECityOperationFailure::AwaitingFreight);
	TestEqual(TEXT("disconnected business explains road failure"), Services.GetFailure(Blocked),
	          ECityOperationFailure::NoRoad);
	Population.QueueBusinessOpenings(Development, 1);
	Population.QueueBusinessOpenings(Development, 1);
	TestEqual(TEXT("one order per finished business, none during construction"),
	          Population.GetSnapshot().TransitTrips.Num(), 2);
	const uint64 OpeningId = Population.GetSnapshot().TransitTrips[0].Id;
	Population.AdvanceTransitToStep(1000);
	TestFalse(TEXT("timer cannot open business"), Population.GetSnapshot().TransitTrips[0].bCompleted);
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityTrafficSimulation Traffic;
	Traffic.UpdateTrips(City, nullptr, Population.GetSnapshot(), Network, 1);
	Traffic.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("only connected opening has a truck"), Traffic.GetSnapshot().Vehicles.Num(), 1);
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy = Economy.GetSnapshot();
	Save.Population = Population.GetSnapshot();
	Save.Traffic = Traffic.GetSnapshot();
	FString Json, Error;
	TestTrue(TEXT("pending opening saves"), FCityPersistence::Serialize(Save, Json, Error));
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("pending opening loads: ") + Error,
	              FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	TestFalse(TEXT("save retains closed business"), Loaded.Population.Businesses[0].bInitialFreightReceived);
	TestEqual(TEXT("save retains opening purpose"), Loaded.Population.TransitTrips[0].Purpose,
	          ECityTripPurpose::OpeningDelivery);
	FCityPopulationSimulation Resumed;
	FCityTrafficSimulation ResumedTraffic;
	TestTrue(TEXT("population resumes"), Resumed.RestoreState(Loaded.Population, Error));
	TestTrue(TEXT("truck resumes"), ResumedTraffic.RestoreState(Loaded.Traffic, Network, 1, Error));
	for (int64 Step = 2; Step <= 20; ++Step)
	{
		for (int32 Run = 0; Run < 2; ++Run)
		{
			FCityPopulationSimulation& P = Run == 0 ? Population : Resumed;
			FCityTrafficSimulation& T = Run == 0 ? Traffic : ResumedTraffic;
			P.PruneTransitTrips(City);
			P.QueueBusinessOpenings(Development, Step);
			T.UpdateTrips(City, nullptr, P.GetSnapshot(), Network, Step);
			T.AdvanceOneStep(Network, Step);
			for (const FCityTrafficVehicleState& V : T.GetSnapshot().Vehicles)
				if (V.bCompleted && V.bTransitTrip)
					P.CompleteTransitTrip(V.SourceId, Step);
		}
	}
	TestTrue(TEXT("actual arrival opens shop"),
	         Population.GetSnapshot().Businesses[0].bInitialFreightReceived);
	TestTrue(TEXT("saved truck opens same shop"),
	         Resumed.GetSnapshot().Businesses[0].bInitialFreightReceived);
	TestEqual(TEXT("saved queue allocates identical IDs"), Population.GetSnapshot().NextTransitTripId,
	          Resumed.GetSnapshot().NextTransitTripId);
	TestFalse(TEXT("disconnected shop remains closed"),
	          Population.GetSnapshot().Businesses.Last().bInitialFreightReceived);
	Population.CompleteTransitTrip(OpeningId, 100);
	Population.QueueBusinessOpenings(Development, 21);
	TestEqual(TEXT("arrived order is never repeated"), Population.GetSnapshot().TransitTrips.Num(), 1);
	Services = Settle();
	TestEqual(TEXT("delivered shop becomes operational"), Services.OperationalShops, 1);
	TestEqual(TEXT("factory still under construction produces nothing"), Economy.GetSnapshot().GoodsProduced,
	          0);
	Development.States.FindChecked(Factory).Stage = ECityDevelopmentStage::Operational;
	Population.QueueBusinessOpenings(Development, 22);
	const auto FactoryTrip = Population.GetSnapshot().TransitTrips.Last();
	TestEqual(TEXT("factory orders after construction"), FactoryTrip.DestinationEntityId, Factory);
	Population.CompleteTransitTrip(FactoryTrip.Id, 23);
	Population.CompleteTransitTrip(FactoryTrip.Id, 24);
	TestEqual(TEXT("duplicate arrival keeps original completion clock"),
	          Population.GetSnapshot().TransitTrips.Last().ArrivalStep, int64(23));
	Services = Settle();
	TestEqual(TEXT("delivered industry starts production"), Services.OperationalIndustries, 1);
	Save.Population = Population.GetSnapshot();
	TestTrue(TEXT("open state saves"), FCityPersistence::Serialize(Save, Json, Error));
	TestTrue(TEXT("open state reloads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestTrue(TEXT("received flag persists"), Loaded.Population.Businesses[0].bInitialFreightReceived);
	// A schema-15 document has no opening deliveries or readiness field.
	Save.Population.TransitTrips.Reset();
	TestTrue(TEXT("legacy fixture serializes"), FCityPersistence::Serialize(Save, Json, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHouseholdBudgetTest, "CitySim.Simulation.SharedHouseholdBudgets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityHouseholdBudgetTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	const uint64 Home = PlacePopulation(City, PopulationDefinition(TEXT("Home")), {1, 1});
	const uint64 OtherHome = PlacePopulation(City, PopulationDefinition(TEXT("Home")), {2, 1});
	const uint64 Shop = PlacePopulation(City, PopulationDefinition(TEXT("Shop")), {3, 1});
	FCityEconomyTuning Tuning;
	Tuning.ResidentsPerGoodsUnit = 1;
	Tuning.HouseholdIncomePerGoodsPurchase = 5;
	FCityEconomySnapshot Economy;
	Economy.Residents = 4;
	Economy.RetailCapacity = 4;
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 3);
	Services.EntityResidents.Add(OtherHome, 1);
	Services.EntityOutput.Add(Shop, 4);
	FCityPopulationSimulation Population;
	Population.Reconcile(City, Economy, Tuning, &Services);
	FCityPopulationSnapshot Initial = Population.GetSnapshot();
	// First buyer owns no money. Two siblings jointly fund two purchases;
	// the other home cannot use their remaining dollar.
	Initial.Citizens[1].AvailableMoney = 4;
	Initial.Citizens[2].AvailableMoney = 7;
	Initial.Businesses[0].Inventory = 4;
	FString Error;
	if (!TestTrue(TEXT("fixture restores"), Population.RestoreState(Initial, Error)))
		return false;
	Economy.GoodsSold = FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services,
	                                                                  Population.GetSnapshot());
	TestEqual(TEXT("shared savings afford exactly two purchases"), Economy.GoodsSold, 2);
	TestEqual(TEXT("forecast does not mutate funds"), Population.GetSnapshot().Citizens[1].AvailableMoney,
	          int64(4));
	Economy.LastBilledMonthId = 1;
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	const FCityPopulationSnapshot Settled = Population.GetSnapshot();
	TestEqual(TEXT("unfunded member queues a visit funded by household"),
	          Settled.Citizens[0].PendingShopEntityId, Shop);
	TestEqual(TEXT("stable debit consumes first contributor"), Settled.Citizens[1].AvailableMoney, int64(0));
	TestEqual(TEXT("remaining savings stay in member wallet"), Settled.Citizens[2].AvailableMoney, int64(1));
	TestEqual(TEXT("household reports remaining shared budget"), Settled.Households[0].Balance, int64(1));
	TestEqual(TEXT("actual purchases match forecast"), Settled.Households[0].CompletedPurchases,
	          Economy.GoodsSold);
	TestEqual(TEXT("another home cannot borrow funds"), Settled.Households[1].CompletedPurchases, 0);
	TestEqual(TEXT("sales consume delivered goods"), Settled.Businesses[0].Inventory, 2);
	TestEqual(TEXT("money conserved across household and retailer"),
	          Settled.Households[0].Balance + Settled.Businesses[0].Balance, int64(11));
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy = Economy;
	Save.Population = Settled;
	FString Json;
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("shared budget saves"), FCityPersistence::Serialize(Save, Json, Error)) ||
	    !TestTrue(TEXT("shared budget loads"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	FCityPopulationSimulation Restored;
	if (!TestTrue(TEXT("saved wallets restore"), Restored.RestoreState(Loaded.Population, Error)))
		return false;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("same month cannot spend twice"), Restored.GetSnapshot().Households[0].Balance, int64(1));
	TestEqual(TEXT("same month cannot sell twice"), Restored.GetSnapshot().Businesses[0].Inventory, 2);
	TestEqual(TEXT("insufficient pooled savings forecast zero"),
	          FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services,
	                                                        Restored.GetSnapshot()),
	          0);
	// A working member's new wage can fund a different member's need.
	Economy.Jobs = Economy.EmployedResidents = 1;
	Services.EntityJobs.Add(Shop, 1);
	TestEqual(TEXT("forecast includes wages in shared funds"),
	          FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services,
	                                                        Restored.GetSnapshot()),
	          1);
	Economy.GoodsSold = 1;
	Economy.LastBilledMonthId = 2;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("wage-funded settlement matches forecast"),
	          Restored.GetSnapshot().Households[0].CompletedPurchases, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPayrollSolvencyTest, "CitySim.Simulation.PayrollSolvency",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPayrollSolvencyTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	const uint64 Home = PlacePopulation(City, PopulationDefinition(TEXT("Home")), {1, 1});
	const uint64 Shop = PlacePopulation(City, PopulationDefinition(TEXT("Shop")), {2, 1});
	FCityEconomyTuning Tuning;
	Tuning.ResidentsPerGoodsUnit = 1;
	FCityEconomySnapshot Economy;
	Economy.Residents = Economy.Jobs = Economy.EmployedResidents = 3;
	Economy.RetailCapacity = 3;
	Economy.HouseholdGoodsDemand = 3;
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 3);
	Services.EntityJobs.Add(Shop, 3);
	Services.EntityOutput.Add(Shop, 3);
	FCityPopulationSimulation Population;
	Population.Reconcile(City, Economy, Tuning, &Services);
	FCityPopulationSnapshot Initial = Population.GetSnapshot();
	Initial.Businesses[0].Balance = 7;
	Initial.Businesses[0].Inventory = 3;
	FString Error;
	if (!TestTrue(TEXT("fixture restores"), Population.RestoreState(Initial, Error)))
		return false;
	Economy.GoodsSold = FCityPopulationSimulation::ForecastGoodsSales(
	    City, Economy, Tuning, Services, Population.GetSnapshot(), &Economy.HouseholdIncome);
	TestEqual(TEXT("income capped by employer cash"), Economy.HouseholdIncome, int64(7));
	TestEqual(TEXT("sales forecast uses affordable payroll"), Economy.GoodsSold, 1);
	TestEqual(TEXT("forecast leaves employer unchanged"), Population.GetSnapshot().Businesses[0].Balance,
	          int64(7));
	// Suppress sales to observe the exact payroll split independently.
	Economy.GoodsSold = 0;
	Economy.LastBilledMonthId = 1;
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	const FCityPopulationSnapshot Paid = Population.GetSnapshot();
	TestEqual(TEXT("first worker gets remainder dollar"), Paid.Citizens[0].AvailableMoney, int64(3));
	TestEqual(TEXT("second worker gets equal base share"), Paid.Citizens[1].AvailableMoney, int64(2));
	TestEqual(TEXT("third worker gets equal base share"), Paid.Citizens[2].AvailableMoney, int64(2));
	TestEqual(TEXT("payroll cannot overdraft"), Paid.Businesses[0].Balance, int64(0));
	TestEqual(TEXT("contracted payroll remains inspectable"), Paid.Businesses[0].PayrollDue, int64(15));
	TestEqual(TEXT("paid payroll remains inspectable"), Paid.Businesses[0].PayrollPaid, int64(7));
	TestEqual(TEXT("money conserved"), Paid.Households[0].Balance + Paid.Businesses[0].Balance, int64(7));
	TestTrue(TEXT("actual income reconciles"), Population.MatchesEconomy(Economy, Tuning));
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy = Economy;
	Save.Population = Paid;
	FString Json;
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("payroll saves"), FCityPersistence::Serialize(Save, Json, Error)) ||
	    !TestTrue(TEXT("payroll loads"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	FCityPopulationSimulation Restored;
	if (!TestTrue(TEXT("payroll restores"), Restored.RestoreState(Loaded.Population, Error)))
		return false;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("same month does not pay twice"), Restored.GetSnapshot().Households[0].Balance, int64(7));
	TestTrue(TEXT("saved payroll still reconciles"), Restored.MatchesEconomy(Economy, Tuning));
	Economy.LastBilledMonthId = 2;
	Economy.HouseholdIncome = 0;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("empty employer pays nothing next month"),
	          Restored.GetSnapshot().Businesses[0].PayrollPaid, int64(0));
	TestEqual(TEXT("opening reserve is not replenished"), Restored.GetSnapshot().Businesses[0].Balance,
	          int64(0));
	FCityPopulationSnapshot Debt = Restored.GetSnapshot();
	Debt.Businesses[0].Balance = -9;
	if (!TestTrue(TEXT("legacy debt remains valid"), Restored.RestoreState(Debt, Error)))
		return false;
	Economy.LastBilledMonthId = 3;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("payroll does not deepen existing debt"), Restored.GetSnapshot().Businesses[0].Balance,
	          int64(-9));
	Debt.Businesses[0].PayrollPaid = Debt.Businesses[0].PayrollDue + 1;
	TestFalse(TEXT("invalid payroll rejected"), Restored.RestoreState(Debt, Error));
	TestEqual(TEXT("invalid restore is atomic"), Restored.GetSnapshot().Businesses[0].Balance, int64(-9));
	FCityPopulationSnapshot Recovery = Restored.GetSnapshot();
	Recovery.Businesses[0].Balance = 20; // Simulate received trading revenue.
	if (!TestTrue(TEXT("funded business restores"), Restored.RestoreState(Recovery, Error)))
		return false;
	Economy.LastBilledMonthId = 4;
	Restored.AdvanceToMonth(City, Economy, Tuning, &Services, true);
	TestEqual(TEXT("payroll recovers when cash returns"), Restored.GetSnapshot().Businesses[0].PayrollPaid,
	          int64(15));
	TestEqual(TEXT("funded payroll retains unspent cash"), Restored.GetSnapshot().Businesses[0].Balance,
	          int64(5));
	TestEqual(TEXT("prior shortfalls do not silently become arrears"),
	          Restored.GetSnapshot().Households[0].Balance, int64(22));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStaffedProductionTest, "CitySim.Simulation.StaffedProduction",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStaffedProductionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("no workers means no output"), FCityPopulationSimulation::StaffedOutput(7, 5, 0), 0);
	TestEqual(TEXT("fractional goods round down"), FCityPopulationSimulation::StaffedOutput(7, 5, 2), 2);
	TestEqual(TEXT("full staff produces capacity"), FCityPopulationSimulation::StaffedOutput(7, 5, 5), 7);
	TestEqual(TEXT("overstaffing cannot exceed capacity"), FCityPopulationSimulation::StaffedOutput(7, 5, 9),
	          7);
	TestEqual(TEXT("zero job capacity is safe"), FCityPopulationSimulation::StaffedOutput(7, 0, 9), 0);
	FCityPlacementModel City(10000);
	const uint64 Home = PlacePopulation(City, PopulationDefinition(TEXT("Home")), {1, 1});
	const uint64 Factory = PlacePopulation(City, PopulationDefinition(TEXT("Factory")), {2, 1});
	FCityEconomyTuning Tuning;
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 5);
	Services.EntityJobs.Add(Factory, 5);
	Services.EntityOutput.Add(Factory, 7);
	FCityPopulationSimulation Population;
	FCityEconomySnapshot Economy;
	Economy.Jobs = 5;
	int32 Total = 0;
	for (const int32 Workers : {0, 2, 5, 0})
	{
		// Citizens now retain lifetime identities. Remove job capacity in the
		// final case instead of deleting five people to simulate no staffing.
		Economy.Residents = FMath::Max(Workers, Population.GetSnapshot().Citizens.Num());
		Economy.EmployedResidents = Workers;
		Economy.Jobs = Workers == 0 && Economy.Residents > 0 ? 0 : 5;
		Services.EntityJobs[Factory] = Economy.Jobs;
		Economy.HouseholdGoodsDemand = (Economy.Residents + 1) / 2;
		Economy.GoodsSold = FCityPopulationSimulation::ForecastGoodsSales(
		    City, Economy, Tuning, Services, Population.GetSnapshot(), &Economy.HouseholdIncome,
		    &Economy.GoodsProduced);
		TestEqual(TEXT("forecast scales actual staffing"), Economy.GoodsProduced,
		          FCityPopulationSimulation::StaffedOutput(7, 5, Workers));
		++Economy.LastBilledMonthId;
		Population.AdvanceToMonth(City, Economy, Tuning, &Services, true);
		Total += Economy.GoodsProduced;
		TestEqual(TEXT("production credits inventory exactly once"),
		          Population.GetSnapshot().Businesses[0].Inventory, Total);
		TestEqual(TEXT("authored output remains capacity"),
		          Population.GetSnapshot().Businesses[0].PendingOrders, 7);
		TestTrue(TEXT("settled production matches economy"), Population.MatchesEconomy(Economy, Tuning));
		Population.AdvanceToMonth(City, Economy, Tuning, &Services, true);
		TestEqual(TEXT("same month cannot produce twice"), Population.GetSnapshot().Businesses[0].Inventory,
		          Total);
	}
	FCitySaveDocument Save;
	Save.Placement = City.ExportState();
	Save.Economy = Economy;
	Save.Population = Population.GetSnapshot();
	Save.Population.Businesses[0].ProducedGoods = 7;
	FString Json, Error;
	FCitySaveDocument Loaded;
	if (!TestTrue(TEXT("production saves"), FCityPersistence::Serialize(Save, Json, Error)) ||
	    !TestTrue(TEXT("production loads"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	TestEqual(TEXT("monthly output persists"), Loaded.Population.Businesses[0].ProducedGoods, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityArrivalEducationTest,
                                 "CitySim.Population.NewArrivalEducationAndMigration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityArrivalEducationTest::RunTest(const FString& Parameters)
{
	for (int32 Percent : {0, 25, 37, 100})
	{
		FCityPlacementModel City(10000);
		PlacePopulation(City, PopulationDefinition(TEXT("Home")), {1, 1});
		FCityEconomyTuning Tuning;
		Tuning.NewArrivalUneducatedPercent = Percent;
		Tuning.ResidentsPerOccupiedHome = 100;
		FCityEconomySnapshot Economy;
		Economy.Residents = 100;
		FCityPopulationSimulation Whole, Batched;
		Whole.Reconcile(City, Economy, Tuning);
		for (int32 Count = 1; Count <= 100; ++Count)
		{
			Economy.Residents = Count;
			Batched.Reconcile(City, Economy, Tuning);
		}
		int32 None = 0;
		for (int32 I = 0; I < 100; ++I)
		{
			None += Whole.GetSnapshot().Citizens[I].Education == ECityEducationLevel::None ? 1 : 0;
			TestEqual(TEXT("Arrival batch size cannot change qualification"),
			          Whole.GetSnapshot().Citizens[I].Education, Batched.GetSnapshot().Citizens[I].Education);
		}
		TestEqual(TEXT("Every hundred arrivals match the configured quota"), None, Percent);
		auto People = Whole.GetSnapshot();
		People.Citizens[0].Education = ECityEducationLevel::Secondary;
		People.Citizens[0].EducationMonths = 2;
		FString Error;
		TestTrue(TEXT("Existing learning restores"), Whole.RestoreState(People, Error));
		Whole.Reconcile(City, Economy, Tuning);
		TestTrue(TEXT("Reconcile preserves existing education and course progress"),
		         Whole.GetSnapshot().Citizens[0].Education == ECityEducationLevel::Secondary &&
		             Whole.GetSnapshot().Citizens[0].EducationMonths == 2);
		FCitySaveDocument Document;
		Document.Placement = City.ExportState();
		Document.Economy = Economy;
		Document.Population = Whole.GetSnapshot();
		Document.DifficultyRules = Tuning;
		FString Json;
		FCitySaveDocument Loaded;
		TestTrue(TEXT("Arrival policy serializes"), FCityPersistence::Serialize(Document, Json, Error));
		if (TestTrue(TEXT("Arrival policy deserializes"), FCityPersistence::Deserialize(Json, Loaded, Error)))
		{
			TestEqual(TEXT("Saved city retains arrival policy"),
			          Loaded.DifficultyRules.NewArrivalUneducatedPercent, Percent);
			FCityPopulationSimulation Resumed;
			TestTrue(TEXT("Citizens resume"), Resumed.RestoreState(Loaded.Population, Error));
			Economy.Residents = 120;
			Tuning.ResidentsPerOccupiedHome = 120;
			Whole.Reconcile(City, Economy, Tuning);
			Resumed.Reconcile(City, Economy, Tuning);
			for (int32 I = 100; I < 120; ++I)
				TestEqual(TEXT("Resume retains the exact next-arrival distribution"),
				          Whole.GetSnapshot().Citizens[I].Education,
				          Resumed.GetSnapshot().Citizens[I].Education);
		}
		TSharedPtr<FJsonObject> Root;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
		for (int32 InvalidPercent : {-1, 101})
		{
			Root->SetNumberField(TEXT("ruleNewArrivalUneducatedPercent"), InvalidPercent);
			FString InvalidJson;
			FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&InvalidJson));
			TestFalse(TEXT("Invalid saved arrival quota rejects load"),
			          FCityPersistence::Deserialize(InvalidJson, Loaded, Error));
		}
		Root->RemoveField(TEXT("ruleNewArrivalUneducatedPercent"));
		FString MissingJson;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&MissingJson));
		TestFalse(TEXT("Current format requires its arrival policy"),
		          FCityPersistence::Deserialize(MissingJson, Loaded, Error));
	}
	return true;
}

#endif
