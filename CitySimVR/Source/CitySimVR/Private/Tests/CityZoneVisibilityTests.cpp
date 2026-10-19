#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityZoneVisibilityTest, "CitySim.Presentation.ContextualZoneVisibility",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityZoneVisibilityTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ContextualZoneVisibility"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	Board->SimulationSubsystem = Sim;
	Board->PlacementModel = &Sim->GetPlacementModel();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Existing catalog available"), Board->BuildingCatalog.Get()))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Board->BuildingCatalog);
	const auto CheckVisibility = [&](const bool Expected)
	{
		for (const auto* Layer : {Board->ResidentialZoneInstances.Get(), Board->CommercialZoneInstances.Get(),
		                          Board->IndustrialZoneInstances.Get()})
			TestEqual(TEXT("Every stored-zone layer follows the active tool and heat map"), Layer->IsVisible(),
			          Expected);
	};
	CheckVisibility(false);
	auto& City = Sim->GetPlacementModel();
	auto Road = City.BeginRoadPreview(City.GetEntranceCell());
	City.UpdateRoadPreviewEnd(Road, {20, 8});
	TestTrue(TEXT("Actual blocked-road preview fixture"), City.CommitRoad(Road).IsSuccess());
	TestTrue(TEXT("Residential zone fixture"), City.SetZone({4, 10}, ECityZoneType::Residential));
	TestTrue(TEXT("Commercial zone fixture"), City.SetZone({8, 10}, ECityZoneType::Commercial));
	TestTrue(TEXT("Industrial zone fixture"), City.SetZone({12, 10}, ECityZoneType::Industrial));
	Board->RebuildZoneInstances();
	CheckVisibility(false);
	for (const auto Zone : {ECityZoneType::Residential, ECityZoneType::Commercial, ECityZoneType::Industrial})
	{
		Board->SetSelectedZone(Zone);
		CheckVisibility(true);
		for (const auto* Layer : {Board->ResidentialZoneInstances.Get(), Board->CommercialZoneInstances.Get(),
		                          Board->IndustrialZoneInstances.Get()})
			TestEqual(TEXT("Every zone type remains populated regardless of paint selection"),
			          Layer->GetInstanceCount(), 1);
	}
	const int64 Funds = City.GetFunds();
	Board->UpdateZoneTool({16, 10}, true, false, false, false);
	TestEqual(TEXT("Active paint preview still appears"), Board->ZonePreviewInstances->GetInstanceCount(), 1);
	Board->UpdateZoneTool({16, 10}, true, true, false, false);
	Board->UpdateZoneTool({17, 10}, true, true, false, false);
	TestEqual(TEXT("Real two-corner paint commits both cells"), City.GetZone({17, 10}), ECityZoneType::Industrial);
	TestEqual(TEXT("Newly painted zones join stored geometry"), Board->IndustrialZoneInstances->GetInstanceCount(), 3);
	Board->UpdateZoneTool(City.GetEntranceCell(), true, false, false, false);
	TestEqual(TEXT("Blocked road cell retains skipped preview"), Board->ZoneSkippedPreviewInstances->GetInstanceCount(),
	          1);
	for (const auto Mode : {ECityToolMode::Inspect, ECityToolMode::Road, ECityToolMode::Home, ECityToolMode::Demolish})
	{
		Board->SetToolMode(Mode);
		CheckVisibility(false);
		TestEqual(TEXT("Leaving zoning clears paint preview"), Board->ZonePreviewInstances->GetInstanceCount(), 0);
		TestEqual(TEXT("Leaving zoning clears skipped preview"), Board->ZoneSkippedPreviewInstances->GetInstanceCount(),
		          0);
		Board->RebuildZoneInstances();
		CheckVisibility(false);
		Board->SetSelectedZone(ECityZoneType::Residential);
		CheckVisibility(true);
	}
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Pollution);
	Board->UpdateServiceCoverage({4, 10}, true);
	CheckVisibility(false);
	Board->RebuildZoneInstances();
	CheckVisibility(false);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
	Board->UpdateServiceCoverage({4, 10}, true);
	CheckVisibility(true);
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Pollution);
	Board->UpdateServiceCoverage({4, 10}, true);
	Board->SetToolMode(ECityToolMode::Inspect);
	Board->UpdateServiceCoverage({4, 10}, true);
	Board->RebuildZoneInstances();
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
	Board->UpdateServiceCoverage({4, 10}, true);
	CheckVisibility(false);
	TestEqual(TEXT("Tool and overlay changes preserve funds"), City.GetFunds(), Funds);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/ZoneVisibility.json");
	FString Error;
	if (TestTrue(TEXT("Current zoned city saves"), Sim->SaveCityToPath(Path, Error)) &&
	    TestTrue(TEXT("Current zoned city loads"), Sim->LoadCityFromPath(Path, Error, false)))
	{
		Board->PrepareCityView();
		CheckVisibility(false);
		Board->SetSelectedZone(ECityZoneType::Commercial);
		CheckVisibility(true);
		TestEqual(TEXT("Reload restores residential zoning"), Board->PlacementModel->GetZone({4, 10}),
		          ECityZoneType::Residential);
		TestEqual(TEXT("Reload restores commercial zoning"), Board->PlacementModel->GetZone({8, 10}),
		          ECityZoneType::Commercial);
		TestEqual(TEXT("Reload restores painted industrial zoning"), Board->IndustrialZoneInstances->GetInstanceCount(),
		          3);
		TestTrue(TEXT("Reload while zoning succeeds"), Sim->LoadCityFromPath(Path, Error, false));
		Board->PrepareCityView();
		CheckVisibility(true);
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_ZONEVISIBILITY_ZONE_VISIBILITY_CHECK_COMPLETE"));
	World->DestroyWorld(false);
	return true;
}

// Uses only the CI-owned packaged profile and the actual game viewport.
class FCityZoneVisibilityCaptureCommand : public IAutomationLatentCommand
{
public:
	explicit FCityZoneVisibilityCaptureCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (World)
			for (TActorIterator<ACityVRToolController> It(World); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->Board || !UI->DesktopHud)
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Zoning capture HUD did not initialize"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Board = UI->Board.Get();
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/ZoneVisibility-Zones");
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = Directory / TEXT("Fixture.json");
		FString Error;
		if (Phase == 0)
		{
			Sim->ResetCity(false);
			UI->ReturnToCity();
			Sim->SetEntryMenuSuspended(false);
			Sim->SetSimulationPaused(true);
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->SetPaletteOpen(false);
			Board->PrepareCityView();
			auto& City = Sim->GetPlacementModel();
			auto Road = City.BeginRoadPreview(City.GetEntranceCell());
			City.UpdateRoadPreviewEnd(Road, {20, 8});
			if (!City.CommitRoad(Road).IsSuccess())
			{
				Test->AddError(TEXT("Zoning capture road fixture failed"));
				return true;
			}
			Test->TestTrue(TEXT("Visible residential patch"),
			               City.SetZoneArea({4, 10}, {3, 3}, ECityZoneType::Residential));
			Test->TestTrue(TEXT("Visible commercial patch"),
			               City.SetZoneArea({8, 10}, {3, 3}, ECityZoneType::Commercial));
			Test->TestTrue(TEXT("Visible industrial patch"),
			               City.SetZoneArea({12, 10}, {3, 3}, ECityZoneType::Industrial));
			Board->RebuildZoneInstances();
			Board->SetLightingPreset(ECityLightingPreset::Day);
			Board->SetToolMode(ECityToolMode::Inspect);
			const auto Pose = Board->GetActorTransform();
			const FVector Target = Pose.TransformPosition(FVector(9.5 * 800, 11.5 * 800, 0));
			const FVector Location = Target + Pose.TransformVector(FVector(0, -9000, 10500));
			auto* Player = World->GetFirstPlayerController();
			// The desktop pawn otherwise reapplies its overview camera each tick.
			// This is the isolated capture process; hold the same view through reload.
			Player->GetPawn()->SetActorTickEnabled(false);
			Player->GetPawn()->SetActorLocation(Location);
			Player->SetControlRotation((Target - Location).Rotation());
			if (!Sim->SaveCityToPath(Path, Error))
			{
				Test->AddError(Error);
				return true;
			}
		}
		else if (Phase == 2 || Phase == 4 || Phase == 6)
		{
			Board->SetSelectedZone(Phase == 2   ? ECityZoneType::Residential
			                       : Phase == 4 ? ECityZoneType::Commercial
			                                    : ECityZoneType::Industrial);
		}
		else if (Phase == 8)
			Board->SetToolMode(ECityToolMode::Inspect);
		else if (Phase == 10)
		{
			if (!Sim->LoadCityFromPath(Path, Error, false))
			{
				Test->AddError(Error);
				return true;
			}
			Board->PrepareCityView();
			Board->SetSelectedZone(ECityZoneType::Residential);
		}
		else if (Phase < 12)
		{
			const bool Expected = Phase != 1 && Phase != 9;
			for (const auto* Layer : {Board->ResidentialZoneInstances.Get(), Board->CommercialZoneInstances.Get(),
			                          Board->IndustrialZoneInstances.Get()})
			{
				Test->TestEqual(TEXT("Actual viewport zone visibility"), Layer->IsVisible(), Expected);
				Test->TestEqual(TEXT("Stored patch survives tools and reload"), Layer->GetInstanceCount(), 9);
			}
			const TCHAR* Name = Phase == 1   ? TEXT("InspectHidden")
			                    : Phase == 3 ? TEXT("ResidentialAllZones")
			                    : Phase == 5 ? TEXT("CommercialAllZones")
			                    : Phase == 7 ? TEXT("IndustrialAllZones")
			                    : Phase == 9 ? TEXT("InspectHiddenAgain")
			                                 : TEXT("ReloadZoning");
			FScreenshotRequest::RequestScreenshot(Directory / (FString(Name) + TEXT(".png")), true, false);
		}
		else
		{
			World->GetFirstPlayerController()->GetPawn()->SetActorTickEnabled(true);
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_ZONEVISIBILITY_ZONE_CAPTURE_COMPLETE Captures=6 PhysicalInputPending=1"));
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + 1;
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0;
	double Next = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityZoneVisibilityCaptureTest, "CitySim.Desktop.ContextualZoneVisibility",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityZoneVisibilityCaptureTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Client-")))
	{
		AddError(TEXT("Zone captures require the isolated CI packaged client profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityZoneVisibilityCaptureCommand(this));
	return true;
}
#endif
