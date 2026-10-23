#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlacementFeedbackTest,"CitySim.Placement.FootprintFacingAndReason",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityPlacementFeedbackTest::RunTest(const FString&)
{
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if(!TestNotNull(TEXT("Current catalog"),Catalog)) return false;
    for(const FName Id : {FName(TEXT("Police")),FName(TEXT("Fire")),FName(TEXT("PrimarySchool")),FName(TEXT("Service"))})
    {
        const auto* D=Catalog->FindDefinition(Id);
        if(!TestNotNull(TEXT("Placed facility"),D)) return false;
        for(int32 Turn=0;Turn<4;++Turn)
        {
            FCityPlacementModel City(1000000,FCityGridDimensions(64,64));
            const FCityCellCoord Anchor(18,16);const FIntPoint Size=D->Footprint.GetRotatedSize(Turn);
            const auto Front=[&](int32 I) {return Turn==0?FCityCellCoord(18+I,15):Turn==1?FCityCellCoord(18+Size.X,16+I):
                Turn==2?FCityCellCoord(18+I,16+Size.Y):FCityCellCoord(17,16+I);};
            const int32 Length=Turn%2?Size.Y:Size.X;
            const auto Road=[&](FCityCellCoord A,FCityCellCoord B) {auto R=City.BeginRoadPreview(A);City.UpdateRoadPreviewEnd(R,B);return City.CommitRoad(R).IsSuccess();};
            TestTrue(TEXT("Partial facade road"),Road(Front(1),Front(2)));
            auto P=City.BeginRoadFacingPreview(*D,Anchor);
            TestEqual(TEXT("Partial road still attracts the authored entrance"),P.QuarterTurnsClockwise,Turn);
            TestEqual(TEXT("Partial facade still rejected authoritatively"),City.ValidatePlacement(P,*D).Failure,ECityPlacementFailure::FullRoadFrontage);
            TestTrue(TEXT("Rejection names complete entrance frontage"),FCityPlacementModel::FailureToString(ECityPlacementFailure::FullRoadFrontage).Contains(TEXT("entrance")));
            TestTrue(TEXT("Finish road along facade"),Road(Front(0),Front(Length-1)));
            TestEqual(TEXT("Unreviewed preview rejects an intervening road edit"),City.ValidatePlacement(P,*D).Failure,ECityPlacementFailure::StalePreview);
            City.UpdateRoadFacingPreviewAnchor(P,*D,Anchor);
            TestEqual(TEXT("Hover rechecks the latest city revision"),P.ExpectedRevision,City.GetRevision());
            TestEqual(TEXT("Full road preserves correct entrance"),P.QuarterTurnsClockwise,Turn);
            TestTrue(TEXT("Full frontage permits the same preview"),City.ValidatePlacement(P,*D).IsSuccess());
            City.RotatePreviewClockwise(P);const int32 Manual=P.QuarterTurnsClockwise;
            City.UpdateRoadFacingPreviewAnchor(P,*D,{19,17});
            TestEqual(TEXT("Explicit rotation remains a player override"),P.QuarterTurnsClockwise,Manual);
        }
    }
    const UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("PlacementFeedback"),nullptr,true,ERHIFeatureLevel::Num,&Values);
    auto* Board=World->SpawnActor<ACityBoardActor>();
    FCityPlacementModel City(1000000,FCityGridDimensions(64,64));
    Board->PlacementModel=&City;Board->BuildingCatalog=Catalog;
    Board->SimulationSubsystem=World->GetSubsystem<UCitySimulationSubsystem>();
    Board->SetToolMode(ECityToolMode::Police);
    const auto* D=Catalog->FindDefinition(TEXT("Police"));
    Board->PlacementPreview=City.BeginPreview(*D,{18,16},1);
    const auto Invalid=City.ValidatePlacement(Board->PlacementPreview,*D);
    Board->UpdateGhost(*D,Invalid);Board->UpdatePlacementStatus(*D,Invalid);
    const auto Size=D->Footprint.GetRotatedSize(1);
    TestTrue(TEXT("Reserved plot outline visible with ghost"),Board->PlacementFootprintInstances->IsVisible());
    TestEqual(TEXT("Every rotated footprint edge has four terrain samples per tile"),Board->PlacementFootprintInstances->GetInstanceCount(),8*(Size.X+Size.Y));
    const auto BlockedColor=Board->PlacementFootprintMaterial->K2_GetVectorParameterValue(TEXT("BarColor"));
    TestTrue(TEXT("Blocked perimeter uses red highlight"),BlockedColor.R>BlockedColor.G);
    Board->UpdatePlacementFootprint(*D,true);
    const auto ReadyColor=Board->PlacementFootprintMaterial->K2_GetVectorParameterValue(TEXT("BarColor"));
    TestTrue(TEXT("Valid perimeter uses green highlight"),ReadyColor.G>ReadyColor.R);
    Board->UpdatePlacementFootprint(*D,false);
    TestTrue(TEXT("Footprint cannot interfere with picking"),Board->PlacementFootprintInstances->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    Board->DiagnosticOverlay=ECityDiagnosticOverlay::Crime;
    Board->UpdatePlacementFootprint(*D,false);
    FTransform Edge;Board->PlacementFootprintInstances->GetInstanceTransform(0,Edge,false);
    const auto Position=Edge.GetLocation();
    TestTrue(TEXT("Footprint stays above raised diagnostic surface"),Position.Z >
        Board->SimulationSubsystem->GetTerrain().SampleHeightCm(Position.X/800.,Position.Y/800.)+300.);
    Board->HeatMapResult.Overlay=ECityDiagnosticOverlay::Crime;
    Board->HeatMapResult.Description=TEXT("Crime coverage");
    const auto Reason=FCityPlacementModel::FailureToString(Invalid.Failure);
    TestTrue(TEXT("Police overlay does not hide placement rejection"),Board->DescribeDesktopToolContext().StartsWith(FString::Printf(TEXT("$%lld | %s"),D->Cost,*Reason)));
    for(const auto Overlay:{ECityDiagnosticOverlay::Service,ECityDiagnosticOverlay::Health,ECityDiagnosticOverlay::Power,ECityDiagnosticOverlay::Education})
    {
        Board->HeatMapResult.Overlay=Overlay;
        TestTrue(TEXT("Other overlays retain first-line rejection"),Board->DescribeDesktopToolContext().Contains(Reason));
    }
    Board->SetToolMode(ECityToolMode::Inspect);
    TestFalse(TEXT("Switching tools clears the footprint"),Board->PlacementFootprintInstances->IsVisible());
    Board->PlacementModel=nullptr;
    World->DestroyWorld(false);
    return true;
}
#endif
