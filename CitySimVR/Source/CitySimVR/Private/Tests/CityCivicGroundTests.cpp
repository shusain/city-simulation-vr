#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityCivicEntrances.h"
#include "Serialization/JsonSerializer.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "StaticMeshResources.h"
#include "ProceduralMeshComponent.h"
#include "Presentation/CityRoadPresentation.h"
#include "Presentation/CityRoadRibbon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCivicGroundCookedTest, "CitySim.Presentation.CivicGroundCooked",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityCivicGroundCookedTest::RunTest(const FString&)
{
    TSet<FString> Targets;
    for(const auto& Entry:CityCivicEntrances::All()) Targets.Add(CityResidentialEntrances::ObjectPath(Entry));
    TestEqual(TEXT("All civic presentations"),Targets.Num(),30);
    for(const auto& Target:Targets)
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Target);
        if(!TestNotNull(TEXT("Cooked civic body resolves"),Mesh)) continue;
        TestTrue(TEXT("Cooked body has explicit materials"),!Mesh->GetStaticMaterials().IsEmpty());
        for(const auto& Slot:Mesh->GetStaticMaterials()) TestNotNull(TEXT("Cooked civic material resolves"),Slot.MaterialInterface.Get());
        const auto Plot=CityCivicEntrances::Footprint(Mesh->GetName());
        TestTrue(TEXT("Civic plot metadata resolves"),Plot.X>0 && Plot.Y>0);
        const auto Bounds=Mesh->GetBoundingBox();
        TestTrue(TEXT("Complete civic body fits the measured specialized plot"),
            FMath::Max(FMath::Abs(Bounds.Min.X),FMath::Abs(Bounds.Max.X))<=Plot.X*400.+.1 &&
            FMath::Max(FMath::Abs(Bounds.Min.Y),FMath::Abs(Bounds.Max.Y))<=Plot.Y*400.+.1);
    }
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCivicGroundTest, "CitySim.Terrain.CivicSidewalkCaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCivicGroundTest::RunTest(const FString&)
{
    // Detailed source/contact coverage now lives in ApprovedCivicGround.
    // Retain the independently useful public sidewalk cap regression.
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Board=World->SpawnActor<ACityBoardActor>();
    Board->SimulationSubsystem=World->GetSubsystem<UCitySimulationSubsystem>();
    // A frontage can meet the closed end of a one-neighbor road. Verify
    // actual generated sidewalk triangles in every orientation, not its
    // height sampler's fallback when there is no paving beneath the point.
    auto& City=Board->SimulationSubsystem->GetPlacementModel();
    City.SetTerrain(nullptr);
    Board->PlacementModel=&City;
    const FCityCellCoord Center(20,20);
    const FCityCellCoord Ends[]={{20,16},{24,20},{20,24},{16,20}};
    for(const auto& End:Ends) {
        auto Preview=City.BeginRoadPreview(Center);City.UpdateRoadPreviewEnd(Preview,End);
        TestTrue(TEXT("Four-orientation sidewalk cap fixture road commits"),City.CommitRoad(Preview).IsSuccess());
    }
    Board->RebuildRoadInstances();
    const auto* Public=Board->SidewalkSurface->GetProcMeshSection(0);
    if(TestNotNull(TEXT("Dead-end sidewalks render"),Public)) for(const auto& End:Ends) {
        const FVector Forward=FVector(End.X-Center.X,End.Y-Center.Y,0).GetSafeNormal();
        const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
        const double Offset=(CityRoadPresentation::SurfaceWidth(TEXT("LocalRoad"))+
            CityRoadPresentation::SidewalkWidth(TEXT("LocalRoad")))*400.;
        for(const double Across:{-240.,0.,240.}) {
            FVector P=FVector((End.X+.5)*800.,(End.Y+.5)*800.,0)+Forward*Offset+Right*Across;
            P.Z=Board->GetRoadProfileHeight(P.X/800.,P.Y/800.)+FCityRoadRibbonMesh::SidewalkLiftCm;
            bool OnPaving=false;
            for(int32 I=0;I<Public->ProcIndexBuffer.Num();I+=3) {
                const FVector A=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I]].Position,
                    B=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+1]].Position,
                    C=Public->ProcVertexBuffer[Public->ProcIndexBuffer[I+2]].Position;
                if(FMath::Abs(FVector::CrossProduct(B-A,C-A).Z)<.001) continue;
                const auto W=FMath::ComputeBaryCentric2D(P,A,B,C);
                OnPaving |= W.X>=-.001 && W.Y>=-.001 && W.Z>=-.001 &&
                    FMath::Abs(P.Z-(W.X*A.Z+W.Y*B.Z+W.Z*C.Z))<.1;
            }
            TestTrue(*FString::Printf(TEXT("Closed road end%d,%d supports actual cap across%.0f"),End.X,End.Y,Across),OnPaving);
        }
    }
	World->DestroyWorld(false);
	return true;
}
#endif

#endif
