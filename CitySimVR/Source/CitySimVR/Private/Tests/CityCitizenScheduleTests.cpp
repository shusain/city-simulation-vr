#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenScheduleTest, "CitySim.Simulation.CitizenArrivalSchedules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCitizenScheduleTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	if (!TestTrue(TEXT("schedule road built"), City.CommitRoad(Road).IsSuccess()))
		return false;
	auto Place = [&](const FName Name, const FCityCellCoord Cell) {
		FCityBuildingDefinition D;
		D.DefinitionId = Name;
		D.DisplayName = Name.ToString();
		D.Footprint = FCityCellFootprint(1, 1);
		D.Cost = 0;
		auto P = City.BeginPreview(D, Cell);
		return City.CommitPlacement(P, D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {2, 9});
	const uint64 Work = Place(TEXT("Factory"), {6, 9});
	const uint64 Shop = Place(TEXT("Shop"), {10, 9});
	if (!TestTrue(TEXT("routine destinations placed"), Home != 0 && Work != 0 && Shop != 0))
		return false;
	FCityPopulationSnapshot Initial;
	FCityCitizenRecord Citizen;
	Citizen.Id = 3;
	Citizen.HomeEntityId = Citizen.HouseholdId = Home;
	Citizen.WorkplaceEntityId = Work;
	Citizen.CurrentLocationEntityId = Home;
	Citizen.PendingShopEntityId = Shop;
	Citizen.AvailableMoney = 12;
	Initial.Citizens.Add(Citizen);
	Initial.NextCitizenId = 4;
	FCityBusinessLedgerRecord Factory;
	Factory.EntityId = Work;
	Factory.bIndustrial = true;
	Factory.JobCapacity = 1;
	FCityBusinessLedgerRecord Retailer;
	Retailer.EntityId = Shop;
	Retailer.PendingOrders = 10;
	Retailer.Inventory = 8;
	Initial.Businesses = {Factory, Retailer};
	FCityPopulationSimulation Population, Resumed;
	FCityTrafficSimulation Traffic, ResumedTraffic;
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FString Error, Json;
	if (!TestTrue(TEXT("schedule fixture restores"), Population.RestoreState(Initial, Error)))
		return false;
	bool bSawWork = false, bSawShop = false, bReturned = false, bSaved = false;
	int64 WorkArrival = -1, ShopArrival = -1;
	for (int64 Step = 1; Step <= 160; ++Step)
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
					P.CompleteCitizenTrip(V.SourceId, V.OriginEntityId, V.DestinationEntityId, Step);
		}
		const auto& C = Population.GetSnapshot().Citizens[0];
		if (C.Schedule == ECityCitizenSchedule::Work && WorkArrival < 0)
		{
			WorkArrival = Step;
			bSawWork = true;
			TestEqual(TEXT("work dwell begins at actual arrival"), C.NextActionStep, Step + FCityCalendar::Duration(6.0));
			Population.CompleteCitizenTrip(C.Id, Home, Work, Step + 1);
			TestEqual(TEXT("duplicate completion cannot extend dwell"),
			          Population.GetSnapshot().Citizens[0].NextActionStep, Step + FCityCalendar::Duration(6.0));
		}
		if (WorkArrival >= 0 && Step < WorkArrival + 4)
			TestEqual(TEXT("work dwell respected"), C.Schedule, ECityCitizenSchedule::Work);
		if (C.Schedule == ECityCitizenSchedule::ToShop)
			TestEqual(TEXT("shopping leg starts at workplace, never teleports home"),
			          C.CurrentLocationEntityId, Work);
		if (C.Schedule == ECityCitizenSchedule::Shop)
		{
			bSawShop = true;
			if (ShopArrival < 0)
				ShopArrival = Step;
			TestEqual(TEXT("shop visit consumes pending destination"), C.PendingShopEntityId, uint64(0));
		}
		if (bSawShop && C.Schedule == ECityCitizenSchedule::Home)
			bReturned = true;
		if (!bSaved && C.Schedule == ECityCitizenSchedule::ToShop)
		{
			FCitySaveDocument Save;
			Save.Placement = City.ExportState();
			Save.Economy.FixedStepId = Save.Disasters.LastFixedStepId = Step;
			Save.Population = Population.GetSnapshot();
			Save.Traffic = Traffic.GetSnapshot();
			FCitySaveDocument Loaded;
			if (!TestTrue(TEXT("mid-shopping route saves"), FCityPersistence::Serialize(Save, Json, Error)) ||
			    !TestTrue(TEXT("mid-shopping route loads"),
			              FCityPersistence::Deserialize(Json, Loaded, Error)) ||
			    !TestTrue(TEXT("schedule resumes"), Resumed.RestoreState(Loaded.Population, Error)) ||
			    !TestTrue(TEXT("traffic resumes"),
			              ResumedTraffic.RestoreState(Loaded.Traffic, Network, Step, Error)))
				return false;
			bSaved = true;
		}
		if (bSaved)
		{
			FCitySaveDocument A, B;
			A.Placement = B.Placement = City.ExportState();
			A.Population = Population.GetSnapshot();
			A.Traffic = Traffic.GetSnapshot();
			B.Population = Resumed.GetSnapshot();
			B.Traffic = ResumedTraffic.GetSnapshot();
			FString AJ, BJ;
			FCityPersistence::Serialize(A, AJ, Error);
			FCityPersistence::Serialize(B, BJ, Error);
			TestEqual(TEXT("resume preserves full schedule and traffic state"), BJ, AJ);
		}
		if (bReturned)
			break;
	}
	TestTrue(TEXT("citizen reaches work"), bSawWork);
	TestTrue(TEXT("citizen reaches shop"), bSawShop);
	TestTrue(TEXT("citizen reaches home after shopping"), bReturned);
	TestTrue(TEXT("mid-trip save exercised"), bSaved);
	TestEqual(TEXT("schedule arrival cannot charge a second time"),
	          Population.GetSnapshot().Citizens[0].AvailableMoney, int64(12));
	TestEqual(TEXT("schedule arrival cannot consume goods again"),
	          Population.GetSnapshot().Businesses[1].Inventory, 8);
	// Routes that cannot be scheduled must never become fake arrivals.
	FCityPopulationSimulation Blocked;
	TestTrue(TEXT("blocked fixture restores"), Blocked.RestoreState(Initial, Error));
	Blocked.AdvanceCitizenSchedules(City, 1);
	for (int64 Step = 2; Step < 20; ++Step)
		Blocked.AdvanceCitizenSchedules(City, Step);
	TestEqual(TEXT("no traffic arrival leaves citizen travelling"),
	          Blocked.GetSnapshot().Citizens[0].Schedule, ECityCitizenSchedule::ToWork);
	TestEqual(TEXT("blocked citizen retains real origin"),
	          Blocked.GetSnapshot().Citizens[0].CurrentLocationEntityId, Home);

	FCityTrafficSimulation BlockedTraffic;
	BlockedTraffic.UpdateTrips(City, nullptr, Blocked.GetSnapshot(), Network, 19);
	TestFalse(TEXT("commute has an authoritative route"), BlockedTraffic.GetSnapshot().Vehicles.IsEmpty());
	City.RemoveRoad({5, 8});
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	BlockedTraffic.UpdateTrips(City, nullptr, Blocked.GetSnapshot(), Network, 20);
	TestTrue(TEXT("road cut removes route without arrival"), BlockedTraffic.GetSnapshot().Vehicles.IsEmpty());
	TestEqual(TEXT("road cut preserves trip origin"),
	          Blocked.GetSnapshot().Citizens[0].CurrentLocationEntityId, Home);
	auto Repair = City.BeginRoadPreview({5, 8});
	TestTrue(TEXT("road reconnects"), City.CommitRoad(Repair).IsSuccess());
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	BlockedTraffic.UpdateTrips(City, nullptr, Blocked.GetSnapshot(), Network, 21);
	TestFalse(TEXT("reconnected commute retries"), BlockedTraffic.GetSnapshot().Vehicles.IsEmpty());
	City.DemolishEntity(Work);
	Blocked.AdvanceCitizenSchedules(City, 20);
	TestEqual(TEXT("demolition cancels stale destination"), Blocked.GetSnapshot().Citizens[0].TripPurpose,
	          ECityTripPurpose::None);
	Blocked.AdvanceCitizenSchedules(City, 21);
	TestEqual(TEXT("unemployed citizen can still visit shop"),
	          Blocked.GetSnapshot().Citizens[0].TripDestinationEntityId, Shop);
	FCityPopulationSnapshot Invalid = Blocked.GetSnapshot();
	Invalid.Citizens[0].Schedule = static_cast<ECityCitizenSchedule>(255);
	TestFalse(TEXT("invalid schedule rejected"), Blocked.RestoreState(Invalid, Error));
	TestEqual(TEXT("invalid schedule rejection preserves destination"),
	          Blocked.GetSnapshot().Citizens[0].TripDestinationEntityId, Shop);
	Invalid = Blocked.GetSnapshot();
	Invalid.Citizens[0].TripDestinationEntityId = 0;
	TestFalse(TEXT("travelling phase requires a destination"), Blocked.RestoreState(Invalid, Error));
	return true;
}
#endif
