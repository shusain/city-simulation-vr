#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"
#include "Persistence/CityPersistence.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadStructureTest, "CitySim.Roads.ExplicitStructures",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRoadStructureTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("Flat land with authoritative lake/river generates"), Terrain.Generate(Config));
	auto Preview = [](FCityPlacementModel& City, FCityCellCoord A, FCityCellCoord B, ECityRoadProfile Type,
	                  FName Id = TEXT("LocalRoad"), int64 Cost = 10)
	{
		auto P = City.BeginRoadPreview(A, Id, Cost);
		City.UpdateRoadPreviewEnd(P, B);
		P.Profile = Type;
		return P;
	};
	FString Error;
	for (const auto& Def : {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(), FCityRoadDefinition::Avenue(),
	                        FCityRoadDefinition::Highway()})
		for (const bool Reverse : {false, true})
			for (const bool Horizontal : {false, true})
			{
				FCityPlacementModel City(100000);
				City.SetTerrain(&Terrain);
				const FCityCellCoord A = Horizontal ? FCityCellCoord(1, 5) : FCityCellCoord(5, 1),
				                     B = Horizontal ? FCityCellCoord(15, 5) : FCityCellCoord(5, 20);
				auto P = Preview(City, Reverse ? B : A, Reverse ? A : B, ECityRoadProfile::Elevated, Def.Id,
				                 Def.PlacementCostPerCell);
				const auto Check = City.ValidateRoad(P);
				TestTrue(TEXT("Every class, direction and axis has a valid span"), Check.IsSuccess());
				if (!Check.IsSuccess())
				{
					AddError(Check.Detail);
					continue;
				}
				const auto Result = City.CommitRoad(P);
				TestEqual(TEXT("Preview cost equals committed cost"), Result.Cost, Check.Cost);
				TestEqual(TEXT("Funds charged once"), City.GetFunds(), 100000 - Result.Cost);
				TestTrue(TEXT("Persisted geometry validates against terrain"), City.ValidateRoadStructures(Error));
				const auto Span = City.GetRoadStructures()[0];
				const FCityCellCoord Middle((A.X + B.X) / 2, (A.Y + B.Y) / 2, 1),
				    Side(Middle.X + (Horizontal ? 0 : 1), Middle.Y + (Horizontal ? 1 : 0));
				auto SideRoad = Preview(City, Side, Side, ECityRoadProfile::Ground);
				TestTrue(TEXT("Adjacent ground road may exist"), City.CommitRoad(SideRoad).IsSuccess());
				TestFalse(TEXT("Ground neighbor cannot become a raised intersection"),
				          City.CanRoadsConnect(Middle, Side));
				TestFalse(TEXT("Raised road cannot provide building frontage"), City.SupportsBuildingFrontage(Middle));
				FCityRoadNetworkSnapshot Graph;
				FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Graph);
				TArray<FCityCellCoord> Route;
				TestTrue(TEXT("Span routes through both approaches"),
				         Graph.FindRoute(A, B, ECityRoadAccess::None, Route));
				TestFalse(TEXT("Graph cannot turn through the side of a span"),
				          Graph.FindRoute(Middle, Side, ECityRoadAccess::None, Route));
				double Height = 0;
				TestTrue(TEXT("Traffic/paving sampler knows the span"),
				         City.TrySampleStructureHeight(Middle.X + .5, Middle.Y + .5, Height));
				TestEqual(TEXT("Deck has 600 cm snapped terrain clearance"), Height, 600.);
				auto Overwrite = Preview(City, Middle, Middle, ECityRoadProfile::Ground);
				TestFalse(TEXT("Ground cannot overwrite span"), City.CommitRoad(Overwrite).IsSuccess());
				FCitySaveDocument D;
				D.Placement = City.ExportState();
				D.Terrain = Config;
				FString Json;
				TestTrue(TEXT("Schema36 persists spans"), FCityPersistence::Serialize(D, Json, Error));
				FCitySaveDocument Loaded;
				TestTrue(TEXT("Span save parses"), FCityPersistence::Deserialize(Json, Loaded, Error));
				TMap<FName, FCityBuildingDefinition> Definitions;
				auto Restored = FCityPlacementModel::CreateValidatedFromState(Loaded.Placement, Definitions, Error);
				TestTrue(TEXT("Span state restores"), Restored.IsValid());
				if (Restored)
				{
					Restored->SetTerrain(&Terrain);
					TestTrue(TEXT("Restored span geometry/cost verified"), Restored->ValidateRoadStructures(Error));
					TestTrue(TEXT("Span profile exactly round trips"), Restored->GetRoadStructures()[0] == Span);
				}
				auto Corrupt = Loaded.Placement;
				Corrupt.RoadCells.Remove(Middle);
				TestFalse(TEXT("Orphan ramps rejected on restore"),
				          FCityPlacementModel::CreateValidatedFromState(Corrupt, Definitions, Error).IsValid());
				Corrupt = Loaded.Placement;
				Corrupt.RoadStructures.Add(Span);
				TestFalse(TEXT("Overlapping spans rejected"),
				          FCityPlacementModel::CreateValidatedFromState(Corrupt, Definitions, Error).IsValid());
				if (Restored)
				{
					Restored->RemoveRoad(Middle);
					TestTrue(TEXT("Span demolition clears all ramps"), Restored->GetRoadStructures().IsEmpty());
					TestFalse(TEXT("Raised middle removed"), Restored->IsRoad(Middle));
					TestTrue(TEXT("Ordinary approaches retained"), Restored->IsRoad(A) && Restored->IsRoad(B));
				}
			}
	FCityPlacementModel City(100000);
	City.SetTerrain(&Terrain);
	auto Bridge = Preview(City, {58, 10}, {58, 62}, ECityRoadProfile::Bridge);
	auto Check = City.ValidateRoad(Bridge);
	TestTrue(TEXT("Water crossing with dry approaches validates"), Check.IsSuccess());
	if (!Check.IsSuccess())
		AddError(Check.Detail);
	auto Wrong = Bridge;
	Wrong.Profile = ECityRoadProfile::Elevated;
	TestFalse(TEXT("Elevated cannot cross water"), City.ValidateRoad(Wrong).IsSuccess());
	Wrong.Profile = ECityRoadProfile::Ground;
	TestFalse(TEXT("Ground cannot silently become a bridge"), City.ValidateRoad(Wrong).IsSuccess());
	Wrong = Preview(City, {1, 5}, {15, 5}, ECityRoadProfile::Bridge);
	TestFalse(TEXT("Bridge must cross actual water"), City.ValidateRoad(Wrong).IsSuccess());
	Wrong = Preview(City, {1, 5}, {5, 5}, ECityRoadProfile::Elevated);
	TestFalse(TEXT("Short spans cannot fit safe ramps"), City.ValidateRoad(Wrong).IsSuccess());
	Wrong = Preview(City, {1, 5}, {15, 8}, ECityRoadProfile::Elevated);
	TestFalse(TEXT("Bent spans require separate construction"), City.ValidateRoad(Wrong).IsSuccess());
	FCityPlacementModel Poor(1);
	Poor.SetTerrain(&Terrain);
	auto TooExpensive = Preview(Poor, {1, 5}, {15, 5}, ECityRoadProfile::Elevated);
	TestEqual(TEXT("Unaffordable span rejected"), Poor.CommitRoad(TooExpensive).Failure,
	          ECityRoadFailure::InsufficientFunds);
	TestTrue(TEXT("Rejected command leaves corridor empty"), Poor.GetRoadCells().IsEmpty());
	auto Elevated = Preview(City, {2, 8}, {16, 8}, ECityRoadProfile::Elevated);
	Elevated.ElevationStep = 1;
	TestTrue(TEXT("Elevated visual fixture builds"), City.CommitRoad(Elevated).IsSuccess());
	// Rebase the water crossing after the first independent command changed revision.
	Bridge = Preview(City, {58, 10}, {58, 62}, ECityRoadProfile::Bridge);
	Bridge.ElevationStep = 2;
	TestTrue(TEXT("Bridge visual fixture builds"), City.CommitRoad(Bridge).IsSuccess());
	auto Stale = Preview(City, {1, 3}, {15, 3}, ECityRoadProfile::Elevated);
	auto Intervening = Preview(City, {3, 3}, {3, 3}, ECityRoadProfile::Ground);
	City.CommitRoad(Intervening);
	TestEqual(TEXT("Stale span cannot spend funds"), City.CommitRoad(Stale).Failure, ECityRoadFailure::StalePreview);
	auto Blocked = Preview(City, {1, 3}, {15, 3}, ECityRoadProfile::Elevated);
	TestFalse(TEXT("Occupied road corridor rejected"), City.CommitRoad(Blocked).IsSuccess());
	FCityPlacementModel Occupied(100000);
	Occupied.SetTerrain(&Terrain);
	FCityBuildingDefinition Building;
	Building.DefinitionId = TEXT("Block");
	Building.DisplayName = TEXT("Block");
	Building.Footprint = FCityCellFootprint(1, 1);
	Building.Cost = 1;
	Building.LogicalHeightCm = 800;
	auto BuildingPreview = Occupied.BeginPreview(Building, {8, 5});
	TestTrue(TEXT("Support obstacle placed"), Occupied.CommitPlacement(BuildingPreview, Building).IsSuccess());
	auto Obstacle = Preview(Occupied, {1, 5}, {15, 5}, ECityRoadProfile::Elevated);
	TestFalse(TEXT("Support cannot claim building lot"), Occupied.CommitRoad(Obstacle).IsSuccess());
	auto State = City.ExportState();
	State.RoadStructures[0].DeckHeightCm += 10;
	TMap<FName, FCityBuildingDefinition> Definitions;
	auto Tampered = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	TestTrue(TEXT("Terrain-independent structural parse"), Tampered.IsValid());
	if (Tampered)
	{
		Tampered->SetTerrain(&Terrain);
		TestFalse(TEXT("Altered saved deck rejected against actual terrain"), Tampered->ValidateRoadStructures(Error));
	}
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityStructureGeometryTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->SimulationSubsystem = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem->Terrain = Terrain;
	Board->PlacementModel = &City;
	Board->RebuildRoadInstances();
	const auto* Paving = Board->RoadSurfaceProfile->GetProcMeshSection(2);
	TestNotNull(TEXT("Raised paving exists in shared component"), Paving);
	if (Paving)
		for (const auto& V : Paving->ProcVertexBuffer)
			TestTrue(TEXT("Raised pavement equals the traffic height profile"),
			         FMath::IsNearlyEqual(static_cast<double>(V.Position.Z),
			                              Board->GetRoadProfileHeight(V.Position.X / 800., V.Position.Y / 800., 1) + 59,
			                              .02));
	int32 Supports = 0;
	for (const auto& Cell : City.GetRoadCells())
		Supports += City.HasRoadSupport(Cell) ? 1 : 0;
	TestEqual(TEXT("Rendered support count uses authority spacing"), Board->RoadSupportInstances->GetInstanceCount(),
	          Supports);
	World->DestroyWorld(false);
	FCitySaveDocument Fixture;
	Fixture.Placement = City.ExportState();
	Fixture.Terrain = Config;
	Fixture.bSimulationPaused = true;
	FString Json;
	TestTrue(TEXT("Native fixture serializes"), FCityPersistence::Serialize(Fixture, Json, Error));
	TestTrue(TEXT("Native fixture written separately from user saves"),
	         FCityPersistence::WriteAtomic(
	             FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/Roads-StructureFixture.json")), Json,
	             Error));
	return true;
}
#endif
