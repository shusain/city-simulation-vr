#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Persistence/CityPersistence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHealthServiceCityTest,"CitySim.HealthService.ReportedCityContinuation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityHealthServiceCityTest::RunTest(const FString&)
{
 FString Path,Output,Original,Error;
 if(!FParse::Value(FCommandLine::Get(),TEXT("HealthServiceSave="),Path)||!FParse::Value(FCommandLine::Get(),TEXT("HealthServiceOutput="),Output)||!FFileHelper::LoadFileToString(Original,*Path)){AddError(TEXT("Requires captured current city and isolated output"));return false;}
 IFileManager::Get().MakeDirectory(*Output,true);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HealthServiceCity"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* WR=UWorld::CreateWorld(EWorldType::Game,false,TEXT("HealthServiceReload"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* S=W->GetSubsystem<UCitySimulationSubsystem>();
 auto* R=WR->GetSubsystem<UCitySimulationSubsystem>();bool Reloaded=false;
 auto Cleanup=[&](){WR->DestroyWorld(false);W->DestroyWorld(false);};
 auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
 if(!TestNotNull(TEXT("catalog"),Catalog)){Cleanup();return false;}S->RegisterBuildingCatalog(Catalog);R->RegisterBuildingCatalog(Catalog);
 if(!TestTrue(TEXT("captured city loads: ")+Error,S->LoadCityFromPath(Path,Error,false))){Cleanup();return false;}
 S->SetSimulationPaused(false);S->SetSimulationSpeed(1);
 TArray<uint64> OriginalAmbulanceTrips;
 for(const auto& Trip:S->GetPopulationSnapshot().TransitTrips)if(Trip.IsHealthJourney()&&Trip.VehicleType==ECityTripVehicleType::Ambulance)OriginalAmbulanceTrips.Add(Trip.Id);
 auto Record=[&](int32 Update)
 {
  const auto& H=S->GetHealthSnapshot();int32 MildWaiting=0,SevereWaiting=0,Beds=0,Visits=0,Inbound=0,Home=0;
  for(const auto& P:H.Patients){MildWaiting+=P.Illness==ECityIllness::Mild&&P.Care==ECityHealthCare::None;SevereWaiting+=P.Illness==ECityIllness::Severe&&P.Care==ECityHealthCare::None;Beds+=P.Care==ECityHealthCare::Hospital;Visits+=P.UsesOutpatientPlace();Inbound+=P.Care==ECityHealthCare::AwaitingAmbulance;Home+=P.Care==ECityHealthCare::HomeRecovery;}
  AddInfo(FString::Printf(TEXT("EMERGENCYROUTING_CITY Update=%d Step=%lld Patients=%d MildWaiting=%d SevereWaiting=%d Inpatient=%d Visits=%d AmbulanceInbound=%d Home=%d Recovered=%lld"),Update,H.LastStep,H.Patients.Num(),MildWaiting,SevereWaiting,Beds,Visits,Inbound,Home,H.TotalRecovered));
  for(const auto& E:S->GetPlacementModel().GetEntities())if(E.Value.DefinitionId==TEXT("Hospital"))
  {
   const auto& Services=S->GetServiceSnapshot();AddInfo(FString::Printf(TEXT("EMERGENCYROUTING_HOSPITAL Id=%llu Capacity=%d Fleet=%d Staff=%d Failure=%d"),E.Key,Services.HospitalCapacities.FindRef(E.Key),Services.HospitalFleetCapacities.FindRef(E.Key),Services.PublicStaffPercent.FindRef(E.Key),static_cast<int32>(Services.GetFailure(E.Key))));
  }
  TestTrue(TEXT("copied continuation saves: ")+Error,S->SaveCityToPath(Output/FString::Printf(TEXT("City-%d.json"),Update),Error));
  if(Reloaded)
  {
   FString Json,A,B;FCitySaveDocument DA;
   FFileHelper::LoadFileToString(Json,*(Output/FString::Printf(TEXT("City-%d.json"),Update)));
   if(TestTrue(TEXT("continuation document reads: ")+Error,FCityPersistence::Deserialize(Json,DA,Error)))
   {
    auto DB=DA;DB.Population=R->GetPopulationSnapshot();DB.Health=R->GetHealthSnapshot();DB.Traffic=R->GetTrafficSnapshot();
    TestTrue(TEXT("reloaded state serializes"),FCityPersistence::Serialize(DA,A,Error)&&FCityPersistence::Serialize(DB,B,Error));
    TestEqual(TEXT("population,health and traffic continuation match reload"),A,B);
   }
  }
 };
 Record(0);for(int32 I=1;I<=450;++I)
 {
  S->Tick(2.4f);if(Reloaded)R->Tick(2.4f);if(I%75==0)Record(I);
  if(I==150){Reloaded=TestTrue(TEXT("actual current-city traffic/health reload: ")+Error,R->LoadCityFromPath(Output/TEXT("City-150.json"),Error,false));if(Reloaded){R->SetSimulationPaused(false);R->SetSimulationSpeed(1);}}
 }
 TestTrue(TEXT("mid-queue reload exercised"),Reloaded);
 for(uint64 Id:OriginalAmbulanceTrips)TestFalse(TEXT("original ambulance finishes instead of permanently blocking its fleet slot"),S->GetPopulationSnapshot().TransitTrips.ContainsByPredicate([&](const auto& Trip){return Trip.Id==Id;}));
 FString After;FFileHelper::LoadFileToString(After,*Path);TestEqual(TEXT("captured original unchanged"),After,Original);
 Cleanup();return true;
}
#endif
