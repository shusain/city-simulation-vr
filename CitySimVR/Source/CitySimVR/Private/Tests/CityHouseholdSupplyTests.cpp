#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Presentation/CityConditionIndicators.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"

namespace
{
uint64 SupplyLot(FCityPlacementModel& City,FName Name,FCityCellCoord Cell)
{
 FCityBuildingDefinition D;D.DefinitionId=Name;D.DisplayName=Name.ToString();D.Footprint={1,1};D.Cost=0;
 auto P=City.BeginPreview(D,Cell);return City.CommitPlacement(P,D).EntityId;
}
void SupplyRoad(FCityPlacementModel& City)
{
 auto P=City.BeginRoadPreview(FCityPlacementModel::EntranceCell);City.UpdateRoadPreviewEnd(P,{24,8});check(City.CommitRoad(P).IsSuccess());
}
FCityPopulationSnapshot SupplyFamily(uint64 Home,uint64 Shop,int32 Stock=100)
{
 FCityPopulationSnapshot P;P.bUsesPurchaseReservations=true;P.NextCitizenId=2;
 FCityCitizenRecord C;C.Id=1;C.HouseholdId=C.HomeEntityId=C.CurrentLocationEntityId=Home;C.AvailableMoney=100;C.ConsumptionNeed=1;C.UnemployedSinceStep=0;C.NextActionStep=39;P.Citizens.Add(C);
 FCityHouseholdRecord H;H.Id=H.HomeEntityId=Home;H.Balance=100;H.ConsumptionOrders=1;P.Households.Add(H);
 FCityBusinessLedgerRecord B;B.EntityId=Shop;B.Inventory=Stock;B.PendingOrders=10;B.bInitialFreightReceived=true;B.bHasReceivedGoods=Stock>0;P.Businesses.Add(B);return P;
}
void SupplyStep(FCityPopulationSimulation& P,FCityTrafficSimulation& T,const FCityPlacementModel& City,const FCityRoadNetworkSnapshot& N,int64 Step,const FCityEconomyTuning& Tune)
{
 P.RetryPurchaseReservations(City,nullptr,N,Step,Tune);P.AdvanceCitizenSchedules(City,Step,150);T.UpdateTrips(City,nullptr,P.GetSnapshot(),N,Step);T.AdvanceOneStep(N,Step);
 for(const auto& V:T.GetSnapshot().Vehicles)if(V.bCompleted){P.CompleteCitizenTrip(V.SourceId,V.OriginEntityId,V.DestinationEntityId,Step,150);P.CompleteCitizenTrip(V.SourceId,V.OriginEntityId,V.DestinationEntityId,Step,150);}
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWeeklySupplyTest,"CitySim.Supplies.WeeklyBulkGraceAndReload",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityWeeklySupplyTest::RunTest(const FString&)
{
 FCityPlacementModel City(10000);SupplyRoad(City);const auto Home=SupplyLot(City,TEXT("Home"),{2,9}),Shop=SupplyLot(City,TEXT("Shop"),{6,9});
 FCityPopulationSimulation P,R;FString Error;auto Initial=SupplyFamily(Home,Shop);TestTrue(TEXT("initial family"),P.RestoreState(Initial,Error));P.EnableHouseholdSupplies(0);
 TestEqual(TEXT("starter pantry gives three days before first trip"),P.GetSnapshot().Households[0].SupplyUnits,3);
 FCityRoadNetworkSnapshot N;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);FCityTrafficSimulation T,RT;FCityEconomyTuning Tune;
 FCityEconomySnapshot E;E.Residents=1;E.RetailCapacity=10;
 int32 Trips=0;int64 LastGoods=0;TArray<int64> ArrivalSteps;bool Saved=false;
 for(int64 Step=1;Step<=14*150;++Step)
 {
  for(int32 Run=0;Run<(Saved?2:1);++Run)
  {
   auto& People=Run?R:P;auto& Traffic=Run?RT:T;
   if(Step%150==0){E.FixedStepId=Step;E.MonthId=E.LastBilledMonthId=Step/150;People.AdvanceToMonth(City,E,Tune,nullptr,true,true);People.EnableHouseholdSupplies(Step/150);}
   SupplyStep(People,Traffic,City,N,Step,Tune);
   const auto& S=People.GetSnapshot();const auto& H=S.Households[0];int32 Held=0;
   for(const auto& O:S.PurchaseReservations)if(!O.ComfortTier)Held+=O.Quantity;
   TestTrue(TEXT("finite pantry plus held baskets stays within eight-day ceiling"),H.SupplyUnits+Held<=8);
   TestTrue(TEXT("weekly replenishment prevents an essentials warning"),H.SupplyUnits>0);
   TestEqual(TEXT("real wallets plus retailer money conserved"),S.Citizens[0].AvailableMoney+S.Businesses[0].Balance,int64(100));
   TestEqual(TEXT("goods only leave retail on real purchase"),S.Businesses[0].Inventory+S.TotalArrivalPurchases,int64(100));
  }
  if(P.GetSnapshot().TotalArrivalPurchases>LastGoods){++Trips;ArrivalSteps.Add(Step);TestEqual(TEXT("one trip brings seven daily units"),P.GetSnapshot().TotalArrivalPurchases-LastGoods,int64(7));LastGoods=P.GetSnapshot().TotalArrivalPurchases;}
  if(!Saved && !T.GetSnapshot().Vehicles.IsEmpty())
  {
   FCitySaveDocument D,L;D.Placement=City.ExportState();D.Population=P.GetSnapshot();D.Traffic=T.GetSnapshot();D.Economy=E;D.Economy.FixedStepId=D.Disasters.LastFixedStepId=Step;FString Json;
   Saved=FCityPersistence::Serialize(D,Json,Error)&&FCityPersistence::Deserialize(Json,L,Error)&&R.RestoreState(L.Population,Error)&&RT.RestoreState(L.Traffic,N,Step,Error);TestTrue(TEXT("bulk basket, money locks and pantry reload: ")+Error,Saved);
  }
  if(Saved){TestEqual(TEXT("reloaded pantry"),R.GetSnapshot().Households[0].SupplyUnits,P.GetSnapshot().Households[0].SupplyUnits);TestEqual(TEXT("reloaded real purchased units"),R.GetSnapshot().TotalArrivalPurchases,P.GetSnapshot().TotalArrivalPurchases);TestEqual(TEXT("reloaded movement count"),RT.GetSnapshot().Vehicles.Num(),T.GetSnapshot().Vehicles.Num());}
 }
 TestTrue(TEXT("in-flight reload exercised"),Saved);TestEqual(TEXT("two weekly shopping visits over fourteen days"),Trips,2);
 if(ArrivalSteps.Num()==2)TestTrue(TEXT("shopping visits about seven days apart"),FMath::Abs(ArrivalSteps[1]-ArrivalSteps[0]-1050)<=10);
 TestEqual(TEXT("daily consumption retains original fourteen goods cost"),P.GetSnapshot().Citizens[0].AvailableMoney,int64(30));
 TestEqual(TEXT("starter plus purchased minus daily consumed"),P.GetSnapshot().Households[0].SupplyUnits,3);
 AddInfo(FString::Printf(TEXT("HOUSEHOLDSUPPLY_WEEKLY Days=14 Visits=%d Bought=%lld Pantry=%d Wallet=%lld"),Trips,P.GetSnapshot().TotalArrivalPurchases,P.GetSnapshot().Households[0].SupplyUnits,P.GetSnapshot().Citizens[0].AvailableMoney));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySupplyCommuteTest,"CitySim.Supplies.BeforeAndAfterWorkStops",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCitySupplyCommuteTest::RunTest(const FString&)
{
 for(const bool Before:{true,false})
 {
  FCityPlacementModel City(10000);SupplyRoad(City);const auto Home=SupplyLot(City,TEXT("Home"),{2,9}),Work=SupplyLot(City,TEXT("Factory"),{6,9}),Shop=SupplyLot(City,TEXT("Shop"),{Before?4:12,9});
  auto Initial=SupplyFamily(Home,Shop);auto& H=Initial.Households[0];H.bUsesStoredSupplies=true;H.SupplyUnits=1;H.SupplyDay=0;
  Initial.Citizens[0].WorkplaceEntityId=Work;Initial.Citizens[0].UnemployedSinceStep=-1;
  FCityBusinessLedgerRecord B;B.EntityId=Work;B.bIndustrial=true;B.JobCapacity=1;Initial.Businesses.Add(B);
  FCityPopulationSimulation P;FString Error;TestTrue(TEXT("commute family restores"),P.RestoreState(Initial,Error));FCityTrafficSimulation T;FCityRoadNetworkSnapshot N;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);FCityEconomyTuning Tune;
  int64 WorkAt=-1,ShopAt=-1,HomeAt=-1;
  for(int64 Step=1;Step<150;++Step){SupplyStep(P,T,City,N,Step,Tune);const auto& C=P.GetSnapshot().Citizens[0];if(C.Schedule==ECityCitizenSchedule::Work && WorkAt<0)WorkAt=Step;if(C.Schedule==ECityCitizenSchedule::Shop && ShopAt<0)ShopAt=Step;if(WorkAt>=0 && ShopAt>=0 && C.Schedule==ECityCitizenSchedule::Home && HomeAt<0)HomeAt=Step;}
  TestTrue(TEXT("work, shop and home all reached in one day"),WorkAt>0 && ShopAt>0 && HomeAt>FMath::Max(WorkAt,ShopAt));
  TestEqual(TEXT("nearby on-route stop before work, otherwise after work"),ShopAt<WorkAt,Before);
  TestEqual(TEXT("one real bulk purchase, no extra commute purchase"),P.GetSnapshot().TotalArrivalPurchases,int64(7));
  AddInfo(FString::Printf(TEXT("HOUSEHOLDSUPPLY_COMMUTE Before=%d Work=%lld Shop=%lld Home=%lld"),Before,WorkAt,ShopAt,HomeAt));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityComfortSupplyTest,"CitySim.Supplies.OptionalComfortAndFiniteFunding",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityComfortSupplyTest::RunTest(const FString&)
{
 for(const int32 Tier:{0,1,2})
 {
  FCityPlacementModel City(10000);SupplyRoad(City);const auto Home=SupplyLot(City,TEXT("Home"),{2,9}),Shop=SupplyLot(City,TEXT("Shop"),{6,9});
  auto Initial=SupplyFamily(Home,Shop);auto& H=Initial.Households[0];H.bUsesStoredSupplies=true;H.SupplyUnits=7;H.SupplyDay=0;
  Initial.Citizens[0].AvailableMoney=H.Balance=Tier==2?200:100;Initial.Citizens[0].Education=ECityEducationLevel::Higher;Initial.Businesses[0].RetailTier=Tier;
  FCityPopulationSimulation P;FString Error;TestTrue(TEXT("comfort family restores"),P.RestoreState(Initial,Error));FCityRoadNetworkSnapshot N;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),N);FCityTrafficSimulation T;FCityEconomyTuning Tune;
  for(int64 Step=1;Step<150;++Step)SupplyStep(P,T,City,N,Step,Tune);
  const auto& After=P.GetSnapshot();TestEqual(TEXT("comfort never changes essential stock"),After.Households[0].SupplyUnits,7);
  TestEqual(TEXT("low density only supplies essentials; higher tiers offer optional goods"),After.TotalArrivalPurchases,int64(Tier?1:0));
  TestEqual(TEXT("correct comfort tier actually purchased"),After.Households[0].ComfortTier,Tier);
  TestTrue(TEXT("funded week of essentials remains protected"),After.Citizens[0].AvailableMoney>=35);
  FCityServiceSnapshot Services;FCityEnvironmentSnapshot Env;FCityEconomySnapshot E;FCityWasteSnapshot Waste;
  const auto Base=FCityHappinessEvaluator::AssessHome(H,City,Services,Env,E,Tune,Waste);
  const auto Actual=FCityHappinessEvaluator::AssessHome(After.Households[0],City,Services,Env,E,Tune,Waste);
  TestEqual(TEXT("comfort gives a small optional happiness benefit"),Actual.Target-Base.Target,Tier==2?8:Tier==1?5:0);
  TestTrue(TEXT("no missing-essential concern without luxury access"),(Actual.Concerns & static_cast<int32>(ECityHappinessConcern::Purchases))==0);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySupplyWarningTest,"CitySim.Supplies.StartupAndActualStockoutIndicators",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCitySupplyWarningTest::RunTest(const FString&)
{
 FCityPlacementModel City(10000);SupplyRoad(City);const auto Home=SupplyLot(City,TEXT("Home"),{2,9}),Shop=SupplyLot(City,TEXT("Shop"),{6,9});
 auto State=SupplyFamily(Home,Shop,0);State.Households[0].bUsesStoredSupplies=true;State.Households[0].SupplyDay=0;State.Households[0].SupplyUnits=3;
 State.Businesses[0].bInitialFreightReceived=false;State.Businesses[0].OpeningGraceUntilStep=450;
 FCityFireSnapshot Fire;Fire.LastFixedStepId=449;FCityCrimeSnapshot Crime;FCityServiceSnapshot S;S.EntityFailures.Add(Shop,ECityOperationFailure::AwaitingFreight);
 auto Rows=FCityConditionIndicators::Build(City,Fire,Crime,8,nullptr,&State,0,nullptr,&S,100,0);
 TestTrue(TEXT("new house and normal first-delivery wait do not warn"),Rows.IsEmpty());
 Fire.LastFixedStepId=450;Rows=FCityConditionIndicators::Build(City,Fire,Crime,8,nullptr,&State,0,nullptr,&S,100,0);
 TestTrue(TEXT("genuinely overdue opening freight is distinguished"),Rows.ContainsByPredicate([&](const auto& R){return R.EntityId==Shop && R.Title(0).Contains(TEXT("OVERDUE"));}));
 State.Businesses[0].bInitialFreightReceived=true;S.EntityFailures.Reset();State.Households[0].SupplyUnits=0;
 Rows=FCityConditionIndicators::Build(City,Fire,Crime,8,nullptr,&State,0,nullptr,&S,100,0);
 TestTrue(TEXT("only exhausted household essentials warn"),Rows.Num()==1 && Rows[0].EntityId==Home && Rows[0].bSupplyEmpty);
 State.Households[0].SupplyUnits=7;State.Businesses[0].bHasReceivedGoods=true;
 Rows=FCityConditionIndicators::Build(City,Fire,Crime,8,nullptr,&State,0,nullptr,&S,100,0);
 TestTrue(TEXT("retail warns only after real goods have been exhausted"),Rows.Num()==1 && Rows[0].EntityId==Shop && Rows[0].bRetailStockout);
 State.Businesses[0].Inventory=5;Rows=FCityConditionIndicators::Build(City,Fire,Crime,8,nullptr,&State,0,nullptr,&S,100,0);
 TestTrue(TEXT("real restock clears the stockout marker immediately"),Rows.IsEmpty());
 FCityPopulationSimulation P;FString Error;TestTrue(TEXT("new saved fields validate"),P.RestoreState(State,Error));State.Households[0].SupplyUnits=-1;
 TestFalse(TEXT("invalid negative pantry rejected atomically"),P.RestoreState(State,Error));TestEqual(TEXT("failed restore preserves real stock"),P.GetSnapshot().Households[0].SupplyUnits,7);
 FCitySaveDocument D,L;D.Placement=City.ExportState();D.Population=P.GetSnapshot();D.Population.Businesses[0].Inventory=0;D.Population.Businesses[0].CompletedSales=1;
 FCityDevelopmentState Level;Level.EntityId=Shop;Level.DensityLevel=ECityDensityLevel::High;D.Development.States.Add(Shop,Level);
 FString Json,Legacy;Error.Reset();TestTrue(TEXT("current city serializes supply state"),FCityPersistence::Serialize(D,Json,Error));
 TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);
 auto OldBusiness=Root->GetObjectField(TEXT("population"))->GetArrayField(TEXT("businesses"))[0]->AsObject();
 Root->SetNumberField(TEXT("schemaVersion"),FCitySaveDocument::PrePantrySchemaVersion);
 for(const TCHAR* Field:{TEXT("retailTier"),TEXT("receivedGoods"),TEXT("openingGraceUntil")})OldBusiness->RemoveField(Field);
 FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Legacy));
 TestTrue(TEXT("prior current-format city reads optional supply extension: ")+Error,FCityPersistence::Deserialize(Legacy,L,Error));
 TestTrue(TEXT("known previous sales prove retail was stocked before upgrade"),L.Population.Businesses[0].bHasReceivedGoods);
 TestEqual(TEXT("existing high density offer retained during current-city transition"),L.Population.Businesses[0].RetailTier,2);
 return true;
}
#endif
