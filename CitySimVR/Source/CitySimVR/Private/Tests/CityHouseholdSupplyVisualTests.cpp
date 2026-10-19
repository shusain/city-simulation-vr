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

class FCityHouseholdSupplyVisualCommand : public IAutomationLatentCommand
{
public:
 explicit FCityHouseholdSupplyVisualCommand(FAutomationTestBase* In):Test(In){}
 bool Update() override
 {
  if(FPlatformTime::Seconds()<Next)return false;
  auto* W=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;if(!W)return false;
  ACityVRToolController* UI=nullptr;ACityBoardActor* Board=nullptr;
  for(TActorIterator<ACityVRToolController> It(W);It;++It){UI=*It;break;}
  for(TActorIterator<ACityBoardActor> It(W);It;++It){Board=*It;break;}
  if(!UI || !Board)return false;
  auto* Sim=W->GetSubsystem<UCitySimulationSubsystem>();
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/HouseholdSupplySupplies");
  IFileManager::Get().MakeDirectory(*Directory,true);
  if(Phase>=10){Test->AddInfo(TEXT("HOUSEHOLDSUPPLY_SUPPLY_VISUAL_COMPLETE Captures=5 PhysicalInputPending=1"));return true;}
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
   if(Index==1){H->bUsesStoredSupplies=true;H->SupplyUnits=0;H->SupplyDay=D.Economy.LastBilledMonthId;H->LastEssentialPurchaseDay=H->SupplyDay;}
   if(Index==2 || Index==3){B->bInitialFreightReceived=Index==3;B->Inventory=0;B->bHasReceivedGoods=Index==3;B->OpeningGraceUntilStep=D.Economy.FixedStepId+450;}
   if(Index==4)
   {
    H->bUsesStoredSupplies=true;H->SupplyUnits=H->ConsumptionOrders*7;H->SupplyDay=D.Economy.LastBilledMonthId;H->LastEssentialPurchaseDay=H->SupplyDay;H->ComfortTier=2;H->LastComfortDay=H->SupplyDay;
    bool First=true;for(auto& C:D.Population.Citizens)if(C.HouseholdId==26){C.AvailableMoney=First?500:0;C.Education=ECityEducationLevel::Higher;First=false;}H->Balance=500;
   }
   const FString Fixture=Directory/TEXT("Fixture.json");
   if(!FCityPersistence::Serialize(D,Json,Error)||!FFileHelper::SaveStringToFile(Json,*Fixture)||!Sim->LoadCityFromPath(Fixture,Error,false)){Test->AddError(Error);return true;}
   UI->ReturnToCity();Sim->SetSimulationPaused(true);UI->SetObjectivesVisible(false);UI->SetCitySummaryVisible(false);Board->PrepareCityView();UI->InspectBuilding(Index==2 || Index==3 ? 6 : 26);
  }
  else
  {
   const FString Summary=UI->GetUiSnapshot().InspectionSummary.ToString();
   const TCHAR* Expected[]={TEXT("Starter supplies"),TEXT("SUPPLIES EXHAUSTED"),TEXT("Opening delivery pending"),TEXT("OUT OF STOCK"),TEXT("high-density")};
   if(Index==2 || Index==3) Test->TestTrue(TEXT("business supply status remains explicit: ")+Summary,Summary.Contains(Expected[Index]));
   else {const auto View=UI->GetUiSnapshot();Test->TestEqual(TEXT("supplies now live in their accordion"),View.InspectionSections[1].Title.ToString(),FString(TEXT("Shopping & supplies")));Test->TestFalse(TEXT("home has no supply explanation above accordions"),Summary.Contains(TEXT("Essentials stored")));}
   const auto Rows=FCityConditionIndicators::Build(Sim->GetPlacementModel(),Sim->GetFireSnapshot(),Sim->GetCrimeSnapshot(),Sim->GetEconomyTuning().FireRepairSteps,nullptr,&Sim->GetPopulationSnapshot(),0,nullptr,&Sim->GetServiceSnapshot(),100,0);
   if(Index==0 || Index==4)Test->TestFalse(TEXT("stored supplies do not create a shopping warning"),Rows.ContainsByPredicate([](const auto& R){return R.EntityId==26 && (R.bSupplyEmpty || R.SettledConcerns);}));
   if(Index==2)Test->TestFalse(TEXT("first opening shipment grace has no warning badge"),Rows.ContainsByPredicate([](const auto& R){return R.EntityId==6;}));
   FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("Supply-%d.png"),Index),true,false);Test->AddInfo(TEXT("HOUSEHOLDSUPPLY_UI ")+Summary);
  }
  ++Phase;Next=FPlatformTime::Seconds()+1.;return false;
 }
private:
 FAutomationTestBase* Test;int32 Phase=0;double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHouseholdSupplyVisualTest,"CitySim.Desktop.HouseholdSuppliesVisual",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityHouseholdSupplyVisualTest::RunTest(const FString&)
{
 FString User;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User)||!User.Contains(TEXT("HouseholdSupply"))){AddError(TEXT("Requires isolated HouseholdSupply profile"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FCityHouseholdSupplyVisualCommand(this));return true;
}
#endif
