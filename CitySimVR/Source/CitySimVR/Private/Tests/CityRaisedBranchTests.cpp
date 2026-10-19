#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRaisedBranchTest, "CitySim.Roads.RaisedBranches",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRaisedBranchTest::RunTest(const FString& Parameters)
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
	for (const auto& Def : {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(), FCityRoadDefinition::Avenue(),
	                        FCityRoadDefinition::Highway()})
		for (bool Reverse : {false, true})
		{
			FCityPlacementModel City(100000);
			City.SetTerrain(&Terrain);
			const auto Preview = [&](FCityCellCoord A, FCityCellCoord B)
			{
				auto P = City.BeginRoadPreview(A, Def.Id, Def.PlacementCostPerCell);
				P.End = B;
				P.Profile = ECityRoadProfile::Elevated;
				return P;
			};
			auto Main = Preview({2, 8}, {16, 8});
			TestTrue(TEXT("Trunk builds"), City.CommitRoad(Main).IsSuccess());
			const FCityCellCoord Junction(8, 8, 1);
			auto Branch =
			    Preview(Reverse ? FCityCellCoord(8, 16) : Junction, Reverse ? Junction : FCityCellCoord(8, 16));
			auto Review = City.ValidateRoad(Branch);
			const int64 Funds = City.GetFunds();
			const auto Result = City.CommitRoad(Branch);
			if (!TestTrue(TEXT("Ground-to-deck branch builds in either direction"), Result.IsSuccess()))
			{
				AddError(Result.Detail);
				continue;
			}
			TestEqual(TEXT("Branch charges reviewed cost"), Funds - City.GetFunds(), Review.Cost);
			TestEqual(TEXT("Raised T has three connected approaches"), City.GetRoadNeighborMask(Junction),
			          static_cast<uint8>(11));
			TestTrue(TEXT("Branch ownership validates"), City.ValidateRoadStructures(Error));
			if (Def.Id == TEXT("LocalRoad") && !Reverse)
			{
				const auto Values = UWorld::InitializationValues()
				                        .AllowAudioPlayback(false)
				                        .CreatePhysicsScene(false)
				                        .CreateNavigation(false)
				                        .CreateAISystem(false)
				                        .ShouldSimulatePhysics(false);
				UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityRaisedBranchGeometry"), nullptr,
				                                    true, ERHIFeatureLevel::Num, &Values);
				auto* Board = World->SpawnActor<ACityBoardActor>();
				Board->SimulationSubsystem = World->GetSubsystem<UCitySimulationSubsystem>();
				Board->SimulationSubsystem->Terrain = Terrain;
				Board->PlacementModel = &City;
				Board->RebuildRoadInstances();
				const auto* Paving = Board->RoadSurfaceProfile->GetProcMeshSection(2);
				bool DeckPaving = false, GroundPaving = false;
				if (Paving)
					for (const auto& V : Paving->ProcVertexBuffer)
						if (V.Position.X > 6400 && V.Position.X < 7200 && V.Position.Y > 6400 && V.Position.Y < 7200)
						{
							DeckPaving |= FMath::IsNearlyEqual(V.Position.Z, 604., .1);
							GroundPaving |= FMath::IsNearlyEqual(V.Position.Z, 4., .1);
						}
				TestTrue(TEXT("T junction paving stays on raised deck"), DeckPaving && !GroundPaving);
				Board->SetToolMode(ECityToolMode::Road);
				FCityCellCoord Picked;
				TestTrue(TEXT("Road tool ray picks raised attachment"),
				         Board->PickCellFromWorldRay(
				             Board->GetActorTransform().TransformPosition(FVector(6800, 6800, 2000)),
				             Board->GetActorTransform().TransformVector(FVector(0, 0, -1)), Picked) &&
				             Picked == Junction);
				Board->RoadPreview = City.BeginRoadPreview(Junction);
				Board->RoadPreview.End = {8, 16};
				Board->RefreshRoadDrawingProfile();
				TestTrue(TEXT("Deck attachment automatically selects raised construction"),
				         Board->RoadPreview.Profile == ECityRoadProfile::Elevated);
				Board->AdjustRoadElevation(1);
				TestEqual(TEXT("First stick flick adjusts automatic branch height"), Board->RoadPreview.ElevationStep,
				          1);
				World->DestroyWorld(false);
			}
			double Height = 0;
			TestTrue(TEXT("Endpoint height samples"), City.TrySampleStructureHeight(8.5, 8.5, Height));
			TestEqual(TEXT("No ramp at raised endpoint"), Height, 600.);
			FCityRoadNetworkSnapshot Graph;
			FCityRoadNetworkBuilder::Build(City, Def, Graph);
			TArray<FCityCellCoord> Route;
			TestTrue(TEXT("Traffic routes from trunk down branch"),
			         Graph.FindRoute({2, 8}, {8, 16}, ECityRoadAccess::Car, Route));
			TestTrue(TEXT("Route uses raised junction"), Route.Contains(Junction));
			FCitySaveDocument Save, Loaded;
			Save.Placement = City.ExportState();
			Save.Terrain = Config;
			FString Json;
			TestTrue(TEXT("Branch serializes"), FCityPersistence::Serialize(Save, Json, Error));
			TestTrue(TEXT("Branch parses"), FCityPersistence::Deserialize(Json, Loaded, Error));
			auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
			if (!TestTrue(TEXT("Branch restores"), Restored.IsValid()))
			{
				AddError(Error);
				continue;
			}
			Restored->SetTerrain(&Terrain);
			TestTrue(TEXT("Restored branch validates terrain"), Restored->ValidateRoadStructures(Error));
			Save.SchemaVersion = 49;
			TestFalse(TEXT("Old schema rejects raised endpoints"), FCityPersistence::Serialize(Save, Json, Error));
			TestTrue(TEXT("Remove trunk away from junction"), Restored->RemoveRoad({6, 8, 1}));
			TestTrue(TEXT("Branch keeps independently owned raised endpoint"),
			         Restored->IsRoad(Junction) && Restored->ValidateRoadStructures(Error));
			double BranchHeight = 0;
			Restored->TrySampleStructureHeight(8.0, 8.0, BranchHeight);
			TestEqual(TEXT("Whole endpoint stays flat after trunk removal"), BranchHeight, 600.);
			auto Corner = Restored->BeginRoadPreview(Junction, Def.Id, Def.PlacementCostPerCell);
			Corner.End = {16, 8};
			Corner.Profile = ECityRoadProfile::Elevated;
			TestTrue(TEXT("Perpendicular endpoint creates corner"), Restored->CommitRoad(Corner).IsSuccess());
			TestEqual(TEXT("Corner has two perpendicular approaches"), Restored->GetRoadNeighborMask(Junction),
			          static_cast<uint8>(3));
			TestTrue(TEXT("Corner validates"), Restored->ValidateRoadStructures(Error));
			TestTrue(TEXT("Remove one corner arm"), Restored->RemoveRoad({12, 8, 1}));
			TestTrue(TEXT("Other arm remains"), Restored->IsRoad(Junction));
			TestTrue(TEXT("Remove last arm"), Restored->RemoveRoad({8, 12, 1}));
			TestFalse(TEXT("No orphan raised endpoint"), Restored->IsRoad(Junction));
		}
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	auto P = City.BeginRoadPreview({8, 8, 1});
	P.End = {8, 16};
	P.Profile = ECityRoadProfile::Elevated;
	TestFalse(TEXT("Raised endpoints cannot be invented on empty cells"), City.CommitRoad(P).IsSuccess());
	return true;
}
#endif
