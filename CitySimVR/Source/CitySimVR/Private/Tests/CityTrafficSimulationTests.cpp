#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Commands/CityPlacementModel.h"

namespace
{
FCityRoadEdge TrafficEdge(const int32 Id, const int32 StartNode, const int32 EndNode, const int32 Speed,
                          const int32 CellCount = 4)
{
	FCityRoadEdge Edge;
	Edge.Id = Id;
	Edge.StartNodeId = StartNode;
	Edge.EndNodeId = EndNode;
	Edge.ForwardLanes = 1;
	Edge.ReverseLanes = 1;
	Edge.SpeedUnitsPerStep = Speed;
	Edge.Capacity = 16;
	Edge.Access = ECityRoadAccess::Car | ECityRoadAccess::Freight | ECityRoadAccess::Pedestrian;
	for (int32 Index = 0; Index < CellCount; ++Index)
		Edge.Cells.Add(FCityCellCoord(Id * 8 + Index, 0));
	return Edge;
}

FCityRoadNetworkSnapshot StraightTrafficNetwork()
{
	FCityRoadNetworkSnapshot Network;
	Network.Nodes = {{0, FCityCellCoord(0, 0), 0}, {1, FCityCellCoord(3, 0), 0}};
	Network.Edges.Add(TrafficEdge(0, 0, 1, 2));
	return Network;
}

FCityTrafficVehicleState TrafficVehicle(const uint64 Id, const int32 Distance)
{
	FCityTrafficVehicleState Vehicle;
	Vehicle.Id = Id;
	Vehicle.DistanceUnits = Distance;
	Vehicle.Route.Add({0, false, 0});
	return Vehicle;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficFollowingTest, "CitySim.Traffic.SpeedLimitsAndFollowing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTrafficFollowingTest::RunTest(const FString& Parameters)
{
	const FCityRoadNetworkSnapshot Network = StraightTrafficNetwork();
	FCityTrafficSimulation Traffic;
	TestTrue(TEXT("leader route is accepted"), Traffic.AddVehicle(TrafficVehicle(1, 1800), Network));
	TestTrue(TEXT("follower route is accepted"), Traffic.AddVehicle(TrafficVehicle(2, 0), Network));
	Traffic.AdvanceOneStep(Network, 1);
	const TArray<FCityTrafficVehicleState>& Vehicles = Traffic.GetSnapshot().Vehicles;
	TestEqual(TEXT("leader accelerates toward the dirt-class speed limit"), Vehicles[0].SpeedUnitsPerStep,
	          1000);
	TestEqual(TEXT("follower stops at its deterministic safe gap"), Vehicles[1].DistanceUnits, 950);
	TestTrue(TEXT("follower cannot overlap or pass leader"),
	         Vehicles[1].DistanceUnits + Vehicles[1].LengthUnits +
	                 FCityTrafficSimulation::SafeFollowingGapUnits <=
	             Vehicles[0].DistanceUnits);
	TestEqual(TEXT("lane occupancy is aggregated"), Traffic.GetSnapshot().LaneLoads[0].VehicleCount, 2);
	TestTrue(TEXT("following delay creates congestion"),
	         Traffic.GetSnapshot().LaneLoads[0].CongestionPermille > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficConnectorQueueTest, "CitySim.Traffic.IntersectionConnectorQueue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTrafficConnectorQueueTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network;
	Network.Nodes = {{0, FCityCellCoord(0, 0), 0},
	                 {1, FCityCellCoord(3, 0), 0},
	                 {2, FCityCellCoord(3, 3), 0},
	                 {3, FCityCellCoord(6, 0), 0}};
	Network.Edges.Add(TrafficEdge(0, 0, 1, 4));
	Network.Edges.Add(TrafficEdge(1, 2, 1, 4));
	Network.Edges.Add(TrafficEdge(2, 1, 3, 10));
	Network.Connectors = {{0, 1, 0, 2, 0, 0, ECityRoadTurn::Straight},
	                      {1, 1, 1, 2, 0, 0, ECityRoadTurn::Right}};

	FCityTrafficVehicleState First = TrafficVehicle(10, 2900);
	First.Route.Add({2, false, 0});
	FCityTrafficVehicleState Second;
	Second.Id = 20;
	Second.DistanceUnits = 2900;
	Second.Route = {{1, false, 0}, {2, false, 0}};
	FCityTrafficSimulation Traffic;
	TestTrue(TEXT("first intersection route is accepted"), Traffic.AddVehicle(First, Network));
	TestTrue(TEXT("second intersection route is accepted"), Traffic.AddVehicle(Second, Network));
	Traffic.AdvanceOneStep(Network, 1);
	const TArray<FCityTrafficVehicleState>& Vehicles = Traffic.GetSnapshot().Vehicles;
	TestEqual(TEXT("stable lower ID enters the shared outgoing lane first"), Vehicles[0].RouteLegIndex, 1);
	TestEqual(TEXT("second vehicle remains queued at its stop line"), Vehicles[1].RouteLegIndex, 0);
	TestEqual(TEXT("queued vehicle has zero speed"), Vehicles[1].SpeedUnitsPerStep, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficRestoreTest, "CitySim.Traffic.RestoreAndCatchUp",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTrafficRestoreTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network = StraightTrafficNetwork();
	Network.Edges[0] = TrafficEdge(0, 0, 1, 2, 30);
	FCityTrafficSimulation Fine;
	TestTrue(TEXT("leader accepted"), Fine.AddVehicle(TrafficVehicle(1, 1800), Network));
	TestTrue(TEXT("follower accepted"), Fine.AddVehicle(TrafficVehicle(2, 0), Network));
	FString Error;
	TestTrue(TEXT("initial catchup succeeds"), Fine.AdvanceToStep(Network, 2, Error));
	FCityTrafficSnapshot Saved = Fine.GetSnapshot();
	Saved.LaneLoads.Reset();
	FCityTrafficSimulation Loaded;
	TestTrue(TEXT("valid traffic restores"), Loaded.RestoreState(Saved, Network, 2, Error));
	TestEqual(TEXT("loads rebuild from vehicles"), Loaded.GetSnapshot().LaneLoads[0].VehicleCount, 2);
	for (int64 Step = 3; Step <= 8; ++Step)
		Fine.AdvanceOneStep(Network, Step);
	TestTrue(TEXT("coarse catchup replays every fixed step"), Loaded.AdvanceToStep(Network, 8, Error));
	for (int32 Index = 0; Index < Fine.GetSnapshot().Vehicles.Num(); ++Index)
	{
		TestEqual(TEXT("restored distance is deterministic"),
		          Loaded.GetSnapshot().Vehicles[Index].DistanceUnits,
		          Fine.GetSnapshot().Vehicles[Index].DistanceUnits);
		TestEqual(TEXT("restored speed is deterministic"),
		          Loaded.GetSnapshot().Vehicles[Index].SpeedUnitsPerStep,
		          Fine.GetSnapshot().Vehicles[Index].SpeedUnitsPerStep);
	}
	const int32 Before = Loaded.GetSnapshot().Vehicles[0].DistanceUnits;
	TestTrue(TEXT("same step is idempotent"), Loaded.AdvanceToStep(Network, 8, Error));
	TestFalse(TEXT("rewind rejected"), Loaded.AdvanceToStep(Network, 7, Error));
	TestEqual(TEXT("rejected rewind preserves movement"), Loaded.GetSnapshot().Vehicles[0].DistanceUnits,
	          Before);
	auto Reject = [&](const FCityTrafficSnapshot& Invalid) {
		TestFalse(TEXT("invalid traffic rejected atomically"),
		          Loaded.RestoreState(Invalid, Network, 8, Error));
		TestEqual(TEXT("failed restore preserves clock"), Loaded.GetSnapshot().FixedStepId, int64(8));
		TestEqual(TEXT("failed restore preserves vehicles"), Loaded.GetSnapshot().Vehicles[0].DistanceUnits,
		          Before);
	};
	FCityTrafficSnapshot Invalid = Saved;
	Invalid.FixedStepId = 9;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[1].Id = Invalid.Vehicles[0].Id;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[0].Route[0].LaneIndex = 99;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[0].DistanceUnits = MAX_int32;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[0].SpeedUnitsPerStep = MAX_int32;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[0].bCompleted = true;
	Reject(Invalid);
	Invalid = Saved;
	Invalid.Vehicles[1].DistanceUnits = Invalid.Vehicles[0].DistanceUnits;
	Reject(Invalid);
	++Network.SourceRevision;
	TestTrue(TEXT("unrelated placement revision keeps routes valid"),
	         Loaded.AdvanceToStep(Network, 8, Error));
	Network.Edges[0].Cells[1].Y = 1;
	TestFalse(TEXT("road geometry change rejects stale edge identity"),
	          Loaded.AdvanceToStep(Network, 9, Error));
	TestFalse(TEXT("changed road geometry rejects restoration"),
	          Loaded.RestoreState(Saved, Network, 8, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficMixedRouteTest, "CitySim.Traffic.SharedLaneAcrossRoutes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTrafficMixedRouteTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network = StraightTrafficNetwork();
	Network.Edges.Add(TrafficEdge(1, 1, 2, 4, 10));
	Network.Connectors.Add({0, 1, 0, 1, 0, 0, ECityRoadTurn::Straight});
	FCityTrafficVehicleState Leader = TrafficVehicle(1, 1800);
	Leader.Route = {{1, false, 0}};
	FCityTrafficVehicleState Follower = TrafficVehicle(2, 0);
	Follower.Route.Add({1, false, 0});
	Follower.RouteLegIndex = 1;
	FCityTrafficSimulation Traffic;
	TestTrue(TEXT("leader added"), Traffic.AddVehicle(Leader, Network));
	TestTrue(TEXT("different route index follower added"), Traffic.AddVehicle(Follower, Network));
	Traffic.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("same-lane follower observes leader on a different route index"),
	          Traffic.GetSnapshot().Vehicles[1].DistanceUnits, 950);
	FCityTrafficSimulation Entering;
	Follower.RouteLegIndex = 0;
	Follower.DistanceUnits = 2900;
	Follower.SpeedUnitsPerStep = 2000;
	TestTrue(TEXT("outgoing leader added"), Entering.AddVehicle(Leader, Network));
	TestTrue(TEXT("approaching vehicle added"), Entering.AddVehicle(Follower, Network));
	Entering.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("connector entry clamps against downstream leader"),
	          Entering.GetSnapshot().Vehicles[1].DistanceUnits, 950);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencyYieldTest, "CitySim.Traffic.EmergencyYieldAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencyYieldTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network = StraightTrafficNetwork();
	Network.Edges[0] = TrafficEdge(0, 0, 1, 2, 30);
	FCityTrafficSimulation Fine;
	auto Car = TrafficVehicle(1, 1800);
	auto Truck = TrafficVehicle(2, 3600);
	Truck.VehicleType = ECityTripVehicleType::FreightTruck;
	Truck.LengthUnits = 550;
	auto Opposite = TrafficVehicle(3, 25000);
	Opposite.Route[0].bReverse = true;
	auto Walk = TrafficVehicle(4, 3600);
	Walk.VehicleType = ECityTripVehicleType::Walk;
	Walk.LengthUnits = 150;
	auto Engine = TrafficVehicle(99, 0);
	Engine.VehicleType = ECityTripVehicleType::FireEngine;
	Engine.bEmergencyResponding = true;
	Engine.LengthUnits = 550;
	for (const auto& V : {Car, Truck, Opposite, Walk, Engine})
		TestTrue(TEXT("yield fixture accepted"), Fine.AddVehicle(V, Network));
	Fine.AdvanceOneStep(Network, 1);
	const auto First = Fine.GetSnapshot();
	TestTrue(TEXT("car, freight and oncoming vehicle pull over"),
	         First.Vehicles[0].bYielding && First.Vehicles[1].bYielding && First.Vehicles[2].bYielding);
	TestFalse(TEXT("sidewalk pedestrian never yields"), First.Vehicles[3].bYielding);
	TestTrue(TEXT("car rolls forward during pull-over"), First.Vehicles[0].DistanceUnits > 1800);
	TestEqual(TEXT("responder waits for lateral pull-over before overtaking"),
	          First.Vehicles[4].DistanceUnits, 950);
	TestEqual(TEXT("curb movement interpolates"),
	          FCityTrafficSimulation::SampleMotorLaneOffsetCm(First.Vehicles[0], &Car, .5, TEXT("LocalRoad")),
	          140.0);
	TestEqual(
	    TEXT("response uses cleared road center"),
	    FCityTrafficSimulation::SampleMotorLaneOffsetCm(First.Vehicles[4], nullptr, 1, TEXT("LocalRoad")),
	    0.0);
	TestEqual(
	    TEXT("dirt curb keeps motor silhouette clear of pedestrian center"),
	    FCityTrafficSimulation::SampleMotorLaneOffsetCm(First.Vehicles[0], nullptr, 1, TEXT("DirtRoad")),
	    160.0);
	FString Error, Json;
	FCitySaveDocument Doc, Read;
	FCityPlacementModel City;
	Doc.Placement = City.ExportState();
	Doc.Traffic = First;
	Doc.Economy.FixedStepId = 1;
	Doc.Crime.LastFixedStepId = Doc.Fire.LastFixedStepId = Doc.Disasters.LastFixedStepId = 1;
	Doc.Disasters.NextCheckStep = 1;
	TestTrue(TEXT("yield state serializes"), FCityPersistence::Serialize(Doc, Json, Error));
	if (!TestTrue(*FString::Printf(TEXT("yield state reads (%s)"), *Error),
	              FCityPersistence::Deserialize(Json, Read, Error)))
		return false;
	FCityTrafficSimulation Loaded;
	if (!TestTrue(TEXT("mid pull-over restores"), Loaded.RestoreState(Read.Traffic, Network, 1, Error)))
		return false;
	Fine.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("engine overtakes stopped car"),
	         Fine.GetSnapshot().Vehicles[4].DistanceUnits > Fine.GetSnapshot().Vehicles[0].DistanceUnits);
	TestTrue(TEXT("overlapping longitudinal positions on separate tracks restore"),
	         Loaded.RestoreState(Fine.GetSnapshot(), Network, 2, Error));
	for (int64 Step = 3; Step <= 20; ++Step)
		Fine.AdvanceOneStep(Network, Step);
	TestTrue(TEXT("coarse continuation succeeds"), Loaded.AdvanceToStep(Network, 20, Error));
	for (int32 I = 0; I < Fine.GetSnapshot().Vehicles.Num(); ++I)
	{
		const auto& A = Fine.GetSnapshot().Vehicles[I];
		const auto& B = Loaded.GetSnapshot().Vehicles[I];
		TestEqual(TEXT("save/coarse preserves distance"), A.DistanceUnits, B.DistanceUnits);
		TestEqual(TEXT("save/coarse preserves yielding"), A.bYielding, B.bYielding);
	}
	TestFalse(TEXT("car rejoins after responder passes"), Fine.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("car resumes actual travel"), Fine.GetSnapshot().Vehicles[0].DistanceUnits > 1800);
	FCityTrafficSimulation Queue;
	auto Pulled = Car;
	Pulled.bYielding = true;
	auto Behind = TrafficVehicle(2, 950);
	TestTrue(TEXT("pulled vehicle accepted"), Queue.AddVehicle(Pulled, Network));
	TestTrue(TEXT("queued follower accepted"), Queue.AddVehicle(Behind, Network));
	Queue.AdvanceOneStep(Network, 1);
	TestFalse(TEXT("queued traffic cannot deadlock the safe merge"),
	          Queue.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("merging vehicle moves forward during lateral transition"),
	          Queue.GetSnapshot().Vehicles[0].SpeedUnitsPerStep > 0);
	Queue.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("merged queue resumes"), Queue.GetSnapshot().Vehicles[0].DistanceUnits > 1800);
	Engine.bEmergencyResponding = false;
	FCityTrafficSimulation Returning;
	Returning.AddVehicle(Car, Network);
	Returning.AddVehicle(Engine, Network);
	Returning.AdvanceOneStep(Network, 1);
	TestFalse(TEXT("returning crew has no response priority"), Returning.GetSnapshot().Vehicles[0].bYielding);
	Engine.VehicleType = ECityTripVehicleType::GarbageTruck;
	FCityTrafficSimulation Garbage;
	Garbage.AddVehicle(Car, Network);
	Garbage.AddVehicle(Engine, Network);
	Garbage.AdvanceOneStep(Network, 1);
	TestFalse(TEXT("Car stays in lane for garbage truck"), Garbage.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("Garbage truck waits behind car at ordinary safe gap"),
	         Garbage.GetSnapshot().Vehicles[1].DistanceUnits + 550 +
	                 FCityTrafficSimulation::SafeFollowingGapUnits <=
	             Garbage.GetSnapshot().Vehicles[0].DistanceUnits);

	auto Bad = First;
	Bad.Vehicles[0].SpeedUnitsPerStep = FCityTrafficSimulation::AccelerationUnitsPerStep + 1;
	TestFalse(TEXT("excessive curbside crawl rejected"), Loaded.RestoreState(Bad, Network, 1, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencyConnectorPriorityTest,
                                 "CitySim.Traffic.EmergencyIntersectionPriority",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencyConnectorPriorityTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network;
	Network.Nodes = {{0, {0, 0}, 0}, {1, {3, 0}, 0}, {2, {3, 3}, 0}, {3, {6, 0}, 0}};
	Network.Edges = {TrafficEdge(0, 0, 1, 4), TrafficEdge(1, 2, 1, 4), TrafficEdge(2, 1, 3, 4)};
	Network.Connectors = {{0, 1, 0, 2, 0, 0, ECityRoadTurn::Straight},
	                      {1, 1, 1, 2, 0, 0, ECityRoadTurn::Right}};
	auto Car = TrafficVehicle(1, 2900);
	Car.Route = {{1, false, 0}, {2, false, 0}};
	auto Police = TrafficVehicle(99, 2900);
	Police.Route.Add({2, false, 0});
	Police.VehicleType = ECityTripVehicleType::PoliceCar;
	Police.bEmergencyResponding = true;
	FCityTrafficSimulation Traffic;
	TestTrue(TEXT("normal low ID accepted"), Traffic.AddVehicle(Car, Network));
	TestTrue(TEXT("higher ID responder accepted"), Traffic.AddVehicle(Police, Network));
	Traffic.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("cross street waits at stop line"), Traffic.GetSnapshot().Vehicles[0].RouteLegIndex, 0);
	TestEqual(TEXT("responder wins connector despite higher ID"),
	          Traffic.GetSnapshot().Vehicles[1].RouteLegIndex, 1);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOpposingEmergencyYieldTest, "CitySim.Traffic.OpposingResponseYield",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityOpposingEmergencyYieldTest::RunTest(const FString& Parameters)
{
	FCityRoadNetworkSnapshot Network = StraightTrafficNetwork();
	Network.Edges[0] = TrafficEdge(0, 0, 1, 2, 20);
	auto Engine = TrafficVehicle(1, 4000);
	Engine.VehicleType = ECityTripVehicleType::FireEngine;
	Engine.LengthUnits = 550;
	Engine.bEmergencyResponding = true;
	auto Police = TrafficVehicle(2, 11000);
	Police.VehicleType = ECityTripVehicleType::PoliceCar;
	Police.bEmergencyResponding = true;
	Police.Route[0].bReverse = true;
	FCityTrafficSimulation Traffic;
	TestTrue(TEXT("opposing engine accepted"), Traffic.AddVehicle(Engine, Network));
	TestTrue(TEXT("opposing police accepted"), Traffic.AddVehicle(Police, Network));
	Traffic.AdvanceOneStep(Network, 1);
	TestFalse(TEXT("lower ID crew keeps priority"), Traffic.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("other responder pulls aside"), Traffic.GetSnapshot().Vehicles[1].bYielding);
	TestEqual(TEXT("yielding responder stops"), Traffic.GetSnapshot().Vehicles[1].SpeedUnitsPerStep, 0);
	TestEqual(TEXT("response curb transition begins at center"),
	          FCityTrafficSimulation::SampleMotorLaneOffsetCm(Traffic.GetSnapshot().Vehicles[1], &Police, 0,
	                                                          TEXT("LocalRoad")),
	          0.0);
	TestEqual(TEXT("response curb transition ends at curb"),
	          FCityTrafficSimulation::SampleMotorLaneOffsetCm(Traffic.GetSnapshot().Vehicles[1], &Police, 1,
	                                                          TEXT("LocalRoad")),
	          195.0);
	FString Error;
	FCityTrafficSimulation Resumed;
	TestTrue(TEXT("opposing yield restores"), Resumed.RestoreState(Traffic.GetSnapshot(), Network, 1, Error));
	for (int64 Step = 2; Step <= 12; ++Step)
		Traffic.AdvanceOneStep(Network, Step);
	TestTrue(TEXT("opposing yield catch-up"), Resumed.AdvanceToStep(Network, 12, Error));
	TestFalse(TEXT("second crew resumes after first clears"), Traffic.GetSnapshot().Vehicles[1].bYielding);
	TestTrue(TEXT("second crew progresses"),
	         Traffic.GetSnapshot().Vehicles[1].DistanceUnits > Police.DistanceUnits);
	TestEqual(TEXT("opposing continuation deterministic"), Traffic.GetSnapshot().Vehicles[1].DistanceUnits,
	          Resumed.GetSnapshot().Vehicles[1].DistanceUnits);
	FCityTrafficSimulation SharedTarget;
	Engine.DistanceUnits = 7300;
	Engine.EndDistanceUnits = 8000;
	Police.DistanceUnits = 10300;
	Police.EndDistanceUnits = 11000;
	TestTrue(TEXT("shared target engine accepted"), SharedTarget.AddVehicle(Engine, Network));
	TestTrue(TEXT("shared target police accepted"), SharedTarget.AddVehicle(Police, Network));
	for (int64 Step = 1; Step <= 6; ++Step)
		SharedTarget.AdvanceOneStep(Network, Step);
	TestTrue(TEXT("opposing crews both arrive at shared portal"),
	         SharedTarget.GetSnapshot().Vehicles[0].bCompleted &&
	             SharedTarget.GetSnapshot().Vehicles[1].bCompleted);
	TestEqual(TEXT("arrived crew parks on its travel side"),
	          FCityTrafficSimulation::SampleMotorLaneOffsetCm(SharedTarget.GetSnapshot().Vehicles[0], nullptr,
	                                                          1, TEXT("LocalRoad")),
	          85.0);
	Engine.DistanceUnits = 4000;
	Engine.EndDistanceUnits = -1;
	Police.EndDistanceUnits = -1;
	// Very close oncoming traffic must finish its lateral transition before
	// the responder can enter its position, rather than intersect while turning.
	FCityTrafficSimulation Close;
	Police.bEmergencyResponding = false;
	Police.VehicleType = ECityTripVehicleType::Car;
	Police.DistanceUnits = 14000;
	Close.AddVehicle(Engine, Network);
	Close.AddVehicle(Police, Network);
	Close.AdvanceOneStep(Network, 1);
	TestEqual(TEXT("response waits for close oncoming vehicle to pull over"),
	          Close.GetSnapshot().Vehicles[0].DistanceUnits, 4000);
	Close.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("response advances after oncoming lateral transition"),
	         Close.GetSnapshot().Vehicles[0].DistanceUnits > 4000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOccupiedEmergencyCurbTest, "CitySim.Traffic.OccupiedResponseCurb",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityOccupiedEmergencyCurbTest::RunTest(const FString& Parameters)
{
	auto Network = StraightTrafficNetwork();
	Network.Edges[0] = TrafficEdge(0, 0, 1, 2, 11);
	auto Priority = TrafficVehicle(1, 1100);
	Priority.VehicleType = ECityTripVehicleType::Ambulance;
	Priority.bEmergencyResponding = true;
	Priority.Route[0].bReverse = true;
	auto Waiting = TrafficVehicle(10, 2900);
	Waiting.VehicleType = ECityTripVehicleType::Ambulance;
	Waiting.bEmergencyResponding = true;
	auto Truck = TrafficVehicle(20, 3000);
	Truck.VehicleType = ECityTripVehicleType::GarbageTruck;
	Truck.LengthUnits = 550;
	Truck.bYielding = true;
	FCityTrafficSimulation Traffic, Resumed;
	FString Error, Json;
	for (const auto& V : {Priority, Waiting, Truck})
		TestTrue(TEXT("legal passing fixture"), Traffic.AddVehicle(V, Network));
	Traffic.AdvanceOneStep(Network, 1);
	TestFalse(TEXT("passing crew waits in center while curb is occupied"),
	          Traffic.GetSnapshot().Vehicles[1].bYielding);
	TestEqual(TEXT("crew retains exact longitudinal progress"),
	          Traffic.GetSnapshot().Vehicles[1].DistanceUnits, 2900);
	TestEqual(TEXT("waiting crew is stopped"), Traffic.GetSnapshot().Vehicles[1].SpeedUnitsPerStep, 0);
	TestTrue(TEXT("ordinary vehicle clears slot on curb track"), Traffic.GetSnapshot().Vehicles[2].bYielding);
	TestEqual(TEXT("bounded curb crawl"), Traffic.GetSnapshot().Vehicles[2].DistanceUnits, 4000);
	TestTrue(TEXT("mid-crawl state validates"),
	         Resumed.RestoreState(Traffic.GetSnapshot(), Network, 1, Error));
	FCitySaveDocument Crawl, CrawlRead;
	FCityPlacementModel CrawlCity;
	Crawl.Placement = CrawlCity.ExportState();
	Crawl.Traffic = Traffic.GetSnapshot();
	Crawl.Economy.FixedStepId = 1;
	Crawl.Crime.LastFixedStepId = Crawl.Fire.LastFixedStepId = Crawl.Disasters.LastFixedStepId = 1;
	Crawl.Disasters.NextCheckStep = 1;
	TestTrue(TEXT("moving curb JSON serializes"), FCityPersistence::Serialize(Crawl, Json, Error));
	if (!TestTrue(TEXT("moving curb JSON reads"), FCityPersistence::Deserialize(Json, CrawlRead, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("moving curb JSON restores"), Resumed.RestoreState(CrawlRead.Traffic, Network, 1, Error));
	Crawl.SchemaVersion = 58;
	TestFalse(TEXT("older writers cannot emit moving curb state"),
	          FCityPersistence::Serialize(Crawl, Json, Error));
	CrawlRead.Traffic.Vehicles[1].bYielding = true;
	CrawlRead.Traffic.Vehicles[1].SpeedUnitsPerStep = 1;
	TestFalse(TEXT("responding crew cannot crawl while yielding"),
	          Resumed.RestoreState(CrawlRead.Traffic, Network, 1, Error));
	Traffic.AdvanceOneStep(Network, 2);
	TestTrue(TEXT("crew only pulls aside after a safe curb gap opens"),
	         Traffic.GetSnapshot().Vehicles[1].bYielding);
	FCitySaveDocument Doc, Read;
	FCityPlacementModel City;
	Doc.Placement = City.ExportState();
	Doc.Traffic = Traffic.GetSnapshot();
	Doc.Economy.FixedStepId = 2;
	Doc.Crime.LastFixedStepId = Doc.Fire.LastFixedStepId = Doc.Disasters.LastFixedStepId = 2;
	Doc.Disasters.NextCheckStep = 2;
	TestTrue(TEXT("mid-pass JSON serializes"), FCityPersistence::Serialize(Doc, Json, Error));
	TestTrue(TEXT("mid-pass JSON reads"), FCityPersistence::Deserialize(Json, Read, Error));
	TestTrue(TEXT("mid-pass restores"), Resumed.RestoreState(Read.Traffic, Network, 2, Error));
	for (int64 Step = 3; Step <= 40; ++Step)
	{
		Traffic.AdvanceOneStep(Network, Step);
		FCityTrafficSimulation Check;
		if (!TestTrue(*FString::Printf(TEXT("every step is loadable (%lld: %s)"), Step, *Error),
		              Check.RestoreState(Traffic.GetSnapshot(), Network, Step, Error)))
			return false;
	}
	TestTrue(TEXT("coarse continuation"), Resumed.AdvanceToStep(Network, 40, Error));
	for (int32 I = 0; I < 3; ++I)
	{
		const auto& A = Traffic.GetSnapshot().Vehicles[I];
		const auto& B = Resumed.GetSnapshot().Vehicles[I];
		TestTrue(TEXT("every trip actually arrives"), A.bCompleted);
		TestEqual(TEXT("reload preserves actual arrival clock"), A.CompletedStep, B.CompletedStep);
		TestEqual(TEXT("reload preserves position"), A.DistanceUnits, B.DistanceUnits);
		TestEqual(TEXT("reload preserves lateral state"), A.bYielding, B.bYielding);
	}
	return true;
}
#endif
