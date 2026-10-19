#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "StaticMeshResources.h"

namespace
{
const TCHAR* TechnologyNames[] = {TEXT("DigitalExchange"), TEXT("ComputingCenter"), TEXT("ResearchCampus"),
    TEXT("AIInstitute"), TEXT("EnterpriseAICenter")};
const int32 ApprovedTriangles[] = {1224, 3256, 3432, 3596, 1284};
UStaticMesh* TechnologyMesh(int32 Index)
{
    const FString Name = FString::Printf(TEXT("/Game/CitySim/Art/Technology/Meshes/SM_%s"), TechnologyNames[Index]);
    return LoadObject<UStaticMesh>(nullptr, *(Name + TEXT(".SM_") + TechnologyNames[Index]));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyAssetTest, "CitySim.Technology.ApprovedBuildingAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyAssetTest::RunTest(const FString&)
{
    auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if (!TestNotNull(TEXT("Current technology catalog"), Catalog)) return false;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const auto* Definition = Catalog->FindDefinition(TechnologyNames[Index]);
        auto* Mesh = TechnologyMesh(Index);
        if (!TestNotNull(TechnologyNames[Index], Definition) || !TestNotNull(TEXT("Approved distinct native mesh"), Mesh)) continue;
        TestEqual(TEXT("Catalog uses approved replacement"), Definition->PresentationMesh.LoadSynchronous(), Mesh);
        TestEqual(TEXT("Reviewed triangle count"), Mesh->GetNumTriangles(0), ApprovedTriangles[Index]);
        TestEqual(TEXT("Five explicit semantic regions"), Mesh->GetNumSections(0), 5);
        const FBox Bounds = Mesh->GetBoundingBox();
        TestTrue(TEXT("Ground pivot retained"), FMath::Abs(Bounds.Min.Z) < .2);
        TestTrue(TEXT("Fits existing logical lot"), Bounds.GetSize().X <= Definition->Footprint.Width * 800.0 &&
            Bounds.GetSize().Y <= Definition->Footprint.Height * 800.0);
        TestTrue(TEXT("Logical selection height follows mesh"), FMath::IsNearlyEqual(Definition->LogicalHeightCm, Bounds.GetSize().Z, .2));
        TestEqual(TEXT("Logical entrance retained"), Definition->EntranceSides, 1);
        TestEqual(TEXT("Checked FBX presentation orientation"), Definition->PresentationYawOffsetQuarterTurns, 2);
        TestTrue(TEXT("Approved UV channel survives import"), Mesh->GetRenderData()->LODResources[0].GetNumTexCoords() > 0);
        TestTrue(TEXT("Metric UV precision retained"), Mesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs());
        for (int32 Slot = 0; Slot < 5; ++Slot)
        {
            auto* Material = Mesh->GetMaterial(Slot);
            if (!TestNotNull(TEXT("Explicit native material"), Material)) continue;
            TestEqual(TEXT("Opaque technology material"), Material->GetBlendMode(), BLEND_Opaque);
            TestTrue(TEXT("No inherited stand-in/default material"), Material->GetPathName().StartsWith(TEXT("/Game/CitySim/Art/Technology/Materials/MI_")));
        }
    }
    return true;
}

class FCityTechnologyArtCapture : public IAutomationLatentCommand
{
public:
    explicit FCityTechnologyArtCapture(FAutomationTestBase* In) : Test(In) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() < Next) return false;
        auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        if (!World)
        {
            if (++Attempts < 120) { Next = FPlatformTime::Seconds() + .5; return false; }
            Test->AddError(TEXT("Technology render world unavailable")); return true;
        }
        if (!Camera)
        {
            World->GetSubsystem<UCitySimulationSubsystem>()->SetSimulationPaused(true);
            FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), Directory);
            if (Directory.IsEmpty()) Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
            Directory /= TEXT("TechnologyTechnologyArt"); IFileManager::Get().MakeDirectory(*Directory, true);
            Camera = World->SpawnActor<ASceneCapture2D>(); Building = World->SpawnActor<AStaticMeshActor>();
            Building->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Building->SetActorLocation(FVector(0, 0, 20000)); Building->SetActorRotation(FRotator(0, 180, 0));
            auto* Capture = Camera->GetCaptureComponent2D();
            Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
            Capture->FOVAngle = 50; Capture->CaptureSource = SCS_FinalColorLDR;
            Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->ShowOnlyComponent(Building->GetStaticMeshComponent());
            Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
            Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
            Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
            auto* Target = NewObject<UTextureRenderTarget2D>(Camera); Target->InitCustomFormat(1000, 900, PF_B8G8R8A8, false);
            Capture->TextureTarget = Target;
            for (TActorIterator<ACitySandboxEnvironment> It(World); It; ++It) It->SetLightingPreset(ECityLightingPreset::Day);
        }
        const int32 View = Phase / 3, Step = Phase % 3, Index = View < 5 ? View : 0;
        auto* Capture = Camera->GetCaptureComponent2D();
        if (Step == 0)
        {
            auto* Mesh = TechnologyMesh(Index);
            if (!Test->TestNotNull(TEXT("Cooked approved technology mesh loads"), Mesh)) return true;
            Building->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            Building->GetStaticMeshComponent()->PrestreamTextures(30.f, true, 0);
            const FBox Bounds = Building->GetComponentsBoundingBox(); const FVector Aim = Bounds.GetCenter();
            const double Size = Bounds.GetSize().GetMax();
            const FVector Offset = View == 5 ? FVector(0, -Size * 1.2, Size * .15) : FVector(Size, -Size * 1.8, Size * .9);
            Camera->SetActorLocation(Aim + Offset); Camera->SetActorRotation((Aim - Camera->GetActorLocation()).Rotation());
        }
        else if (Step == 1) Capture->CaptureScene();
        else
        {
            TArray<FColor> Pixels;
            if (!Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
            { Test->AddError(TEXT("Technology art pixel readback failed")); return true; }
            int32 Visible = 0; for (const auto& Pixel : Pixels) if (Pixel.R > 20 || Pixel.G > 20 || Pixel.B > 20) ++Visible;
            Test->TestTrue(TEXT("Technology asset has rendered surfaces"), Visible > 5000);
            TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1000, 900, Pixels, Png);
            const FString Label = FString(TechnologyNames[Index]) + (View == 5 ? TEXT("-Front") : TEXT("-Day"));
            Test->TestTrue(TEXT("Technology native capture saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / (Label + TEXT(".png")))));
        }
        if (++Phase == 18)
        {
            Camera->Destroy(); Building->Destroy();
            Test->AddInfo(TEXT("CITYSIM_TECHNOLOGY_TECHNOLOGY_ART_COMPLETE Captures=6 ApprovedMeshes=5 PhysicalAppearancePending=1")); return true;
        }
        Next = FPlatformTime::Seconds() + (Step == 0 ? 2.0 : .7); return false;
    }
private:
    FAutomationTestBase* Test; ASceneCapture2D* Camera = nullptr; AStaticMeshActor* Building = nullptr;
    FString Directory; int32 Phase = 0, Attempts = 0; double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTechnologyArtCaptureTest, "CitySim.Desktop.TechnologyBuildingAssets",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityTechnologyArtCaptureTest::RunTest(const FString&)
{
    FString User;
    if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) || !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), User) ||
        (!User.Contains(TEXT("Technology")) && !FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation"))))
    { AddError(TEXT("Technology art capture requires isolated Desktop automation profile")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCityTechnologyArtCapture(this)); return true;
}
#endif
