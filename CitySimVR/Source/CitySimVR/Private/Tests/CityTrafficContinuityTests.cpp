#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityPedestrianPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTrafficContinuityTest, "CitySim.Traffic.PlacementContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTrafficContinuityTest::RunTest(const FString& Parameters)
{
    FCityPlacementModel City(10000);
    auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
    City.UpdateRoadPreviewEnd(Road, {20,8});
    if (!TestTrue(TEXT("initial road"), City.CommitRoad(Road).IsSuccess())) return false;
    FCityRoadNetworkSnapshot Before;
    FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Before);
    FCityTrafficSnapshot State;
    State.NetworkFingerprint = FCityTrafficSimulation::GetNetworkFingerprint(Before);
    State.FixedStepId = 4;
    // Real route conversion, including a reverse journey and a queued departure.
    for (int32 I=0; I<4; ++I)
    {
        FCityTrafficVehicleState V;
        V.Id = (I+1)*2; V.SourceId = I+1;
        V.VehicleType = I==2 ? ECityTripVehicleType::Walk : ECityTripVehicleType::Car;
        const bool Reverse = I==1;
        if (!TestTrue(TEXT("route builds"), FCityTrafficSimulation::BuildRoute(Before,
            Reverse ? FCityCellCoord(18,8) : FCityCellCoord(2,8),
            Reverse ? FCityCellCoord(2,8) : FCityCellCoord(I==2 ? 6 : 18,8), V))) return false;
        if (I==3) State.WaitingDepartures.Add(V);
        else
        {
            V.DistanceUnits += 1500+I*2000;
            if (I==2)
            {
                for(int32 X=2; X<=6; ++X) V.WalkPath.Add(FIntVector(X*1000+500,8750,0));
                V.WalkDistanceUnits=1500;
                CityPedestrianPaths::UpdateRoadPosition(V,Before);
            }
            State.Vehicles.Add(V);
        }
    }
    TArray<FVector> Positions;
    for (const auto& V : State.Vehicles)
    {
        FVector P,D;
        TestTrue(TEXT("before samples"), FCityTrafficSimulation::SamplePosition(V,nullptr,Before,1,P,D));
        Positions.Add(P);
    }
    // A new branch splits an occupied edge and renumbers unrelated edge IDs.
    auto Branch = City.BeginRoadPreview({10,8}); City.UpdateRoadPreviewEnd(Branch,{10,12});
    if (!TestTrue(TEXT("branch commits"),City.CommitRoad(Branch).IsSuccess())) return false;
    FCityRoadNetworkSnapshot After;
    FCityRoadNetworkBuilder::Build(City,FCityRoadDefinition::Local(),After);
    TestNotEqual(TEXT("topology changes"),State.NetworkFingerprint,FCityTrafficSimulation::GetNetworkFingerprint(After));
    FCityTrafficSnapshot Affected;
    Affected.NetworkFingerprint=State.NetworkFingerprint;
    FCityTrafficVehicleState Crossing=State.Vehicles[2];
    FCityTrafficSimulation::BuildRoute(Before,{2,8},{18,8},Crossing);
    Crossing.WalkPath.Reset();
    for(int32 X=2; X<=18; ++X) Crossing.WalkPath.Add(FIntVector(X*1000+500,8750,0));
    Crossing.WalkDistanceUnits=1500;
    CityPedestrianPaths::UpdateRoadPosition(Crossing,Before);
    Affected.Vehicles.Add(Crossing);
    FCityTrafficSimulation::ReconcileSnapshot(Affected,Before,After);
    TestTrue(TEXT("walker whose crossing geometry changed retires locally"),Affected.Vehicles.IsEmpty());
    FCityTrafficSimulation::ReconcileSnapshot(State,Before,After);
    if (!TestEqual(TEXT("all moving identities retained"),State.Vehicles.Num(),3)) return false;
    TestEqual(TEXT("waiting departure retained"),State.WaitingDepartures.Num(),1);
    for (int32 I=0; I<State.Vehicles.Num(); ++I)
    {
        const auto& V=State.Vehicles[I]; FVector P,D;
        TestEqual(TEXT("stable identity"),V.Id,uint64((I+1)*2));
        TestTrue(TEXT("remapped position samples"),FCityTrafficSimulation::SamplePosition(V,nullptr,After,1,P,D));
        TestTrue(TEXT("exact position retained through split"),P.Equals(Positions[I],1.e-8));
        TestFalse(TEXT("reconciliation cannot fabricate arrival"),V.bCompleted);
        if (I==2) TestEqual(TEXT("sidewalk progress retained"),V.WalkDistanceUnits,1500);
        FCityTrafficSimulation Probe;
        TestTrue(TEXT("remapped record validates"),Probe.AddVehicle(V,After));
    }
    if (State.WaitingDepartures.Num())
    {
        const auto& V=State.WaitingDepartures[0];
        TestEqual(TEXT("waiting route begins at its portal"),V.RouteLegIndex,0);
        TestEqual(TEXT("waiting progress is unchanged"),V.DistanceUnits,V.StartDistanceUnits);
    }
    // Remove only one mode from one physical segment: walkers must invalidate,
    // while the motor journeys retain their position and cannot be duplicated.
    FCityRoadNetworkSnapshot Restricted=After;
    for(auto& Edge:Restricted.Edges) Edge.Access &= ~ECityRoadAccess::Pedestrian;
    Restricted.PrepareRouting();
    FCityTrafficSimulation::ReconcileSnapshot(State,After,Restricted);
    TestEqual(TEXT("only invalidated walking journey removed"),State.Vehicles.Num(),2);
    TestEqual(TEXT("unaffected queued motor journey survives"),State.WaitingDepartures.Num(),1);
    return true;
}
#endif
