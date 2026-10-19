#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMixedRaisedTest, "CitySim.Roads.MixedRaisedJunctions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityMixedRaisedTest::RunTest(const FString& Parameters)
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
	const TArray<FCityRoadDefinition> Types = {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(),
	                                           FCityRoadDefinition::Avenue(), FCityRoadDefinition::Highway()};
	for (const auto& A : Types)
		for (const auto& B : Types)
			for (bool Crossing : {false, true})
				for (bool Reverse : {false, true})
				{
					if (A.Id == B.Id)
						continue;
					FCityPlacementModel City(100000);
					City.SetTerrain(&Terrain);
					const FCityCellCoord Junction(8, 8, 1);
					auto Main = City.BeginRoadPreview({2, 8}, A.Id, A.PlacementCostPerCell);
					Main.End = {16, 8};
					Main.Profile = ECityRoadProfile::Elevated;
					TestTrue(TEXT("First typed span builds"), City.CommitRoad(Main).IsSuccess());
					FCityCellCoord Start = Crossing ? FCityCellCoord(8, 2) : Junction, End(8, 16);
					if (Reverse)
						Swap(Start, End);
					auto Branch = City.BeginRoadPreview(Start, B.Id, B.PlacementCostPerCell);
					Branch.End = End;
					Branch.Profile = ECityRoadProfile::Elevated;
					const auto Review = City.ValidateRoad(Branch);
					const int64 Funds = City.GetFunds();
					const auto Result = City.CommitRoad(Branch);
					if (!TestTrue(TEXT("Mixed raised T/cross builds"), Result.IsSuccess()))
					{
						AddError(Result.Detail);
						continue;
					}
					TestEqual(TEXT("No unreviewed junction charge"), Funds - City.GetFunds(), Review.Cost);
					const auto Wide = A.PlacementCostPerCell > B.PlacementCostPerCell ? A : B;
					TestEqual(TEXT("Shared paving uses widest connected class"),
					          City.GetRoadDefinitionId(Junction), Wide.Id);
					TestEqual(TEXT("First span retains purchased type"),
					          City.GetRoadStructures()[0].DefinitionId, A.Id);
					TestEqual(TEXT("Second span retains purchased type"),
					          City.GetRoadStructures()[1].DefinitionId, B.Id);
					TestTrue(TEXT("Mixed span classes validate"), City.ValidateRoadStructures(Error));
					FCityRoadNetworkSnapshot Graph;
					FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Graph);
					TestTrue(TEXT("Mixed graph validates"), Graph.IsValid(&Error));
					TArray<FCityCellCoord> Route;
					TestTrue(TEXT("Cars turn between differing raised classes"),
					         Graph.FindRoute({2, 8}, {8, 16}, ECityRoadAccess::Car, Route));
					if (Crossing && B.Id != TEXT("Highway"))
						TestTrue(TEXT("Walkable arm stays connected across highway junction"),
						         Graph.FindRoute({8, 2}, {8, 16}, ECityRoadAccess::Pedestrian, Route));
					if (A.Id == TEXT("LocalRoad") && B.Id == TEXT("Avenue") && Crossing && !Reverse)
					{
						const auto Values = UWorld::InitializationValues()
						                        .AllowAudioPlayback(false)
						                        .CreatePhysicsScene(false)
						                        .CreateNavigation(false)
						                        .CreateAISystem(false)
						                        .ShouldSimulatePhysics(false);
						UWorld* World =
						    UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityMixedRaisedGeometry"),
						                        nullptr, true, ERHIFeatureLevel::Num, &Values);
						auto* Board = World->SpawnActor<ACityBoardActor>();
						Board->SimulationSubsystem = World->GetSubsystem<UCitySimulationSubsystem>();
						Board->SimulationSubsystem->Terrain = Terrain;
						Board->PlacementModel = &City;
						Board->RebuildRoadInstances();
						TestEqual(TEXT("Raised class boundary has matching portal widths"),
						          Board->GetRoadWidthAt(Junction, true, 0.),
						          Board->GetRoadWidthAt({7, 8, 1}, true, 1.));
						TestEqual(TEXT("Portal tapers between local and avenue"),
						          Board->GetRoadWidthAt(Junction, true, 0.), .78);
						TestEqual(TEXT("Mixed junction stays on deck"),
						          Board->GetRoadProfileHeight(8.5, 8.5, 1), 600.);
						World->DestroyWorld(false);
					}
					FCitySaveDocument Save, Loaded;
					Save.Placement = City.ExportState();
					Save.Terrain = Config;
					FString Json;
					TestTrue(TEXT("Mixed ownership serializes"),
					         FCityPersistence::Serialize(Save, Json, Error));
					TestTrue(TEXT("Mixed ownership parses"),
					         FCityPersistence::Deserialize(Json, Loaded, Error));
					auto Restored =
					    FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
					if (!TestTrue(TEXT("Mixed ownership restores"), Restored.IsValid()))
					{
						AddError(Error);
						continue;
					}
					Restored->SetTerrain(&Terrain);
					TestTrue(TEXT("Restored types validate geometry and original costs"),
					         Restored->ValidateRoadStructures(Error));
					Save.SchemaVersion = 50;
					TestFalse(TEXT("Old format cannot lose individual span types"),
					          FCityPersistence::Serialize(Save, Json, Error));
					TestTrue(TEXT("Remove first typed span"), Restored->RemoveRoad({6, 8, 1}));
					TestEqual(TEXT("Surviving junction reverts to remaining span type"),
					          Restored->GetRoadDefinitionId(Junction), B.Id);
					TestTrue(TEXT("Remaining typed span validates"), Restored->ValidateRoadStructures(Error));
				}
	return true;
}
#endif
