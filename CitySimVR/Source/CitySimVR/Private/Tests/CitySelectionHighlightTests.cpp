#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CitySelectionHighlight.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"
#include "Presentation/CityBoardActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySelectionHighlightTest, "CitySim.Presentation.SelectionExtent",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySelectionHighlightTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Config;
	Config.BroadElevationAmplitudeCm = 0;
	Config.HillAmplitudeCm = 0;
	Config.RidgeAmplitudeCm = 0;
	Config.StarterHalfWidthCells = 16;
	Config.StarterDepthCells = 24;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(Config);
	for (bool Reverse : {false, true})
	{
		FCityPlacementModel City(100000);
		City.SetTerrain(&Terrain);
		auto Add = [&](FCityCellCoord A, FCityCellCoord B, ECityRoadProfile Profile)
		{
			auto P = City.BeginRoadPreview(A, FCityRoadDefinition::Local().Id, 10);
			P.End = B;
			P.Profile = Profile;
			TestTrue(TEXT("Selection fixture road commits"), City.CommitRoad(P).IsSuccess());
		};
		Add(Reverse ? FCityCellCoord(16, 8) : FCityCellCoord(2, 8),
		    Reverse ? FCityCellCoord(2, 8) : FCityCellCoord(16, 8), ECityRoadProfile::Elevated);
		Add({8, 2}, {8, 16}, ECityRoadProfile::Elevated);
		const auto Inspect = CitySelectionHighlight::Build(City, {8, 8, 1}, false);
		TestTrue(TEXT("Inspect selects one tile on the actual deck"),
		         Inspect.Cells.Num() == 1 && Inspect.Cells[0] == FCityCellCoord(8, 8, 1));
		for (const auto Click : {FCityCellCoord(6, 8, 1), FCityCellCoord(8, 8, 1), FCityCellCoord(2, 8)})
		{
			FString Error;
			TMap<FName, FCityBuildingDefinition> Definitions;
			auto Copy = FCityPlacementModel::CreateValidatedFromState(City.ExportState(), Definitions, Error);
			if (!TestTrue(TEXT("Fixture restores"), Copy.IsValid()))
				continue;
			Copy->SetTerrain(&Terrain);
			const auto Plan = CitySelectionHighlight::Build(*Copy, Click, true);
			const auto Before = Copy->ExportState().RoadCells;
			TestTrue(TEXT("Demolition succeeds"), Copy->RemoveRoad(Click));
			int32 Removed = 0;
			for (const auto& C : Before)
				if (!Copy->IsRoad(C))
				{
					++Removed;
					TestTrue(TEXT("Every removed address was highlighted"), Plan.Cells.Contains(C));
				}
			TestEqual(TEXT("Highlight contains exactly the command removal extent"), Plan.Cells.Num(), Removed);
			for (const auto& C : Plan.RetainedCells)
				TestTrue(TEXT("Retained approach or shared junction survives"), Copy->IsRoad(C));
			TestTrue(TEXT("Ground approaches have a retained marker"),
			         Plan.RetainedCells.Contains(FCityCellCoord(2, 8)) ||
			             Plan.RetainedCells.Contains(FCityCellCoord(8, 2)));
		}
		TestTrue(TEXT("Empty inspect remains a single tile"),
		         CitySelectionHighlight::Build(City, {24, 24}, false).Cells.Num() == 1);
		TestTrue(TEXT("Empty demolition has no removal outline"),
		         CitySelectionHighlight::Build(City, {24, 24}, true).Cells.IsEmpty());
	}
	FCityPlacementModel Buildings(100000);
	FCityBuildingDefinition Def;
	Def.DefinitionId = TEXT("SelectionFixture");
	Def.DisplayName = TEXT("Selection fixture");
	Def.Footprint = FCityCellFootprint(2, 3);
	Def.Cost = 10;
	Def.LogicalHeightCm = 800;
	auto Preview = Buildings.BeginPreview(Def, {20, 20}, 1);
	const auto Result = Buildings.CommitPlacement(Preview, Def);
	TestTrue(TEXT("Rotated building fixture commits"), Result.IsSuccess());
	const auto Plan = CitySelectionHighlight::Build(Buildings, {21, 21}, false);
	TestEqual(TEXT("Rotated selection includes the full six-cell footprint"), Plan.Cells.Num(), 6);
	TestTrue(TEXT("Rotated footprint uses authoritative dimensions"),
	         Plan.Cells.Contains(FCityCellCoord(22, 21)) && !Plan.Cells.Contains(FCityCellCoord(20, 22)));
	TestEqual(TEXT("Empty hover remains a tile instead of the previously inspected building"),
	          CitySelectionHighlight::Build(Buildings, {30, 30}, false).Cells.Num(), 1);
	TestTrue(TEXT("Empty demolition remains empty"),
	         CitySelectionHighlight::Build(Buildings, {30, 30}, true).Cells.IsEmpty());
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CitySelectionGeometry"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->PlacementModel = &Buildings;
	Board->SetToolMode(ECityToolMode::Demolish);
	Board->SetHoveredCell({21, 21}, true);
	TestTrue(TEXT("Actual presentation produces pooled outline geometry"),
	         Board->SelectionOutlineInstances->GetInstanceCount() > 0);
	TestTrue(TEXT("Outline does not block picking"),
	         Board->SelectionOutlineInstances->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	const int32 Count = Board->SelectionOutlineInstances->GetInstanceCount();
	Board->SetHoveredCell({21, 21}, true);
	TestEqual(TEXT("Repeated hover preserves bounded geometry"), Board->SelectionOutlineInstances->GetInstanceCount(),
	          Count);
	Board->SetHoveredCell({21, 21}, false);
	TestEqual(TEXT("Lost pointer clears selection"), Board->SelectionOutlineInstances->GetInstanceCount(), 0);
	Board->SetHoveredCell({21, 21}, true);
	Board->SetToolMode(ECityToolMode::Inspect);
	TestEqual(TEXT("Tool switch clears stale demolition geometry"),
	          Board->SelectionOutlineInstances->GetInstanceCount(), 0);
	Board->RenderPlacement(Result.EntityId, Buildings.GetEntities().FindChecked(Result.EntityId), Def);
	auto Front = Def;
	Front.DefinitionId = TEXT("SelectionForeground");
	Front.Footprint = {1, 1};
	Front.LogicalHeightCm = 3200;
	auto FrontPreview = Buildings.BeginPreview(Front, {18, 20});
	const auto FrontResult = Buildings.CommitPlacement(FrontPreview, Front);
	TestTrue(TEXT("Tall foreground fixture commits"), FrontResult.IsSuccess());
	Board->RenderPlacement(FrontResult.EntityId, Buildings.GetEntities().FindChecked(FrontResult.EntityId), Front);
	const FVector Origin = Board->GetActorTransform().TransformPosition(FVector(17.5 * 800, 20.5 * 800, 3200));
	const FVector Direction = Board->GetActorTransform().TransformVector(FVector(1, 0, -1)).GetSafeNormal();
	FBox FrontBounds;
	FTransform FrontTransform;
	FVector MeshHit, Normal;
	float HitTime = 0;
	TestTrue(TEXT("Foreground rendered bounds resolve"),
	         Board->GetInspectionMesh(FrontResult.EntityId, FrontBounds, FrontTransform));
	TestTrue(TEXT("Oblique ray physically intersects the foreground mesh"),
	         FMath::LineExtentBoxIntersection(FrontBounds, FrontTransform.InverseTransformPosition(Origin),
	                                          FrontTransform.InverseTransformPosition(Origin + Direction * 1000),
	                                          FVector::ZeroVector, MeshHit, Normal, HitTime));
	FCityCellCoord Picked;
	uint64 Selected = 0;
	FVector Leader;
	TestTrue(TEXT("Oblique ray projects to the rear building tile"),
	         Board->PickCellFromWorldRay(Origin, Direction, Picked) && Picked == FCityCellCoord(21, 20));
	TestTrue(TEXT("Inspection resolves the highlighted tile despite foreground mesh"),
	         Board->PickInspectionBuilding(Origin, Direction, Selected, Leader));
	TestEqual(TEXT("Inspect and hover agree on the rear building identity"), Selected,
	          CitySelectionHighlight::Build(Buildings, Picked, false).EntityId);
	TestEqual(TEXT("Rear building is the trigger target"), Selected, Result.EntityId);
	Board->SetInspectionEntity(FrontResult.EntityId);
	Board->SetHoveredCell(Picked, true);
	TestTrue(TEXT("Pinned tooltip does not redirect hover outline"),
	         Board->SelectionOutlineInstances->GetInstanceCount() > 0);
	for (int32 I = 0; I < Board->SelectionOutlineInstances->GetInstanceCount(); ++I)
	{
		FTransform Outline;
		Board->SelectionOutlineInstances->GetInstanceTransform(I, Outline);
		TestTrue(TEXT("Pinned tooltip leaves every outline on the rear footprint"),
		         Outline.GetLocation().X >= 20 * 800);
	}
	const FVector EmptyOrigin = Board->GetActorTransform().TransformPosition(FVector(17.5 * 800, 20.5 * 800, 1600));
	TestFalse(TEXT("A foreground mesh over an empty target tile cannot be inspected"),
	          Board->PickInspectionBuilding(EmptyOrigin, Direction, Selected, Leader));
	TestEqual(TEXT("Empty tile clears the building target"), Selected, FCityGrid::EmptyEntityId);
	TestFalse(TEXT("Zero-length inspection ray has no target"),
	          Board->PickInspectionBuilding(Origin, FVector::ZeroVector, Selected, Leader));
	World->DestroyWorld(false);
	return true;
}
#endif
