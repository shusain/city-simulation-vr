#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityTutorialSettings.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UObject/UnrealType.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTutorialLifecycleTest, "CitySim.Desktop.TutorialAttentionLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityTutorialLifecycleTest::RunTest(const FString& Parameters)
{
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TutorialLifecycle"), nullptr, true,
        ERHIFeatureLevel::Num, &Values);
    auto* UI = World->SpawnActor<ACityVRToolController>();
    auto* Board = World->SpawnActor<ACityBoardActor>();
    UI->Board = Board;
    auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr,
        TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    FindFProperty<FObjectProperty>(ACityBoardActor::StaticClass(), TEXT("BuildingCatalog"))
        ->SetObjectPropertyValue_InContainer(Board, Catalog);
    Board->DispatchBeginPlay();
    auto* CameraOwner = World->SpawnActor<AActor>();
    auto* Camera = NewObject<UCameraComponent>(CameraOwner);
    CameraOwner->SetRootComponent(Camera); Camera->RegisterComponent(); UI->HeadsetCamera = Camera;
    auto* Settings = GetMutableDefault<UCityTutorialSettings>();
    const bool Original = Settings->bEnabled;
    Settings->bEnabled = true;
    auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
    Sim->StartScenario(); Sim->SetSimulationPaused(true);
    Sim->GetPlacementModel().SetTerrain(nullptr);
    UI->bDesktopUi = true;
    UI->SetPaletteOpen(true); UI->SetPalettePage(ECityPalettePage::Build);
    TestTrue(TEXT("Entrance step points at the actual Roads control"), UI->IsTutorialTarget(ECityPaletteAction::OpenRoads));
    TestFalse(TEXT("Entrance step does not highlight unrelated construction"), UI->IsTutorialTarget(ECityPaletteAction::SelectPower));
    const auto Build = UI->GetUiSnapshot();
    TestTrue(TEXT("Roads keeps its ordinary label while highlighted"), Build.MenuEntries.ContainsByPredicate([](const FCityUiActionView& V) {
        return V.bTutorialAttention && V.ActionId == static_cast<int32>(ECityPaletteAction::OpenRoads) &&
            V.Label.ToString().Equals(TEXT("ROADS"), ESearchCase::IgnoreCase); }));
    TestFalse(TEXT("Tutorial does not prepend instructions to button labels"), Build.MenuEntries.ContainsByPredicate([](const FCityUiActionView& V) {
        return V.Label.ToString().StartsWith(TEXT("Next:")); }));
    UI->SetPalettePage(ECityPalettePage::Roads);
    TestTrue(TEXT("Nested menu advances the target to Local"), UI->IsTutorialTarget(ECityPaletteAction::SelectLocalRoad));
    Board->UpdateEntranceGuidance();
    TestTrue(TEXT("New entrance marker is present"), Board->EntranceMarker->IsVisible());
    auto Road = Sim->GetPlacementModel().BeginRoadPreview(Sim->GetPlacementModel().GetEntranceCell());
    Sim->GetPlacementModel().UpdateRoadPreviewEnd(Road, {5, 8});
    TestTrue(TEXT("Entrance connection commits"), Sim->GetPlacementModel().CommitRoad(Road).IsSuccess());
    Sim->Tick(0); Board->UpdateEntranceGuidance();
    TestFalse(TEXT("Connection retires marker"), Board->EntranceMarker->IsVisible());
    const FString SavePath = FPaths::ProjectSavedDir() / TEXT("Automation/Tutorial/ConnectedCity.json");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath), true);
    FString Error;
    TestTrue(TEXT("Connected tutorial city saves"), Sim->SaveCityToPath(SavePath, Error));
    TestTrue(TEXT("Connected tutorial city loads"), Sim->LoadCityFromPath(SavePath, Error, false));
    Board->PrepareCityView(); Board->UpdateEntranceGuidance();
    TestFalse(TEXT("Already-connected load does not replay the initial marker"), Board->EntranceMarker->IsVisible());
    IFileManager::Get().Delete(*SavePath);
    IFileManager::Get().Delete(*(SavePath + TEXT(".bak")));
    UI->SetPalettePage(ECityPalettePage::Build);
    TestFalse(TEXT("City change removes stale road attention"), UI->IsTutorialTarget(ECityPaletteAction::OpenRoads));
    TestTrue(TEXT("City change advances attention to Power"), UI->IsTutorialTarget(ECityPaletteAction::OpenPowerFacilities));
    Sim->GetPlacementModel().RemoveRoad(Sim->GetPlacementModel().GetEntranceCell());
    Sim->Tick(0); Board->UpdateEntranceGuidance();
    TestFalse(TEXT("Recovery does not revive initial marker"), Board->EntranceMarker->IsVisible());
    const auto Funds = Sim->GetPlacementModel().GetFunds();
    const auto History = Sim->GetGuidedProgress().CompletedIds;
    const auto Step = Sim->GetEconomySnapshot().FixedStepId;
    UI->ExecuteSharedPaletteAction(ECityPaletteAction::ToggleTutorial);
    TestFalse(TEXT("Skip suppresses attention"), UI->IsTutorialTarget(ECityPaletteAction::OpenRoads));
    TestFalse(TEXT("Skip is a saved player preference"), Settings->bEnabled);
    UI->ExecuteSharedPaletteAction(ECityPaletteAction::ResetTutorial);
    TestTrue(TEXT("Reset resumes guidance"), Settings->bEnabled && UI->bObjectivesOpen);
    TestEqual(TEXT("Reset preserves funds"), Sim->GetPlacementModel().GetFunds(), Funds);
    TestEqual(TEXT("Reset preserves simulation time"), Sim->GetEconomySnapshot().FixedStepId, Step);
    TestTrue(TEXT("Reset preserves learned milestones"), Sim->GetGuidedProgress().CompletedIds == History);
    // Seed completed opening history to represent a mature save without simulating its entire economy.
    auto& Progress = const_cast<FCityGuidedProgress&>(Sim->GetGuidedProgress());
    Progress.CompletedIds = {TEXT("entrance_road"), TEXT("power"), TEXT("water"), TEXT("homes_zone"), TEXT("first_residents"), TEXT("first_jobs")};
    Progress.CurrentId = TEXT("goods");
    TestEqual(TEXT("Mature reset explains completed opening instead of silently doing nothing"),
        UI->GetObjectiveCard().Id, FName(TEXT("tutorial_complete")));
    TestFalse(TEXT("Completion summary does not highlight a stale recovery tool"), UI->IsTutorialTarget(ECityPaletteAction::OpenRoads));
    TestTrue(TEXT("Pulse remains bounded"), CityTutorialAttention::Pulse(0) >= 0 && CityTutorialAttention::Pulse(2.25) <= 1);
    Settings->SetEnabled(Original);
    World->DestroyWorld(false);
    return true;
}
// This is deliberately separate from the synthetic lifecycle test: these captures
// show the live HUD and map in the packaged candidate's isolated automation profile.
class FCityTutorialWalkthroughCommand : public IAutomationLatentCommand
{
public:
    explicit FCityTutorialWalkthroughCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() < Next) return false;
        auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        ACityVRToolController* UI = nullptr;
        if (World) for (TActorIterator<ACityVRToolController> It(World); It; ++It) { UI = *It; break; }
        if (!UI || !UI->Board || !UI->DesktopHud)
        {
            if (++Attempts < 120) { Next = FPlatformTime::Seconds() + .5; return false; }
            Test->AddError(TEXT("Tutorial walkthrough HUD did not initialize")); return true;
        }
        auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
        auto* Board = UI->Board.Get();
        UI->bDesktopFocused = true; UI->LifecycleInputGate.Update(false);
        auto Capture = [](const TCHAR* Name) {
            const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/Desktop");
            IFileManager::Get().MakeDirectory(*Directory, true);
            FScreenshotRequest::RequestScreenshot(Directory / Name, true, false);
        };
        auto* Pawn = World->GetFirstPlayerController() ?
            Cast<ACitySandboxDesktopPawn>(World->GetFirstPlayerController()->GetPawn()) : nullptr;
        if (!Pawn) { Test->AddError(TEXT("Tutorial walkthrough requires the desktop camera pawn")); return true; }
        // Apply and capture occupy separate latent phases. Present() reparents menu controls;
        // a screenshot in the mutation phase catches Slate before its next layout prepass.
        switch (Phase++)
        {
        case 0:
            Original = GetDefault<UCityTutorialSettings>()->bEnabled;
            UI->ReturnToCity();
            if (!Sim->IsResetConfirmationPending()) Sim->RequestResetScenario();
            Sim->RequestResetScenario(); Sim->StartScenario(); Sim->SetSimulationPaused(true);
            Board->PrepareCityView();
            UI->SetTutorialEnabled(true, true); UI->SetCitySummaryVisible(false); UI->SetPaletteOpen(false);
            break;
        case 1:
        {
            // The reset's session change gets its ordinary camera update before this phase.
            // Frame the entrance itself instead of relying on the full-board initial camera.
            OriginalCamera = Pawn->CameraState;
            const auto Entrance = Sim->GetPlacementModel().GetEntranceCell();
            const auto Pose = Board->GetActorTransform();
            Pawn->CameraState.Focus = Pose.TransformPosition(Board->GetCellSurfaceCenter(Entrance) + FVector(1600, 0, 0));
            Pawn->CameraState.Distance = 14000. * Pose.GetScale3D().X;
            Pawn->CameraState.Yaw = 90;
            Pawn->CameraState.Pitch = -55;
            Pawn->ObservedCitySession = Sim->GetCitySessionGeneration();
            Pawn->bCameraInitialized = true;
            break;
        }
        case 2:
            if (CityTutorialAttention::Pulse(FPlatformTime::Seconds()) < .9f) { --Phase; Next = FPlatformTime::Seconds() + .05; return false; }
            Test->TestTrue(TEXT("Live new entrance marker appears"), Board->EntranceMarker->IsVisible());
            Test->TestTrue(TEXT("Live opening instructions appear in the HUD"), UI->GetUiSnapshot().bTutorialGuidance);
            Capture(TEXT("Tutorial-01-Entrance.png"));
            break;
        case 3:
            if (CityTutorialAttention::Pulse(FPlatformTime::Seconds()) > .1f) { --Phase; Next = FPlatformTime::Seconds() + .05; return false; }
            Capture(TEXT("Tutorial-01b-EntrancePulse.png"));
            break;
        case 4:
            UI->SetPaletteOpen(true); UI->SetPalettePage(ECityPalettePage::Build);
            break;
        case 5:
            Test->TestEqual(TEXT("Live nested attention targets Roads"), UI->GetUiSnapshot().TutorialActionId,
                static_cast<int32>(ECityPaletteAction::OpenRoads));
            Capture(TEXT("Tutorial-02-NextRoads.png"));
            break;
        case 6:
        {
            Sim->GetPlacementModel().SetTerrain(nullptr);
            const auto Entrance = Sim->GetPlacementModel().GetEntranceCell();
            auto Road = Sim->GetPlacementModel().BeginRoadPreview(Entrance);
            Sim->GetPlacementModel().UpdateRoadPreviewEnd(Road, {Entrance.X + 5, Entrance.Y});
            Test->TestTrue(TEXT("Live entrance road commits"), Sim->GetPlacementModel().CommitRoad(Road).IsSuccess());
            Sim->Tick(0); Board->RebuildAllPresentation(); Board->UpdateEntranceGuidance();
            UI->SetPaletteOpen(false);
            break;
        }
        case 7:
            Test->TestFalse(TEXT("Live connected entrance marker retires"), Board->EntranceMarker->IsVisible());
            Capture(TEXT("Tutorial-03-Connected.png"));
            break;
        case 8:
            UI->SetPaletteOpen(true); UI->SetPalettePage(ECityPalettePage::Settings);
            break;
        case 9:
            Test->TestTrue(TEXT("Settings exposes reset tutorial"), UI->GetUiSnapshot().MenuEntries.ContainsByPredicate(
                [](const FCityUiActionView& E) { return E.ActionId == static_cast<int32>(ECityPaletteAction::ResetTutorial); }));
            Capture(TEXT("Tutorial-04-Settings.png"));
            break;
        case 10:
        {
            const auto Funds = Sim->GetPlacementModel().GetFunds();
            const auto History = Sim->GetGuidedProgress().CompletedIds;
            UI->ExecuteSharedPaletteAction(ECityPaletteAction::ToggleTutorial);
            UI->ExecuteSharedPaletteAction(ECityPaletteAction::ResetTutorial);
            UI->SetPaletteOpen(false);
            Test->TestEqual(TEXT("Live reset preserves funds"), Sim->GetPlacementModel().GetFunds(), Funds);
            Test->TestTrue(TEXT("Live reset preserves city history"), Sim->GetGuidedProgress().CompletedIds == History);
            break;
        }
        case 11:
            Test->TestTrue(TEXT("Live reset resumes actionable guidance"), UI->GetUiSnapshot().bTutorialGuidance);
            Capture(TEXT("Tutorial-05-ResetResumed.png"));
            break;
        default:
            Pawn->CameraState = OriginalCamera;
            GetMutableDefault<UCityTutorialSettings>()->SetEnabled(Original);
            return true;
        }
        Next = FPlatformTime::Seconds() + .6;
        return false;
    }
private:
    FAutomationTestBase* Test;
    int32 Phase = 0, Attempts = 0;
    double Next = 0;
    bool Original = true;
    FCityDesktopCamera OriginalCamera;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTutorialWalkthroughTest, "CitySim.Desktop.TutorialWalkthrough",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityTutorialWalkthroughTest::RunTest(const FString& Parameters)
{
    FString UserDirectory;
    if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) ||
        !(UserDirectory.Contains(TEXT("DesktopSmoke-Automation")) || UserDirectory.Contains(TEXT("EditorVisualTests-User")) ||
          (FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation")) && UserDirectory.Contains(TEXT("Client-")))))
    {
        AddError(TEXT("Tutorial walkthrough requires -CityDesktop and an isolated automation UserDir"));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FCityTutorialWalkthroughCommand(this));
    return true;
}
#endif
