#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadUnderpassTest, "CitySim.Roads.LayeredUnderpasses",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRoadUnderpassTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	FString Error;
	TMap<FName, FCityBuildingDefinition> Definitions;
	auto Road = [](FCityPlacementModel& City, FCityCellCoord A, FCityCellCoord B, ECityRoadProfile Profile,
	               const FCityRoadDefinition& Def) {
		auto P = City.BeginRoadPreview(A, Def.Id, Def.PlacementCostPerCell);
		P.End = B;
		P.Profile = Profile;
		return P;
	};
	for (const auto& Def : {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(),
	                        FCityRoadDefinition::Avenue(), FCityRoadDefinition::Highway()})
		for (bool Horizontal : {false, true})
			for (bool Reverse : {false, true})
				for (bool GroundFirst : {false, true})
				{
					FCityPlacementModel City(100000);
					City.SetTerrain(&Terrain);
					FCityCellCoord A = Horizontal ? FCityCellCoord(2, 8) : FCityCellCoord(8, 2),
					               B = Horizontal ? FCityCellCoord(16, 8) : FCityCellCoord(8, 16);
					if (Reverse)
						Swap(A, B);
					const FCityCellCoord C = Horizontal ? FCityCellCoord(8, 4) : FCityCellCoord(4, 8),
					                     D = Horizontal ? FCityCellCoord(8, 12) : FCityCellCoord(12, 8);
					auto Build = [&](bool Raised) {
						auto P = Road(City, Raised ? A : C, Raised ? B : D,
						              Raised ? ECityRoadProfile::Elevated : ECityRoadProfile::Ground, Def);
						const auto Preview = City.ValidateRoad(P);
						const auto Commit = City.CommitRoad(P);
						TestTrue(TEXT("Both orders, axes, directions and classes permit a clear crossing"),
						         Commit.IsSuccess());
						if (!Commit.IsSuccess())
							AddError(Commit.Detail);
						TestEqual(TEXT("Crossing commit charges exactly the validated cost"), Commit.Cost,
						          Preview.Cost);
					};
					Build(!GroundFirst);
					Build(GroundFirst);
					const FCityCellCoord Ground(8, 8), Deck(8, 8, 1);
					TestTrue(TEXT("Independent road addresses share the physical crossing"),
					         City.IsRoad(Ground) && City.IsRoad(Deck));
					TestTrue(TEXT("Grid occupants exist independently"),
					         City.GetGrid().GetOccupant(Ground) == FCityPlacementModel::RoadEntityId &&
					             City.GetGrid().GetOccupant(Deck) == FCityPlacementModel::RoadEntityId);
					TestFalse(TEXT("Crossing deck has no pier"), City.HasRoadSupport(Deck));
					TestEqual(TEXT("Ground neighbors remain perpendicular"), City.GetRoadNeighborMask(Ground),
					          static_cast<uint8>(Horizontal ? 5 : 10));
					TestEqual(TEXT("Deck neighbors remain along the span"), City.GetRoadNeighborMask(Deck),
					          static_cast<uint8>(Horizontal ? 10 : 5));
					TestTrue(TEXT("Crossing state validates"), City.ValidateRoadStructures(Error));
					FCityRoadNetworkSnapshot Network;
					FCityRoadNetworkBuilder::Build(City, Def, Network);
					TestTrue(TEXT("Layered graph is valid"), Network.IsValid(&Error));
					TArray<FCityCellCoord> Cells;
					TestTrue(TEXT("Deck route remains connected through its ramps"),
					         Network.FindRoute(A, B, ECityRoadAccess::Car, Cells));
					TestTrue(TEXT("Deck route includes raised crossing"),
					         Cells.Contains(Deck) && !Cells.Contains(Ground));
					TestTrue(TEXT("Ground route passes beneath"),
					         Network.FindRoute(C, D, ECityRoadAccess::Car, Cells));
					TestTrue(TEXT("Ground route contains ground crossing only"),
					         Cells.Contains(Ground) && !Cells.Contains(Deck));
					TestFalse(TEXT("No vertical turn at the crossing"),
					          Network.FindRoute(A, D, ECityRoadAccess::Car, Cells));
					FCityTrafficVehicleState Upper, Lower;
					Upper.Id = 1;
					Lower.Id = 2;
					Upper.bEmergencyResponding = true;
					Upper.VehicleType = ECityTripVehicleType::FireEngine;
					TestTrue(TEXT("Upper vehicle route builds"),
					         FCityTrafficSimulation::BuildRoute(Network, A, B, Upper));
					TestTrue(TEXT("Lower vehicle route builds"),
					         FCityTrafficSimulation::BuildRoute(Network, C, D, Lower));
					Upper.DistanceUnits = Reverse ? 8000 : 6000;
					Lower.DistanceUnits = 4000;
					FCityTrafficSimulation Traffic;
					TestTrue(TEXT("Upper vehicle accepted"), Traffic.AddVehicle(Upper, Network, &Error));
					TestTrue(TEXT("Lower vehicle accepted at same map position"),
					         Traffic.AddVehicle(Lower, Network, &Error));
					if (Traffic.GetSnapshot().Vehicles.Num() != 2)
					{
						AddError(Error);
						continue;
					}
					FVector Up, Down, Direction;
					FCityTrafficSimulation::SamplePosition(Upper, nullptr, Network, 1, Up, Direction);
					FCityTrafficSimulation::SamplePosition(Lower, nullptr, Network, 1, Down, Direction);
					TestTrue(TEXT("Sampled positions preserve the independent road levels"),
					         Up.X == Down.X && Up.Y == Down.Y && Up.Z == 1 && Down.Z == 0);
					Traffic.AdvanceOneStep(Network, 1);
					TestFalse(TEXT("Emergency vehicle above cannot pull ground vehicle aside"),
					          Traffic.GetSnapshot().Vehicles[1].bYielding);
					TestTrue(TEXT("Both level vehicles move"),
					         Traffic.GetSnapshot().Vehicles[0].DistanceUnits > Upper.DistanceUnits &&
					             Traffic.GetSnapshot().Vehicles[1].DistanceUnits > Lower.DistanceUnits);
					FCitySaveDocument Save;
					Save.Placement = City.ExportState();
					Save.Terrain = Config;
					Save.Traffic = Traffic.GetSnapshot();
					Save.Economy.FixedStepId = 1;
					FString Json;
					TestTrue(TEXT("Layered city serializes"), FCityPersistence::Serialize(Save, Json, Error));
					FCitySaveDocument Loaded;
					TestTrue(TEXT("Layered city parses"), FCityPersistence::Deserialize(Json, Loaded, Error));
					auto Restored =
					    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
					TestTrue(TEXT("Layered placement restores"), Restored.IsValid());
					if (!Restored)
					{
						AddError(Error);
						continue;
					}
					Restored->SetTerrain(&Terrain);
					TestTrue(TEXT("Restored clearance revalidates"), Restored->ValidateRoadStructures(Error));
					FCityRoadNetworkSnapshot RestoredGraph;
					FCityRoadNetworkBuilder::Build(*Restored, Def, RestoredGraph);
					FCityTrafficSimulation Resumed;
					TestTrue(TEXT("Traffic resumes exact layered routes/progress"),
					         Resumed.RestoreState(Loaded.Traffic, RestoredGraph, 1, Error));
					TestTrue(TEXT("Fine traffic stepping continues"),
					         Traffic.AdvanceToStep(Network, 8, Error));
					TestTrue(TEXT("Saved traffic catch-up continues"),
					         Resumed.AdvanceToStep(RestoredGraph, 8, Error));
					for (int32 I = 0; I < 2; ++I)
						TestTrue(TEXT("Save/catch-up preserves each level's movement"),
						         Traffic.GetSnapshot().Vehicles[I].DistanceUnits ==
						                 Resumed.GetSnapshot().Vehicles[I].DistanceUnits &&
						             Traffic.GetSnapshot().Vehicles[I].SpeedUnitsPerStep ==
						                 Resumed.GetSnapshot().Vehicles[I].SpeedUnitsPerStep);
					TestEqual(TEXT("Traffic fingerprint survives"),
					          FCityTrafficSimulation::GetNetworkFingerprint(RestoredGraph),
					          Save.Traffic.NetworkFingerprint);
					auto Corrupt = Loaded.Placement;
					Corrupt.RoadCells.Add({20, 20, 1});
					TestFalse(
					    TEXT("Raised orphan is rejected"),
					    FCityPlacementModel::CreateValidatedFromState(Corrupt, Definitions, Error).IsValid());
					TestTrue(TEXT("Ground crossing can be demolished independently"),
					         Restored->RemoveRoad(Ground));
					TestTrue(TEXT("Deck survives ground demolition"),
					         Restored->IsRoad(Deck) && Restored->ValidateRoadStructures(Error));
					TestTrue(TEXT("Deck can be demolished independently"), City.RemoveRoad(Deck));
					TestTrue(TEXT("Ground crossing survives deck demolition"),
					         City.IsRoad(Ground) && !City.IsRoad(Deck));
				}
	const auto Def = FCityRoadDefinition::Local();
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	auto Span = Road(City, {2, 8}, {16, 8}, ECityRoadProfile::Elevated, Def);
	City.CommitRoad(Span);
	const int64 Funds = City.GetFunds();
	auto Pier = Road(City, {9, 4}, {9, 12}, ECityRoadProfile::Ground, Def);
	TestFalse(TEXT("Pier crossing rejected without charge"), City.CommitRoad(Pier).IsSuccess());
	auto Ramp = Road(City, {4, 4}, {4, 12}, ECityRoadProfile::Ground, Def);
	TestFalse(TEXT("Ramp crossing rejected"), City.CommitRoad(Ramp).IsSuccess());
	auto Parallel = Road(City, {7, 8}, {9, 8}, ECityRoadProfile::Ground, Def);
	TestFalse(TEXT("Parallel ground road beneath deck rejected"), City.CommitRoad(Parallel).IsSuccess());
	TestEqual(TEXT("Rejected underpasses preserve treasury"), City.GetFunds(), Funds);
	for (const int32 X : {4, 9})
	{
		FCityPlacementModel GroundFirst(100000);
		GroundFirst.SetTerrain(&Terrain);
		auto Existing = Road(GroundFirst, {X, 4}, {X, 12}, ECityRoadProfile::Ground, Def);
		GroundFirst.CommitRoad(Existing);
		const auto Before = GroundFirst.ExportState();
		auto Invalid = Road(GroundFirst, {2, 8}, {16, 8}, ECityRoadProfile::Elevated, Def);
		TestFalse(TEXT("Ground-first ramp/pier crossing is rejected"),
		          GroundFirst.CommitRoad(Invalid).IsSuccess());
		TestTrue(TEXT("Rejected span preserves the existing ground road and funds"),
		         GroundFirst.ExportState().RoadCells == Before.RoadCells &&
		             GroundFirst.GetFunds() == Before.Funds);
	}
	auto LowSpan = City.GetRoadStructures()[0];
	LowSpan.DeckHeightCm = 449;
	TestFalse(TEXT("Vehicle clearance envelope rejects a low deck"),
	          City.CanGroundCrossSpan(LowSpan, {8, 8}, Error));
	FCityBuildingDefinition Building;
	Building.DefinitionId = TEXT("Home");
	Building.DisplayName = TEXT("Home");
	Building.Cost = 0;
	Building.Footprint = {1, 1};
	auto Lot = City.BeginPreview(Building, {8, 8});
	TestFalse(TEXT("Building cannot occupy the underpass corridor"),
	          City.CommitPlacement(Lot, Building).IsSuccess());
	TestFalse(TEXT("Zoning cannot occupy raised corridor"), City.CanSetZone({8, 8}));
	// Schema37 migration preserves IDs, chosen geometry and moving-vehicle progress.
	FCityRoadNetworkSnapshot Graph;
	FCityRoadNetworkBuilder::Build(City, Def, Graph);
	FCityTrafficVehicleState Vehicle;
	Vehicle.Id = 10;
	FCityTrafficSimulation::BuildRoute(Graph, {2, 8}, {16, 8}, Vehicle);
	Vehicle.DistanceUnits = 6000;
	FCityTrafficSimulation Moving;
	Moving.AddVehicle(Vehicle, Graph, &Error);
	return true;
}
#endif
