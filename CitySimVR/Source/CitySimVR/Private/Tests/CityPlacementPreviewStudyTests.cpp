#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Sandbox/CitySandboxEnvironment.h"

class FCityPlacementPreviewStudyCommand : public IAutomationLatentCommand
{
    struct FCase {const TCHAR* Name;const TCHAR* Id;int32 RoadTurn;bool Full;bool Manual;bool Overlay=false;};
    const TArray<FCase> Cases={{TEXT("Police-Partial-East"),TEXT("Police"),1,false,false},
        {TEXT("Police-Full-East"),TEXT("Police"),1,true,false},
        {TEXT("Police-Partial-North"),TEXT("Police"),2,false,false},
        {TEXT("Police-Manual-South"),TEXT("Police"),1,true,true},
        {TEXT("Fire-Partial-West"),TEXT("Fire"),3,false,false},
        {TEXT("WaterTreatment-Partial-North"),TEXT("Service"),2,false,false},
        {TEXT("School-Full-South"),TEXT("PrimarySchool"),0,true,false},
        {TEXT("Police-Overlay-Partial-East"),TEXT("Police"),1,false,false,true}};
public:
    explicit FCityPlacementPreviewStudyCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Next) return false;
        auto* W=GEngine && GEngine->GameViewport?GEngine->GameViewport->GetWorld():nullptr;
        if(!W) {Test->AddError(TEXT("Preview study world missing"));return true;}
        if(!Board)
        {
            for(TActorIterator<ACityBoardActor> It(W);It;++It) {Board=*It;break;}
            if(!Test->TestNotNull(TEXT("Preview study board"),Board)) return true;
            for(TActorIterator<ACityVRToolController> It(W);It;++It) It->ReturnToCity();
            for(TActorIterator<ACitySandboxEnvironment> It(W);It;++It) It->SetLightingPreset(ECityLightingPreset::Day);
            Sim=W->GetSubsystem<UCitySimulationSubsystem>();Sim->SetSimulationPaused(true);
            OriginalTick=Board->IsActorTickEnabled();Board->SetActorTickEnabled(false);
            FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Directory);Directory/=TEXT("PlacementPreview");
            IFileManager::Get().MakeDirectory(*Directory,true);
            Camera=W->SpawnActor<ASceneCapture2D>();auto* Capture=Camera->GetCaptureComponent2D();
            Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=SCS_FinalColorLDR;
            Capture->FOVAngle=48;Capture->bOverride_CustomNearClippingPlane=true;Capture->CustomNearClippingPlane=.1f;
            Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
            Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
            auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->InitCustomFormat(1200,900,PF_B8G8R8A8,false);Capture->TextureTarget=Target;
            TInlineComponentArray<UPrimitiveComponent*> Components(Board);for(auto* C:Components) Capture->ShowOnlyComponent(C);
        }
        if(Stage==0)
        {
            auto Params=Sim->Terrain.GetParameters();Params.GeneratorVersion=0;Params.WaterRulesVersion=0;
            Params.BroadElevationAmplitudeCm=Params.HillAmplitudeCm=Params.RidgeAmplitudeCm=0;
            Sim->Terrain.Generate(Params);Sim->ResetCity(false);Sim->SetSimulationPaused(true);
            Sim->RegisterBuildingCatalog(Board->BuildingCatalog);auto& City=Sim->GetPlacementModel();City.ApplyMonthlyEconomy(1000000,0);
            const auto& C=Cases[Index];const auto* D=Board->BuildingCatalog->FindDefinition(C.Id);
            if(!Test->TestNotNull(TEXT("Study definition"),D)) return Finish();
            const FCityCellCoord Anchor(18,16);const auto Size=D->Footprint.GetRotatedSize(C.RoadTurn);
            const auto Front=[&](int32 I) {return C.RoadTurn==0?FCityCellCoord(18+I,15):C.RoadTurn==1?FCityCellCoord(18+Size.X,16+I):
                C.RoadTurn==2?FCityCellCoord(18+I,16+Size.Y):FCityCellCoord(17,16+I);};
            auto R=City.BeginRoadPreview(Front(C.Full?0:1));City.UpdateRoadPreviewEnd(R,Front(C.Full?(C.RoadTurn%2?Size.Y:Size.X)-1:2));
            if(!Test->TestTrue(TEXT("Study entrance road respects normal validation"),City.CommitRoad(R).IsSuccess())) return Finish();
            Board->PrepareCityView();
            Board->SetDiagnosticOverlay(C.Overlay ? ECityDiagnosticOverlay::Crime : ECityDiagnosticOverlay::None);
            Board->PlacementPreview=City.BeginRoadFacingPreview(*D,Anchor);
            Test->TestEqual(TEXT("Road attracts preview before explicit rotation"),Board->PlacementPreview.QuarterTurnsClockwise,C.RoadTurn);
            if(C.Manual) {City.RotatePreviewClockwise(Board->PlacementPreview);City.RotatePreviewClockwise(Board->PlacementPreview);City.RotatePreviewClockwise(Board->PlacementPreview);}
            const auto Result=City.ValidatePlacement(Board->PlacementPreview,*D);
            Test->TestEqual(TEXT("Only complete correctly oriented frontage is valid"),Result.IsSuccess(),C.Full && !C.Manual);
            Board->UpdateGhost(*D,Result);Board->UpdatePlacementStatus(*D,Result);
            const auto Plot=D->Footprint.GetRotatedSize(Board->PlacementPreview.QuarterTurnsClockwise);
            const auto Aim=FVector((18+Plot.X*.5)*800,(16+Plot.Y*.5)*800,250);
            const double Span=FMath::Max(Plot.X,Plot.Y)*800.;const auto Pose=Board->GetActorTransform();
            const auto Position=Pose.TransformPosition(Aim+FVector(Span*.8,-Span*.9,Span*.85));
            Camera->SetActorLocation(Position);Camera->SetActorRotation((Pose.TransformPosition(Aim)-Position).Rotation());
            FFileHelper::SaveStringToFile(Board->GetConstructionStatus(),*(Directory/(FString(C.Name)+TEXT(".txt"))));
            Stage=1;Next=FPlatformTime::Seconds()+1.5;return false;
        }
        if(Stage==1) {Camera->GetCaptureComponent2D()->CaptureScene();Stage=2;Next=FPlatformTime::Seconds()+.5;return false;}
        TArray<FColor> Pixels;Camera->GetCaptureComponent2D()->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        if(!Test->TestEqual(TEXT("Preview capture pixels"),Pixels.Num(),1200*900)) return Finish();
        int32 HighlightPixels=0;
        const bool Valid=Cases[Index].Full && !Cases[Index].Manual;
        for(const auto& P:Pixels)
            HighlightPixels += Valid ? (P.G>150 && P.G>P.R*1.5) : (P.R>150 && P.R>P.G*1.5);
        Test->TestTrue(TEXT("Actual rendered perimeter has the correct valid/blocked color"),HighlightPixels>100);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(1200,900,Pixels,Png);
        FFileHelper::SaveArrayToFile(Png,*(Directory/(FString(Cases[Index].Name)+TEXT(".png"))));
        if(++Index==Cases.Num()) return Finish();Stage=0;return false;
    }
private:
    bool Finish(){if(Camera)Camera->Destroy();if(Board){Board->SetToolMode(ECityToolMode::Inspect);Board->SetActorTickEnabled(OriginalTick);}return true;}
    FAutomationTestBase* Test;ACityBoardActor* Board=nullptr;UCitySimulationSubsystem* Sim=nullptr;ASceneCapture2D* Camera=nullptr;
    FString Directory;bool OriginalTick=true;int32 Stage=0,Index=0;double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlacementPreviewStudyTest,"CitySim.Desktop.PlacementPreviewStudy",EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityPlacementPreviewStudyTest::RunTest(const FString&)
{
    FString Profile;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests"))) {AddError(TEXT("Preview study requires isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityPlacementPreviewStudyCommand(this));return true;
}
#endif
