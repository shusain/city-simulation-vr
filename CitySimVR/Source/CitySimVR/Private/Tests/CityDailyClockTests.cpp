#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityCalendar.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityReportHistory.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDailyRoutineTest,"CitySim.Calendar.FullDayRoutine",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDailyRoutineTest::RunTest(const FString&)
{
 for(int32 Speed : {1,3,5})
 {
  FCityPlacementModel City(100000);
  auto Road=City.BeginRoadPreview(FCityPlacementModel::EntranceCell);City.UpdateRoadPreviewEnd(Road,{20,8});
  if(!TestTrue(TEXT("routine road"),City.CommitRoad(Road).IsSuccess()))return false;
  auto Place=[&](FName Id,FCityCellCoord Cell){FCityBuildingDefinition D;D.DefinitionId=Id;D.DisplayName=Id.ToString();D.Footprint={1,1};D.Cost=0;auto P=City.BeginPreview(D,Cell);return City.CommitPlacement(P,D).EntityId;};
  const uint64 Home=Place(TEXT("Home"),{2,9}),Work=Place(TEXT("Factory"),{6,9}),Shop=Place(TEXT("Shop"),{10,9});
  FCityPopulationSnapshot Initial;Initial.NextCitizenId=2;Initial.bUsesPurchaseReservations=true;
  FCityCitizenRecord C;C.Id=1;C.HomeEntityId=C.HouseholdId=C.CurrentLocationEntityId=Home;C.WorkplaceEntityId=Work;
  C.PendingShopEntityId=Shop;C.AvailableMoney=50;C.ConsumptionNeed=1;C.NextActionStep=FCityCalendar::NextMorning(0,C.Id,150);Initial.Citizens.Add(C);
  FCityHouseholdRecord H;H.Id=H.HomeEntityId=Home;H.Balance=50;H.ConsumptionOrders=1;Initial.Households.Add(H);
  FCityBusinessLedgerRecord Factory;Factory.EntityId=Work;Factory.bIndustrial=true;Factory.JobCapacity=1;
  FCityBusinessLedgerRecord Retail;Retail.EntityId=Shop;Retail.PendingOrders=10;Retail.Inventory=8;Initial.Businesses={Factory,Retail};
  FCityPurchaseReservation Order;Order.BuyerId=1;Order.HouseholdId=Home;Order.ShopId=Shop;Order.Price=5;Order.Contributions.Add({1,5});Initial.PurchaseReservations.Add(Order);
  FCityPopulationSimulation Population,Reload;FCityTrafficSimulation Traffic,ReloadTraffic;FCityEconomySimulation Clock;FCityEconomyTuning Tuning;
  FCityRoadNetworkSnapshot Network;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),Network);
  FString Error; if(!TestTrue(TEXT("funded routine restores"),Population.RestoreState(Initial,Error))){AddError(Error);return false;}
  Clock.SetSpeedMultiplier(Speed);bool Saved=false;int64 WorkAt=-1,ShopAt=-1,HomeAt=-1;
  for(int32 Frame=0;Frame<1500/Speed;++Frame)
  {
   Clock.Advance(.24,City,Tuning,nullptr,nullptr,nullptr,[&]()
   {
    const int64 Step=Clock.GetSnapshot().FixedStepId;
    for(int32 Run=0;Run<(Saved?2:1);++Run)
    {
     auto& P=Run?Reload:Population;auto& T=Run?ReloadTraffic:Traffic;
     P.AdvanceCitizenSchedules(City,Step,Tuning.FixedStepsPerMonth);T.UpdateTrips(City,nullptr,P.GetSnapshot(),Network,Step);T.AdvanceOneStep(Network,Step);
     for(const auto& V:T.GetSnapshot().Vehicles)if(V.bCompleted)P.CompleteCitizenTrip(V.SourceId,V.OriginEntityId,V.DestinationEntityId,Step,Tuning.FixedStepsPerMonth);
    }
    const auto& Citizen=Population.GetSnapshot().Citizens[0];
    if(Citizen.Schedule==ECityCitizenSchedule::Work && WorkAt<0)WorkAt=Step;
    if(Citizen.Schedule==ECityCitizenSchedule::Shop && ShopAt<0)ShopAt=Step;
    if(ShopAt>=0 && Citizen.Schedule==ECityCitizenSchedule::Home && HomeAt<0)HomeAt=Step;
    if(!Saved && Citizen.Schedule==ECityCitizenSchedule::ToShop)
    {
     FCitySaveDocument D,L;D.Placement=City.ExportState();D.Economy=Clock.GetSnapshot();D.Disasters.LastFixedStepId=Step;D.Population=Population.GetSnapshot();D.Traffic=Traffic.GetSnapshot();FString Json;
     const bool Good=FCityPersistence::Serialize(D,Json,Error)&&FCityPersistence::Deserialize(Json,L,Error)&&Reload.RestoreState(L.Population,Error)&&ReloadTraffic.RestoreState(L.Traffic,Network,Step,Error);
     TestTrue(TEXT("mid-routine current-format save/reload"),Good);if(!Good)AddError(Error);Saved=Good;
    }
    if(Saved)
    {
     const auto& R=Reload.GetSnapshot().Citizens[0];
     TestEqual(TEXT("resumed location"),R.CurrentLocationEntityId,Citizen.CurrentLocationEntityId);TestEqual(TEXT("resumed deadline"),R.NextActionStep,Citizen.NextActionStep);
     TestEqual(TEXT("resumed purchase authority"),Reload.GetSnapshot().TotalArrivalPurchases,Population.GetSnapshot().TotalArrivalPurchases);
     TestEqual(TEXT("resumed movement count"),ReloadTraffic.GetSnapshot().Vehicles.Num(),Traffic.GetSnapshot().Vehicles.Num());
    }
    const auto& P=Population.GetSnapshot();
    TestEqual(TEXT("wallet and shop money conserved"),P.Citizens[0].AvailableMoney+P.Businesses[1].Balance,int64(50));
    TestEqual(TEXT("stock and completed purchases conserved"),P.Businesses[1].Inventory+P.TotalArrivalPurchases,int64(8));
   });
  }
  TestEqual(TEXT("one six-minute day at every speed"),Clock.GetSnapshot().FixedStepId,int64(150));
  TestTrue(TEXT("commute, work, purchase and return finish within day"),WorkAt>0 && ShopAt>WorkAt && HomeAt>ShopAt && HomeAt<150);
  TestEqual(TEXT("one real purchase"),Population.GetSnapshot().TotalArrivalPurchases,int64(1));
  TestEqual(TEXT("home until next morning"),Population.GetSnapshot().Citizens[0].Schedule,ECityCitizenSchedule::Home);
  TestTrue(TEXT("next action is in next day"),Population.GetSnapshot().Citizens[0].NextActionStep>150);
  TestTrue(TEXT("reload exercised"),Saved);
  AddInfo(FString::Printf(TEXT("CALENDAR_ROUTINE Speed=%d Work=%lld Shop=%lld Home=%lld Purchase=%lld DaySteps=150"),Speed,WorkAt,ShopAt,HomeAt,Population.GetSnapshot().TotalArrivalPurchases));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDailySettlementTest,"CitySim.Calendar.DailySettlementAndPause",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDailySettlementTest::RunTest(const FString&)
{
 FCityEconomyTuning T;FCityPlacementModel City(10000);auto Road=City.BeginRoadPreview(FCityPlacementModel::EntranceCell);City.UpdateRoadPreviewEnd(Road,{10,8});City.CommitRoad(Road);
 FCityEconomySimulation E;const auto Funds=City.GetFunds();
 for(int32 I=0;I<149;++I)E.Advance(T.FixedStepSeconds,City,T);
 TestEqual(TEXT("no early daily bill"),E.GetSnapshot().LastBilledMonthId,int64(0));TestEqual(TEXT("funds stable before daily boundary"),City.GetFunds(),Funds);
 E.SetPaused(true);E.Advance(100,City,T);TestEqual(TEXT("pause freezes the common clock"),E.GetSnapshot().FixedStepId,int64(149));
 E.SetPaused(false);E.Advance(T.FixedStepSeconds,City,T);
 TestEqual(TEXT("daily settlement at midnight"),E.GetSnapshot().LastBilledMonthId,int64(1));
 TestEqual(TEXT("daily bills debit exactly once"),City.GetFunds(),Funds-E.GetSnapshot().LastUpkeep);
 TestEqual(TEXT("calendar day advances"),FCityCalendar::DayIndex(150),int64(1));
 TestEqual(TEXT("calendar month waits for25 days"),FCityCalendar::MonthIndex(3749),int64(0));
 TestEqual(TEXT("calendar month boundary"),FCityCalendar::MonthIndex(3750),int64(1));
 FString Error;FCityEconomySimulation R;
 TestTrue(TEXT("clock remainder restores"),R.RestoreState(E.GetSnapshot(),.6,false,5,T,Error));
 TestEqual(TEXT("restored fractional clock"),R.GetFixedStepProgress(T),.25);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDailyReportTest,"CitySim.Calendar.MonthlyReportRollup",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDailyReportTest::RunTest(const FString&)
{
 FCityReportHistory History;FCityEconomySnapshot E;FCityPopulationSnapshot P;FCityServiceSnapshot S;FCityEnvironmentSnapshot Env;
 for(int64 Day=1;Day<=26;++Day){E.LastBilledMonthId=P.LastSettledMonthId=Day;E.LastIncome=10+Day;TestTrue(TEXT("daily observation captured"),History.Capture(E,P,S,Env,100+Day));}
 const auto M=History.Monthly();TestEqual(TEXT("26 daily facts form two months"),M.Num(),2);
 TestEqual(TEXT("first month complete"),M[0].ObservedDays,25);TestEqual(TEXT("second month explicitly partial"),M[1].ObservedDays,1);
 TestEqual(TEXT("monthly cashflow sums actual daily income"),M[0].Get(ECityReportMetric::Income),int64(575));
 TestEqual(TEXT("monthly treasury is closing stock"),M[0].Get(ECityReportMetric::Treasury),int64(125));
 TestEqual(TEXT("annual sums actual flows"),History.Annual()[0].Get(ECityReportMetric::Income),int64(611));
 TestEqual(TEXT("annual coverage does not invent days"),History.Annual()[0].ObservedDays,26);
 TestFalse(TEXT("partial quarter is not complete"),FCityQuarterlyNews::Editions(History)[0].IsComplete());
 FCitySaveDocument D,L;D.Placement.Zones.Init(ECityZoneType::None,FCityGrid::CellCount);D.Economy.LastBilledMonthId=D.Economy.MonthId=26;D.Population.LastSettledMonthId=26;D.Economy.FixedStepId=D.Disasters.LastFixedStepId=26*150;D.Reports=History;
 FString Json,Error;TestTrue(TEXT("daily report facts save"),FCityPersistence::Serialize(D,Json,Error));TestTrue(TEXT("daily report facts reload"),FCityPersistence::Deserialize(Json,L,Error));
 TestEqual(TEXT("reload preserves monthly totals"),L.Reports.Monthly()[0].Get(ECityReportMetric::Income),int64(575));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDailyLightTest,"CitySim.Calendar.DaylightAlignment",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDailyLightTest::RunTest(const FString&)
{
 const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("DailyClockLight"),nullptr,true,ERHIFeatureLevel::Num,&Values);
 auto* A=W->SpawnActor<ACitySandboxEnvironment>();auto* Sun=A->FindComponentByClass<UDirectionalLightComponent>();A->SetLightingPreset(ECityLightingPreset::Automatic);
 A->UpdateLightingClock(0,0,150,false,.01);TestTrue(TEXT("midnight is night"),Sun->Intensity<1);
 A->UpdateLightingClock(75,0,150,false,.01);TestTrue(TEXT("12:00 is daylight"),Sun->Intensity>3.9f);
 const auto Rotation=Sun->GetRelativeRotation();const float Noon=Sun->Intensity;
 for(int32 Speed:{1,3,5}){A->UpdateLightingClock(75,0,150,false,.02f/Speed);TestEqual(TEXT("same game time has same light at each speed"),Sun->Intensity,Noon);TestEqual(TEXT("same sun position at each speed"),Sun->GetRelativeRotation(),Rotation);}
 A->UpdateLightingClock(75,0,150,true,10);TestEqual(TEXT("paused daylight holds"),Sun->Intensity,Noon);
 A->UpdateLightingClock(150,0,150,false,.01);TestTrue(TEXT("next midnight closes the same calendar day"),Sun->Intensity<1);
 W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDailyStationTest,"CitySim.Calendar.StationHalfDay",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDailyStationTest::RunTest(const FString&)
{
 auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
 if(!TestNotNull(TEXT("station catalog"),Catalog))return false;
 FCityPlacementModel City(100000);auto Road=City.BeginRoadPreview({0,8});City.UpdateRoadPreviewEnd(Road,{20,8});City.CommitRoad(Road);
 const auto* Definition=Catalog->FindDefinition(TEXT("Police"));if(!Definition)return false;
 auto Preview=City.BeginPreview(*Definition,{8,9});const uint64 Id=City.CommitPlacement(Preview,*Definition).EntityId;
 FCityDevelopmentSimulation D,R;FCityEconomyTuning T;FCityServiceSnapshot S;FCityNeedsSnapshot N;FCityEnvironmentSnapshot E;FString Error;
 D.Reconcile(City,Catalog,0);TestEqual(TEXT("construction deadline uses fine steps"),D.GetSnapshot().Find(Id)->ConstructionCompleteStep,int64(75));
 D.AdvanceToStep(74,City,Catalog,S,N,E,T);TestFalse(TEXT("station waits for actual noon"),D.GetSnapshot().IsOperational(Id));
 TestTrue(TEXT("pending half-day construction restores"),R.RestoreState(D.GetSnapshot(),City,Catalog,74,Error,0));
 D.AdvanceToStep(75,City,Catalog,S,N,E,T);TestTrue(TEXT("station completes at noon"),D.GetSnapshot().IsOperational(Id));
 TestTrue(TEXT("upgrade starts at noon"),D.PurchaseCivicUpgrade(Id,ECityDensityLevel::Low,City,Catalog,S,Error,1));
 TestEqual(TEXT("upgrade deadline is midnight"),D.GetSnapshot().Find(Id)->CivicUpgradeCompleteStep,int64(150));
 D.AdvanceToStep(149,City,Catalog,S,N,E,T);TestTrue(TEXT("no early upgrade"),D.GetSnapshot().Find(Id)->HasCivicUpgrade());
 TestTrue(TEXT("pending upgrade restores"),R.RestoreState(D.GetSnapshot(),City,Catalog,149,Error,1));
 D.AdvanceToStep(150,City,Catalog,S,N,E,T);TestEqual(TEXT("upgrade at calendar midnight"),D.GetSnapshot().Find(Id)->DensityLevel,ECityDensityLevel::Medium);
 return true;
}
#endif
