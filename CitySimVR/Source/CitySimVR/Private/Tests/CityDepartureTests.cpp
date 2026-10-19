#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Commands/CityPlacementModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDepartureFairnessTest, "CitySim.Traffic.FairPortalDepartures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDepartureFairnessTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	TestTrue(TEXT("fixture road"), City.CommitRoad(Road).IsSuccess());
	auto Lot = [&](const FCityCellCoord Cell) {
		FCityBuildingDefinition D;
		D.DefinitionId = TEXT("DepartureFixture"); D.DisplayName = TEXT("Departure fixture");
		D.Footprint = {1, 1}; D.Cost = 0; D.EntranceSides = 15;
		auto Preview = City.BeginPreview(D, Cell);
		return City.CommitPlacement(Preview, D).EntityId;
	};
	const uint64 ThroughHome = Lot({2, 9}), Home = Lot({10, 9}), Shop = Lot({19, 9});
	if (!TestTrue(TEXT("fixture lots placed"), ThroughHome && Home && Shop)) return false;
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityPopulationSnapshot People;
	auto Citizen = [&](uint64 Id, uint64 Origin, int64 Requested) {
		FCityCitizenRecord C;
		C.Id = Id; C.HouseholdId = C.HomeEntityId = C.CurrentLocationEntityId = Origin;
		C.TripDestinationEntityId = Shop; C.TripPurpose = ECityTripPurpose::Purchase;
		C.TripVehicleType = ECityTripVehicleType::Walk; C.NextActionStep = Requested;
		People.Citizens.Add(C);
	};
	Citizen(23, Home, 1);
	Citizen(24, Home, 2);
	FCityTrafficSnapshot Initial;
	Initial.FixedStepId = 100;
	Initial.NetworkFingerprint = FCityTrafficSimulation::GetNetworkFingerprint(Network);
	for (int32 I = 0; I < 16; ++I)
	{
		Citizen(100 + I, ThroughHome, 1);
		FCityTrafficVehicleState V;
		V.SourceId = 100 + I; V.Id = V.SourceId * 2; V.OriginEntityId = ThroughHome;
		V.DestinationEntityId = Shop; V.VehicleType = ECityTripVehicleType::Walk; V.LengthUnits = 150;
		TestTrue(TEXT("through route"), FCityTrafficSimulation::BuildRoute(Network, {2, 8}, {19, 8}, V));
		V.DistanceUnits = 6000 + I * 650;
		Initial.Vehicles.Add(V);
		Citizen(1000 + I, ThroughHome, 1);
		V.SourceId = 1000 + I; V.Id = V.SourceId * 2; V.PedestrianTrack = 1;
		Initial.Vehicles.Add(V);
	}
	FCityTrafficSimulation Traffic, Loaded;
	FString Error, Json;
	if (!TestTrue(TEXT("packed traffic restores"), Traffic.RestoreState(Initial, Network, 100, Error)))
	{ AddError(Error); return false; }
	Traffic.UpdateTrips(City, nullptr, People, Network, 100);
	if (!TestEqual(TEXT("both shoppers wait off road"), Traffic.GetSnapshot().WaitingDepartures.Num(), 2)) return false;
	TestEqual(TEXT("queue does not create arrivals or overlap"), Traffic.GetSnapshot().Vehicles.Num(), 32);
	const auto& Waiting = Traffic.GetSnapshot().WaitingDepartures;
	TestEqual(TEXT("oldest request goes first"), Waiting[0].SourceId, uint64(23));
	FCitySaveDocument Doc, Read;
	Doc.Placement = City.ExportState(); Doc.Traffic = Traffic.GetSnapshot(); Doc.Economy.FixedStepId = 100;
	Doc.Crime.LastFixedStepId = Doc.Fire.LastFixedStepId = Doc.Disasters.LastFixedStepId = 100;
	Doc.Disasters.NextCheckStep = 100;
	TestTrue(TEXT("waiting departures serialize"), FCityPersistence::Serialize(Doc, Json, Error));
	if (!TestTrue(TEXT("waiting departures read"), FCityPersistence::Deserialize(Json, Read, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("waiting departures restore"), Loaded.RestoreState(Read.Traffic, Network, 100, Error));
	int64 FirstDeparture = -1, SecondDeparture = -1;
	// No further route scheduling: cached departures must catch safe gaps themselves.
	for (int64 Step = 101; Step <= 180; ++Step)
	{
		Traffic.AdvanceOneStep(Network, Step);
		Loaded.AdvanceOneStep(Network, Step);
		const auto& State = Traffic.GetSnapshot();
		for (const auto& V : State.Vehicles)
		{
			if (V.SourceId == 23 && FirstDeparture < 0) FirstDeparture = Step;
			if (V.SourceId == 24 && SecondDeparture < 0) SecondDeparture = Step;
		}
		FCityTrafficSimulation Check;
		if (!TestTrue(TEXT("every moving snapshot retains safe gaps"), Check.RestoreState(State, Network, Step, Error)))
		{ AddError(Error); return false; }
		TestEqual(TEXT("reload retains daily admission"), Loaded.GetSnapshot().Vehicles.Num(), State.Vehicles.Num());
		for (int32 I = 0; I < State.Vehicles.Num(); ++I)
		{
			TestEqual(TEXT("reload retains movement"), Loaded.GetSnapshot().Vehicles[I].DistanceUnits, State.Vehicles[I].DistanceUnits);
			TestEqual(TEXT("reload retains actual arrival"), Loaded.GetSnapshot().Vehicles[I].CompletedStep, State.Vehicles[I].CompletedStep);
		}
	}
	TestTrue(TEXT("old shopper gets a bounded safe departure"), FirstDeparture > 100 && FirstDeparture <= 125);
	TestTrue(TEXT("second shopper follows fairly or uses parallel track"), SecondDeparture >= FirstDeparture && SecondDeparture <= 135);
	for (const auto& V : Traffic.GetSnapshot().Vehicles)
		TestTrue(TEXT("every journey actually completes"), V.bCompleted);
	AddInfo(FString::Printf(TEXT("Shopping busy portal: first=%lld second=%lld"), FirstDeparture, SecondDeparture));
	auto Invalid = Read.Traffic;
	Invalid.WaitingDepartures[0].DepartureRequestedStep = 101;
	TestFalse(TEXT("future departure rejected atomically"), Loaded.RestoreState(Invalid, Network, 100, Error));
	Invalid = Read.Traffic;
	const auto Duplicate = Invalid.WaitingDepartures[0];
	Invalid.WaitingDepartures.Add(Duplicate);
	TestFalse(TEXT("duplicate departure rejected"), Loaded.RestoreState(Invalid, Network, 100, Error));
	Traffic.RestoreState(Read.Traffic, Network, 100, Error);
	City.DemolishEntity(Shop);
	Traffic.UpdateTrips(City, nullptr, People, Network, 100);
	TestTrue(TEXT("demolition cancels waiting orders without arrival"), Traffic.GetSnapshot().WaitingDepartures.IsEmpty());
	TestTrue(TEXT("demolition cancels moving orders without arrival"), Traffic.GetSnapshot().Vehicles.IsEmpty());
	auto EditedNetwork = Network;
	++EditedNetwork.Edges[0].Cells[0].Y;
	Traffic.RestoreState(Read.Traffic, Network, 100, Error);
	Traffic.ReconcileNetwork(EditedNetwork);
	TestTrue(TEXT("road changes discard queued stale routes"), Traffic.GetSnapshot().WaitingDepartures.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDepartureConnectorTest, "CitySim.Traffic.PortalMergeAtConnector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDepartureConnectorTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network;
	for (int32 Id = 0; Id < 2; ++Id)
	{
		FCityRoadEdge Edge;
		Edge.Id = Id; Edge.StartNodeId = Id; Edge.EndNodeId = Id + 1;
		Edge.ForwardLanes = Edge.ReverseLanes = 1;
		Edge.SpeedUnitsPerStep = 2; Edge.Capacity = 16;
		Edge.Access = ECityRoadAccess::Pedestrian;
		for (int32 X = 0; X <= 6; ++X) Edge.Cells.Add({Id * 6 + X, 0});
		Network.Edges.Add(Edge);
	}
	Network.Connectors.Add({0, 1, 0, 1, 0, 0, ECityRoadTurn::Straight});
	FCityTrafficSnapshot State;
	State.NetworkFingerprint = FCityTrafficSimulation::GetNetworkFingerprint(Network);
	auto Walker = [](uint64 Id, int32 Edge, int32 Position) {
		FCityTrafficVehicleState V;
		V.Id = Id * 2; V.SourceId = Id; V.VehicleType = ECityTripVehicleType::Walk;
		V.LengthUnits = 150; V.DistanceUnits = Position; V.Route.Add({Edge, false, 0});
		return V;
	};
	auto Approach = Walker(1, 0, 5900);
	Approach.Route.Add({1, false, 0});
	State.Vehicles.Add(Approach); State.Vehicles.Add(Walker(2, 1, 0));
	State.WaitingDepartures.Add(Walker(23, 1, 0));
	FCityTrafficSimulation Traffic;
	FString Error;
	if (!TestTrue(TEXT("connector fixture restores"), Traffic.RestoreState(State, Network, 0, Error)))
	{ AddError(Error); return false; }
	Traffic.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("upstream connector holds for the departure gap"), Traffic.GetSnapshot().Vehicles[0].RouteLegIndex, 0);
	TestEqual(TEXT("occupant already inside slot clears it"), Traffic.GetSnapshot().Vehicles[1].DistanceUnits, 500);
	Traffic.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("portal departure catches first safe gap"), Traffic.GetSnapshot().WaitingDepartures.IsEmpty());
	for (int64 Step = 3; Step <= 30; ++Step)
	{
		Traffic.AdvanceOneStep(Network, Step);
		FCityTrafficSimulation Check;
		TestTrue(TEXT("connector merge preserves safe gaps"), Check.RestoreState(Traffic.GetSnapshot(), Network, Step, Error));
	}
	for (const auto& V : Traffic.GetSnapshot().Vehicles) TestTrue(TEXT("connector journeys arrive"), V.bCompleted);

	// Arrivals ending at a busy portal must finish before departure admission;
	// an off-road reservation is not a permanent barrier to its destination.
	State.Vehicles.Reset(); State.WaitingDepartures.Reset();
	auto Arriving = Walker(1, 1, 0); Arriving.EndDistanceUnits = 1000;
	State.Vehicles.Add(Arriving); State.Vehicles.Add(Walker(2, 1, 1000));
	auto Departing = Walker(23, 1, 1000); Departing.StartDistanceUnits = 1000;
	State.WaitingDepartures.Add(Departing);
	TestTrue(TEXT("shared arrival/departure portal restores"), Traffic.RestoreState(State, Network, 0, Error));
	Traffic.AdvanceOneStep(Network, 1); Traffic.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("arrival can pass the virtual stop line"), Traffic.GetSnapshot().Vehicles[0].DistanceUnits > 350 &&
	         Traffic.GetSnapshot().Vehicles[0].DistanceUnits <= 1000);
	Traffic.AdvanceOneStep(Network, 3);
	TestTrue(TEXT("arrival finishes rather than waits behind destination"), Traffic.GetSnapshot().Vehicles[0].bCompleted);
	TestTrue(TEXT("departure then safely occupies the released portal"), Traffic.GetSnapshot().WaitingDepartures.IsEmpty());
	return true;
}
#endif
