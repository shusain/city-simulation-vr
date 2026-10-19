#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityRoadPresentation.h"
#include "Interaction/CityBoardPicking.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

namespace
{
	uint64 PlaceTrafficLot(FCityPlacementModel& City, const FName Id, const FCityCellCoord& Cell)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(1, 1);
		Definition.Cost = 0;
		Definition.EntranceSides = 15;
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Cell);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}
	void MakeTrafficRoad(FCityPlacementModel& City)
	{
		FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, FCityCellCoord(20, 8));
		check(City.CommitRoad(Road).IsSuccess());
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficTripTest, "CitySim.Traffic.LiveTripLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTrafficTripTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	MakeTrafficRoad(City);
	const uint64 Home = PlaceTrafficLot(City, TEXT("Home"), {2, 9});
	const uint64 Shop = PlaceTrafficLot(City, TEXT("Shop"), {16, 9});
	const uint64 Factory = PlaceTrafficLot(City, TEXT("Factory"), {18, 9});
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityTrafficVehicleState Reverse;
	Reverse.Id = 900;
	TestTrue(TEXT("reverse partial-edge route exists"),
	         FCityTrafficSimulation::BuildRoute(Network, {16, 8}, {2, 8}, Reverse));
	TestEqual(TEXT("partial first edge begins at portal"), Reverse.StartDistanceUnits, 4000);
	TestEqual(TEXT("partial final edge ends at portal"), Reverse.EndDistanceUnits, 18000);
	FVector Position, Direction;
	TestTrue(TEXT("reverse route samples"),
	         FCityTrafficSimulation::SamplePosition(Reverse, nullptr, Network, 1, Position, Direction));
	TestEqual(TEXT("reverse route starts at correct cell"), Position.X, 16.0);
	TestTrue(TEXT("reverse travel faces backwards"), Direction.X < 0);
	FCityTrafficSimulation ReverseTraffic;
	TestTrue(TEXT("partial route validates"), ReverseTraffic.AddVehicle(Reverse, Network));
	FString Error;
	TestTrue(TEXT("partial route completes"), ReverseTraffic.AdvanceToStep(Network, 10, Error));
	TestEqual(TEXT("vehicle stops at final portal"), ReverseTraffic.GetSnapshot().Vehicles[0].DistanceUnits, 18000);
	FCityRoadNetworkSnapshot Highway = Network;
	for (FCityRoadEdge& Edge : Highway.Edges)
		Edge.Access = ECityRoadAccess::Car | ECityRoadAccess::Freight;
	// Production road-class changes rebuild the snapshot through the builder.
	// This hand-edited copy must replace its shared derived routing data too.
	Highway.PrepareRouting();
	Reverse.VehicleType = ECityTripVehicleType::Walk;
	TestFalse(TEXT("pedestrian route excludes highways"),
	          FCityTrafficSimulation::BuildRoute(Highway, {2, 8}, {16, 8}, Reverse));
	TestTrue(TEXT("Preparing the edited copy preserves the original walking route"),
	         FCityTrafficSimulation::BuildRoute(Network, {2, 8}, {16, 8}, Reverse));

	FCityPopulationSnapshot Population;
	for (uint64 Id = 11; Id <= 13; ++Id)
	{
		FCityCitizenRecord Citizen;
		Citizen.Id = Id;
		Citizen.HouseholdId = Citizen.HomeEntityId = Home;
		Citizen.TripDestinationEntityId = Shop;
		Citizen.TripPurpose = ECityTripPurpose::Commute;
		Citizen.TripVehicleType = Id == 12 ? ECityTripVehicleType::Walk : ECityTripVehicleType::Car;
		Population.Citizens.Add(Citizen);
	}
	FCityTransitTripRecord Delivery;
	Delivery.Id = 1;
	Delivery.DestinationEntityId = Factory;
	Delivery.VehicleType = ECityTripVehicleType::FreightTruck;
	Delivery.Purpose = ECityTripPurpose::MoveInArrival;
	Delivery.ArrivalStep = 4;
	Population.TransitTrips.Add(Delivery);
	FCityTrafficSimulation Traffic;
	Traffic.UpdateTrips(City, nullptr, Population, Network, 1);
	TestEqual(TEXT("blocked car queues while walker and truck can spawn"), Traffic.GetSnapshot().Vehicles.Num(), 3);
	FCityTrafficSnapshot Previous = Traffic.GetSnapshot();
	Traffic.AdvanceOneStep(Network, 1);
	const FCityTrafficVehicleState* Car = Traffic.GetSnapshot().Vehicles.FindByPredicate(
	    [](const FCityTrafficVehicleState& V) { return V.SourceId == 11; });
	const FCityTrafficVehicleState* Before =
	    Previous.Vehicles.FindByPredicate([](const FCityTrafficVehicleState& V) { return V.SourceId == 11; });
	if (!TestNotNull(TEXT("car is scheduled from household"), Car))
		return false;
	TestTrue(TEXT("authoritative states interpolate"),
	         FCityTrafficSimulation::SamplePosition(*Car, Before, Network, 0.5, Position, Direction));
	TestEqual(TEXT("interpolation is between committed positions"), Position.X, 2.5);
	bool bReturned = false, bDelivered = false, bQueuedCarSpawned = false;
	for (int64 Step = 2; Step <= 40; ++Step)
	{
		Traffic.UpdateTrips(City, nullptr, Population, Network, Step);
		Traffic.AdvanceOneStep(Network, Step);
		for (const FCityTrafficVehicleState& Vehicle : Traffic.GetSnapshot().Vehicles)
		{
			bReturned |= Vehicle.bReturning;
			bQueuedCarSpawned |= Vehicle.SourceId == 11 && !Vehicle.bTransitTrip;
			if (Vehicle.bTransitTrip && Vehicle.bCompleted)
			{
				Population.TransitTrips[0].bCompleted = true;
				bDelivered = true;
			}
		}
		FCityTrafficSimulation Restored;
		TestTrue(TEXT("every live state is restorable"),
		         Restored.RestoreState(Traffic.GetSnapshot(), Network, Step, Error));
	}
	TestTrue(TEXT("citizens make a return trip after dwell"), bReturned);
	TestTrue(TEXT("queued citizen eventually enters traffic"), bQueuedCarSpawned);
	TestTrue(TEXT("delivery completes through movement"), bDelivered);
	TestTrue(TEXT("completed delivery record retires"),
	         Traffic.GetSnapshot().Vehicles.Num() <= Population.Citizens.Num());
	City.DemolishEntity(Shop);
	Traffic.UpdateTrips(City, nullptr, Population, Network, 41);
	TestTrue(TEXT("deleted destinations remove traffic"), Traffic.GetSnapshot().Vehicles.IsEmpty());
	Population.TransitTrips[0].bCompleted = false;
	Traffic.UpdateTrips(City, nullptr, Population, Network, 41);
	TestFalse(TEXT("delivery can retry"), Traffic.GetSnapshot().Vehicles.IsEmpty());
	FCityRoadPreview Extension = City.BeginRoadPreview({20, 8});
	City.UpdateRoadPreviewEnd(Extension, {24, 8});
	TestTrue(TEXT("road edit commits"), City.CommitRoad(Extension).IsSuccess());
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	Traffic.ReconcileNetwork(Network);
	TestTrue(TEXT("road edit cancels stale routes without arrival"), Traffic.GetSnapshot().Vehicles.IsEmpty());
	TestFalse(TEXT("cancellation does not mark delivery complete"), Population.TransitTrips[0].bCompleted);
	Traffic.UpdateTrips(City, nullptr, Population, Network, 42);
	TestFalse(TEXT("pending delivery reroutes on next scheduling step"), Traffic.GetSnapshot().Vehicles.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityVisibleTrafficTest, "CitySim.Traffic.VisiblePool",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityVisibleTrafficTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityTrafficPoolTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACityBoardActor* Board = World->SpawnActor<ACityBoardActor>();
	Board->bStreetSkeletalAnimation = false; // Retain the instanced fallback regression; CitizenWalk tests cover animation.
	UCitySimulationSubsystem* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	Simulation->PlacementModel->SetTerrain(nullptr);
	MakeTrafficRoad(*Simulation->PlacementModel);
	FCityRoadNetworkBuilder::Build(*Simulation->PlacementModel, FCityRoadDefinition::Local(),
	                               Simulation->RoadNetworkSnapshot);
	Board->SimulationSubsystem = Simulation;
	Board->PlacementModel = Simulation->PlacementModel.Get();
	Board->RebuildRoadInstances();
	TestTrue(TEXT("Straight road paving is a bounded sampled class section"),
	         Board->RoadSurfaceProfile->GetProcMeshSection(2) != nullptr);
	if (const auto* Paving = Board->RoadSurfaceProfile->GetProcMeshSection(2))
	{
		TestTrue(TEXT("Sampled pavement is populated"), !Paving->ProcVertexBuffer.IsEmpty());
		bool bSharedHeight = true;
		for (const auto& Vertex : Paving->ProcVertexBuffer)
			bSharedHeight &= FMath::IsNearlyEqual(
			    static_cast<double>(Vertex.Position.Z),
			    Board->GetRoadProfileHeight(Vertex.Position.X / CityBoardPicking::LogicalCellSizeCm,
			                                Vertex.Position.Y / CityBoardPicking::LogicalCellSizeCm) +
			        4.0,
			    .01);
		TestTrue(TEXT("Paving and vehicle profile share terrain samples"), bSharedHeight);
	}
	TestTrue(TEXT("walkable roads generate sidewalk geometry"), Board->SidewalkSurface->GetNumSections() > 0);
	if (const auto* Walk = Board->SidewalkSurface->GetProcMeshSection(0))
	{
		bool bSharedCoordinates = true;
		for (const auto& Vertex : Walk->ProcVertexBuffer)
			bSharedCoordinates &= Vertex.UV0.Equals(
			    FVector2D(Vertex.Position.X, Vertex.Position.Y) / CityBoardPicking::LogicalCellSizeCm, 0.0001);
		TestTrue(TEXT("Sidewalk texture coordinates agree across cell and strip boundaries"), bSharedCoordinates);
	}

	const FProcMeshSection* Details = Board->RoadDetailSurface->GetProcMeshSection(0);
	TestNotNull(TEXT("Streets generate pooled curb geometry"), Details);
	if (Details)
	{
		TestTrue(TEXT("Road details remain bounded per tile"),
		         Details->ProcVertexBuffer.Num() <= Simulation->PlacementModel->GetRoadCells().Num() * 128);
		bool bBlockedApproach = false;
		for (const FProcMeshVertex& Vertex : Details->ProcVertexBuffer)
		{
			const double CellX = Vertex.Position.X / CityBoardPicking::LogicalCellSizeCm;
			const double CellY = Vertex.Position.Y / CityBoardPicking::LogicalCellSizeCm;
			// Interior of the fixture's horizontal road: curbs must stay at its lateral edges.
			if (CellX > 3 && CellX < 18 && CellY > 8 && CellY < 9)
				bBlockedApproach |= FMath::Abs(CellY - 8.5) < 0.339;
		}
		TestFalse(TEXT("Curbs do not cross open road approaches"), bBlockedApproach);
		bool bFacesOutward = true;
		for (int32 Index = 0; Index < Details->ProcIndexBuffer.Num(); Index += 3)
		{
			const auto& A = Details->ProcVertexBuffer[Details->ProcIndexBuffer[Index]];
			const auto& B = Details->ProcVertexBuffer[Details->ProcIndexBuffer[Index + 1]];
			const auto& C = Details->ProcVertexBuffer[Details->ProcIndexBuffer[Index + 2]];
			bFacesOutward &= FVector::DotProduct(
			                     FVector::CrossProduct(B.Position - A.Position, C.Position - A.Position), A.Normal) < 0;
		}
		TestTrue(TEXT("Curb tops and walls face outward with Unreal clockwise winding"), bFacesOutward);
	}

	for (const FName Id : {FName(TEXT("DirtRoad")), FName(TEXT("LocalRoad")), FName(TEXT("Avenue"))})
	{
		const double Width = CityRoadPresentation::SidewalkWidth(Id);
		const double Offset = CityRoadPresentation::SidewalkOffset(Id);
		TestTrue(TEXT("walking strip sits outside roadway and inside tile"),
		         Width > 0 && Offset - Width * 0.5 >= CityRoadPresentation::SurfaceWidth(Id) * 0.5 - 0.0001 &&
		             Offset + Width * 0.5 <= 0.5001);
	}
	TestEqual(TEXT("highways have no sidewalk"), CityRoadPresentation::SidewalkWidth(TEXT("Highway")), 0.0);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FCityTrafficVehicleState Vehicle;
		Vehicle.Id = Index + 1;
		Vehicle.VehicleType = static_cast<ECityTripVehicleType>(Index);
		TestTrue(
		    TEXT("visible fixture route builds"),
		    FCityTrafficSimulation::BuildRoute(Simulation->RoadNetworkSnapshot, {2 + Index * 3, 8}, {18, 8}, Vehicle));
		TestTrue(TEXT("visible fixture vehicle added"),
		         Simulation->Traffic.AddVehicle(Vehicle, Simulation->RoadNetworkSnapshot));
	}
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("walker has one pooled instance"), Board->CitizenInstances->GetInstanceCount(), 1);
	TestEqual(TEXT("car has one pooled instance"), Board->TrafficVehicleInstances->GetInstanceCount(), 1);
	TestEqual(TEXT("truck has one pooled instance"), Board->TrafficTruckInstances->GetInstanceCount(), 1);
	FTransform WalkerTransform;
	Board->CitizenInstances->GetInstanceTransform(0, WalkerTransform);
	TestEqual(TEXT("walker follows its sidewalk track"), WalkerTransform.GetLocation().Y,
	          (8.9 - CityRoadPresentation::SidewalkWidth(TEXT("LocalRoad")) * .25) * CityBoardPicking::LogicalCellSizeCm);
	FTransform Initial;
	Board->TrafficVehicleInstances->GetInstanceTransform(0, Initial);
	Simulation->PreviousTraffic = Simulation->Traffic.GetSnapshot();
	Simulation->Traffic.AdvanceOneStep(Simulation->RoadNetworkSnapshot, 1);
	Simulation->PreviousTraffic = Simulation->Traffic.GetSnapshot();
	Board->UpdatePopulationTraffic();
	FTransform Moved;
	Board->TrafficVehicleInstances->GetInstanceTransform(0, Moved);
	TestTrue(TEXT("visible car follows committed traffic position"), Moved.GetLocation().X > Initial.GetLocation().X);
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("repeated render reuses pool size"), Board->TrafficVehicleInstances->GetInstanceCount(), 1);
	FCityTrafficSnapshot YieldState = Simulation->Traffic.GetSnapshot();
	YieldState.Vehicles[1].bYielding = true;
	YieldState.Vehicles[1].SpeedUnitsPerStep = 0;
	FString YieldError;
	TestTrue(TEXT("curbside car state restores"),
	         Simulation->Traffic.RestoreState(YieldState, Simulation->RoadNetworkSnapshot, YieldState.FixedStepId,
	                                          YieldError));
	Simulation->PreviousTraffic = Simulation->Traffic.GetSnapshot();
	Board->UpdatePopulationTraffic();
	FTransform Curbside;
	Board->TrafficVehicleInstances->GetInstanceTransform(0, Curbside);
	TestEqual(TEXT("native car pool visibly moves toward curb"), Curbside.GetLocation().Y - Moved.GetLocation().Y,
	          110.0);
	Board->CitizenInstances->GetInstanceTransform(0, WalkerTransform);
	TestEqual(TEXT("yielding leaves walker on sidewalk"), WalkerTransform.GetLocation().Y,
	          (8.9 - CityRoadPresentation::SidewalkWidth(TEXT("LocalRoad")) * .25) * CityBoardPicking::LogicalCellSizeCm);
	Simulation->Traffic.Reset();
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("reset clears pooled traffic"), Board->TrafficVehicleInstances->GetInstanceCount(), 0);
	for (uint64 Identity = 1; Identity <= 3; ++Identity)
	{
		FCityTrafficVehicleState Walker;
		Walker.Id = Identity;
		Walker.VehicleType = ECityTripVehicleType::Walk;
		TestTrue(TEXT("three-palette walker route builds"),
		         FCityTrafficSimulation::BuildRoute(Simulation->RoadNetworkSnapshot,
		                                            {2 + static_cast<int32>(Identity - 1) * 3, 8}, {18, 8}, Walker));
		TestTrue(TEXT("three-palette walker added"),
		         Simulation->Traffic.AddVehicle(Walker, Simulation->RoadNetworkSnapshot));
	}
	Simulation->PreviousTraffic = Simulation->Traffic.GetSnapshot();
	Board->UpdatePopulationTraffic();
	const auto WalkerCount = [&]()
	{
		return Board->CitizenInstances->GetInstanceCount() + Board->OfficeCitizenInstances->GetInstanceCount() +
		       Board->WorkerCitizenInstances->GetInstanceCount();
	};
	TestEqual(TEXT("style variation retains the number of real walkers"), WalkerCount(), 3);
	if (Board->OfficeCitizenInstances->GetStaticMesh()->GetName().Contains(TEXT("PC_Office")))
	{
		TestEqual(TEXT("office walker has a separate bounded pool"), Board->OfficeCitizenInstances->GetInstanceCount(),
		          1);
		TestEqual(TEXT("worker walker has a separate bounded pool"), Board->WorkerCitizenInstances->GetInstanceCount(),
		          1);
	}
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("repeated style rendering cannot duplicate walkers"), WalkerCount(), 3);
	Simulation->Traffic.Reset();
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("reset clears all citizen styles"), WalkerCount(), 0);
	Simulation->BuildingCatalog = NewObject<UCityBuildingCatalog>();
	FCityBuildingDefinition HomeDefinition;
	HomeDefinition.DefinitionId = TEXT("Home");
	HomeDefinition.DisplayName = TEXT("Home");
	HomeDefinition.Footprint = FCityCellFootprint(1, 1);
	HomeDefinition.Cost = 0;
	HomeDefinition.EntranceSides = 15;
	HomeDefinition.ConstructionSteps = 2;
	HomeDefinition.DevelopmentDemandType = ECityDevelopmentDemandType::Residential;
	HomeDefinition.ConstructionMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT(
	    "/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A")));
	Simulation->BuildingCatalog->Definitions.Add(HomeDefinition);
	Board->BuildingCatalog = Simulation->BuildingCatalog;
	const uint64 Home = PlaceTrafficLot(*Simulation->PlacementModel, TEXT("Home"), {16, 9});
	Simulation->Development.Reconcile(*Simulation->PlacementModel, Simulation->BuildingCatalog);
	Board->RebuildLiveConstructionAnimations();
	Board->UpdateLiveConstructionAnimations(.25f);
	TestEqual(TEXT("active construction has one pooled crane cable"),
	          Board->LiveConstructionCraneCables->GetInstanceCount(), 1);
	FTransform Cable, Hook, Jib;
	Board->LiveConstructionCraneCables->GetInstanceTransform(0, Cable);
	Board->LiveConstructionCraneHooks->GetInstanceTransform(0, Hook);
	Board->LiveConstructionCraneJibs->GetInstanceTransform(0, Jib);
	TestTrue(TEXT("cable bottom stays attached to moving hook"),
	         Cable.TransformPosition(FVector(0, 0, -50)).Equals(Hook.GetLocation(), .01));
	TestTrue(TEXT("cable top stays at jib height"),
	         FMath::IsNearlyEqual(Cable.TransformPosition(FVector(0, 0, 50)).Z, Jib.GetLocation().Z, .01));
	Simulation->Population.QueueConstructionArrival(Home, 0);
	Board->RebuildLiveConstructionAnimations();
	TestEqual(TEXT("waiting for delivery removes animated cable"),
	          Board->LiveConstructionCraneCables->GetInstanceCount(), 0);

	Simulation->EconomyTuning.FixedStepSeconds = 0.1;
	Simulation->SetSimulationPaused(false);
	Simulation->Tick(0.05f);
	TestTrue(TEXT("render-only frame schedules no vehicle"), Simulation->GetTrafficSnapshot().Vehicles.IsEmpty());
	Simulation->Tick(0.05f);
	TestEqual(TEXT("subsystem schedules real delivery"), Simulation->GetTrafficSnapshot().Vehicles.Num(), 1);
	for (int32 Step = 2; Step <= 4; ++Step)
		Simulation->Tick(0.1f);
	TestFalse(TEXT("old four-step timer cannot complete a distant delivery"),
	          Simulation->GetPopulationSnapshot().TransitTrips[0].bCompleted);
	TestEqual(TEXT("construction waits for delivery"), Simulation->GetDevelopmentSnapshot().Find(Home)->Stage,
	          ECityDevelopmentStage::Construction);
	for (int32 Step = 5; Step <= 12; ++Step)
		Simulation->Tick(0.1f);
	TestTrue(TEXT("actual delivery retires its source record"),
	         Simulation->GetPopulationSnapshot().TransitTrips.IsEmpty());
	TestEqual(TEXT("construction finishes after actual arrival"),
	          Simulation->GetDevelopmentSnapshot().Find(Home)->Stage, ECityDevelopmentStage::Operational);
	Board->RebuildLiveConstructionAnimations();
	TestEqual(TEXT("completed construction has no cable instances"),
	          Board->LiveConstructionCraneCables->GetInstanceCount(), 0);
	FCityBuildingDefinition ShopDefinition = HomeDefinition;
	ShopDefinition.DefinitionId = TEXT("Shop");
	ShopDefinition.DisplayName = TEXT("Shop");
	ShopDefinition.DevelopmentDemandType = ECityDevelopmentDemandType::Commercial;
	Simulation->BuildingCatalog->Definitions.Add(ShopDefinition);
	const uint64 Shop = PlaceTrafficLot(*Simulation->PlacementModel, TEXT("Shop"), {18, 9});
	Simulation->Development.Reconcile(*Simulation->PlacementModel, Simulation->BuildingCatalog);
	Simulation->Population.QueueConstructionArrival(Shop, Simulation->GetEconomySnapshot().FixedStepId);
	bool bSawOpeningTruck = false;
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Simulation->Tick(0.1f);
		for (const FCityTransitTripRecord& Trip : Simulation->GetPopulationSnapshot().TransitTrips)
		{
			if (Trip.DestinationEntityId == Shop && Trip.Purpose == ECityTripPurpose::OpeningDelivery)
			{
				bSawOpeningTruck = true;
				TestEqual(TEXT("opening truck follows completed construction"),
				          Simulation->GetDevelopmentSnapshot().Find(Shop)->Stage, ECityDevelopmentStage::Operational);
				if (!Trip.bCompleted)
					TestEqual(TEXT("pending opening contributes no jobs"), Simulation->GetEconomySnapshot().Jobs, 0);
			}
		}
	}
	TestTrue(TEXT("subsystem schedules opening truck after construction truck"), bSawOpeningTruck);
	const FCityBusinessLedgerRecord* OpenedShop = Simulation->GetPopulationSnapshot().Businesses.FindByPredicate(
	    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == Shop; });
	if (TestNotNull(TEXT("shop ledger exists"), OpenedShop))
		TestTrue(TEXT("subsystem arrival opens business"), OpenedShop->bInitialFreightReceived);
	Simulation->SetSimulationPaused(true);
	const int64 PausedTrafficStep = Simulation->GetTrafficSnapshot().FixedStepId;
	Simulation->Tick(1.0f);
	TestEqual(TEXT("pause holds authoritative traffic clock"), Simulation->GetTrafficSnapshot().FixedStepId,
	          PausedTrafficStep);
	Simulation->Traffic.Reset();
	FCityTrafficVehicleState Export;
	Export.Id = 201;
	Export.SourceId = 100;
	Export.bTransitTrip = true;
	Export.OriginEntityId = Shop;
	Export.DestinationEntityId = 0;
	Export.VehicleType = ECityTripVehicleType::FreightTruck;
	TestTrue(TEXT("export route reaches entrance"),
	         FCityTrafficSimulation::BuildRoute(Simulation->RoadNetworkSnapshot, {18, 8},
	                                            FCityPlacementModel::EntranceCell, Export));
	TestTrue(TEXT("export vehicle enters pool model"),
	         Simulation->Traffic.AddVehicle(Export, Simulation->RoadNetworkSnapshot));
	Board->UpdatePopulationTraffic();
	TestEqual(TEXT("city-edge export is visibly rendered"), Board->TrafficTruckInstances->GetInstanceCount(), 1);
	World->DestroyWorld(false);
	return true;
}
#endif
