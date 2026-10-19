#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySeasonalDecorationPresentationTest,
                                 "CitySim.Presentation.SeasonalDecorationPoolsGroundingAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySeasonalDecorationPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SeasonalDecorationPresentationTest"), nullptr,
	                                  true, ERHIFeatureLevel::Num, &Init);
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->SimulationSubsystem = Simulation;
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/SeasonalDecorationPresentation.json");
	FString Error, Json;
	do
	{
		if (!TestNotNull(TEXT("Existing authored catalog loads"), Board->BuildingCatalog.Get()))
			break;
		Simulation->RegisterBuildingCatalog(Board->BuildingCatalog);
		FCitySaveDocument Document;
		Document.bSimulationPaused = true;
		Document.Terrain.BroadElevationAmplitudeCm = Document.Terrain.HillAmplitudeCm =
		    Document.Terrain.RidgeAmplitudeCm = 0;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(Document.Terrain);
		FCityPlacementModel City(100000);
		City.SetTerrain(&Terrain);
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, {40, 8});
		if (!TestTrue(TEXT("Operational road fixture commits"), City.CommitRoad(Road).IsSuccess()))
			break;
		const auto* Park = Board->BuildingCatalog->FindDefinition(TEXT("Park"));
		if (!TestNotNull(TEXT("Pocket park exists"), Park))
			break;
		auto Preview = City.BeginPreview(*Park, {8, 9});
		const auto Result = City.CommitPlacement(Preview, *Park);
		if (!TestTrue(TEXT("Pocket park commits"), Result.IsSuccess()))
			break;
		Document.Placement = City.ExportState();
		if (!TestTrue(TEXT("Isolated fixture writes and loads"), FCityPersistence::Serialize(Document, Json, Error) &&
		                                                             FCityPersistence::WriteAtomic(Path, Json, Error) &&
		                                                             Simulation->LoadCityFromPath(Path, Error, false)))
		{
			AddError(Error);
			break;
		}
		Board->PrepareCityView();
		Board->UpdateSeasonalDecorations();
		TestEqual(TEXT("Only eight shared non-ticking prop pools"), Board->SeasonalDecorationPools.Num(), 8);
		for (const auto& Pool : Board->SeasonalDecorationPools)
		{
			TestNotNull(TEXT("Every reviewed prop mesh is imported"), Pool->GetStaticMesh().Get());
			TestEqual(TEXT("Props cannot steal inspection collisions"), Pool->GetCollisionEnabled(),
			          ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Props have no per-component tick"), Pool->PrimaryComponentTick.bCanEverTick);
		}
		if (!TestEqual(TEXT("Operating spring park has two flower accents"), Board->SeasonalDecorationSamples.Num(), 2))
			break;
		const auto Frozen = Board->SeasonalDecorationSamples;
		for (int32 I = 0; I < Frozen.Num(); ++I)
		{
			FTransform Transform;
			Board->SeasonalDecorationPools[0]->GetInstanceTransform(I, Transform, false);
			const auto& Entity = Simulation->GetPlacementModel().GetEntities().FindChecked(Frozen[I].EntityId);
			const double Base = Board->GetBuildingBaseHeight(Entity.Anchor, *Park, Entity.QuarterTurnsClockwise) + 40 +
			                    Frozen[I].SurfaceOffsetCm;
			TestTrue(TEXT("Ground pivot sits just above the actual authored lawn"),
			         FMath::IsNearlyEqual(Transform.GetLocation().Z, Base, .001));
		}
		const auto ParkInstances = Board->BuildingInstances.FindChecked(TEXT("Park"));
		FTransform OriginalParkPose;
		ParkInstances->GetInstanceTransform(0, OriginalParkPose, false);
		Board->UpdateSeasonalBuildingMeshes(12);
		TestTrue(TEXT("December replaces the central fountain with a reviewed tree pedestal"),
		         ParkInstances->GetStaticMesh() == Board->PocketParkDecemberMesh);
		FTransform DecemberParkPose;
		ParkInstances->GetInstanceTransform(0, DecemberParkPose, false);
		TestTrue(TEXT("Seasonal park mesh swap preserves all instance transforms"),
		         OriginalParkPose.Equals(DecemberParkPose));
		Board->UpdateSeasonalBuildingMeshes(2);
		TestTrue(TEXT("Spring restores the original park fountain"),
		         ParkInstances->GetStaticMesh() == Board->PocketParkMesh);
		const auto* Home = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
		TestNotNull(TEXT("Reviewed smaller house display asset loads"), Board->SeasonalResidentialMesh.Get());
		TestTrue(TEXT("Smaller display house preserves authoritative16m lot"),
		         Home && Home->Footprint.Width == 2 && Home->Footprint.Height == 2);
		FString Before, After;
		FCityPersistence::Serialize(Simulation->BuildSaveDocument(), Before, Error);
		const int32 Rebuilds = Board->SeasonalDecorationRebuildCount;
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			Board->SetHoveredCell({Frame, 20}, true);
			Board->UpdateSeasonalDecorations();
		}
		TestEqual(TEXT("Hover and rendered frames cannot upload unchanged props"),
		          Board->SeasonalDecorationRebuildCount, Rebuilds);
		Board->SetHeatMapLayers(true);
		Board->UpdateSeasonalDecorations();
		TestFalse(TEXT("Analytical overlays suppress decorations across updates"),
		          Board->SeasonalDecorationPools[0]->IsVisible());
		Board->SetHeatMapLayers(false);
		Board->UpdateSeasonalDecorations();
		TestTrue(TEXT("Normal view restores the same decorations"),
		         Board->SeasonalDecorationPools[0]->IsVisible() && Board->SeasonalDecorationSamples == Frozen);
		Simulation->SetEntryMenuSuspended(true);
		Board->UpdateSeasonalDecorations();
		TestTrue(TEXT("Menu clears every sample and instance"),
		         Board->SeasonalDecorationSamples.IsEmpty() &&
		             Board->SeasonalDecorationPools[0]->GetInstanceCount() == 0);
		Simulation->SetEntryMenuSuspended(false);
		Board->UpdateSeasonalDecorations();
		TestTrue(TEXT("Resume reproduces exact placement"), Board->SeasonalDecorationSamples == Frozen);
		FCityPersistence::Serialize(Simulation->BuildSaveDocument(), After, Error);
		TestEqual(TEXT("Cosmetics do not alter saved authority"), After, Before);
		if (!TestTrue(TEXT("Real load replaces the city"), Simulation->LoadCityFromPath(Path, Error, false)))
			break;
		Board->Tick(0);
		TestTrue(TEXT("First load frame rebinds and restores deterministic props"),
		         Board->PlacementModel == &Simulation->GetPlacementModel() &&
		             Board->SeasonalDecorationSamples == Frozen);
		TestTrue(TEXT("Park demolition succeeds"),
		         Simulation->GetPlacementModel().DemolishEntity(Result.EntityId).IsSuccess());
		Simulation->Tick(0);
		Board->UpdateSeasonalDecorations();
		TestTrue(TEXT("Demolition clears all decorations immediately"),
		         Board->SeasonalDecorationSamples.IsEmpty() &&
		             Board->SeasonalDecorationPools[0]->GetInstanceCount() == 0);
	} while (false);
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	World->DestroyWorld(false);
	return true;
}
#endif
