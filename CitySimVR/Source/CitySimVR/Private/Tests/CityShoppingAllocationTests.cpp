#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityConditionIndicators.h"
#include "Persistence/CityPersistence.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShoppingAllocationLedgerTest, "CitySim.Shopping.LocksQuotaRoutesAndArrival",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityShoppingAllocationLedgerTest::RunTest(const FString&)
{
 FCityPlacementModel City(10000);
 auto Road=City.BeginRoadPreview(FCityPlacementModel::EntranceCell); City.UpdateRoadPreviewEnd(Road,{24,8});
 if(!TestTrue(TEXT("real pedestrian road"),City.CommitRoad(Road).IsSuccess()))return false;
 auto Place=[&](FName Name,FCityCellCoord Cell){FCityBuildingDefinition D;D.DefinitionId=Name;D.DisplayName=Name.ToString();D.Footprint={1,1};D.Cost=0;auto P=City.BeginPreview(D,Cell);return City.CommitPlacement(P,D).EntityId;};
 const uint64 Disconnected=Place(TEXT("Shop"),{18,9}),Home=Place(TEXT("Home"),{2,9}),Near=Place(TEXT("Shop"),{8,9}),Tie=Place(TEXT("Shop"),{8,7});
 TestTrue(TEXT("disconnect first business"),City.RemoveRoad({14,8}));
 FCityRoadNetworkSnapshot Network;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),Network);
 FCityPopulationSnapshot State;State.NextCitizenId=4;State.bUsesPurchaseReservations=true;
 FCityHouseholdRecord H;H.Id=H.HomeEntityId=Home;H.ConsumptionOrders=2;State.Households.Add(H);
 for(uint64 Id=1;Id<=3;++Id){FCityCitizenRecord C;C.Id=Id;C.HouseholdId=C.HomeEntityId=C.CurrentLocationEntityId=Home;C.UnemployedSinceStep=0;C.ConsumptionNeed=Id<3?1:0;C.NextActionStep=190;State.Citizens.Add(C);}
 for(uint64 Id:{Disconnected,Near,Tie}){FCityBusinessLedgerRecord B;B.EntityId=Id;B.PendingOrders=10;B.Inventory=10;State.Businesses.Add(B);}
 FCityEconomyTuning Tuning;FCityPopulationSimulation P;FString Error;
 auto Restore=[&](){Error.Reset();const bool OK=P.RestoreState(State,Error);TestTrue(TEXT("valid allocation fixture: ")+Error,OK);return OK;};
 if(!Restore())return false;
 P.RetryPurchaseReservations(City,nullptr,Network,75,Tuning);
 TestTrue(TEXT("no cash cannot lock stock"),P.GetSnapshot().PurchaseReservations.IsEmpty());
 State.Citizens[0].AvailableMoney=State.Households[0].Balance=5;
 State.Businesses[1].Inventory=State.Businesses[2].Inventory=0;
 if(!Restore())return false;
 P.RetryPurchaseReservations(City,nullptr,Network,76,Tuning);
 TestTrue(TEXT("unreachable stocked first business is skipped"),P.GetSnapshot().PurchaseReservations.IsEmpty());
 State.Businesses[1].Inventory=1;
 if(!Restore())return false;
 P.RetryPurchaseReservations(City,nullptr,Network,77,Tuning);
 TestEqual(TEXT("midday restock allocates without settlement"),P.GetSnapshot().PurchaseReservations.Num(),1);
 if(P.GetSnapshot().PurchaseReservations.IsEmpty())return false;
 TestEqual(TEXT("reachable stock chosen"),P.GetSnapshot().PurchaseReservations[0].ShopId,Near);
 TestEqual(TEXT("cash held rather than charged"),P.GetSnapshot().Citizens[0].AvailableMoney,int64(5));
 TestEqual(TEXT("stock held rather than consumed"),P.GetSnapshot().Businesses[1].Inventory,1);
 P.RetryPurchaseReservations(City,nullptr,Network,78,Tuning);
 TestEqual(TEXT("finite cash and stock cannot be locked twice"),P.GetSnapshot().PurchaseReservations.Num(),1);
 // Fresh finite daily capacity, plus already completed purchases, constrain retries.
 State.Citizens[0].AvailableMoney=State.Households[0].Balance=100;
 State.Businesses[1].Inventory=State.Businesses[2].Inventory=10;
 State.Businesses[1].PendingOrders=1;State.Businesses[1].SalesThisMonth=1;
 State.Businesses[2].PendingOrders=1;State.Businesses[2].SalesThisMonth=1;
 if(!Restore())return false;
 P.RetryPurchaseReservations(City,nullptr,Network,79,Tuning);
 TestTrue(TEXT("completed sales consume today's shop capacity"),P.GetSnapshot().PurchaseReservations.IsEmpty());
 State.Businesses[1].PendingOrders=State.Businesses[2].PendingOrders=10;
 State.Businesses[1].SalesThisMonth=State.Businesses[2].SalesThisMonth=0;
 if(!Restore())return false;
 FCityTrafficSimulation Traffic,ReloadTraffic;FCityPopulationSimulation Reload;
 FCityTrafficSnapshot Empty;Empty.FixedStepId=79;
 if(!TestTrue(TEXT("traffic shares the midday clock"),Traffic.RestoreState(Empty,Network,79,Error)))return false;
 bool Saved=false;
 for(int64 Step=80;Step<150;++Step)
 {
  for(int32 Run=0;Run<(Saved?2:1);++Run)
  {
   auto& People=Run?Reload:P;auto& Movement=Run?ReloadTraffic:Traffic;
   People.RetryPurchaseReservations(City,nullptr,Network,Step,Tuning);
   const auto& PS=People.GetSnapshot();
   TestTrue(TEXT("daily completed plus held quota"),PS.Households[0].PurchasesThisMonth+PS.PurchaseReservations.Num()<=2);
   for(const auto& O:PS.PurchaseReservations)TestEqual(TEXT("nearest reachable stable tie"),O.ShopId,Near);
   People.AdvanceCitizenSchedules(City,Step,150);Movement.UpdateTrips(City,nullptr,People.GetSnapshot(),Network,Step);Movement.AdvanceOneStep(Network,Step);
   for(const auto& V:Movement.GetSnapshot().Vehicles)if(V.bCompleted){People.CompleteCitizenTrip(V.SourceId,V.OriginEntityId,V.DestinationEntityId,Step,150);People.CompleteCitizenTrip(V.SourceId,V.OriginEntityId,V.DestinationEntityId,Step,150);}
   const auto& After=People.GetSnapshot();int64 Money=0,Stock=0;
   for(const auto& C:After.Citizens)Money+=C.AvailableMoney;
   for(const auto& B:After.Businesses){Money+=B.Balance;Stock+=B.Inventory;}
   TestEqual(TEXT("finite money conserved"),Money,int64(100));
   TestEqual(TEXT("goods only consumed by actual arrivals"),Stock+After.TotalArrivalPurchases,int64(30));
  }
  if(!Saved && !Traffic.GetSnapshot().Vehicles.IsEmpty())
  {
   FCitySaveDocument D,L;D.Placement=City.ExportState();D.Economy.FixedStepId=D.Disasters.LastFixedStepId=Step;D.Population=P.GetSnapshot();D.Traffic=Traffic.GetSnapshot();FString Json;
   Saved=FCityPersistence::Serialize(D,Json,Error)&&FCityPersistence::Deserialize(Json,L,Error)&&Reload.RestoreState(L.Population,Error)&&ReloadTraffic.RestoreState(L.Traffic,Network,Step,Error);
   TestTrue(TEXT("midday held order and moving purchase reload: ")+Error,Saved);
  }
  if(Saved){TestEqual(TEXT("reloaded actual purchases"),Reload.GetSnapshot().TotalArrivalPurchases,P.GetSnapshot().TotalArrivalPurchases);TestEqual(TEXT("reloaded quota and locks"),Reload.GetSnapshot().PurchaseReservations.Num(),P.GetSnapshot().PurchaseReservations.Num());TestEqual(TEXT("reloaded movement"),ReloadTraffic.GetSnapshot().Vehicles.Num(),Traffic.GetSnapshot().Vehicles.Num());}
 }
 TestTrue(TEXT("in-flight reload exercised"),Saved);
 TestEqual(TEXT("two real daily purchases, no repeated overshopping"),P.GetSnapshot().TotalArrivalPurchases,int64(2));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShoppingAllocationIntegrationTest, "CitySim.Shopping.MiddaySubsystemAndReportedCity",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityShoppingAllocationIntegrationTest::RunTest(const FString&)
{
 FString Path,Output,Before,Error;
 if(!FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationSave="),Path)){AddInfo(TEXT("Supply ShoppingAllocationSave/Output for immutable City837230 regression"));return true;}
 if(!FParse::Value(FCommandLine::Get(),TEXT("ShoppingAllocationOutput="),Output)||!FFileHelper::LoadFileToString(Before,*Path)){AddError(TEXT("Requires input and separate output"));return false;}
 IFileManager::Get().MakeDirectory(*Output,true);
 const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto MakeWorld=[&](FName Name){return UWorld::CreateWorld(EWorldType::Game,false,Name,nullptr,true,ERHIFeatureLevel::Num,&Values);};
 auto* AWorld=MakeWorld(TEXT("AllocationCity"));auto* BWorld=MakeWorld(TEXT("AllocationReload"));auto* MWorld=MakeWorld(TEXT("AllocationRestock"));
 auto* A=AWorld->GetSubsystem<UCitySimulationSubsystem>();auto* B=BWorld->GetSubsystem<UCitySimulationSubsystem>();auto* M=MWorld->GetSubsystem<UCitySimulationSubsystem>();
 auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
 bool Good=true;for(auto* Sim:{A,B,M}){Sim->RegisterBuildingCatalog(Catalog);Good &= Sim->LoadCityFromPath(Path,Error,false);Sim->SetSimulationSpeed(1);Sim->SetSimulationPaused(false);}
 if(!TestTrue(TEXT("all copies of current city load: ")+Error,Good)){AWorld->DestroyWorld(false);BWorld->DestroyWorld(false);MWorld->DestroyWorld(false);return false;}
 TestEqual(TEXT("starter pantry covers essentials without same-day shopping"),A->GetNeedsSnapshot().UnmetGoodsDemand,0);
 // Full-subsystem fixture: later stock and funds become available after midnight's
 // allocation. Empty traffic isolates allocation/arrival from unrelated congestion.
 auto Mid=M->GetPopulationSnapshot();Mid.PurchaseReservations.Reset();Mid.TransitTrips.Reset();
 for(auto& C:Mid.Citizens){C.AvailableMoney=0;C.PendingShopEntityId=0;C.TripPurpose=ECityTripPurpose::None;C.TripDestinationEntityId=0;C.Schedule=ECityCitizenSchedule::Home;C.CurrentLocationEntityId=C.HomeEntityId;C.NextActionStep=1000;}
 for(auto& Hh:Mid.Households)Hh.Balance=0;
 for(auto& Shop:Mid.Businesses){Shop.Inventory=0;Shop.bInitialFreightReceived=true;}
 TestTrue(TEXT("midday fixture restored"),M->Population.RestoreState(Mid,Error));
 FCityTrafficSnapshot Empty;Empty.FixedStepId=M->GetEconomySnapshot().FixedStepId;
 TestTrue(TEXT("midday traffic cleared in isolated copy"),M->Traffic.RestoreState(Empty,M->GetRoadNetworkSnapshot(),Empty.FixedStepId,Error));
 M->Tick(2.4f);TestTrue(TEXT("no order without money or stock"),M->GetPopulationSnapshot().PurchaseReservations.IsEmpty());
 Mid=M->GetPopulationSnapshot();
 auto* Buyer=Mid.Citizens.FindByPredicate([](const auto& C){return C.Id==111;});
 auto* Home=Mid.Households.FindByPredicate([](const auto& Hh){return Hh.Id==26;});
 auto* Shop=Mid.Businesses.FindByPredicate([](const auto& S){return S.EntityId==6;});
 if(!TestTrue(TEXT("City837230 exact home, citizen and shop exist"),Buyer && Home && Shop)){AWorld->DestroyWorld(false);BWorld->DestroyWorld(false);MWorld->DestroyWorld(false);return false;}
 Buyer->AvailableMoney=Home->Balance=5;Home->SupplyUnits=0;Shop->Inventory=1;
 TestTrue(TEXT("later funds/restock restored"),M->Population.RestoreState(Mid,Error));
 const int64 StartPurchases=Mid.TotalArrivalPurchases;bool SawOrder=false,SawMoving=false;
 for(int32 I=0;I<60;++I){M->Tick(2.4f);const auto& People=M->GetPopulationSnapshot();SawOrder|=People.PurchaseReservations.ContainsByPredicate([](const auto& O){return O.HouseholdId==26;});SawMoving|=M->GetTrafficSnapshot().Vehicles.ContainsByPredicate([](const auto& V){return !V.bTransitTrip && V.SourceId==111 && V.DestinationEntityId==6;});}
 TestTrue(TEXT("subsystem allocates later supply before midnight"),SawOrder);
 TestTrue(TEXT("home shopper wakes and actually travels"),SawMoving);
 TestEqual(TEXT("one real arrival settles finite funds and goods"),M->GetPopulationSnapshot().TotalArrivalPurchases,StartPurchases+1);
 TestTrue(TEXT("settlement did not wait for next day"),M->GetEconomySnapshot().FixedStepId<450);
 TestTrue(TEXT("save full-subsystem restock result"),M->SaveCityToPath(Output/TEXT("Midday.json"),Error));
 bool Reloaded=false,SawWork=false,SawTravel=false;int64 Home26Purchases=0;
 for(int32 I=1;I<=400;++I)
 {
  A->Tick(2.4f);if(Reloaded)B->Tick(2.4f);
  const auto& People=A->GetPopulationSnapshot();
  TMap<uint64,int32> Held;for(const auto& O:People.PurchaseReservations)if(!O.ComfortTier)Held.FindOrAdd(O.HouseholdId)+=O.Quantity;
  for(const auto& Hh:People.Households)TestTrue(TEXT("reported household finite supply quota"),Hh.bUsesStoredSupplies ? Hh.SupplyUnits+Held.FindRef(Hh.Id)<=Hh.ConsumptionOrders*8 : Hh.PurchasesThisMonth+Held.FindRef(Hh.Id)<=Hh.ConsumptionOrders);
  for(const auto& Hh:People.Households){const FString Why=FCityConditionIndicators::Explain(*A,Hh.HomeEntityId);SawWork|=Why.Contains(TEXT("Buyer at work"))||Why.Contains(TEXT("commuting to work"));SawTravel|=Why.Contains(TEXT("Shopper travelling"));if(Hh.Id==26)Home26Purchases=FMath::Max<int64>(Home26Purchases,Hh.PurchasesThisMonth);}
  if(Reloaded)
  {
   FCitySaveDocument DA,DB;DA.Placement=DB.Placement=A->GetPlacementModel().ExportState();DA.Economy=DB.Economy=A->GetEconomySnapshot();DA.Disasters.LastFixedStepId=DB.Disasters.LastFixedStepId=DA.Economy.FixedStepId;DA.Population=People;DB.Population=B->GetPopulationSnapshot();DA.Traffic=A->GetTrafficSnapshot();DB.Traffic=B->GetTrafficSnapshot();
   FString AJ,BJ;const bool Serialized=FCityPersistence::Serialize(DA,AJ,Error)&&FCityPersistence::Serialize(DB,BJ,Error);
   if(!TestTrue(TEXT("continuation states serialize: ")+Error,Serialized)||!TestTrue(TEXT("every-step entire population/traffic equals reloaded authority"),AJ==BJ))break;
  }
  if(I==4){Good=A->SaveCityToPath(Output/TEXT("Reload.json"),Error)&&B->LoadCityFromPath(Output/TEXT("Reload.json"),Error,false);Reloaded=TestTrue(TEXT("copy reload: ")+Error,Good);B->SetSimulationSpeed(1);B->SetSimulationPaused(false);}
 }
 TestTrue(TEXT("reported city completes actual shopping"),A->GetPopulationSnapshot().TotalArrivalPurchases>0);
 TestTrue(TEXT("previously unfulfilled home26 now buys"),Home26Purchases>0);
 TestTrue(TEXT("inspection distinguishes work/commute"),SawWork);TestTrue(TEXT("inspection distinguishes real shopping travel"),SawTravel);TestTrue(TEXT("copied-city reload exercised"),Reloaded);
 AddInfo(FString::Printf(TEXT("SHOPPING_ALLOCATION City837230 Steps=400 Purchases=%lld Home26MaxToday=%lld WorkStatus=%d TravelStatus=%d"),A->GetPopulationSnapshot().TotalArrivalPurchases,Home26Purchases,SawWork,SawTravel));
 TestTrue(TEXT("save continued city"),A->SaveCityToPath(Output/TEXT("After.json"),Error));
 FString After;FFileHelper::LoadFileToString(After,*Path);TestEqual(TEXT("captured input untouched"),After,Before);
 AWorld->DestroyWorld(false);BWorld->DestroyWorld(false);MWorld->DestroyWorld(false);return true;
}
#endif
