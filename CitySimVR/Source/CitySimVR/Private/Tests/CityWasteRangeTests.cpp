#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Persistence/CityWastePersistence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWasteRangeTest,"CitySim.Waste.DoubledRangeBoundaryAndArrival",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityWasteRangeTest::RunTest(const FString&)
{
 const auto* Authored=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
 if(!TestNotNull(TEXT("authored catalog"),Authored))return false;
 for(FName Facility:{FName(TEXT("WasteDepot")),FName(TEXT("Recycling"))})for(auto Level:{ECityDensityLevel::Low,ECityDensityLevel::Medium})
 {
  const auto* Definition=Authored->FindDefinition(Facility);if(!TestNotNull(TEXT("facility level"),Definition?Definition->FindDensityLevel(Level):nullptr))return false;
  TestEqual(TEXT("both actual catalog levels use64"),Definition->FindDensityLevel(Level)->CivicRoadRange,64);
  auto* Catalog=NewObject<UCityBuildingCatalog>();Catalog->Definitions=Authored->Definitions;
  for(auto& D:Catalog->Definitions){D.Footprint={1,1};D.EntranceSides=15;}
  FCityPlacementModel City(100000,FCityGridDimensions(80,80));auto Road=City.BeginRoadPreview(City.GetEntranceCell());City.UpdateRoadPreviewEnd(Road,{75,8});
  if(!TestTrue(TEXT("long connected route"),City.CommitRoad(Road).IsSuccess()))return false;
  auto Place=[&](FName Name,int32 X){const auto* D=Catalog->FindDefinition(Name);auto P=City.BeginPreview(*D,{X,9});return City.CommitPlacement(P,*D).EntityId;};
  const uint64 Depot=Place(Facility,1),Inside=Place(TEXT("Home"),65),Outside=Place(TEXT("Home"),66);
  if(!TestTrue(TEXT("boundary lots placed"),Depot&&Inside&&Outside))return false;
  FCityRoadNetworkSnapshot Network;FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),Network);
  FCityDevelopmentSnapshot Development;for(uint64 Id:{Depot,Inside,Outside}){auto& D=Development.States.Add(Id);D.EntityId=Id;D.DensityLevel=Id==Depot?Level:ECityDensityLevel::Low;}
  FCityServiceSnapshot Services;Services.PublicStaffPercent.Add(Depot,100);Services.HospitalCapacities.Add(999,24);
  TestEqual(TEXT("effective runtime range64"),FCityWasteSimulation::Policy(Depot,City,Catalog,Development,Services,false).Range,64);
  FCityCellCoord A,B;TArray<FCityCellCoord> Cells;FCityTrafficSimulation::FindPortal(City,Catalog,Depot,A);FCityTrafficSimulation::FindPortal(City,Catalog,Inside,B);
  TestTrue(TEXT("boundary path exists"),Network.FindRoute(A,B,ECityRoadAccess::Freight,Cells));TestEqual(TEXT("inside is exactly64 route cells"),Cells.Num()-1,64);
  FCityPopulationSnapshot People;FCityWasteSimulation Waste;FCityWasteTuning T;T.PopulationThreshold=0;
  auto Update=[&](int64 Step){Waste.Update(Step,150,City,Catalog,Network,Services,Development,People,false,T);};
  Update(0);Waste.AddTestPile(Inside);Waste.AddTestPile(Outside);Update(1);
  bool PlannedInside=false,PlannedOutside=false;for(const auto& Truck:Waste.GetSnapshot().Trucks){PlannedInside|=Truck.Stops.Contains(Inside);PlannedOutside|=Truck.Stops.Contains(Outside);}
  TestTrue(TEXT("64-cell site reserved"),PlannedInside);TestFalse(TEXT("65-cell site excluded"),PlannedOutside);TestEqual(TEXT("reservation never collects"),Waste.GetSnapshot().Collected,int64(0));
  FCityTrafficSimulation Traffic;FString Error;Traffic.AdvanceToStep(Network,1,Error);
  for(int64 Step=2;Step<=100;++Step)
  {
   Update(Step);Traffic.UpdateTrips(City,Catalog,People,Network,Step);Traffic.AdvanceOneStep(Network,Step);
   for(const auto& V:Traffic.GetSnapshot().Vehicles)if(V.bCompleted)for(auto& Trip:People.TransitTrips)if(Trip.Id==V.SourceId){Trip.bCompleted=true;Trip.ArrivalStep=Step;}
   Update(Step);Traffic.UpdateTrips(City,Catalog,People,Network,Step);
  }
  TestTrue(TEXT("real distant arrival collects"),Waste.GetSnapshot().Collected>0);TestEqual(TEXT("outside pile untouched"),Waste.GetSnapshot().Find(Outside)->Units,60);
  TestTrue(TEXT("extended-range conservation"),FCityWasteSimulation::ValidateState(Waste.GetSnapshot(),100,Error));
  FCityWasteSnapshot Copy;TestTrue(TEXT("extended route/cargo saves"),FCityWastePersistence::Read(FCityWastePersistence::Write(Waste.GetSnapshot()),Copy,100,Error));
  TestEqual(TEXT("saved collected mass"),Copy.Collected,Waste.GetSnapshot().Collected);
 }
 return true;
}
#endif
