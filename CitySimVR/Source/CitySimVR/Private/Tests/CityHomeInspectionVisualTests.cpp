#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityConditionIndicators.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"
#include "Presentation/CityDesktopHud.h"
#include "UObject/UObjectIterator.h"

class FCityHomeInspectionVisualCommand : public IAutomationLatentCommand
{
public:
 explicit FCityHomeInspectionVisualCommand(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()>Deadline){Test->AddError(FString::Printf(TEXT("Inspection capture timed out at phase %d"),Phase));return true;}
  if(FPlatformTime::Seconds()<Next)return false;
  auto* W=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;if(!W)return false;
  ACityVRToolController* UI=nullptr;ACityBoardActor* Board=nullptr;
  for(TActorIterator<ACityVRToolController> It(W);It;++It){UI=*It;break;}
  for(TActorIterator<ACityBoardActor> It(W);It;++It){Board=*It;break;}
  if(!UI || !Board)return false;
  auto* Sim=W->GetSubsystem<UCitySimulationSubsystem>();
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/HomeInspectionInspection");
  IFileManager::Get().MakeDirectory(*Directory,true);
  if(Phase>=8){Test->AddInfo(TEXT("HOMEINSPECTION_INSPECTION_VISUAL_COMPLETE Captures=4 PhysicalInputPending=1"));return true;}
  const int32 Index=Phase/2;
  if(Phase%2==0)
  {
   FString Path,Json,Error;FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationSave="),Path);
   FCitySaveDocument D;
   if(!FFileHelper::LoadFileToString(Json,*Path)||!FCityPersistence::Deserialize(Json,D,Error)){Test->AddError(Error);return true;}
   D.bSimulationPaused=true;
   D.Population.PurchaseReservations.RemoveAll([&](const auto& O){const bool Remove=O.HouseholdId==26 || O.ShopId==6;if(Remove)for(auto& C:D.Population.Citizens)if(C.Id==O.BuyerId)C.PendingShopEntityId=0;return Remove;});
   auto* H=D.Population.Households.FindByPredicate([](const auto& V){return V.Id==26;});
   auto* B=D.Population.Businesses.FindByPredicate([](const auto& V){return V.EntityId==6;});
   if(!H || !B){Test->AddError(TEXT("Expected City837230 fixture IDs"));return true;}
   H->bUsesStoredSupplies=true;H->SupplyUnits=Index==1 || Index==2 ? 0 : H->ConsumptionOrders*3;
   H->SupplyDay=D.Economy.LastBilledMonthId;H->LastEssentialPurchaseDay=H->SupplyDay;
   if(Index==1){H->Balance=0;for(auto& C:D.Population.Citizens)if(C.HouseholdId==26)C.AvailableMoney=0;}
   if(Index==2){for(auto& Shop:D.Population.Businesses)if(!Shop.bIndustrial){Shop.Inventory=0;for(const auto& O:D.Population.PurchaseReservations)if(O.ShopId==Shop.EntityId)Shop.Inventory+=O.Quantity;}}
   if(Index==3)
   {
    D.Health.Patients.Reset();int32 Patient=0;
    for(auto& C:D.Population.Citizens)if(C.HouseholdId==26 && Patient<2)
    {
     FCityHealthPatient P;P.CitizenId=C.Id;P.Illness=Patient++ ? ECityIllness::Severe : ECityIllness::Mild;
     C.bOnMedicalLeave=P.Illness==ECityIllness::Severe;D.Health.Patients.Add(P);
    }
   }
   const FString Fixture=Directory/TEXT("Fixture.json");
   if(!FCityPersistence::Serialize(D,Json,Error)||!FFileHelper::SaveStringToFile(Json,*Fixture)||!Sim->LoadCityFromPath(Fixture,Error,false)){Test->AddError(Error);return true;}
   UI->ReturnToCity();Sim->SetSimulationPaused(true);UI->SetObjectivesVisible(false);UI->SetCitySummaryVisible(false);Board->PrepareCityView();Board->SetToolMode(ECityToolMode::Inspect);UI->InspectBuilding(26);
   for(TObjectIterator<UCityDesktopHud> It;It;++It)if(It->GetWorld()==W && It->IsInViewport())
   {for(int32 Section=0;Section<6;++Section)if(It->IsInspectionSectionOpen(Section))It->ToggleInspectionSection(Section);It->ToggleInspectionSection(Index==3?3:1);}
  }
  else
  {
   // The first presentation tick resets accordion state for a new selection.
   bool Opening=false;
   for(TObjectIterator<UCityDesktopHud> It;It;++It)if(It->GetWorld()==W && It->IsInViewport() && !It->IsInspectionSectionOpen(Index==3?3:1))
   {It->ToggleInspectionSection(Index==3?3:1);Opening=true;}
   if(Opening){Next=FPlatformTime::Seconds()+1.;return false;}
   const auto View=UI->GetUiSnapshot();const FString Summary=View.InspectionSummary.ToString();
   Test->TestFalse(TEXT("no supply or illness prose above accordions"),Summary.Contains(TEXT("suppl"),ESearchCase::IgnoreCase)||Summary.Contains(TEXT("ILL "))||Summary.Contains(TEXT("shopping"),ESearchCase::IgnoreCase));
   const auto& Supplies=View.InspectionSections[1];const auto& Health=View.InspectionSections[3];
   Test->TestEqual(TEXT("dedicated supplies accordion"),Supplies.Title.ToString(),FString(TEXT("Shopping & supplies")));
   Test->TestEqual(TEXT("four supplies bars"),Supplies.Gauges.Num(),4);
   Test->TestTrue(TEXT("no supply prose rows/help"),Supplies.Rows.IsEmpty() && Supplies.Help.IsEmpty());
   Test->TestEqual(TEXT("only actual concise supply blocker"),Supplies.Detail.ToString(),FString(Index==1?TEXT("Insufficient available cash"):Index==2?TEXT("No available shop stock"):TEXT("")));
   Test->TestTrue(TEXT("health represented only by charts"),Health.Rows.IsEmpty() && Health.Detail.IsEmpty() && Health.Help.IsEmpty() && Health.Gauges.Num()==4);
   if(Index==3){Test->TestTrue(TEXT("ill count in chart"),Health.Gauges[0].Value.ToString().StartsWith(TEXT("2 / ")));Test->TestTrue(TEXT("severe count in chart"),Health.Gauges[1].Value.ToString().StartsWith(TEXT("1 / ")));}
   Test->TestEqual(TEXT("UI preserves authoritative illness state"),Sim->GetHealthSnapshot().Patients.Num(),Index==3?2:0);
   FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Inspect-%d.png"),Index),true,false);Test->AddInfo(TEXT("HOMEINSPECTION_UI ")+Supplies.Detail.ToString());
  }
  ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
 }
private:
 FAutomationTestBase* Test;int32 Phase=0;double Next=0,Deadline=FPlatformTime::Seconds()+120.;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHomeInspectionVisualTest,"CitySim.Desktop.HomeInspectionCharts",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityHomeInspectionVisualTest::RunTest(const FString&)
{
 FString User;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("HomeInspection"))){AddError(TEXT("Requires isolated HomeInspection profile"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FCityHomeInspectionVisualCommand(this));return true;
}
#endif
