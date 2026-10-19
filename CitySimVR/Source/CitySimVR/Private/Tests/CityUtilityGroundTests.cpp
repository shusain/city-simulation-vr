#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityUtilityEntrances.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "StaticMeshResources.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityGroundCookedTest,"CitySim.Presentation.UtilityGroundCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityGroundCookedTest::RunTest(const FString&)
{
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Utility catalog resolves"),Catalog)) return false;
    const auto* Definition=Catalog->FindDefinition(TEXT("NuclearPower"));
    if(!TestNotNull(TEXT("Nuclear definition resolves"),Definition)) return false;
    TestEqual(TEXT("Reviewed nuclear site width"),Definition->Footprint.Width,4);
    TestEqual(TEXT("Reviewed nuclear site depth"),Definition->Footprint.Height,4);
    TestEqual(TEXT("Nuclear capacity unchanged"),Definition->UtilityCapacity,2000);
    TestEqual(TEXT("Import yaw remains explicit"),Definition->PresentationYawOffsetQuarterTurns,2);
    TestFalse(TEXT("Nuclear construction has an explicit temporary body"),Definition->ConstructionMesh.IsNull());
    for(int32 Turn=0;Turn<4;++Turn) {
        FCityPlacementModel City(100000);
        const bool Horizontal=Turn%2==0;
        const int32 RoadX=Turn==1?22:17,RoadY=Turn==0?15:20;
        const FCityCellCoord First=Horizontal?FCityCellCoord(14,RoadY):FCityCellCoord(RoadX,12);
        const FCityCellCoord Last=Horizontal?FCityCellCoord(26,RoadY):FCityCellCoord(RoadX,24);
        auto Road=[&](FCityCellCoord A,FCityCellCoord B) {
            auto Preview=City.BeginRoadPreview(A);City.UpdateRoadPreviewEnd(Preview,B);
            return City.CommitRoad(Preview).IsSuccess();
        };
        TestTrue(TEXT("Rotated nuclear frontage connects"),Road(City.GetEntranceCell(),{12,8}) && Road({12,8},{12,12}) && Road({12,12},First) && Road(First,Last));
        auto Preview=City.BeginPreview(*Definition,{18,16},Turn);
        const auto Result=City.CommitPlacement(Preview,*Definition);
        if(!TestTrue(TEXT("Actual4x4 nuclear places in each rotation"),Result.IsSuccess())) continue;
        const auto* Entity=City.GetEntities().Find(Result.EntityId);
        TestTrue(TEXT("Whole reviewed reservation is authoritative"),Entity && Entity->FootprintSize==FIntPoint(4,4));
        auto Probe=*Definition;Probe.Footprint.Width=Probe.Footprint.Height=1;
        const auto Occupied=City.BeginPreview(Probe,{21,19},Turn);
        TestEqual(TEXT("Fourth row/column blocks overlapping plant"),City.ValidatePlacement(Occupied,Probe).Failure,ECityPlacementFailure::Occupied);
    }
    for(const auto& Entry:CityUtilityEntrances::All()) {
        if(FString(Entry.SupportMesh)!=TEXT("Nuclear_v6_Support")) continue;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*CityResidentialEntrances::ObjectPath(Entry));
        if(!TestNotNull(TEXT("Healthy/damaged utility resolves in package"),Mesh)) continue;
        TestEqual(TEXT("Exact native LOD0"),Mesh->GetRenderData()->LODResources[0].GetNumTriangles(),4616u);
        TestEqual(TEXT("Three prepared LODs"),Mesh->GetNumLODs(),3);
        TestEqual(TEXT("Checked LOD1 geometry"),Mesh->GetRenderData()->LODResources[1].GetNumTriangles(),3592u);
        TestEqual(TEXT("Checked LOD2 geometry"),Mesh->GetRenderData()->LODResources[2].GetNumTriangles(),2240u);
        TestEqual(TEXT("Five semantic sections"),Mesh->GetStaticMaterials().Num(),5);
        for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(TEXT("Explicit utility material resolves"),Slot.MaterialInterface.Get());
        const auto Bounds=Mesh->GetBoundingBox();
        TestTrue(TEXT("Body fits4x4 with approach margin"),Bounds.Min.X>-1600 && Bounds.Max.X<1600 && Bounds.Min.Y>-1600 && Bounds.Max.Y<1600);
        TestTrue(TEXT("Source grade pivot retained"),FMath::Abs(Bounds.Min.Z)<.1);
        const auto Bays=CityUtilityEntrances::Bays(Mesh->GetName());
        TestEqual(TEXT("Separate service portal"),Bays.Num(),1);
        TestTrue(TEXT("Personnel/service corridors separate"),FMath::Abs(Entry.EntryCm.X-Bays[0].EntryCm.X)>(Entry.WidthCm+Bays[0].WidthCm)*.5);
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityGroundTest,"CitySim.Terrain.UtilityGroundVariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUtilityGroundTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Board=World->SpawnActor<ACityBoardActor>();
    auto* Sim=World->GetSubsystem<UCitySimulationSubsystem>();
    Board->SimulationSubsystem=Sim;
    for(const auto& Entry:CityUtilityEntrances::All()) {
        if(FString(Entry.SupportMesh)!=TEXT("Nuclear_v6_Support")) continue;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*CityResidentialEntrances::ObjectPath(Entry));
        if(!TestNotNull(TEXT("Native utility mesh exists"),Mesh)) continue;
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];
        TArray<FVector> Portals={Entry.EntryCm,CityUtilityEntrances::Bays(Mesh->GetName())[0].EntryCm};
        for(const FVector& Portal:Portals) {
            double Distance=MAX_dbl;
            for(int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3) {
                const FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
                const FVector B(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
                const FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
                Distance=FMath::Min(Distance,FVector::Dist(Portal,FMath::ClosestPointOnTriangleToPoint(Portal,A,B,C)));
            }
            TestTrue(TEXT("Actual imported personnel/service leaf matches native anchor"),Distance<.1);
        }
        FCityBuildingDefinition Definition;
        Definition.DefinitionId=TEXT("NuclearPower");Definition.Footprint.Width=Definition.Footprint.Height=4;
        Definition.PresentationYawOffsetQuarterTurns=2;
        const FString Original=FString(TEXT("/Game/CitySim/Art/Meshes/"))+Entry.OriginalMesh;
        Definition.PresentationMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Original+TEXT(".")+Entry.OriginalMesh));
        Board->PlacementPreview.Anchor={18,16};
        Board->UpdateGhost(Definition,FCityPlacementResult());
        TestTrue(TEXT("Placement preview uses the same approved body"),Board->PlacementGhost->GetStaticMesh()==Mesh);
        const auto Resolved=Board->ResolvePresentationDefinition(0,Definition);
        TestEqual(TEXT("Both original lifecycle keys map to approved geometry"),Resolved.PresentationMesh.Get(),Mesh);
        const auto Regions=CityBuildingSupports::Find(Entry.TargetMesh);
        TestEqual(TEXT("Only structural support strips"),Regions.Num(),97);
        for(int32 Turn=0;Turn<4;++Turn) {
            const auto Pose=Board->MakeBuildingTransform({18,16},Resolved,Turn);
            TestEqual(TEXT("Ground-level utility uses5cm clearance"),Pose.GetLocation().Z-Board->GetBuildingBaseHeight({18,16},Resolved,Turn),5.);
            TArray<FTransform> Supports;
            CityBuildingSupports::MakeTransforms(Regions,Pose,Sim->GetTerrain(),Supports);
            TestEqual(TEXT("All measured supports survive rotation"),Supports.Num(),97);
            for(const FVector Center:{FVector(-800,-1010,0),FVector(800,-1010,0)}) {
                const FVector P=Pose.TransformPosition(Center);
                bool Filled=false;
                for(const auto& Support:Supports) {
                    const FVector Local=Support.InverseTransformPosition(P);
                    Filled |= FMath::Abs(Local.X)<50 && FMath::Abs(Local.Y)<50;
                }
                TestFalse(TEXT("Cooling shell centre stays open rather than a square plinth"),Filled);
            }
        }
    }
    World->DestroyWorld(false);
    return true;
}
#endif
#endif
