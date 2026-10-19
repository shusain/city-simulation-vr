#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityFrontageTest,"CitySim.Utilities.FullEntranceFrontage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityFrontageTest::RunTest(const FString&)
{
    const auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Utility catalog"),Catalog)) return false;
    for(const FName Id:{FName(TEXT("WaterTower")),FName(TEXT("WindPower")),FName(TEXT("SolarPower"))})
        TestFalse(TEXT("Narrow personnel entrances retain ordinary access"),FCityPlacementModel::RequiresFullRoadFrontage(Id));
    for(const FName Id:{FName(TEXT("CoalPower")),FName(TEXT("OilPower")),FName(TEXT("Power")),FName(TEXT("Service")),FName(TEXT("NuclearPower"))})
    for(int32 Turns=0;Turns<4;++Turns)
    {
        const auto* Definition=Catalog->FindDefinition(Id);
        if(!TestNotNull(TEXT("Wide-entrance utility exists"),Definition)) return false;
        FCityPlacementModel City(500000);
        const FCityCellCoord Anchor(18,16);
        const auto Size=Definition->Footprint.GetRotatedSize(Turns);
        const int32 Right=Anchor.X+Size.X,Bottom=Anchor.Y+Size.Y;
        const auto Preview=[&](){return City.BeginPreview(*Definition,Anchor,Turns);};
        const auto Road=[&](FCityCellCoord A,FCityCellCoord B,FName RoadId=TEXT("LocalRoad")) {
            auto R=City.BeginRoadPreview(A,RoadId);City.UpdateRoadPreviewEnd(R,B);
            return TestTrue(TEXT("Frontage road commits"),City.CommitRoad(R).IsSuccess());
        };
        if(!Road(City.GetEntranceCell(),{17,8}) || !Road({17,8},{17,15}) ||
            !Road({17,15},{Right,15}) || !Road({Right,15},{Right,Bottom}) ||
            !Road({Right,Bottom},{17,Bottom}) || !Road({17,Bottom},{17,15})) return false;
        TArray<FCityCellCoord> Front;
        const int32 Length=Turns%2?Size.Y:Size.X;
        for(int32 I=0;I<Length;++I) Front.Add(Turns==0?FCityCellCoord(Anchor.X+I,Anchor.Y-1):
            Turns==1?FCityCellCoord(Right,Anchor.Y+I):Turns==2?FCityCellCoord(Anchor.X+I,Bottom):FCityCellCoord(Anchor.X-1,Anchor.Y+I));
        TestTrue(TEXT("Complete frontage permits every rotation and rectangular water plot"),City.ValidatePlacement(Preview(),*Definition).IsSuccess());
        for(const auto& Gap:Front)
        {
            TestTrue(TEXT("Remove one front tile"),City.RemoveRoad(Gap));
            const int64 Funds=City.GetFunds();auto P=Preview();
            TestEqual(TEXT("Incomplete frontage rejects preview"),City.ValidatePlacement(P,*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
            TestEqual(TEXT("Incomplete frontage rejects paid placement"),City.CommitPlacement(P,*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
            TestEqual(TEXT("Invalid frontage spends no money"),City.GetFunds(),Funds);
            TestTrue(TEXT("Invalid frontage creates no building"),City.GetEntities().IsEmpty());
            if(!Road(Gap,Gap)) return false;
        }
        if(!Road(Front[1],Front[1],TEXT("Highway"))) return false;
        TestEqual(TEXT("Highway is not an entrance road"),City.ValidatePlacement(Preview(),*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
        if(!Road(Front[1],Front[1])) return false;
        auto P=Preview();const auto Placed=City.CommitPlacement(P,*Definition);
        if(!TestTrue(TEXT("Full entrance road permits placement"),Placed.IsSuccess())) return false;
        TestTrue(TEXT("Connected complete frontage provides access"),City.HasRoadAccess(Placed.EntityId));
        FCityDevelopmentSimulation Development;Development.InitializeExistingAsOperational(City,Catalog,0);
        TestTrue(TEXT("Front road can be removed later"),City.RemoveRoad(Front[1]));
        TestFalse(TEXT("One missing tile disables access"),City.HasRoadAccess(Placed.EntityId));
        FCityEconomyTuning Tuning;
        const auto Services=FCityServiceEvaluator::BuildSnapshot(City,Tuning,Catalog,&Development.GetSnapshot());
        TestEqual(TEXT("Utility operation reports missing road"),Services.GetFailure(Placed.EntityId),ECityOperationFailure::NoRoad);
        FString Error;
        if(Definition->bPlayerPurchasedLevels) {
            const int64 Funds=City.GetFunds();
            TestFalse(TEXT("Missing utility frontage blocks paid expansion"),Development.PurchaseCivicUpgrade(Placed.EntityId,ECityDensityLevel::Low,City,Catalog,Services,Error));
            TestTrue(TEXT("Upgrade explains full entrance frontage"),Error.Contains(TEXT("entire entrance frontage")));
            TestEqual(TEXT("Blocked expansion spends no funds"),City.GetFunds(),Funds);
        }
        TMap<FName,FCityBuildingDefinition> Definitions;Definitions.Add(Id,*Definition);
        auto Loaded=FCityPlacementModel::CreateValidatedFromState(City.ExportState(),Definitions,Error);
        if(!TestNotNull(TEXT("Current city with removed road remains repairable after loading"),Loaded.Get())) return false;
        TestFalse(TEXT("Load does not invent frontage"),Loaded->HasRoadAccess(Placed.EntityId));
        if(!Road(Front[1],Front[1])) return false;
        TestTrue(TEXT("Restored road restores utility access"),City.HasRoadAccess(Placed.EntityId));
        TestTrue(TEXT("Disconnect city entrance from intact facade"),City.RemoveRoad({17,8}));
        TestFalse(TEXT("Full but disconnected frontage supplies no access"),City.HasRoadAccess(Placed.EntityId));
        if(!Road({17,8},{17,8})) return false;
        TestTrue(TEXT("Restored connection restores access"),City.HasRoadAccess(Placed.EntityId));
    }
    return true;
}
#endif
