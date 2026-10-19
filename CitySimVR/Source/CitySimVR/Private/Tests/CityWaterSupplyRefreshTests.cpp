#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityConditionIndicators.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityReportedWaterSupplyTest, "CitySim.Water.ReportedSchoolSupply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityReportedWaterSupplyTest::RunTest(const FString& Parameters)
{
    FString Path, Output, Before, Error;
    if (!FParse::Value(FCommandLine::Get(), TEXT("WaterRefreshSave="), Path))
    { AddInfo(TEXT("Supply -WaterRefreshSave for the preserved reported city")); return true; }
    if (!FParse::Value(FCommandLine::Get(), TEXT("WaterRefreshOutput="), Output) ||
        !FFileHelper::LoadFileToString(Before, *Path)) return false;
    if (FPaths::IsSamePath(Path, Output) || FPaths::IsSamePath(Path, Output + TEXT(".json")))
    { AddError(TEXT("WaterRefresh output must differ from input city")); return false; }
    const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WaterRefreshWater"), nullptr,
        true, ERHIFeatureLevel::Num, &Values);
    auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
    auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr,
        TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    Sim->RegisterBuildingCatalog(Catalog);
    if (!TestTrue(TEXT("Reported city loads"), Sim->LoadCityFromPath(Path, Error, false)))
    { AddError(Error); World->DestroyWorld(false); return false; }
    Sim->SetSimulationPaused(true);
    const auto& City = Sim->GetPlacementModel();
    const auto& S = Sim->GetServiceSnapshot();
    const auto& W = S.Water;
    TestEqual(TEXT("Reported school receives all current water demand"), W.EntityDelivered.FindRef(63), W.EntityDemand.FindRef(63));
    TestEqual(TEXT("Reported school has no current water shortage"), W.EntityCauses.FindRef(63), ECityWaterFailure::None);
    FString Report = FString::Printf(TEXT("water=%d/%d demand=%d power=%d/%d\n"),
        S.ServiceUsed, S.ServiceCapacity, S.ServiceDemand, S.PowerUsed, S.PowerCapacity);
    for (const auto& Source : W.Sources)
        Report += FString::Printf(TEXT("source=%llu active=%d used=%d capacity=%d pressure=%d yield=%d staff=%d\n"),
            Source.EntityId, Source.bOperational, Source.Used, Source.Capacity, Source.SourcePressurePercent,
            Source.YieldPercent, Source.StaffPercent);
    TArray<uint64> Ids; City.GetEntities().GetKeys(Ids); Ids.Sort();
    for (const uint64 Id : Ids)
    {
        const auto& Entity = City.GetEntities().FindChecked(Id);
        Report += FString::Printf(TEXT("entity=%llu %s water=%d/%d head=%d cause=%d operation=%d\n"),
            Id, *Entity.DefinitionId.ToString(), W.EntityDelivered.FindRef(Id), W.EntityDemand.FindRef(Id),
            W.EntityPressurePercent.FindRef(Id), static_cast<int32>(W.EntityCauses.FindRef(Id)),
            static_cast<int32>(S.GetFailure(Id)));
        if (FCityBuildingDefinition::IsSchoolFacility(Entity.DefinitionId))
        {
            Report += FCityConditionIndicators::Explain(*Sim, Id) + TEXT("\n");
            TArray<FCityCellCoord> Roads; City.GetAdjacentRoadCells(Entity.Anchor, Entity.FootprintSize, true, Roads, true);
            for (const auto& Source : W.Sources)
            {
                int32 Hops = MAX_int32;
                for (const auto& Road : Roads) if (const auto* D = Source.RoadDistances.Find(Road)) Hops = FMath::Min(Hops, *D);
                Report += FString::Printf(TEXT("school source=%llu hops=%d\n"), Source.EntityId, Hops);
            }
        }
    }
    AddInfo(Report);
    FFileHelper::SaveStringToFile(Report, *Output);
    const FString ReloadPath = Output + TEXT(".json");
    if (!FPaths::IsSamePath(Path, ReloadPath))
    {
        TestTrue(TEXT("Current city with two station engines saves to a copy"), Sim->SaveCityToPath(ReloadPath, Error));
        TestTrue(TEXT("Current city with two station engines reloads"), Sim->LoadCityFromPath(ReloadPath, Error, false));
        TestEqual(TEXT("Reload retains full school delivery"), Sim->GetServiceSnapshot().Water.EntityDelivered.FindRef(63),
            Sim->GetServiceSnapshot().Water.EntityDemand.FindRef(63));
        TestEqual(TEXT("Reload retains no school water failure"), Sim->GetServiceSnapshot().Water.EntityCauses.FindRef(63), ECityWaterFailure::None);
        const FString Explanation = FCityConditionIndicators::Explain(*Sim, 63);
        TestFalse(TEXT("Current school indicator no longer reports water interruption"), Explanation.Contains(TEXT("WATER INTERRUPTED")));
    }
    FString After; FFileHelper::LoadFileToString(After, *Path);
    TestEqual(TEXT("Reported input preserved"), After, Before);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEmergencyFleetRestoreTest, "CitySim.Stations.FleetPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityEmergencyFleetRestoreTest::RunTest(const FString& Parameters)
{
    FCityPopulationSnapshot P;
    P.NextTransitTripId = 5;
    for (int32 I = 0; I < 4; ++I)
    {
        auto& T = P.TransitTrips.AddDefaulted_GetRef();
        T.Id = I + 1; T.StationEntityId = I < 2 ? 35 : 34;
        T.OriginEntityId = T.StationEntityId; T.DestinationEntityId = 50 + I;
        T.IncidentId = I + 1;
        T.Purpose = I < 2 ? ECityTripPurpose::FireResponse : ECityTripPurpose::PoliceResponse;
        T.VehicleType = I < 2 ? ECityTripVehicleType::FireEngine : ECityTripVehicleType::PoliceCar;
    }
    FCityPopulationSimulation Restored;
    FString Error;
    TestTrue(TEXT("Multiple fire and police vehicles per station restore"), Restored.RestoreState(P, Error));
    TestEqual(TEXT("All actual trips retained"), Restored.GetSnapshot().TransitTrips.Num(), 4);
    P.TransitTrips[0].Purpose = ECityTripPurpose::EmergencyReturn;
    P.TransitTrips[0].OriginEntityId = 50;
    P.TransitTrips[0].DestinationEntityId = 35;
    TestTrue(TEXT("Returning crew and another dispatched crew restore together"), Restored.RestoreState(P, Error));
    auto Invalid = P;
    Invalid.TransitTrips[0].DestinationEntityId = 99;
    TestFalse(TEXT("Malformed return remains rejected"), Restored.RestoreState(Invalid, Error));
    Invalid = P; Invalid.TransitTrips[1].VehicleType = ECityTripVehicleType::PoliceCar;
    TestFalse(TEXT("Wrong response vehicle remains rejected"), Restored.RestoreState(Invalid, Error));
    Invalid = P; Invalid.TransitTrips[1].Id = Invalid.TransitTrips[0].Id;
    TestFalse(TEXT("Duplicate trip identity remains rejected"), Restored.RestoreState(Invalid, Error));
    Invalid = P; Invalid.TransitTrips[1].IncidentId = 0;
    TestFalse(TEXT("Missing incident remains rejected"), Restored.RestoreState(Invalid, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAddedWaterSupplyTest, "CitySim.Water.AddedLocalSupply",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityAddedWaterSupplyTest::RunTest(const FString& Parameters)
{
    FCityPlacementModel City(100000);
    auto Road = City.BeginRoadPreview(City.GetEntranceCell());
    City.UpdateRoadPreviewEnd(Road, {63, 8});
    if (!TestTrue(TEXT("Connected road commits"), City.CommitRoad(Road).IsSuccess())) return false;
    auto Place = [&](FName Name, int32 X) {
        FCityBuildingDefinition D; D.DefinitionId = Name; D.DisplayName = Name.ToString();
        D.Footprint = FCityCellFootprint(1, 1); D.LogicalHeightCm = 100;
        auto P = City.BeginPreview(D, {X, 9});
        return City.CommitPlacement(P, D).EntityId;
    };
    const uint64 Old = Place(TEXT("Service"), 0), Local = Place(TEXT("Service"), 22);
    for (const bool Prepared : {false, true})
        for (const bool Reverse : {false, true})
            for (const int32 LocalPressure : {100, 120})
            {
                FCityWaterSnapshot W; W.RulesVersion = 1; W.bPrepareQueries = Prepared;
                FCityWaterSource A; A.EntityId = Old; A.Capacity = 200; A.bOperational = true;
                FCityWaterSource B; B.EntityId = Local; B.Capacity = 10; B.bOperational = true;
                B.SourcePressurePercent = LocalPressure;
                W.Sources.Add(A);
                FCityWaterEvaluator::BuildNetwork(City, W, 32);
                FCityWaterEvaluator::Allocate(City, W, 100, {20, 9}, {1, 1}, 10);
                TestEqual(TEXT("School is pressure limited before adding local supply"),
                    FCityWaterEvaluator::Allocate(City, W, 101, {35, 9}, {1, 1}, 10), 5);
                TestEqual(TEXT("Before addition, school reports actual low pressure"),
                    W.EntityCauses.FindRef(101), ECityWaterFailure::LowPressure);
                W.Sources = Reverse ? TArray<FCityWaterSource>{B, A} : TArray<FCityWaterSource>{A, B};
                FCityWaterEvaluator::BuildNetwork(City, W, 32);
                TestEqual(TEXT("Earlier consumer has full older-source flow"),
                    FCityWaterEvaluator::Allocate(City, W, 100, {20, 9}, {1, 1}, 10), 10);
                TestEqual(TEXT("Source preference retains actual maximum physical pressure"),
                    W.EntityPressurePercent.FindRef(100), LocalPressure - 4);
                TestEqual(TEXT("Local school candidate can use the added source"),
                    FCityWaterEvaluator::GetAvailableAt(City, W, {35, 9}, {1, 1}, 10), 10);
                TestEqual(TEXT("Local school receives full demand after source addition"),
                    FCityWaterEvaluator::Allocate(City, W, 101, {35, 9}, {1, 1}, 10), 10);
                TestEqual(TEXT("Local school warning clears"), W.EntityCauses.FindRef(101), ECityWaterFailure::None);
                for (const auto& Source : W.Sources)
                    TestEqual(TEXT("Each source supplies exactly one demand, without creating water"), Source.Used, 10);
                for (auto& Source : W.Sources) if (Source.EntityId == Local) Source.bOperational = false;
                FCityWaterEvaluator::BuildNetwork(City, W, 32);
                FCityWaterEvaluator::Allocate(City, W, 100, {20, 9}, {1, 1}, 10);
                TestEqual(TEXT("Source outage restores the real shortage on reevaluation"),
                    FCityWaterEvaluator::Allocate(City, W, 101, {35, 9}, {1, 1}, 10), 5);
            }
    return true;
}
#endif
