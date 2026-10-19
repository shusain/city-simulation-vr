#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UObjectIterator.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityOutpatientVisualCommand:public IAutomationLatentCommand
{
public:
 explicit FCityOutpatientVisualCommand(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()>Deadline){Test->AddError(FString::Printf(TEXT("Inspection capture timed out at phase %d"),Phase));return true;}
  if(FPlatformTime::Seconds()<Next)return false;
  auto* W=GEngine&&GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;if(!W)return false;
  ACityVRToolController* UI=nullptr;ACityBoardActor* Board=nullptr;
  for(TActorIterator<ACityVRToolController> It(W);It;++It){UI=*It;break;}
  for(TActorIterator<ACityBoardActor> It(W);It;++It){Board=*It;break;}
  if(!UI||!Board)return false;auto* Sim=W->GetSubsystem<UCitySimulationSubsystem>();
  if(Phase>=10){Test->AddInfo(TEXT("OUTPATIENT_HEALTH_VISUAL_COMPLETE Captures=5 PhysicalInputPending=1"));return true;}
  const int32 Index=Phase/2;FString Root;FParse::Value(FCommandLine::Get(),TEXT("OutpatientFixtureRoot="),Root);
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/OutpatientHealth");IFileManager::Get().MakeDirectory(*Directory,true);
  if(Phase%2==0)
  {
   const TCHAR* Files[]={TEXT("Outward.json"),TEXT("Returning.json"),TEXT("Home.json"),TEXT("Outward.json"),TEXT("Recovered.json")};
   FString Json,Error;FCitySaveDocument D;
   if(!FFileHelper::LoadFileToString(Json,*(Root/Files[Index]))||!FCityPersistence::Deserialize(Json,D,Error)){Test->AddError(Error);return true;}
   if(Index==0)
   {
    const auto* P=D.Health.Patients.FindByPredicate([](const auto& V){return V.Care==ECityHealthCare::WalkIn;});
    if(!P){Test->AddError(TEXT("Native outward walk fixture missing"));return true;}
    Home=D.Population.Citizens.FindByPredicate([&](const auto& C){return C.Id==P->CitizenId;})->HomeEntityId;
   }
   uint64 Id=Home;
   if(Index==1||Index==2)
   {
    const auto* P=D.Health.Patients.FindByPredicate([&](const auto& V){return V.Care==(Index==1?ECityHealthCare::ReturningHome:ECityHealthCare::HomeRecovery);});
    if(!P){Test->AddError(TEXT("Native return/home fixture missing"));return true;}
    Id=D.Population.Citizens.FindByPredicate([&](const auto& C){return C.Id==P->CitizenId;})->HomeEntityId;
   }
   if(Index==3)Id=D.Health.Patients[0].HospitalId;
   if(!Sim->LoadCityFromPath(Root/Files[Index],Error,false)){Test->AddError(Error);return true;}
   UI->ReturnToCity();Sim->SetSimulationPaused(true);UI->SetObjectivesVisible(false);UI->SetCitySummaryVisible(false);Board->PrepareCityView();Board->SetToolMode(ECityToolMode::Inspect);UI->InspectBuilding(Id);
   for(TObjectIterator<UCityDesktopHud> It;It;++It)if(It->GetWorld()==W && It->IsInViewport())for(int32 I=0;I<6;++I)if(It->IsInspectionSectionOpen(I))It->ToggleInspectionSection(I);
  }
  else
  {
   bool Opening=false;for(TObjectIterator<UCityDesktopHud> It;It;++It)if(It->GetWorld()==W&&It->IsInViewport()&&!It->IsInspectionSectionOpen(3)){It->ToggleInspectionSection(3);Opening=true;}
   if(Opening){Next=FPlatformTime::Seconds()+1.;return false;}
   const auto View=UI->GetUiSnapshot();const auto& Health=View.InspectionSections[3];
   if(Index!=3)Test->TestTrue(TEXT("accepted home health remains charts only"),Health.Gauges.Num()==4&&Health.Rows.IsEmpty()&&Health.Detail.IsEmpty()&&Health.Help.IsEmpty()&&!View.InspectionSummary.ToString().Contains(TEXT("ILL ")));
   else
   {
    for(const TCHAR* Label:{TEXT("Inpatient beds"),TEXT("Beds reserved"),TEXT("Outpatient visits")})Test->TestTrue(FString(TEXT("separate clinic and ward chart: "))+Label,Health.Gauges.ContainsByPredicate([&](const auto& G){return G.Label.ToString()==Label;}));
   }
   FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Health-%d.png"),Index),true,false);
  }
  ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
 }
private:
 FAutomationTestBase* Test;int32 Phase=0;uint64 Home=0;double Next=0,Deadline=FPlatformTime::Seconds()+120.;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOutpatientVisualTest,"CitySim.Desktop.OutpatientHealthVisual",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityOutpatientVisualTest::RunTest(const FString&)
{
 FString User,Root;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("Outpatient"))||!FParse::Value(FCommandLine::Get(),TEXT("OutpatientFixtureRoot="),Root)){AddError(TEXT("Requires isolated Outpatient profile and native fixtures"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FCityOutpatientVisualCommand(this));return true;
}
#endif
