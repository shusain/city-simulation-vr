#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPurchaseArrivalTest, "CitySim.Simulation.PurchaseArrivalSettlement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPurchaseArrivalTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {12, 8});
	if (!TestTrue(TEXT("purchase road built"), City.CommitRoad(Road).IsSuccess()))
		return false;
	auto Place = [&](const FName Name, const FCityCellCoord Cell) {
		FCityBuildingDefinition D;
		D.DefinitionId = Name;
		D.DisplayName = Name.ToString();
		D.Footprint = FCityCellFootprint(1, 1);
		D.Cost = 0;
		auto Preview = City.BeginPreview(D, Cell);
		return City.CommitPlacement(Preview, D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {2, 9});
	const uint64 Shop = Place(TEXT("Shop"), {8, 9});
	if (!TestTrue(TEXT("purchase destinations exist"), Home != 0 && Shop != 0))
		return false;
	FCityEconomyTuning Tuning;
	Tuning.ResidentsPerGoodsUnit = 1;
	FCityEconomySnapshot Economy;
	Economy.Residents = 3;
	Economy.RetailCapacity = 3;
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 3);
	Services.EntityOutput.Add(Shop, 3);
	FCityPopulationSimulation Population;
	Population.Reconcile(City, Economy, Tuning, &Services);
	auto Initial = Population.GetSnapshot();
	Initial.Citizens[0].AvailableMoney = 4;
	Initial.Citizens[1].AvailableMoney = 7;
	Initial.Businesses[0].Inventory = 2;
	FString Error, Json;
	if (!TestTrue(TEXT("funded fixture restores"), Population.RestoreState(Initial, Error)))
		return false;
	Economy.LastBilledMonthId = 1;
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	const auto Reserved = Population.GetSnapshot();
	if (!TestEqual(TEXT("funds and stock reserve only two orders"), Reserved.PurchaseReservations.Num(), 2))
		return false;
	TestEqual(TEXT("reserving stock does not consume it"), Reserved.Businesses[0].Inventory, 2);
	TestEqual(TEXT("reserving funds does not transfer them"), Reserved.Households[0].Balance, int64(11));
	TestEqual(TEXT("unarrived orders are not sales"), Reserved.Businesses[0].SalesThisMonth, 0);
	TestEqual(TEXT("unarrived orders create no retailer income"), Reserved.Businesses[0].Balance, int64(0));
	TestTrue(TEXT("reservation funding validates"),
	         FCityPopulationSimulation::ValidatePurchaseReservations(Reserved, Error));
	TestEqual(TEXT("forecast includes held orders without double booking"),
	          FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services, Reserved), 2);
	auto Invalid = Reserved;
	Invalid.PurchaseReservations.Add(Reserved.PurchaseReservations[0]);
	TestFalse(TEXT("duplicate reservations rejected atomically"), Population.RestoreState(Invalid, Error));
	Invalid = Reserved;
	Invalid.Citizens[0].AvailableMoney = 0;
	TestFalse(TEXT("overcommitted wallets rejected"), Population.RestoreState(Invalid, Error));
	Invalid = Reserved;
	Invalid.Businesses[0].Inventory = 1;
	TestFalse(TEXT("overcommitted stock rejected"), Population.RestoreState(Invalid, Error));
	TestEqual(TEXT("failed restore retains reservations"),
	          Population.GetSnapshot().PurchaseReservations.Num(), 2);
	Tuning.HouseholdIncomePerGoodsPurchase = 99;
	Economy.LastBilledMonthId = 2;
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	TestEqual(TEXT("month boundary cannot reserve locked funds again"),
	          Population.GetSnapshot().PurchaseReservations.Num(), 2);
	TestEqual(TEXT("blocked period closes with zero sales"), Population.GetSnapshot().LastMonthSales, 0);
	TestEqual(TEXT("reserved price survives later price changes"),
	          Population.GetSnapshot().PurchaseReservations[0].Price, int64(5));
	FCityTrafficSimulation Traffic, ResumedTraffic;
	FCityPopulationSimulation Resumed;
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	bool bSaved = false;
	for (int64 Step = 1; Step <= 100; ++Step)
	{
		const int32 Runs = bSaved ? 2 : 1;
		for (int32 Run = 0; Run < Runs; ++Run)
		{
			auto& P = Run == 0 ? Population : Resumed;
			auto& T = Run == 0 ? Traffic : ResumedTraffic;
			P.AdvanceCitizenSchedules(City, Step);
			T.UpdateTrips(City, nullptr, P.GetSnapshot(), Network, Step);
			T.AdvanceOneStep(Network, Step);
			for (const auto& V : T.GetSnapshot().Vehicles)
				if (V.bCompleted)
				{
					P.CompleteCitizenTrip(V.SourceId, V.OriginEntityId, V.DestinationEntityId, Step);
					P.CompleteCitizenTrip(V.SourceId, V.OriginEntityId, V.DestinationEntityId,
					                      Step); // Idempotent.
				}
		}
		if (!bSaved && !Traffic.GetSnapshot().Vehicles.IsEmpty() &&
		    Population.GetSnapshot().TotalArrivalPurchases == 0)
		{
			FCitySaveDocument Save;
			Save.Placement = City.ExportState();
			Save.Economy = Economy;
			Save.Economy.FixedStepId = Step;
			Save.Disasters.LastFixedStepId = Step;
			Save.Population = Population.GetSnapshot();
			Save.Traffic = Traffic.GetSnapshot();
			FCitySaveDocument Loaded;
			Error.Reset(); // Earlier negative restore assertions intentionally populated this output.
			const bool Saved = FCityPersistence::Serialize(Save, Json, Error);
			if (!TestTrue(FString(TEXT("reserved trip saves: ")) + Error, Saved) ||
			    !TestTrue(TEXT("reserved trip loads"), FCityPersistence::Deserialize(Json, Loaded, Error)) ||
			    !TestTrue(TEXT("reserved funds resume"), Resumed.RestoreState(Loaded.Population, Error)) ||
			    !TestTrue(TEXT("shopping movement resumes"),
			              ResumedTraffic.RestoreState(Loaded.Traffic, Network, Step, Error)))
				return false;
			bSaved = true;
		}
		if (bSaved)
		{
			FCitySaveDocument A, B;
			A.Placement = B.Placement = City.ExportState();
			A.Economy = B.Economy = Economy;
			A.Economy.FixedStepId = B.Economy.FixedStepId = Step;
			A.Disasters.LastFixedStepId = B.Disasters.LastFixedStepId = Step;
			A.Population = Population.GetSnapshot();
			B.Population = Resumed.GetSnapshot();
			A.Traffic = Traffic.GetSnapshot();
			B.Traffic = ResumedTraffic.GetSnapshot();
			FString AJ, BJ;
			if (!TestTrue(TEXT("Original purchase state serializes"),
			              FCityPersistence::Serialize(A, AJ, Error)) ||
			    !TestTrue(TEXT("Resumed purchase state serializes"),
			              FCityPersistence::Serialize(B, BJ, Error)))
				return false;
			TestEqual(TEXT("reserved mid-trip resume is deterministic"), BJ, AJ);
		}
		TestEqual(TEXT("goods conserved through arrivals"),
		          Population.GetSnapshot().Businesses[0].Inventory +
		              Population.GetSnapshot().TotalArrivalPurchases,
		          int64(2));
		TestEqual(TEXT("money conserved through arrivals"),
		          Population.GetSnapshot().Households[0].Balance +
		              Population.GetSnapshot().Businesses[0].Balance,
		          int64(11));
		if (Population.GetSnapshot().TotalArrivalPurchases == 2)
			break;
	}
	TestTrue(TEXT("mid-trip save exercised"), bSaved);
	TestEqual(TEXT("actual arrivals settle both orders once"), Population.GetSnapshot().TotalArrivalPurchases,
	          int64(2));
	TestEqual(TEXT("arrival transfers captured price"), Population.GetSnapshot().Businesses[0].Balance,
	          int64(10));
	TestEqual(TEXT("household paid exact cost"), Population.GetSnapshot().Households[0].Balance, int64(1));
	TestEqual(TEXT("arrival removes holds"), Population.GetSnapshot().PurchaseReservations.Num(), 0);
	Economy.LastBilledMonthId = 3;
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	TestEqual(TEXT("closed month contains only arrived sales"), Population.GetSnapshot().LastMonthSales, 2);
	TestEqual(TEXT("closed spending uses captured prices"), Population.GetSnapshot().LastMonthSpending,
	          int64(10));
	TestEqual(TEXT("household closes arrived purchases"),
	          Population.GetSnapshot().Households[0].CompletedPurchases, 2);
	TestEqual(TEXT("new month starts zero arrivals"), Population.GetSnapshot().SalesThisMonth, 0);
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	TestEqual(TEXT("same month cannot close twice"), Population.GetSnapshot().LastMonthSales, 2);
	FCityPopulationSimulation Cancelled;
	FCityPlacementModel ShopLossCity = City;
	TestTrue(TEXT("cancellation fixture restores"), Cancelled.RestoreState(Reserved, Error));
	City.DemolishEntity(Home);
	Cancelled.PrunePurchaseReservations(City);
	TestEqual(TEXT("Demolished home releases pending household orders"),
	          Cancelled.GetSnapshot().PurchaseReservations.Num(), 0);
	TestEqual(TEXT("housing loss preserves all three people"), Cancelled.GetSnapshot().Citizens.Num(), 3);
	TestEqual(TEXT("surviving member was not charged"), Cancelled.GetSnapshot().Citizens[0].AvailableMoney,
	          int64(4));
	TestTrue(TEXT("shop cancellation fixture restores"), Cancelled.RestoreState(Reserved, Error));
	ShopLossCity.DemolishEntity(Shop);
	Cancelled.PrunePurchaseReservations(ShopLossCity);
	TestEqual(TEXT("demolished shop releases all reservations"),
	          Cancelled.GetSnapshot().PurchaseReservations.Num(), 0);
	TestEqual(TEXT("cancelled orders never debit household"), Cancelled.GetSnapshot().Households[0].Balance,
	          int64(11));
	return true;
}
#endif
