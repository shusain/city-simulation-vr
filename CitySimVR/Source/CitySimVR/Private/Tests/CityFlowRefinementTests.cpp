#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityRoadPresentation.h"

namespace
{
FCityRoadNetworkSnapshot FlowNetwork(bool Highway = false)
{
	FCityRoadNetworkSnapshot N;
	N.Nodes = {{0, {0, 0}, 0}, {1, {30, 0}, 0}};
	FCityRoadEdge E;
	E.Id=0; E.StartNodeId=0; E.EndNodeId=1; E.SpeedUnitsPerStep=Highway ? 10 : 4;
	E.ForwardLanes=E.ReverseLanes=Highway ? 2 : 1;
	E.Access=ECityRoadAccess::Car | ECityRoadAccess::Freight | ECityRoadAccess::Pedestrian;
	E.RoadClass=Highway ? ECityRoadClass::Highway : ECityRoadClass::Local;
	for(int32 X=0;X<=30;++X) E.Cells.Add({X,0});
	N.Edges.Add(E); return N;
}
FCityTrafficVehicleState FlowVehicle(uint64 Id, int32 Position, bool Walk = false, int32 Track = 0)
{
	FCityTrafficVehicleState V;
	V.Id=Id; V.DistanceUnits=Position; V.Route.Add({0,false,0});
	V.VehicleType=Walk ? ECityTripVehicleType::Walk : ECityTripVehicleType::Car;
	V.LengthUnits=Walk ? 80 : 350; V.PedestrianTrack=Track; return V;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWalkingFlowTest,"CitySim.Flow.SideBySideAndPassing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityWalkingFlowTest::RunTest(const FString& Parameters)
{
	const auto N=FlowNetwork(); FCityTrafficSimulation T;
	for(const auto& V : {FlowVehicle(1,1000,true),FlowVehicle(2,800,true),FlowVehicle(3,1150,true),FlowVehicle(4,1500,true,1)})
		TestTrue(TEXT("pedestrian spacing accepts real tracks"),T.AddVehicle(V,N));
	const auto Before=T.GetSnapshot(); T.AdvanceOneStep(N,1);
	const auto& State=T.GetSnapshot();
	TestEqual(TEXT("blocked follower uses free passing track"),State.Vehicles[1].PedestrianTrack,1);
	TestTrue(TEXT("pedestrian actually passes the blocked walker"),State.Vehicles[1].DistanceUnits>State.Vehicles[0].DistanceUnits);
	TestTrue(TEXT("side-by-side occupants use distinct visible offsets"),
		FCityTrafficSimulation::SamplePedestrianOffsetCm(State.Vehicles[0],nullptr,1,100)!=
		FCityTrafficSimulation::SamplePedestrianOffsetCm(State.Vehicles[1],nullptr,1,100));
	FString Error,Json; FCitySaveDocument Doc,Read; FCityPlacementModel City;
	Doc.Placement=City.ExportState(); Doc.Traffic=State; Doc.Economy.FixedStepId=Doc.Disasters.LastFixedStepId=1;
	TestTrue(TEXT("pedestrian tracks serialize"),FCityPersistence::Serialize(Doc,Json,Error));
	TestTrue(TEXT("pedestrian tracks read"),FCityPersistence::Deserialize(Json,Read,Error));
	FCityTrafficSimulation Loaded;
	TestTrue(TEXT("passing tracks restore"),Loaded.RestoreState(Read.Traffic,N,1,Error));
	for(int64 Step=2;Step<=30;++Step)
	{
		T.AdvanceOneStep(N,Step); Loaded.AdvanceOneStep(N,Step);
		FCityTrafficSimulation Safe;
		if(!TestTrue(TEXT("every passing step is safe to restore"),Safe.RestoreState(T.GetSnapshot(),N,Step,Error))) {AddError(Error);return false;}
		for(int32 I=0;I<4;++I)
		{
			TestEqual(TEXT("passing reload retains movement"),Loaded.GetSnapshot().Vehicles[I].DistanceUnits,T.GetSnapshot().Vehicles[I].DistanceUnits);
			TestEqual(TEXT("passing reload retains track"),Loaded.GetSnapshot().Vehicles[I].PedestrianTrack,T.GetSnapshot().Vehicles[I].PedestrianTrack);
		}
	}
	auto Invalid=Read.Traffic; Invalid.Vehicles[0].PedestrianTrack=2;
	TestFalse(TEXT("third walking track rejected"),Loaded.RestoreState(Invalid,N,1,Error));
	for(FName Id : {FName(TEXT("DirtRoad")),FName(TEXT("LocalRoad")),FName(TEXT("Avenue"))})
		TestTrue(TEXT("two sidewalk tracks have at least40cm lateral centre spacing"),CityRoadPresentation::SidewalkWidth(Id)*800*.5>=39.99);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOccupiedShopDepartureTest,"CitySim.Flow.OccupiedShopDeparture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityOccupiedShopDepartureTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000); auto Road=City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road,{20,8}); TestTrue(TEXT("shopping road"),City.CommitRoad(Road).IsSuccess());
	auto Lot=[&](FCityCellCoord P) {FCityBuildingDefinition D;D.DefinitionId=TEXT("FlowLot");D.DisplayName=TEXT("Flow lot");D.Cost=0;D.Footprint={1,1}; auto Preview=City.BeginPreview(D,P);return City.CommitPlacement(Preview,D).EntityId;};
	const auto Home=Lot({10,9}),Shop=Lot({18,9});
	FCityRoadNetworkSnapshot N;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);
	FCityPopulationSnapshot P;
	auto Person=[&](uint64 Id) {FCityCitizenRecord C;C.Id=Id;C.HomeEntityId=C.CurrentLocationEntityId=Home;C.TripDestinationEntityId=Shop;C.TripPurpose=ECityTripPurpose::Purchase;C.TripVehicleType=ECityTripVehicleType::Walk;P.Citizens.Add(C);};
	FCityTrafficSimulation T;
	for(uint64 Id : {99ull,100ull})
	{
		Person(Id);auto V=FlowVehicle(Id*2,0,true,Id%2);V.SourceId=Id;V.OriginEntityId=Home;V.DestinationEntityId=Shop;
		TestTrue(TEXT("busy shop route"),FCityTrafficSimulation::BuildRoute(N,{10,8},{18,8},V));
		V.DistanceUnits=V.EndDistanceUnits;TestTrue(TEXT("store portal occupied on both tracks"),T.AddVehicle(V,N));
	}
	Person(23);T.UpdateTrips(City,nullptr,P,N,0);
	TestTrue(TEXT("free origin starts toward occupied store"),T.GetSnapshot().Vehicles.ContainsByPredicate([](const auto& V){return V.SourceId==23 && !V.bCompleted;}));
	Person(25);Person(24);T.UpdateTrips(City,nullptr,P,N,0);
	TestTrue(TEXT("full origin holds the next shopper rather than teleporting"),!T.GetSnapshot().WaitingDepartures.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRollingYieldTest,"CitySim.Flow.RollingYieldAndRejoin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityRollingYieldTest::RunTest(const FString& Parameters)
{
	const auto N=FlowNetwork(); FCityTrafficSimulation T;
	auto Car=FlowVehicle(1,1800);auto Engine=FlowVehicle(99,0);Engine.VehicleType=ECityTripVehicleType::FireEngine;Engine.bEmergencyResponding=true;Engine.LengthUnits=550;
	T.AddVehicle(Car,N);T.AddVehicle(Engine,N);T.AdvanceOneStep(N,1);
	const auto Rolled=T.GetSnapshot().Vehicles[0];
	TestTrue(TEXT("car moves forward as it pulls over"),Rolled.bYielding && Rolled.DistanceUnits>Car.DistanceUnits);
	TestTrue(TEXT("pull-over heading follows its curve"),FCityTrafficSimulation::SampleMotorHeading(Rolled,&Car,.5,TEXT("LocalRoad"),FVector(1,0,0),Rolled.SpeedUnitsPerStep).Y>0);
	FCityTrafficSimulation Merge;Car.bYielding=true;
	TestTrue(TEXT("curbside vehicle accepted"),Merge.AddVehicle(Car,N));Merge.AdvanceOneStep(N,1);
	const auto Rejoined=Merge.GetSnapshot().Vehicles[0];
	TestTrue(TEXT("clear responder permits immediate forward rejoin"),!Rejoined.bYielding && Rejoined.SpeedUnitsPerStep>0 && Rejoined.DistanceUnits>Car.DistanceUnits);
	TestTrue(TEXT("rejoin heading turns back while moving forward"),FCityTrafficSimulation::SampleMotorHeading(Rejoined,&Car,.5,TEXT("LocalRoad"),FVector(1,0,0),Rejoined.SpeedUnitsPerStep).Y<0);
	FString Error;FCityTrafficSimulation Loaded;
	for(int64 Step=2;Step<=20;++Step)
	{
		T.AdvanceOneStep(N,Step);
		if(!TestTrue(TEXT("rolling yield retains safe loadable state"),Loaded.RestoreState(T.GetSnapshot(),N,Step,Error))) {AddError(Error);return false;}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHighwayFlowTest,"CitySim.Flow.HighwayLaneYield",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHighwayFlowTest::RunTest(const FString& Parameters)
{
	const auto N=FlowNetwork(true);FCityTrafficSimulation T;
	auto Car=FlowVehicle(1,3000);Car.SpeedUnitsPerStep=2000;
	auto Engine=FlowVehicle(99,0);Engine.bEmergencyResponding=true;Engine.VehicleType=ECityTripVehicleType::FireEngine;Engine.LengthUnits=550;Engine.SpeedUnitsPerStep=10000;
	auto Opposite=FlowVehicle(2,24000);Opposite.Route[0].bReverse=true;
	T.AddVehicle(Car,N);T.AddVehicle(Opposite,N);T.AddVehicle(Engine,N);T.AdvanceOneStep(N,1);
	TestEqual(TEXT("highway car moves into right lane"),T.GetSnapshot().Vehicles[0].Route[0].LaneIndex,1);
	TestTrue(TEXT("right-lane avoidance keeps forward travel"),T.GetSnapshot().Vehicles[0].DistanceUnits>3000 && !T.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("opposite carriageway continues"),!T.GetSnapshot().Vehicles[1].bYielding && T.GetSnapshot().Vehicles[1].DistanceUnits>24000);
	TestTrue(TEXT("fast responder waits for visible right-lane transition"),T.GetSnapshot().Vehicles[2].DistanceUnits<=3000-Car.LengthUnits-FCityTrafficSimulation::SafeFollowingGapUnits);
	FVector Pos,Dir;TestTrue(TEXT("changing lanes retains forward interpolation"),FCityTrafficSimulation::SamplePosition(T.GetSnapshot().Vehicles[0],&Car,N,.5,Pos,Dir));
	TestTrue(TEXT("lane change is between committed forward positions"),Pos.X>3 && Pos.X<6);
	FCityTrafficSimulation Blocked;auto Right=Car;Right.Id=3;Right.Route[0].LaneIndex=1;
	Blocked.AddVehicle(Car,N);Blocked.AddVehicle(Right,N);Blocked.AddVehicle(Engine,N);Blocked.AdvanceOneStep(N,1);
	TestEqual(TEXT("occupied right lane prevents unsafe merge"),Blocked.GetSnapshot().Vehicles[0].Route[0].LaneIndex,0);
	TestFalse(TEXT("blocked inner lane does not invent shoulder clearance"),Blocked.GetSnapshot().Vehicles[0].bYielding);
	TestTrue(TEXT("blocked merge keeps the inner queue moving"),Blocked.GetSnapshot().Vehicles[0].DistanceUnits>Car.DistanceUnits);
	FString Error;FCityTrafficSimulation Check;
	TestTrue(TEXT("blocked highway merge is safe to reload"),Check.RestoreState(Blocked.GetSnapshot(),N,1,Error));
	auto Avenue=N;Avenue.Edges[0].RoadClass=ECityRoadClass::Avenue;
	FCityTrafficSimulation AvenueFlow;
	AvenueFlow.AddVehicle(Car,Avenue);Engine.SpeedUnitsPerStep=0;AvenueFlow.AddVehicle(Engine,Avenue);AvenueFlow.AdvanceOneStep(Avenue,1);
	TestTrue(TEXT("four-lane avenue also avoids stacking cars on one shoulder"),AvenueFlow.GetSnapshot().Vehicles[0].Route[0].LaneIndex==1 && !AvenueFlow.GetSnapshot().Vehicles[0].bYielding);
	TestEqual(TEXT("avenue responder stays in its own inner carriageway"),FCityTrafficSimulation::SampleMotorLaneOffsetCm(AvenueFlow.GetSnapshot().Vehicles[1],nullptr,1,TEXT("Avenue")),85.0);
	FCityPlacementModel City(100000);auto Road=City.BeginRoadPreview({0,8},TEXT("Highway"));City.UpdateRoadPreviewEnd(Road,{10,8});City.CommitRoad(Road);
	auto Local=City.BeginRoadPreview({10,8});City.UpdateRoadPreviewEnd(Local,{15,8});City.CommitRoad(Local);
	FCityRoadNetworkSnapshot Graph;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),Graph);
	TestTrue(TEXT("outer lane has real connectors"),Graph.Connectors.ContainsByPredicate([](const auto& C){return C.IncomingLane==1 || C.OutgoingLane==1;}));
	for(bool Reverse : {false,true})
	{
		FCityTrafficVehicleState Route;Route.Id=1;
		TestTrue(TEXT("multi-lane to single-lane route exists both ways"),FCityTrafficSimulation::BuildRoute(Graph,Reverse?FCityCellCoord(14,8):FCityCellCoord(1,8),Reverse?FCityCellCoord(1,8):FCityCellCoord(14,8),Route));
		FCityTrafficSimulation Trip;TestTrue(TEXT("outer route validates"),Trip.AddVehicle(Route,Graph));
		TestTrue(TEXT("outer lane connector reaches destination"),Trip.AdvanceToStep(Graph,20,Error));
		if(!Trip.GetSnapshot().Vehicles.IsEmpty()) TestTrue(TEXT("outer lane trip completed"),Trip.GetSnapshot().Vehicles[0].bCompleted);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMultiLaneExitTest,"CitySim.Flow.ResponseAtQueuedMultiLaneExit",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityMultiLaneExitTest::RunTest(const FString&)
{
	auto N=FlowNetwork(true);N.Nodes={{0,{0,0},0},{1,{1,0},0},{2,{2,0},0}};
	N.Edges[0].EndNodeId=1;N.Edges[0].Cells={{0,0},{1,0}};
	auto Local=N.Edges[0];Local.Id=1;Local.StartNodeId=1;Local.EndNodeId=2;
	Local.Cells={{1,0},{2,0}};Local.ForwardLanes=Local.ReverseLanes=1;Local.RoadClass=ECityRoadClass::Local;
	N.Edges.Add(Local);N.Connectors={{0,1,0,1,0,0,ECityRoadTurn::Straight},{1,1,0,1,1,0,ECityRoadTurn::Straight}};
	auto Inner=FlowVehicle(1,900);Inner.Route.Add({1,false,0});
	auto Outer=Inner;Outer.Id=2;Outer.Route[0].LaneIndex=1;
	auto Rear=FlowVehicle(3,50);Rear.Route[0].EdgeId=1;Rear.bYielding=true;
	auto Front=Rear;Front.Id=4;Front.DistanceUnits=900;
	auto Ambulance=FlowVehicle(5,50);Ambulance.LengthUnits=350;Ambulance.VehicleType=ECityTripVehicleType::Ambulance;Ambulance.bEmergencyResponding=true;Ambulance.Route.Add({1,false,0});
	FCityTrafficSimulation T;FString Error;
	for(const auto& V:{Inner,Outer,Rear,Front,Ambulance})if(!TestTrue(TEXT("legal queued exit fixture"),T.AddVehicle(V,N,&Error))){AddError(Error);return false;}
	for(int64 Step=1;Step<=20;++Step)
	{
		T.AdvanceOneStep(N,Step);FCityTrafficSimulation Reload;
		if(!TestTrue(TEXT("every draining-queue step keeps safe restorable gaps"),Reload.RestoreState(T.GetSnapshot(),N,Step,Error))){AddError(Error);return false;}
	}
	for(const auto& V:T.GetSnapshot().Vehicles)TestTrue(TEXT("ordinary and emergency traffic all clear the exit"),V.bCompleted);
	return true;
}
#endif
