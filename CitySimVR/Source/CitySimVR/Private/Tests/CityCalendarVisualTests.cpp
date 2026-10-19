#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityCalendarVisualCommand : public IAutomationLatentCommand
{
public:
 explicit FCityCalendarVisualCommand(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()<Next)return false;
  auto* W=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
  if(!W)return false;
  ACityBoardActor* Board=nullptr;ACityVRToolController* UI=nullptr;
  for(TActorIterator<ACityBoardActor> It(W);It;++It){Board=*It;break;}
  for(TActorIterator<ACityVRToolController> It(W);It;++It){UI=*It;break;}
  if(!Board || !UI)return false;
  auto* Sim=W->GetSubsystem<UCitySimulationSubsystem>();
  if(!bReady)
  {
   UI->ReturnToCity();
   Directory=FPaths::ProjectSavedDir()/TEXT("Automation/Calendar");IFileManager::Get().MakeDirectory(*Directory,true);
   FCityPlacementModel City(100000);auto Road=City.BeginRoadPreview({0,8});City.UpdateRoadPreviewEnd(Road,{22,8});
   if(!City.CommitRoad(Road).IsSuccess()){Test->AddError(TEXT("Calendar fixture road failed"));return true;}
   auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
   if(!Catalog){Test->AddError(TEXT("Calendar fixture catalog missing"));return true;}
   int32 X=2;for(FName Name:{FName(TEXT("Home")),FName(TEXT("Shop")),FName(TEXT("Factory"))})
   {
    const auto* D=Catalog->FindDefinition(Name);if(!D){Test->AddError(TEXT("Calendar definition missing"));return true;}
    auto P=City.BeginPreview(*D,{X,9});if(!City.CommitPlacement(P,*D).IsSuccess()){Test->AddError(TEXT("Calendar fixture placement failed"));return true;}X+=7;
   }
   Fixture.Placement=City.ExportState();Fixture.bSimulationPaused=true;
   Fixture.Terrain.BroadElevationAmplitudeCm=Fixture.Terrain.HillAmplitudeCm=Fixture.Terrain.RidgeAmplitudeCm=0;
   FCityDevelopmentSimulation Development;Development.InitializeExistingAsOperational(City,Catalog,0);Fixture.Development=Development.GetSnapshot();
   auto* Camera=W->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->FieldOfView=55;
   const auto Pose=Board->GetActorTransform();const FVector Aim=Pose.TransformPosition({9000,8000,0});
   const FVector Position=Aim+Pose.TransformVector({0,-18000,18000});Camera->SetActorLocation(Position);Camera->SetActorRotation((Aim-Position).Rotation());
   W->GetFirstPlayerController()->SetViewTarget(Camera);
   Board->SetLightingPreset(ECityLightingPreset::Automatic);
   bReady=true;
  }
  if(Phase>=10){Test->AddInfo(TEXT("CITYSIM_CALENDAR_CALENDAR_VISUAL_COMPLETE Captures=5"));return true;}
  const int32 Index=Phase/2;const int64 Steps[]={0,38,75,113,150};const TCHAR* Times[]={TEXT("00:00"),TEXT("06:04"),TEXT("12:00"),TEXT("18:04"),TEXT("00:00")};
  if(Phase%2==0)
  {
   Fixture.Economy.FixedStepId=Fixture.Disasters.LastFixedStepId=Steps[Index];Fixture.Economy.MonthId=Steps[Index]/150;Fixture.Fire.LastFixedStepId=Fixture.Crime.LastFixedStepId=Fixture.Development.LastFixedStepId=Fixture.Traffic.FixedStepId=Steps[Index];
   FString Json,Error;const FString Path=Directory/TEXT("Fixture.json");
   if(!FCityPersistence::Serialize(Fixture,Json,Error)||!FFileHelper::SaveStringToFile(Json,*Path)||!Sim->LoadCityFromPath(Path,Error,false))
   {Test->AddError(TEXT("Calendar paused-load fixture: ")+Error);return true;}
   Sim->SetSimulationPaused(true);Sim->SetSimulationSpeed(Index%3==0?1:Index%3==1?3:5);
   Board->SetLightingPreset(ECityLightingPreset::Automatic);
  }
  else
  {
   const auto View=UI->GetUiSnapshot();
   Test->TestTrue(TEXT("HUD calendar uses saved common time"),View.Calendar.ToString().Contains(Times[Index]));
   Test->TestTrue(TEXT("HUD bills are daily"),View.Budget.ToString().Contains(TEXT("daily bills")));
   Test->TestTrue(TEXT("calendar day agrees at midnight"),View.Calendar.ToString().Contains(Index==4?TEXT("DAY 2/25"):TEXT("DAY 1/25")));
   for(TActorIterator<ACitySandboxEnvironment> It(W);It;++It)
   {
    const auto* Sun=It->FindComponentByClass<UDirectionalLightComponent>();
    if(Index==0 || Index==4)Test->TestTrue(TEXT("paused midnight load shows night"),Sun && Sun->Intensity<1);
    if(Index==2)Test->TestTrue(TEXT("paused noon load shows day"),Sun && Sun->Intensity>3.5f);
   }
   FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Calendar-%d.png"),Index),true,false);
   Test->AddInfo(TEXT("CALENDAR_HUD ")+View.Calendar.ToString()+TEXT(" | ")+View.Budget.ToString());
  }
  ++Phase;Next=FPlatformTime::Seconds()+1.2;return false;
 }
private:
 FAutomationTestBase* Test;FCitySaveDocument Fixture;FString Directory;int32 Phase=0;double Next=0;bool bReady=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCalendarVisualTest,"CitySim.Desktop.CalendarVisual",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityCalendarVisualTest::RunTest(const FString&)
{
 FString User;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("Calendar")))
 {AddError(TEXT("Calendar visual requires isolated Calendar profile"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FCityCalendarVisualCommand(this));return true;
}
#endif
