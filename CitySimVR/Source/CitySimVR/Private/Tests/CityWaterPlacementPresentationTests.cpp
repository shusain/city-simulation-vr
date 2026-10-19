#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionVertexColor.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterPlacementPresentationTest,
                                 "CitySim.Water.PlacementOverlayVisibilityCachingAndCleanup",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterPlacementPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WaterPlacementPresentationTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Init);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Simulation;
	Board->PlacementModel = &Simulation->GetPlacementModel();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Native catalog supplies the real water tools"), Board->BuildingCatalog.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}
	Board->RoadInstances->SetVisibility(false);
	Board->RoadSurfaceProfile->SetVisibility(true);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Pollution);
	Board->UpdateServiceCoverage({1, 1}, true);
	TestTrue(TEXT("Fixture begins with normal diagnostic fading"), Board->bBuildingsFaded);
	Board->SetSelectedFacility(TEXT("Service"));
	Board->UpdateWaterPlacementOverlay();
	TestTrue(TEXT("Selecting treatment automatically enables siting overlay"),
	         Board->WaterPlacementSurface->IsVisible());
	TestFalse(TEXT("Existing buildings keep their original materials"), Board->bBuildingsFaded);
	TestTrue(TEXT("Roads remain visible during siting"), Board->RoadSurfaceProfile->IsVisible());
	TestTrue(TEXT("Sidewalks remain visible during siting"), Board->SidewalkSurface->IsVisible());
	TestTrue(TEXT("Terrain and surface water remain visible"),
	         Board->BoardSurface->IsVisible() && Board->WaterSurface->IsVisible());
	TestFalse(TEXT("Previously hidden layer is not forced visible"), Board->RoadInstances->IsVisible());
	TestEqual(TEXT("Opaque analytical mesh is cleared"), Board->HeatMapSurface->GetNumSections(), 0);
	TestTrue(TEXT("Legend distinguishes site yield from existing pressure"),
	         Board->DescribeHeatMap().Contains(TEXT("groundwater yield")) &&
	             Board->DescribeHeatMap().Contains(TEXT("pipe pressure")));
	const auto* Ground = Board->WaterPlacementSurface->GetProcMeshSection(0);
	if (TestNotNull(TEXT("Translucent terrain film generated"), Ground))
	{
		TestEqual(TEXT("Terrain film reuses bounded terrain sampling"), Ground->ProcVertexBuffer.Num(),
		          FCityTerrainHeightfield::SampleCount);
		TestTrue(TEXT("Vertex opacity leaves terrain visible"),
		         Ground->ProcVertexBuffer[0].Color.A > 0 && Ground->ProcVertexBuffer[0].Color.A < 128);
	}
	const int32 Rebuilds = Board->WaterPlacementRebuildCount;
	Board->SetHoveredCell({12, 12}, true);
	Board->UpdateWaterPlacementOverlay();
	Board->SetHoveredCell({14, 12}, true);
	Board->UpdateWaterPlacementOverlay();
	TestEqual(TEXT("Pointer movement does not upload another mesh"), Board->WaterPlacementRebuildCount, Rebuilds);
	Simulation->SetEntryMenuSuspended(true);
	Board->UpdateWaterPlacementOverlay();
	TestTrue(TEXT("Menu clears placement geometry and legend"),
	         !Board->WaterPlacementSurface->IsVisible() && Board->WaterPlacementSurface->GetNumSections() == 0 &&
	             Board->DescribeHeatMap().IsEmpty());
	Simulation->SetEntryMenuSuspended(false);
	Board->SetSelectedFacility(TEXT("WaterTower"));
	Board->UpdateWaterPlacementOverlay();
	TestTrue(TEXT("Tower also enables siting overlay on return"), Board->WaterPlacementSurface->IsVisible());
	Board->SetCommandInputBound(true);
	auto* Controller = World->SpawnActor<APlayerController>();
	Board->UpdatePlacementPreview(Controller, {10, 10}, true, false, true, false);
	TestFalse(TEXT("Cancel clears overlay immediately"), Board->WaterPlacementSurface->IsVisible());
	Board->UpdatePlacementPreview(Controller, {10, 10}, true, false, false, false);
	TestFalse(TEXT("Canceled cell stays clear"), Board->WaterPlacementSurface->IsVisible());
	Board->UpdatePlacementPreview(Controller, {11, 10}, true, false, false, false);
	TestTrue(TEXT("Moving to another cell resumes water guidance"), Board->WaterPlacementSurface->IsVisible());
	Board->SetToolMode(ECityToolMode::Inspect);
	TestTrue(TEXT("Leaving water tool clears geometry"),
	         !Board->WaterPlacementSurface->IsVisible() && Board->WaterPlacementSurface->GetNumSections() == 0);
#if WITH_EDITOR
	auto* Material = LoadObject<UMaterial>(
	    nullptr, TEXT("/Game/CitySim/UI/M_CityWaterPlacementOverlay.M_CityWaterPlacementOverlay"));
	if (TestNotNull(TEXT("Overlay material exists"), Material))
	{
		TestEqual(TEXT("Overlay is translucent rather than opaque"), Material->GetBlendMode(), BLEND_Translucent);
		TestFalse(TEXT("Overlay respects buildings and roads in depth"), Material->bDisableDepthTest);
		TestTrue(TEXT("Vertex alpha is bound to actual material opacity"),
		         Cast<UMaterialExpressionVertexColor>(Material->GetEditorOnlyData()->Opacity.Expression) != nullptr);
	}
#endif
	// Real loads replace the unique placement model. Tick must rebind before
	// construction animation reads any restored development states.
	const auto* Home = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
	auto HomePreview = Simulation->GetPlacementModel().BeginPreview(*Home, {4, 12});
	TestTrue(TEXT("Load fixture has a building"),
	         Simulation->GetPlacementModel().CommitPlacement(HomePreview, *Home).IsSuccess());
	Simulation->RegisterBuildingCatalog(Board->BuildingCatalog);
	const FString LoadPath = FPaths::ProjectSavedDir() / TEXT("Automation/WaterPlacementCityLoad.json");
	FString Error;
	if (TestTrue(TEXT("Isolated city save succeeds"), Simulation->SaveCityToPath(LoadPath, Error)))
	{
		for (int32 I = 0; I < 3; ++I)
		{
			Board->SetSelectedFacility(TEXT("Service"));
			Board->UpdateWaterPlacementOverlay();
			if (!TestTrue(TEXT("City replacement succeeds"), Simulation->LoadCityFromPath(LoadPath, Error, false)))
				break;
			TestTrue(TEXT("Load really replaces the old model"),
			         Board->PlacementModel != &Simulation->GetPlacementModel());
			Board->Tick(0.f);
			TestTrue(TEXT("First frame safely binds the new model"),
			         Board->PlacementModel == &Simulation->GetPlacementModel());
			// This isolated world has no local-player registration; supply its
			// controller through the same placement input path tested above.
			Board->UpdatePlacementPreview(Controller, {}, false, false, false, false);
			TestTrue(TEXT("Selected water overlay rebuilds for the new city"),
			         Board->WaterPlacementSurface->IsVisible());
		}
	}
	IFileManager::Get().Delete(*LoadPath);
	IFileManager::Get().Delete(*(LoadPath + TEXT(".bak")));
	World->DestroyWorld(false);
	return true;
}
#endif
