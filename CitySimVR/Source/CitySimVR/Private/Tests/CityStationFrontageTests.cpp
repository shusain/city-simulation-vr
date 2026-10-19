#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStationFrontageTest,"CitySim.Stations.FullGarageFrontage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityStationFrontageTest::RunTest(const FString&)
{
    const auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Station catalog"),Catalog)) return false;
    for(const FName Id:{FName(TEXT("Police")),FName(TEXT("Fire"))}) for(int32 Turns=0;Turns<4;++Turns) {
        const auto* Definition=Catalog->FindDefinition(Id);
        if(!TestNotNull(TEXT("Upgradeable station definition"),Definition)) return false;
        FCityPlacementModel City(500000);
        const FCityCellCoord Anchor(18,16);
        const auto Size=Definition->Footprint.GetRotatedSize(Turns);
        const auto Preview=[&](){return City.BeginPreview(*Definition,Anchor,Turns);};
        TestEqual(TEXT("A bare station site is invalid"),City.ValidatePlacement(Preview(),*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
        const auto Road=[&](FCityCellCoord A,FCityCellCoord B,FName RoadId=TEXT("LocalRoad")) {
            auto R=City.BeginRoadPreview(A,RoadId);City.UpdateRoadPreviewEnd(R,B);
            return TestTrue(TEXT("Frontage fixture road commits"),City.CommitRoad(R).IsSuccess());
        };
        const int32 Right=Anchor.X+Size.X,Bottom=Anchor.Y+Size.Y;
        if(!Road(City.GetEntranceCell(),{17,8}) || !Road({17,8},{17,15}) ||
            !Road({17,15},{Right,15}) || !Road({Right,15},{Right,Bottom}) ||
            !Road({Right,Bottom},{17,Bottom}) || !Road({17,Bottom},{17,15})) return false;
        TArray<FCityCellCoord> Front;
        for(int32 I=0;I<3;++I) Front.Add(Turns==0?FCityCellCoord(Anchor.X+I,Anchor.Y-1):
            Turns==1?FCityCellCoord(Right,Anchor.Y+I):Turns==2?FCityCellCoord(Anchor.X+I,Bottom):FCityCellCoord(Anchor.X-1,Anchor.Y+I));
        TestTrue(TEXT("Complete frontage permits siting in every rotation"),City.ValidatePlacement(Preview(),*Definition).IsSuccess());
        for(const auto& Gap:Front) {
            TestTrue(TEXT("Remove one garage frontage tile"),City.RemoveRoad(Gap));
            for(const auto& Other:Front) if(!(Other==Gap)) TestTrue(TEXT("Remaining frontage stays connected"),City.IsRoadConnected(Other));
            const int64 Funds=City.GetFunds();auto P=Preview();
            TestEqual(TEXT("A missing front tile rejects preview"),City.ValidatePlacement(P,*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
            TestEqual(TEXT("A missing front tile rejects commit"),City.CommitPlacement(P,*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
            TestEqual(TEXT("Rejected station cannot spend funds"),City.GetFunds(),Funds);
            TestTrue(TEXT("Rejected station creates no entity"),City.GetEntities().IsEmpty());
            if(!Road(Gap,Gap)) return false;
        }
        if(!Road(Front[1],Front[1],TEXT("Highway"))) return false;
        TestEqual(TEXT("A highway cannot substitute for garage-access frontage"),City.ValidatePlacement(Preview(),*Definition).Failure,ECityPlacementFailure::FullRoadFrontage);
        if(!Road(Front[1],Front[1])) return false;
        auto P=Preview();const auto Placed=City.CommitPlacement(P,*Definition);
        if(!TestTrue(TEXT("Complete connected frontage commits"),Placed.IsSuccess())) return false;
        TestTrue(TEXT("Complete connected frontage supplies road access"),City.HasRoadAccess(Placed.EntityId));
        FCityDevelopmentSimulation Development;Development.InitializeExistingAsOperational(City,Catalog,0);
        TestTrue(TEXT("Removing a station road succeeds"),City.RemoveRoad(Front[1]));
        TestFalse(TEXT("Partial frontage closes station road access"),City.HasRoadAccess(Placed.EntityId));
        FCityEconomyTuning Tuning;
        const auto Services=FCityServiceEvaluator::BuildSnapshot(City,Tuning,Catalog,&Development.GetSnapshot());
        TestEqual(TEXT("Partial frontage reports no road operation"),Services.GetFailure(Placed.EntityId),ECityOperationFailure::NoRoad);
        FString Error;const int64 Funds=City.GetFunds();
        TestFalse(TEXT("Partial frontage cannot buy an upgrade"),Development.PurchaseCivicUpgrade(Placed.EntityId,ECityDensityLevel::Low,City,Catalog,Services,Error));
        TestTrue(TEXT("Upgrade explains the full-frontage requirement"),Error.Contains(TEXT("entire entrance frontage")));
        TestEqual(TEXT("Blocked upgrade cannot spend funds"),City.GetFunds(),Funds);
        TMap<FName,FCityBuildingDefinition> Definitions;Definitions.Add(Id,*Definition);
        auto Loaded=FCityPlacementModel::CreateValidatedFromState(City.ExportState(),Definitions,Error);
        if(!TestNotNull(TEXT("Existing partial-frontage city remains loadable"),Loaded.Get())) return false;
        TestFalse(TEXT("Loaded partial station remains closed"),Loaded->HasRoadAccess(Placed.EntityId));
        if(!Road(Front[1],Front[1])) return false;
        TestTrue(TEXT("Restoring the front road restores access"),City.HasRoadAccess(Placed.EntityId));
    }
    return true;
}
#endif
