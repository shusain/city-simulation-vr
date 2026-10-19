#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityConditionIndicators.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityShoppingStatusVisualCommand : public IAutomationLatentCommand
{
public:
 explicit FCityShoppingStatusVisualCommand(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()<Next)return false;
  auto* W=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
  if(!W)return false;
  ACityVRToolController* UI=nullptr;ACityBoardActor* Board=nullptr;
  for(TActorIterator<ACityVRToolController> It(W);It;++It){UI=*It;break;}
  for(TActorIterator<ACityBoardActor> It(W);It;++It){Board=*It;break;}
  if(!UI || !Board)return false;
  auto* Sim=W->GetSubsystem<UCitySimulationSubsystem>();
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/ShoppingShopping");
  if(Phase==0)
  {
   FString Path,Error;FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationSave="),Path);
   UI->ReturnToCity();UI->SetObjectivesVisible(false);UI->SetCitySummaryVisible(false);
   if(!Sim->LoadCityFromPath(Path,Error,false)){Test->AddError(Error);return true;}
   Sim->SetSimulationPaused(true);Board->PrepareCityView();UI->InspectBuilding(26);
   if(auto* Pawn=Cast<ACitySandboxDesktopPawn>(W->GetFirstPlayerController()->GetPawn())){FVector Roof;Board->GetBuildingMarkerAnchor(26,Roof);Pawn->FocusCondition(Roof);}
  }
  else if(Phase==2)
  {
   Sim->SetSimulationSpeed(1);Sim->SetSimulationPaused(false);
   bool Moving=false;
   for(int32 I=0;I<120 && !Moving;++I){Sim->Tick(2.4f);Moving=FCityConditionIndicators::Explain(*Sim,26).Contains(TEXT("Shopper travelling"));}
   Sim->SetSimulationPaused(true);Test->TestTrue(TEXT("actual household shopping travel reached"),Moving);Board->PrepareCityView();UI->InspectBuilding(26);
  }
  else if(Phase==4)
  {
   FString Path,Error;FParse::Value(FCommandLine::Get(),TEXT("ShoppingMidday="),Path);
   if(!Sim->LoadCityFromPath(Path,Error,false)){Test->AddError(Error);return true;}
   Sim->SetSimulationPaused(true);Board->PrepareCityView();UI->InspectBuilding(26);
  }
  else if(Phase==1 || Phase==3 || Phase==5)
  {
   const auto View=UI->GetUiSnapshot();const FString Summary=FCityConditionIndicators::Explain(*Sim,26);
   const auto& Supplies=View.InspectionSections[1];
   Test->TestTrue(TEXT("shopping status is inside concise charts"),Supplies.Gauges.Num()==4 && Supplies.Rows.IsEmpty() && Supplies.Help.IsEmpty());
   IFileManager::Get().MakeDirectory(*Directory,true);
   FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Shopping-%d.png"),Phase/2),true,false);
   Test->AddInfo(TEXT("SHOPPING_SHOPPING_UI ")+Summary);
  }
  else {Test->AddInfo(TEXT("SHOPPING_SHOPPING_VISUAL_COMPLETE Captures=3 PhysicalInputPending=1"));return true;}
  ++Phase;Next=FPlatformTime::Seconds()+1.1;return false;
 }
private:
 FAutomationTestBase* Test;int32 Phase=0;double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShoppingStatusVisualTest,"CitySim.Desktop.ShoppingStatusVisual",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityShoppingStatusVisualTest::RunTest(const FString&)
{
 FString User;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("Shopping"))){AddError(TEXT("Requires isolated Shopping profile"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FCityShoppingStatusVisualCommand(this));return true;
}
#endif
