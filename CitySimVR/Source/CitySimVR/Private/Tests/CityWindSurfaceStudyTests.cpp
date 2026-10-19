#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Sandbox/CitySandboxEnvironment.h"

class FCityWindSurfaceStudyCommand : public IAutomationLatentCommand
{
public:
    explicit FCityWindSurfaceStudyCommand(FAutomationTestBase* InTest):Test(InTest) {}
    bool Update() override
    {
        if(FPlatformTime::Seconds()<Next) return false;
        auto* World=GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        if(!World) {if(++Attempts<120) {Next=FPlatformTime::Seconds()+.5;return false;} Test->AddError(TEXT("Wind study world missing"));return true;}
        if(!Camera)
        {
            // The entry menu hides late-created scenery every frame. Use the
            // same explicit city transition as the established ground study.
            for(TActorIterator<ACityVRToolController> It(World);It;++It) It->ReturnToCity();
            FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Directory);Directory/=TEXT("Wind273");
            IFileManager::Get().MakeDirectory(*Directory,true);
            for(TActorIterator<ACitySandboxEnvironment> It(World);It;++It) It->SetLightingPreset(ECityLightingPreset::Day);
            Camera=World->SpawnActor<ASceneCapture2D>(); auto* Capture=Camera->GetCaptureComponent2D();
            Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->CaptureSource=SCS_FinalColorLDR;Capture->FOVAngle=48;
            Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
            auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->InitCustomFormat(1200,900,PF_B8G8R8A8,false);Capture->TextureTarget=Target;
        }
        if(Stage==0)
        {
            auto* Capture=Camera->GetCaptureComponent2D();Capture->ClearShowOnlyComponents();
            for(auto* Part:Parts) Part->DestroyComponent();Parts.Empty();
            const int32 LOD=Case/4,Variant=Case%4;
            const FVector Base(0,0,20000);
            auto Add=[&](const TCHAR* Name,const FTransform& Pose)
            {
                auto* Part=NewObject<UStaticMeshComponent>(Camera);Part->SetMobility(EComponentMobility::Movable);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/"))+Name)));
                Test->TestNotNull(TEXT("Wind closeup mesh resolves"),Part->GetStaticMesh().Get());
                Part->SetForcedLodModel(LOD+1);Part->SetWorldTransform(Pose);Part->RegisterComponentWithWorld(World);Parts.Add(Part);Capture->ShowOnlyComponent(Part);
            };
            double Yaw=0;
            if(Variant<2)
            {
                Yaw=Variant*120.;
                Add(TEXT("Wind/SM_Wind_Stationary_v1"),FTransform(Base));
                const FTransform Nacelle(FRotator(0,Yaw,0),Base+FVector(0,0,2116.499996185303));
                Add(TEXT("Wind/SM_Wind_YawNacelle_v1"),Nacelle);
                const FTransform Rotor(FQuat(FVector(0,1,0),FMath::DegreesToRadians(Variant*73.)),FVector(0,290,83.500003814697));
                Add(TEXT("Wind/SM_Wind_SpinRotor_v1"),Rotor*Nacelle);
            }
            else Add(Variant==2?TEXT("Meshes/SM_wind_turbine_v1"):TEXT("Meshes/SM_wind_turbine_v1_Damaged"),FTransform(Base));
            const FVector Aim=Base+FVector(0,0,2200);
            const FVector Offset=FRotator(0,Yaw,0).RotateVector(FVector(Variant%2?900:-900,Variant%2?-1100:1100,650));
            Camera->SetActorLocation(Aim+Offset);Camera->SetActorRotation((-Offset).Rotation());
            Stage=1;Next=FPlatformTime::Seconds()+2;return false;
        }
        if(Stage==1) {Camera->GetCaptureComponent2D()->CaptureScene();Stage=2;Next=FPlatformTime::Seconds()+.6;return false;}
        TArray<FColor> Pixels;auto* Target=Camera->GetCaptureComponent2D()->TextureTarget.Get();
        Test->TestTrue(TEXT("Wind closeup readback"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        if(Pixels.Num()!=1200*900) return Finish();
        int32 Visible=0;for(const auto& P:Pixels) if(P.R>25 || P.G>25 || P.B>25) ++Visible;
        Test->TestTrue(TEXT("Wind closeup has visible geometry"),Visible>10000);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(1200,900,Pixels,Png);
        const TCHAR* Variants[]={TEXT("Moving-Front"),TEXT("Moving-Rear-Spin73"),TEXT("Whole-Inspector"),TEXT("Whole-Damaged")};
        const FString File=FString::Printf(TEXT("LOD%d-%s.png"),Case/4,Variants[Case%4]);
        Test->TestTrue(TEXT("Wind closeup saved"),FFileHelper::SaveArrayToFile(Png,*(Directory/File)));
        if(++Case==12) {Test->AddInfo(TEXT("WIND273_VISUAL_COMPLETE twelve closeups; three forced LODs; yaw/spin, whole and damaged"));return Finish();}
        Stage=0;return false;
    }
private:
    bool Finish() {for(auto* Part:Parts) Part->DestroyComponent();if(Camera) Camera->Destroy();return true;}
    FAutomationTestBase* Test;ASceneCapture2D* Camera=nullptr;TArray<UStaticMeshComponent*> Parts;
    FString Directory;int32 Case=0,Stage=0,Attempts=0;double Next=0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWindSurfaceStudyTest,"CitySim.Desktop.WindSurfaceStudy",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityWindSurfaceStudyTest::RunTest(const FString&)
{
    FString Profile;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CityGroundPlotStudy")) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),Profile) || !Profile.Contains(TEXT("EditorVisualTests")))
    {AddError(TEXT("Wind study needs the isolated CI visual profile"));return false;}
    ADD_LATENT_AUTOMATION_COMMAND(FCityWindSurfaceStudyCommand(this));return true;
}
#endif
