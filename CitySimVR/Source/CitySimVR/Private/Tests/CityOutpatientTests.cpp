#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOutpatientPoolsTest,"CitySim.Outpatients.IndependentBedsAndVisitCapacity",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityOutpatientPoolsTest::RunTest(const FString&)
{
 FCityHealthSimulation H;FCityHealthContext C;C.Hospitals.Add({10,1,true});
 for(uint64 I=1;I<=4;++I){C.Citizens.Add({I,0,false});H.StartIllness(I,I%2?ECityIllness::Mild:ECityIllness::Severe);}
 TestTrue(TEXT("one clinic place"),H.ReserveCare(1,10,ECityHealthCare::WalkIn,8,C));
 TestTrue(TEXT("severe bed remains available while clinic full"),H.ReserveCare(2,10,ECityHealthCare::AwaitingAmbulance,0,C));
 TestFalse(TEXT("clinic queue finite"),H.ReserveCare(3,10,ECityHealthCare::DriveIn,9,C));
 TestFalse(TEXT("ward queue finite"),H.ReserveCare(4,10,ECityHealthCare::AwaitingAmbulance,0,C));
 TestFalse(TEXT("no new remote course without actual visit"),H.ReserveCare(3,10,ECityHealthCare::HomeRecovery,0,C));
 TestTrue(TEXT("visit arrival dispenses"),H.ConfirmArrival(1,10));
 TestTrue(TEXT("dispensed patient releases clinic while severe bed remains full"),H.ReserveCare(3,10,ECityHealthCare::DriveIn,9,C));
 TestFalse(TEXT("mild patient never has a bed"),H.GetSnapshot().Find(1)->UsesBed());
 TestFalse(TEXT("mild case cannot call an ambulance"),H.ReserveCare(1,10,ECityHealthCare::AwaitingAmbulance,0,C));
 auto Bad=H.GetSnapshot();Bad.Patients[0].Care=ECityHealthCare::Hospital;FString Error;
 TestFalse(TEXT("malformed mild inpatient rejects restore"),H.RestoreState(Bad,Error));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOutpatientReturnTest,"CitySim.Outpatients.BlockedReturnAndDispensedMedicine",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityOutpatientReturnTest::RunTest(const FString&)
{
 FCityPlacementModel City(100000);auto Road=City.BeginRoadPreview(City.GetEntranceCell());City.UpdateRoadPreviewEnd(Road,{20,8});City.CommitRoad(Road);
 FCityBuildingDefinition D;D.DefinitionId=TEXT("Home");D.DisplayName=TEXT("Home");D.Footprint={1,1};D.Cost=0;
 auto Preview=City.BeginPreview(D,{4,9});const uint64 Home=City.CommitPlacement(Preview,D).EntityId;
 D.DefinitionId=TEXT("Hospital");Preview=City.BeginPreview(D,{12,9});const uint64 Hospital=City.CommitPlacement(Preview,D).EntityId;
 FCityPopulationSnapshot S;S.NextCitizenId=2;FCityCitizenRecord Person;Person.Id=1;Person.HomeEntityId=Person.HouseholdId=Person.CurrentLocationEntityId=Home;Person.UnemployedSinceStep=0;S.Citizens.Add(Person);
 FCityHouseholdRecord Household;Household.Id=Household.HomeEntityId=Home;S.Households.Add(Household);
 FCityPopulationSimulation People;FString Error;TestTrue(TEXT("patient population restores"),People.RestoreState(S,Error));
 FCityHealthSimulation Health;Health.StartIllness(1,ECityIllness::Mild);FCityServiceSnapshot Services;Services.HospitalCapacities.Add(Hospital,1);
 FCityRoadNetworkSnapshot Network;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),Network);const auto Connected=Network;FCityTrafficSimulation Traffic;int64 Step=0;
 auto Advance=[&](){++Step;People.UpdateHospitalCare(Health,City,nullptr,Network,Services,Step,25,1,false,true);Traffic.UpdateTrips(City,nullptr,People.GetSnapshot(),Network,Step);Traffic.AdvanceOneStep(Network,Step);for(const auto& V:Traffic.GetSnapshot().Vehicles)if(V.bTransitTrip&&V.bCompleted)People.CompleteTransitTrip(V.SourceId,Step);People.UpdateHospitalCare(Health,City,nullptr,Network,Services,Step,25,1,false,false);Traffic.UpdateTrips(City,nullptr,People.GetSnapshot(),Network,Step);};
 while(Step<120 && Health.GetSnapshot().Find(1)->Care!=ECityHealthCare::ReturningHome)Advance();
 if(!TestTrue(TEXT("actual visit reaches homeward leg without ambulance"),Health.GetSnapshot().Find(1)->Care==ECityHealthCare::ReturningHome))return false;
 Network=FCityRoadNetworkSnapshot();for(int32 I=0;I<30;++I)Advance();
 TestTrue(TEXT("blocked homeward journey remains pending"),Health.GetSnapshot().Find(1)->Care==ECityHealthCare::ReturningHome);
 TestEqual(TEXT("blocked return grants no healing"),Health.GetSnapshot().Find(1)->TreatmentSteps,int64(0));
 TestFalse(TEXT("blocked return uses no clinic or bed"),Health.GetSnapshot().Find(1)->UsesBed()||Health.GetSnapshot().Find(1)->UsesOutpatientPlace());
 Network=Connected;Services.HospitalCapacities.Add(Hospital,0);
 while(Step<250 && Health.GetSnapshot().Find(1) && Health.GetSnapshot().Find(1)->Care!=ECityHealthCare::HomeRecovery)Advance();
 if(!TestTrue(TEXT("medicine still returns home after clinic closes"),Health.GetSnapshot().Find(1)&&Health.GetSnapshot().Find(1)->Care==ECityHealthCare::HomeRecovery))return false;
 TestEqual(TEXT("actual home arrival updates citizen location"),People.GetSnapshot().Citizens[0].CurrentLocationEntityId,Home);
 for(int32 I=0;I<26;++I)Advance();TestEqual(TEXT("dispensed course heals at home despite clinic outage"),Health.GetSnapshot().TotalRecovered,int64(1));
 TestFalse(TEXT("leave ends after home recovery"),People.GetSnapshot().Citizens[0].bOnMedicalLeave);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOutpatientIntegrationTest,"CitySim.Outpatients.CurrentCityTravelHomeAndReload",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityOutpatientIntegrationTest::RunTest(const FString&)
{
 FString Path,Output,Original,Error;
 if(!FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationSave="),Path)||!FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationOutput="),Output)||!FFileHelper::LoadFileToString(Original,*Path)){AddError(TEXT("Requires immutable City837230 input and isolated output"));return false;}
 IFileManager::Get().MakeDirectory(*Output,true);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* WA=UWorld::CreateWorld(EWorldType::Game,false,TEXT("OutpatientCity"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* WB=UWorld::CreateWorld(EWorldType::Game,false,TEXT("OutpatientReload"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto* A=WA->GetSubsystem<UCitySimulationSubsystem>();auto* B=WB->GetSubsystem<UCitySimulationSubsystem>();
 auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
 for(auto* Sim:{A,B})Sim->RegisterBuildingCatalog(Catalog);
 auto Cleanup=[&](){WA->DestroyWorld(false);WB->DestroyWorld(false);};
 if(!TestTrue(TEXT("current city imports: ")+Error,A->LoadCityFromPath(Path,Error,false))){Cleanup();return false;}
 auto& City=A->GetPlacementModel();City.ApplyMonthlyEconomy(100000,0);
 const auto* Definition=Catalog->FindDefinition(TEXT("Hospital"));uint64 Hospital=0;
 for(int32 Y=0;Y<32 && !Hospital;++Y)for(int32 X=0;X<32 && !Hospital;++X)
 {
  auto Preview=City.BeginPreview(*Definition,{X,Y});TArray<FCityCellCoord> Roads;City.GetAdjacentRoadCells({X,Y},Definition->Footprint.GetRotatedSize(Preview.QuarterTurnsClockwise),true,Roads);
  if(Roads.IsEmpty())continue;
  for(int32 Rotation=0;Rotation<4 && !Hospital;++Rotation)
  {
   Preview=City.BeginPreview(*Definition,{X,Y});Preview.QuarterTurnsClockwise=Rotation;Preview.bRotationOverridden=true;
   const auto Result=City.CommitPlacement(Preview,*Definition);if(!Result.IsSuccess())continue;
   FCityCellCoord Portal;TMap<FCityCellCoord,int32> Reach;
   if(FCityTrafficSimulation::FindPortal(City,Catalog,Result.EntityId,Portal))A->GetRoadNetworkSnapshot().FindWalkingDistances(Portal,Reach);
   if(!Reach.IsEmpty())Hospital=Result.EntityId;else City.DemolishEntity(Result.EntityId);
  }
 }
 if(!TestTrue(TEXT("actual hospital added to isolated city"),Hospital!=0)){Cleanup();return false;}
 auto People=A->GetPopulationSnapshot();People.PurchaseReservations.Reset();People.TransitTrips.Reset();
 for(auto& C:People.Citizens){C.Education=ECityEducationLevel::Secondary;C.PendingShopEntityId=0;C.TripPurpose=ECityTripPurpose::None;C.TripDestinationEntityId=0;C.Schedule=ECityCitizenSchedule::Home;C.Activity=ECityCitizenActivity::Home;C.CurrentLocationEntityId=C.HomeEntityId;C.NextActionStep=100000;C.bShopBeforeWork=false;}
 TestTrue(TEXT("educated fixture population restores"),A->Population.RestoreState(People,Error));
 FCityTrafficSnapshot Empty;Empty.FixedStepId=A->GetEconomySnapshot().FixedStepId;
 TestTrue(TEXT("isolated traffic clears"),A->Traffic.RestoreState(Empty,A->GetRoadNetworkSnapshot(),Empty.FixedStepId,Error));
 A->Development.InitializeExistingAsOperational(City,Catalog,Empty.FixedStepId);A->RefreshDerivedSnapshots();
 A->SetDisasterFrequency(ECityDisasterFrequency::Off);A->SetSimulationSpeed(1);A->SetSimulationPaused(false);
 for(int32 I=0;I<4;++I)A->Tick(2.4f);
 if(!TestTrue(TEXT("real staff/utilities/funding provide operating hospital"),A->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital)>0)){Cleanup();return false;}
 uint64 Near=0,Far=0,Severe=0;int32 NearDistance=MAX_int32,FarDistance=0;FCityCellCoord End;
 FCityTrafficSimulation::FindPortal(City,Catalog,Hospital,End);
 for(const auto& C:A->GetPopulationSnapshot().Citizens)
 {
  if(C.WorkplaceEntityId==Hospital||C.IsDetained())continue;FCityCellCoord Start;TArray<FCityCellCoord> Route;
  if(!FCityTrafficSimulation::FindPortal(City,Catalog,C.HomeEntityId,Start)||!A->GetRoadNetworkSnapshot().FindRoute(Start,End,ECityRoadAccess::Pedestrian,Route))continue;
  const int32 Distance=Route.Num()-1;
  if(Distance<NearDistance){NearDistance=Distance;Near=C.Id;}
  if(Distance>FarDistance){FarDistance=Distance;Far=C.Id;}
 }
 for(const auto& C:A->GetPopulationSnapshot().Citizens)if(C.Id!=Near&&C.Id!=Far&&C.WorkplaceEntityId!=Hospital&&!C.IsDetained()){Severe=C.Id;break;}
 if(!TestTrue(TEXT("native fixture spans near walk and farther drive"),Near && Far && Severe && NearDistance<=8 && FarDistance>8)){AddInfo(FString::Printf(TEXT("Near=%d Far=%d"),NearDistance,FarDistance));Cleanup();return false;}
 // Exercise the immediately preceding city's real care semantics without editing any user file.
 auto Legacy=A->BuildSaveDocument();for(auto& C:Legacy.Population.Citizens)if(C.Id==Far)C.CurrentLocationEntityId=Hospital;
 FString LegacyJson;TSharedPtr<FJsonObject> Root;
 TestTrue(TEXT("prior-care base serializes"),FCityPersistence::Serialize(Legacy,LegacyJson,Error));
 FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LegacyJson),Root);
 Root->SetNumberField(TEXT("schemaVersion"),69);TArray<TSharedPtr<FJsonValue>> Patients;
 for(uint64 Id:{Near,Far})
 {
  auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("citizen"),FString::Printf(TEXT("%llu"),Id));P->SetStringField(TEXT("hospital"),FString::Printf(TEXT("%llu"),Hospital));
  P->SetNumberField(TEXT("illness"),0);P->SetNumberField(TEXT("care"),Id==Near?1:4);P->SetNumberField(TEXT("risk"),0);
  P->SetStringField(TEXT("onset"),FString::Printf(TEXT("%lld"),Legacy.Health.LastStep-10));P->SetStringField(TEXT("treatment"),TEXT("5"));P->SetStringField(TEXT("treatedAt"),FString::Printf(TEXT("%lld"),Legacy.Health.LastStep));Patients.Add(MakeShared<FJsonValueObject>(P));
 }
 Root->GetObjectField(TEXT("health"))->SetArrayField(TEXT("patients"),Patients);FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&LegacyJson));
 const FString Prior=Output/TEXT("Prior69.json");FFileHelper::SaveStringToFile(LegacyJson,*Prior);
 if(TestTrue(TEXT("native previous69 care import: ")+Error,B->LoadCityFromPath(Prior,Error,false)))
 {
  TestTrue(TEXT("old remote case requires actual clinic visit"),B->GetHealthSnapshot().Find(Near)->Care==ECityHealthCare::None);
  TestTrue(TEXT("old mild inpatient releases bed and returns home"),B->GetHealthSnapshot().Find(Far)->Care==ECityHealthCare::ReturningHome);
  TestEqual(TEXT("old actual inpatient progress retained"),B->GetHealthSnapshot().Find(Far)->TreatmentSteps,int64(5));
  FString Unchanged;FFileHelper::LoadFileToString(Unchanged,*Prior);TestEqual(TEXT("69 import never overwrites original"),Unchanged,LegacyJson);
 }
 for(uint64 Id:{Near,Far,Severe})TestTrue(TEXT("actual illness begins"),A->Health.StartIllness(Id,Id==Severe?ECityIllness::Severe:ECityIllness::Mild));
 A->Population.SyncHealthAvailability(A->GetHealthSnapshot());A->RefreshDerivedSnapshots();A->Tick(2.4f);
 TestTrue(TEXT("near native patient walks"),A->GetHealthSnapshot().Find(Near)->Care==ECityHealthCare::WalkIn);
 TestTrue(TEXT("far native patient drives"),A->GetHealthSnapshot().Find(Far)->Care==ECityHealthCare::DriveIn);
 TestTrue(TEXT("severe native patient uses ambulance"),A->GetHealthSnapshot().Find(Severe)->Care==ECityHealthCare::AwaitingAmbulance);
 TestTrue(TEXT("outward fixture saves"),A->SaveCityToPath(Output/TEXT("Outward.json"),Error));
 bool Reloaded=TestTrue(TEXT("outward journey reloads natively: ")+Error,B->LoadCityFromPath(Output/TEXT("Outward.json"),Error,false));
 B->SetSimulationSpeed(1);B->SetSimulationPaused(false);
 bool Returned=false,Home=false,SavedReturn=false;int32 Updates=0;
 for(;Updates<650 && A->GetHealthSnapshot().TotalRecovered<3;++Updates)
 {
  A->Tick(2.4f);if(Reloaded)B->Tick(2.4f);
  for(const auto& P:A->GetHealthSnapshot().Patients)if(P.Illness==ECityIllness::Mild)
  {
   TestFalse(TEXT("mild never uses inpatient bed"),P.UsesBed());
   if(P.Care==ECityHealthCare::ReturningHome){Returned=true;if(!SavedReturn){SavedReturn=true;TestTrue(TEXT("return fixture saves"),A->SaveCityToPath(Output/TEXT("Returning.json"),Error));TestTrue(TEXT("native return reload: ")+Error,B->LoadCityFromPath(Output/TEXT("Returning.json"),Error,false));B->SetSimulationSpeed(1);B->SetSimulationPaused(false);Reloaded=true;}}
   if(P.Care==ECityHealthCare::HomeRecovery){if(!Home){TestTrue(TEXT("home-care fixture saves"),A->SaveCityToPath(Output/TEXT("Home.json"),Error));TestTrue(TEXT("native home-care reload: ")+Error,B->LoadCityFromPath(Output/TEXT("Home.json"),Error,false));B->SetSimulationSpeed(1);B->SetSimulationPaused(false);}Home=true;TestEqual(TEXT("home recovery holds no hospital capacity"),P.HospitalId,uint64(0));}
   else TestEqual(TEXT("no healing before actual home arrival"),P.TreatmentSteps,int64(0));
  }
  if(Reloaded)
  {
   auto DA=A->BuildSaveDocument();auto DB=DA;DB.Population=B->GetPopulationSnapshot();DB.Traffic=B->GetTrafficSnapshot();DB.Health=B->GetHealthSnapshot();FString JA,JB;
   if(!TestTrue(TEXT("medical continuation serializes: ")+Error,FCityPersistence::Serialize(DA,JA,Error)&&FCityPersistence::Serialize(DB,JB,Error))||!TestEqual(TEXT("every-step population/traffic/health matches return reload"),JA,JB))break;
  }
 }
 TestTrue(TEXT("actual outward, homeward, home-treatment and reload exercised"),Returned&&Home&&Reloaded);
 TestEqual(TEXT("all three native patients recover"),A->GetHealthSnapshot().TotalRecovered,int64(3));
 TestTrue(TEXT("no medical journey remains"),!A->GetPopulationSnapshot().TransitTrips.ContainsByPredicate([](const auto& T){return T.IsHealthJourney();}));
 TestTrue(TEXT("final city saves"),A->SaveCityToPath(Output/TEXT("Recovered.json"),Error));
 FString After;FFileHelper::LoadFileToString(After,*Path);TestEqual(TEXT("original input untouched"),After,Original);
 AddInfo(FString::Printf(TEXT("OUTPATIENT_NATIVE Near=%d Far=%d Updates=%d Recovered=%lld Reload=%d"),NearDistance,FarDistance,Updates,A->GetHealthSnapshot().TotalRecovered,Reloaded));
 Cleanup();return true;
}
#endif
