#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityCivicEntrances.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedCivicCookedTest,"CitySim.Presentation.ApprovedCivicBatchCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedCivicCookedTest::RunTest(const FString&)
{
    struct FExpected {const TCHAR* Package;int32 LODs;uint32 Triangles[3];};
    static const FExpected Expected[]={
#include "CityApprovedCivicMeshChecks.inl"
    };
    TestEqual(TEXT("Fifteen healthy and damaged approved designs"),static_cast<int32>(UE_ARRAY_COUNT(Expected)),30);
    for(const auto& E:Expected)
    {
        const FString Package=E.Package;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(Package+TEXT(".")+FPaths::GetBaseFilename(Package)));
        if(!TestNotNull(TEXT("Approved civic package resolves in this build"),Mesh)) continue;
        TestEqual(TEXT("All three prepared LODs survive cooking"),Mesh->GetNumLODs(),3);
        if(!TestNotNull(TEXT("Actual render data"),Mesh->GetRenderData())) continue;
        for(int32 LOD=0;LOD<3 && LOD<Mesh->GetRenderData()->LODResources.Num();++LOD)
            TestEqual(TEXT("Exact native render counts survive cooking"),Mesh->GetRenderData()->LODResources[LOD].GetNumTriangles(),E.Triangles[LOD]);
        for(const auto& Slot:Mesh->GetStaticMaterials())
        {
            if(TestNotNull(TEXT("Explicit material instance survives cooking"),Slot.MaterialInterface.Get()))
                TestFalse(TEXT("Opaque surfaces retain single-sided culling"),Slot.MaterialInterface->GetMaterial()->IsTwoSided());
        }
        const auto Plot=CityCivicEntrances::Footprint(Mesh->GetName());const auto B=Mesh->GetBoundingBox();
        TestTrue(TEXT("Complete detailed body fits its reserved specialized plot"),Plot.X>0 && Plot.Y>0 &&
            FMath::Max(FMath::Abs(B.Min.X),FMath::Abs(B.Max.X))<Plot.X*400. && FMath::Max(FMath::Abs(B.Min.Y),FMath::Abs(B.Max.Y))<Plot.Y*400.);
    }
    const auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Normal catalog"),Catalog)) return false;
    struct FSite {const TCHAR* Id;int32 X,Y;};
    const FSite Sites[]={{TEXT("PrimarySchool"),4,4},{TEXT("SecondarySchool"),5,5},{TEXT("College"),6,5},
        {TEXT("Police"),5,4},{TEXT("Fire"),5,4},{TEXT("Hospital"),5,5},{TEXT("WasteDepot"),6,5},{TEXT("Recycling"),6,5}};
    for(const auto& Site:Sites)
    {
        const auto* D=Catalog->FindDefinition(Site.Id);if(!TestNotNull(TEXT("Selected facility definition"),D)) continue;
        TestEqual(TEXT("Measured full-stage plot width"),D->Footprint.Width,Site.X);TestEqual(TEXT("Measured plot depth"),D->Footprint.Height,Site.Y);
        TestEqual(TEXT("Explicit approved-source yaw"),D->PresentationYawOffsetQuarterTurns,2);
        TestFalse(TEXT("Temporary construction presentation remains explicit"),D->ConstructionMesh.IsNull());
        TestTrue(TEXT("Existing economic/progression definition remains valid"),D->IsValid());
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityApprovedCivicGroundTest,"CitySim.Terrain.ApprovedCivicGround",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityApprovedCivicGroundTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Board=World->SpawnActor<ACityBoardActor>();
    auto* Sim=World->GetSubsystem<UCitySimulationSubsystem>();Board->SimulationSubsystem=Sim;
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Explicit catalog for the isolated preview world"),Catalog)) {World->DestroyWorld(false);return false;}
    Board->BuildingCatalog=Catalog;Sim->RegisterBuildingCatalog(Catalog);
    Board->PlacementModel=&Sim->GetPlacementModel();
    for(const FName Id:{FName(TEXT("PrimarySchool")),FName(TEXT("SecondarySchool")),FName(TEXT("College")),FName(TEXT("Police")),FName(TEXT("Fire")),FName(TEXT("Hospital")),FName(TEXT("WasteDepot")),FName(TEXT("Recycling"))})
    {
        const auto* D=Catalog->FindDefinition(Id);if(!TestNotNull(TEXT("Placement-preview definition"),D)) continue;
        const auto* Entry=CityCivicEntrances::Find(D->PresentationMesh.GetAssetName());if(!TestNotNull(TEXT("Preview has approved counterpart"),Entry)) continue;
        Board->PlacementPreview=Board->PlacementModel->BeginPreview(*D,{18,16});
        Board->UpdateGhost(*D,FCityPlacementResult{});
        auto* Expected=LoadObject<UStaticMesh>(nullptr,*CityResidentialEntrances::ObjectPath(*Entry));
        TestTrue(TEXT("Placement ghost uses the actual approved body instead of the legacy model"),Expected && Board->PlacementGhost->GetStaticMesh()==Expected);
    }
    TSet<FString> Checked;
    for(const auto& Entry:CityCivicEntrances::All())
    {
        const FString Path=CityResidentialEntrances::ObjectPath(Entry);if(Checked.Contains(Path)) continue;Checked.Add(Path);
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);if(!TestNotNull(TEXT("Actual detailed civic body"),Mesh)) continue;
        const auto Regions=CityBuildingSupports::Find(Mesh->GetName());TestTrue(TEXT("Building has structural-only concrete supports"),!Regions.IsEmpty());
        const auto Plot=CityCivicEntrances::Footprint(Mesh->GetName());
        for(const auto& R:Regions) TestTrue(TEXT("No support recreates the full parcel slab"),(R.MaxX-R.MinX)*(R.MaxY-R.MinY)<Plot.X*Plot.Y*640000.);
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];
        TArray<FVector> Portals={Entry.EntryCm};for(const auto& Bay:CityCivicEntrances::Bays(Mesh->GetName())) Portals.Add(Bay.EntryCm);
        for(const auto& Point:Portals)
        {
            double Distance=MAX_dbl;
            for(int32 I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3)
            {
                const FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I)));
                const FVector B(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+1)));
                const FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(LOD.IndexBuffer.GetIndex(I+2)));
                Distance=FMath::Min(Distance,FVector::Dist(Point,FMath::ClosestPointOnTriangleToPoint(Point,A,B,C)));
            }
            TestTrue(*FString::Printf(TEXT("%s entrance/bay centre meets its real multipart door (%.3fcm)"),*Mesh->GetName(),Distance),Distance<15.);
        }
        for(int32 Turn=0;Turn<4;++Turn)
        {
            FCityBuildingDefinition D;D.PresentationMesh=Mesh;D.Footprint.Width=Plot.X;D.Footprint.Height=Plot.Y;D.PresentationYawOffsetQuarterTurns=2;
            const auto Pose=Board->MakeBuildingTransform({18,16},D,Turn);
            TestEqual(TEXT("Shared ground-floor clearance"),Pose.GetLocation().Z-Board->GetBuildingBaseHeight({18,16},D,Turn),5.);
            TArray<FTransform> Supports;CityBuildingSupports::MakeTransforms(Regions,Pose,Sim->GetTerrain(),Supports);
            TestEqual(TEXT("Every measured enclosure/post retains its concrete support"),Supports.Num(),Regions.Num());
            for(const auto& S:Supports) TestTrue(TEXT("Rotated structural support remains positive"),S.GetScale3D().GetMin()>0.);
        }
        for(int32 Level=0;Level<Mesh->GetNumLODs();++Level)
        {
            const auto& R=Mesh->GetRenderData()->LODResources[Level];double Volume=0.;int32 Reversed=0;
            for(int32 I=0;I<R.IndexBuffer.GetNumIndices();I+=3)
            {
                const uint32 A=R.IndexBuffer.GetIndex(I),B=R.IndexBuffer.GetIndex(I+1),C=R.IndexBuffer.GetIndex(I+2);
                const FVector P(R.VertexBuffers.PositionVertexBuffer.VertexPosition(A)),Q(R.VertexBuffers.PositionVertexBuffer.VertexPosition(B)),T(R.VertexBuffers.PositionVertexBuffer.VertexPosition(C));
                const FVector N(R.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(A));
                // Native conversion reflects Y and reverses index order. Its
                // outward render winding is the opposite of source right-hand winding.
                const FVector Cross=FVector::CrossProduct(T-P,Q-P);Volume+=FVector::DotProduct(P,FVector::CrossProduct(T,Q))/6.;
                if(Cross.SizeSquared()>.001 && FVector::DotProduct(Cross,N)<-.01) ++Reversed;
            }
            TestEqual(TEXT("Actual render indices agree with imported outward normals"),Reversed,0);
            TestTrue(TEXT("Native whole solid signed volume is outward"),Volume>0.);
        }
    }
    TestEqual(TEXT("Every healthy/damaged design exercised"),Checked.Num(),30);World->DestroyWorld(false);return true;
}
#endif
#endif
