#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWindWindingTest,"CitySim.Presentation.WindOutwardSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWindWindingTest::RunTest(const FString&)
{
    // Signed volume uses render indices, independently of shading normals.
    // UE's reflected clockwise geometry has negative outward signed volume.
    struct FExpected {const TCHAR* Name; int32 Components[3];};
    const FExpected Expected[]={
        {TEXT("Meshes/SM_wind_turbine_v1"),{22,17,12}},
        {TEXT("Meshes/SM_wind_turbine_v1_Damaged"),{22,17,12}},
        {TEXT("Wind/SM_Wind_YawNacelle_v1"),{10,7,4}},
        {TEXT("Wind/SM_Wind_SpinRotor_v1"),{5,5,5}}};
    for(const auto& E:Expected)
    {
        const FString Path=FString(TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/"))+E.Name;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
        if(!TestNotNull(TEXT("Wind presentation mesh resolves"),Mesh)) continue;
        for(const auto& Slot:Mesh->GetStaticMaterials())
            if(TestNotNull(TEXT("Wind material resolves"),Slot.MaterialInterface.Get()))
                TestFalse(TEXT("Winding is corrected without a two-sided material"),Slot.MaterialInterface->IsTwoSided());
        if(!TestNotNull(TEXT("Wind render data resolves"),Mesh->GetRenderData())) continue;
        TestEqual(TEXT("Every wind LOD is present"),Mesh->GetNumLODs(),3);
        for(int32 LOD=0;LOD<3;++LOD)
        {
            const auto& Data=Mesh->GetRenderData()->LODResources[LOD];
            const auto Indices=Data.IndexBuffer.GetArrayView();
            TArray<FVector> Positions;TArray<int32> Parent;TArray<FIntVector> Triangles;
            // The differently materialed hub/spinner are separate closed
            // solids sharing a cap ring. Preserve their section identity.
            for(const auto& Section:Data.Sections)
            {
                TMap<FIntVector,int32> Welded;
                for(uint32 I=0;I<Section.NumTriangles;++I)
                {
                    FIntVector Triangle;
                    for(int32 Corner=0;Corner<3;++Corner)
                    {
                        const FVector P(Data.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+I*3+Corner]));
                        const FIntVector Key(FMath::RoundToInt32(P.X*1000),FMath::RoundToInt32(P.Y*1000),FMath::RoundToInt32(P.Z*1000));
                        int32* Existing=Welded.Find(Key);
                        if(!Existing) {const int32 ID=Positions.Add(P);Welded.Add(Key,ID);Parent.Add(ID);Existing=Welded.Find(Key);}
                        Triangle[Corner]=*Existing;
                    }
                    Triangles.Add(Triangle);
                }
            }
            auto Find=[&Parent](int32 I) {while(Parent[I]!=I) {Parent[I]=Parent[Parent[I]];I=Parent[I];} return I;};
            TMap<FIntPoint,int32> HalfEdges;
            for(const auto& Triangle:Triangles)
            {
                const int32 A=Triangle.X,B=Triangle.Y,C=Triangle.Z;
                Parent[Find(A)]=Find(B); Parent[Find(B)]=Find(C);
                ++HalfEdges.FindOrAdd(FIntPoint(A,B));++HalfEdges.FindOrAdd(FIntPoint(B,C));++HalfEdges.FindOrAdd(FIntPoint(C,A));
            }
            for(const auto& Edge:HalfEdges)
                TestTrue(TEXT("Every solid edge has exactly one oppositely wound neighbour"),Edge.Value==1 && HalfEdges.FindRef(FIntPoint(Edge.Key.Y,Edge.Key.X))==1);
            TMap<int32,FVector> Centers; TMap<int32,int32> Counts;
            for(int32 I=0;I<Positions.Num();++I) {const int32 Group=Find(I);Centers.FindOrAdd(Group,FVector::ZeroVector)+=Positions[I];++Counts.FindOrAdd(Group);}
            for(auto& Pair:Centers) Pair.Value/=Counts[Pair.Key];
            TMap<int32,double> Volumes;
            for(const auto& Triangle:Triangles)
            {
                const int32 A=Triangle.X,B=Triangle.Y,C=Triangle.Z,Group=Find(A);
                const FVector Center=Centers[Group];
                Volumes.FindOrAdd(Group)+=FVector::DotProduct(Positions[A]-Center,FVector::CrossProduct(Positions[B]-Center,Positions[C]-Center))/6.;
            }
            TestEqual(TEXT("Every separate solid remains present"),Volumes.Num(),E.Components[LOD]);
            for(const auto& Pair:Volumes)
                TestTrue(FString::Printf(TEXT("%s LOD%d solid%d has outward winding"),E.Name,LOD,Pair.Key),Pair.Value<-.01);
        }
    }
    return true;
}
#endif
