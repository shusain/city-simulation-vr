#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Blueprint/WidgetTree.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "UnrealClient.h"

class FCityDesktopContinuityCommand : public IAutomationLatentCommand
{
public:
 explicit FCityDesktopContinuityCommand(FAutomationTestBase* In) : Test(In) {}
 bool Update() override
 {
  if (FPlatformTime::Seconds() < Next) return false;
  auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
  ACityVRToolController* UI = nullptr;
  if (W) for (TActorIterator<ACityVRToolController> It(W); It; ++It) { UI = *It; break; }
  if (!UI || !UI->DesktopHud || !UI->Board) {
   if (++Attempts < 120) { Next = FPlatformTime::Seconds()+.5; return false; }
   Test->AddError(TEXT("Continuity HUD missing")); return true;
  }
  UI->bDesktopFocused = true;
  UI->LifecycleInputGate.Update(false);
  auto* Board = UI->Board.Get();
  auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
  if (!Initialized) {
   FCityIdentity Identity;
   Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
   Identity.Name = TEXT("Interaction continuity fixture");
   FString Error;
   const FString Dir = FPaths::ProjectSavedDir()/TEXT("Automation/InteractionContinuity/Cities");
   FCityTerrainGenerationParameters Terrain;
   Terrain.BroadElevationAmplitudeCm = Terrain.HillAmplitudeCm = Terrain.RidgeAmplitudeCm = 0;
   if (!Sim->CreateCity(Identity, Terrain, Dir, Error)) { Test->AddError(Error); return true; }
   Initialized = true;
  }
  auto& Model = Sim->GetPlacementModel();
  auto* Player = W->GetFirstPlayerController();
  if (!Player || !Player->GetPawn()) { Test->AddError(TEXT("Desktop camera missing")); return true; }
  Player->GetPawn()->SetActorTickEnabled(false);
  auto Aim = [&](const FCityCellCoord& Cell) {
   const auto Pose = Board->GetActorTransform();
   const FVector Target = Pose.TransformPosition(Board->GetCellSurfaceCenter(Cell, 500));
   const FVector Location = Target + Pose.TransformVector(FVector(1,-1,1).GetSafeNormal()*8500);
   Player->GetPawn()->SetActorLocation(Location);
   Player->SetControlRotation((Target-Location).Rotation());
   if (auto* Camera = Player->GetPawn()->FindComponentByClass<UCameraComponent>()) Camera->SetFieldOfView(45);
   FVector2D Screen;
   if (Player->ProjectWorldLocationToScreen(Target, Screen)) Player->SetMouseLocation(FMath::RoundToInt(Screen.X), FMath::RoundToInt(Screen.Y));
  };
  // Give Slate visibility/layout transitions several actual frames before capture.
  // A screenshot requested in the same phase can precede the next widget prepass.
  if (!PendingCapture.IsEmpty()) {
   if (GFrameCounter < CaptureStateFrame + 3) return false;
   if (PendingCapture == TEXT("InspectorAfterChange.png")) {
    auto* Preview = Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
    Test->TestTrue(TEXT("Visible inspector contains captured current building preview"),
        Preview && Preview->IsVisible() && Preview->HasModel() && Preview->GetPreviewEntity() == Entity &&
        Preview->GetPreviewMesh() && Preview->GetPreviewTarget() && Preview->GetCaptureCount() > 0);
   }
   const FString Path = FPaths::ProjectSavedDir()/TEXT("Automation/InteractionContinuity")/PendingCapture;
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
   FScreenshotRequest::RequestScreenshot(Path, true, false);
   PendingCapture.Reset();
   Next=FPlatformTime::Seconds()+.8;
   return false;
  }
  auto Capture = [&](const TCHAR* Name) {
   PendingCapture=Name;
   CaptureStateFrame=GFrameCounter;
  };
  switch (Phase) {
  case 0: {
   UI->ReturnToCity();
   UI->SetObjectivesVisible(false);
   Aim({21,21});
   Board->PrepareCityView();
   Model.SetTerrain(nullptr);
   Board->SetCommandInputBound(true);
   Board->SetToolMode(ECityToolMode::Inspect);
   auto* Def = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
   if (!Def) { Test->AddError(TEXT("Home missing")); return true; }
   auto P = Model.BeginPreview(*Def, {20,20});
   const auto R = Model.CommitPlacement(P,*Def);
   if (!R.IsSuccess()) { Test->AddError(TEXT("Home fixture placement failed")); return true; }
   Entity = R.EntityId;
   Board->RebuildAllPresentation();
   UI->SetCitySummaryVisible(true);
   UI->InspectBuilding(Entity);
   break;
  }
  case 1: {
   // Actual unrelated placement changes authoritative revision; notification exercises the
   // same full presentation refresh used by growth and development completion.
   auto* Def = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
   auto P = Model.BeginPreview(*Def, {24,24});
   Test->TestTrue(TEXT("Unrelated city placement"), Model.CommitPlacement(P,*Def).IsSuccess());
   ++Sim->PresentationGeneration;
   Board->PrepareCityView();
   UI->UpdateInspectionTooltip();
   Test->TestEqual(TEXT("Inspector survives city refresh"), UI->InspectedEntityId, Entity);
   Test->TestFalse(TEXT("City info suppressed during inspection"), UI->GetUiSnapshot().bCityInfoOpen);
   Capture(TEXT("InspectorAfterChange.png"));
   break;
  }
  case 2:
   UI->ClearInspection();
   Test->TestTrue(TEXT("Dismissal restores enabled city info"), UI->GetUiSnapshot().bCityInfoOpen);
   Capture(TEXT("CityInfoRestored.png"));
   break;
  case 3: {
   UI->SetCitySummaryVisible(false);
   // Freeze only pointer ingestion; Board and native HUD keep ticking normally.
   // A vertical fixture ray keeps the endpoint stable through delayed capture.
   UI->SetActorTickEnabled(false);
   Board->SetToolMode(ECityToolMode::Road);
   Start = Model.GetEntranceCell(); End = {Start.X+3,Start.Y};
   Aim(End);
   const auto Pose = Board->GetActorTransform();
   Board->SetControllerRay(Pose.TransformPosition(Board->GetCellSurfaceCenter(End,10000)),
       Pose.TransformVector(-FVector::UpVector),true,true);
   Board->UpdateRoadTool(nullptr,Start,true,true,false);
   ++Sim->PresentationGeneration;
   Board->PrepareCityView();
   Board->UpdateRoadTool(nullptr,End,true,false,false);
   Test->TestTrue(TEXT("Road gesture survives refresh"), Board->RoadPreview.bActive && Board->RoadPreview.Start == Start && Board->RoadPreviewInstances->GetInstanceCount()>0);
   Capture(TEXT("RoadPreviewAfterChange.png"));
   break;
  }
  case 4:
   UI->SetActorTickEnabled(true);
   Board->UpdateRoadTool(nullptr,End,true,true,false);
   Test->TestTrue(TEXT("Road endpoint commits after refresh"),Model.IsRoad(End));
   Capture(TEXT("RoadCompleted.png"));
   break;
  default:
   Player->GetPawn()->SetActorTickEnabled(true);
   UE_LOG(LogTemp,Display,TEXT("CITYSIM_INTERACTION_CONTINUITY_COMPLETE Captures=4 PhysicalInputPending=1"));
   return true;
  }
  ++Phase; Next=FPlatformTime::Seconds()+.8; return false;
 }
private:
 FAutomationTestBase* Test;
 FString PendingCapture;
 uint64 CaptureStateFrame=0;
 uint64 Entity=0;
 FCityCellCoord Start,End;
 bool Initialized=false;
 int32 Phase=0,Attempts=0;
 double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopContinuityTest,"CitySim.Desktop.InteractionContinuity",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDesktopContinuityTest::RunTest(const FString& Parameters)
{
 FString UserDir;
 if (!FParse::Param(FCommandLine::Get(),TEXT("CityDesktop")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || (!UserDir.Contains(TEXT("InteractionContinuity")) && !UserDir.Contains(TEXT("EditorVisualTests-User")) && !FParse::Param(FCommandLine::Get(),TEXT("CityPackagedAutomation")))) {
  AddError(TEXT("Requires isolated InteractionContinuity UserDir and CityDesktop")); return false;
 }
 ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopContinuityCommand(this));
 return true;
}
#endif
