#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "StaticMeshResources.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityUtilityEntrances.h"
#include "Presentation/CityUtilityFoundations.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedUtilityCookedTest,"CitySim.Presentation.ApprovedUtilityBatchCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedUtilityCookedTest::RunTest(const FString&)
{
    struct FExpected {const TCHAR* Package;int32 LODs;uint32 Triangles[3];};
    static const FExpected Expected[]={
#include "CityApprovedUtilityMeshChecks.inl"
    };
    TestEqual(TEXT("All batch whole/damage/wind/foundation packages covered"),static_cast<int32>(UE_ARRAY_COUNT(Expected)),37);
    for(const auto& E:Expected)
    {
        const FString Package=E.Package;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+FPaths::GetBaseFilename(Package)));
        if(!TestNotNull(TEXT("Approved native package is retained in the actual build"),Mesh)) continue;
        TestEqual(TEXT("Prepared LOD chain survives cooking"),Mesh->GetNumLODs(),E.LODs);
        if(!TestNotNull(TEXT("Actual render data exists"),Mesh->GetRenderData())) continue;
        for(int32 LOD=0;LOD<E.LODs && LOD<Mesh->GetRenderData()->LODResources.Num();++LOD)
            TestEqual(TEXT("Expected render triangle count survives cooking"),Mesh->GetRenderData()->LODResources[LOD].GetNumTriangles(),E.Triangles[LOD]);
        for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(TEXT("Every explicit material resolves"),Slot.MaterialInterface.Get());
    }
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Current catalog resolves"),Catalog)) return false;
    struct FSite {const TCHAR* Id;int32 Width,Height,Capacity;};
    const FSite Sites[]={{TEXT("CoalPower"),4,4,400},{TEXT("OilPower"),4,4,600},{TEXT("Power"),4,4,0},
        {TEXT("Service"),6,4,0},{TEXT("WaterTower"),2,2,100},{TEXT("WindPower"),3,3,150},{TEXT("SolarPower"),4,4,300}};
    for(const auto& Site:Sites)
    {
        const auto* Definition=Catalog->FindDefinition(Site.Id);
        if(!TestNotNull(TEXT("Each utility definition resolves"),Definition)) continue;
        TestEqual(TEXT("All upgrade versions reserve their reviewed width"),Definition->Footprint.Width,Site.Width);
        TestEqual(TEXT("All upgrade versions reserve their reviewed depth"),Definition->Footprint.Height,Site.Height);
        TestEqual(TEXT("Capacity is unchanged by model adoption"),Definition->UtilityCapacity,Site.Capacity);
        TestFalse(TEXT("Construction has a temporary work deck"),Definition->ConstructionMesh.IsNull());
        TestEqual(TEXT("Front is explicitly aligned to the road"),Definition->PresentationYawOffsetQuarterTurns,2);
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedUtilityGroundTest,"CitySim.Terrain.ApprovedUtilityBatchGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedUtilityGroundTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Board=World->SpawnActor<ACityBoardActor>();
    auto* Sim=World->GetSubsystem<UCitySimulationSubsystem>();
    Board->SimulationSubsystem=Sim;
    int32 Profiles=0;
    for(const auto& Entry:CityUtilityEntrances::All())
    {
        if(FString(Entry.SupportMesh)==TEXT("Nuclear_v6_Support")) continue;
        ++Profiles;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*CityResidentialEntrances::ObjectPath(Entry));
        if(!TestNotNull(TEXT("Every adopted lifecycle profile has native geometry"),Mesh)) continue;
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];
        TArray<FVector> Portals={Entry.EntryCm};
        for(const auto& Bay:CityUtilityEntrances::Bays(Mesh->GetName())) Portals.Add(Bay.EntryCm);
        for(const FVector& Portal:Portals)
        {
            double Distance=MAX_dbl;
            for(int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3)
            {
                const FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
                const FVector B(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
                const FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
                Distance=FMath::Min(Distance,FVector::Dist(Portal,FMath::ClosestPointOnTriangleToPoint(Portal,A,B,C)));
            }
            if(FString(Entry.SupportMesh)==TEXT("water_tower_Support"))
                TestTrue(TEXT("Ladder approach stays in the measured gap between its rails"),Distance>20. && Distance<60.);
            else
                TestTrue(*FString::Printf(TEXT("%s actual door/bay sill matches native anchor (%.4fcm)"),*Mesh->GetName(),Distance),Distance<.1);
        }
        const auto Groups=CityUtilityFoundations::Find(Mesh->GetName());
        TestTrue(TEXT("Every approved body resolves contour foundations"),!Groups.IsEmpty());
        FCityBuildingDefinition Definition;
        Definition.PresentationMesh=Mesh;Definition.PresentationYawOffsetQuarterTurns=2;
        Definition.Footprint.Width=6;Definition.Footprint.Height=4;
        for(int32 Turn=0;Turn<4;++Turn)
        {
            const auto Pose=Board->MakeBuildingTransform({18,16},Definition,Turn);
            TestEqual(TEXT("Ground-level source gets the shared5cm clearance"),Pose.GetLocation().Z-Board->GetBuildingBaseHeight({18,16},Definition,Turn),5.);
            for(const auto& Group:Groups)
            {
                const auto Support=CityUtilityFoundations::MakeTransform(Group,Pose,Sim->GetTerrain());
                TestTrue(TEXT("Contours retain horizontal scale"),Support.GetScale3D().Equals(FVector(1,1,Support.GetScale3D().Z)));
                TestTrue(TEXT("Foundation has positive buried depth"),Support.GetScale3D().Z>=.4);
                TestTrue(TEXT("Foundation top tracks actual support source Z and seam"),FMath::IsNearlyEqual(Support.GetLocation().Z,Pose.GetLocation().Z+Group.TopCm+1.,.0001));
                TestTrue(TEXT("Foundation does not shift the source XY pivot"),Support.GetLocation().Equals(FVector(Pose.GetLocation().X,Pose.GetLocation().Y,Support.GetLocation().Z),.0001));
                TestTrue(TEXT("Foundation keeps entity yaw"),Support.GetRotation().Equals(Pose.GetRotation(),.0001));
            }
        }
    }
    TestEqual(TEXT("Eleven healthy/damaged bodies plus stationary wind profile"),Profiles,23);
    TestEqual(TEXT("Twelve independently pooled contour groups"),CityUtilityFoundations::All().Num(),12);
    World->DestroyWorld(false);
    return true;
}
#endif
#endif
