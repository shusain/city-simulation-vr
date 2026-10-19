#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SceneCapture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "EngineUtils.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Widgets/Images/SImage.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStorefrontRoomAssetTest, "CitySim.Art.StorefrontRoomReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityStorefrontRoomAssetTest::RunTest(const FString& Parameters)
{
    const TCHAR* Names[] = {TEXT("Housing"), TEXT("Commerce"), TEXT("Automation")};
    for (const auto* Name : Names)
    {
        auto* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/CitySim/UI/Newspaper/T_%s.T_%s"), Name, Name));
        if (!TestNotNull(TEXT("Pinned newspaper texture is available, including cooked builds"), Texture))
            return false;
        TestTrue(TEXT("Ink image retains useful resolution"), Texture->GetSizeX() >= 500 && Texture->GetSizeY() >= 350);
        TestEqual(TEXT("UI ink has one mip"), Texture->GetNumMips(), 1);
#if WITH_EDITORONLY_DATA
        TArray64<uint8> SourcePixels;
        if (TestTrue(TEXT("Imported source pixels are readable"), Texture->Source.GetMipData(SourcePixels, 0)))
        {
            int32 Clear = 0, Partial = 0, Solid = 0;
            for (int64 Index = 3; Index < SourcePixels.Num(); Index += 4)
            {
                const uint8 Alpha = SourcePixels[Index];
                if (Alpha == 0) ++Clear;
                else if (Alpha == 255) ++Solid;
                else ++Partial;
            }
            TestTrue(TEXT("Imported newspaper retains clear paper and soft ink alpha"), Clear > 1000 && Partial > 1000 && Solid > 1000);
        }
#endif
    }
    const TCHAR* Businesses[] = {TEXT("Market"), TEXT("Bakery"), TEXT("Cafe"), TEXT("Books"),
        TEXT("Pharmacy"), TEXT("Repairs"), TEXT("Outfitters"), TEXT("Tech")};
    TMap<FName, UMaterialInterface*> Rooms;
    auto* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/CitySim/Art/Materials/M_StorefrontRoomWindows.M_StorefrontRoomWindows"));
    if (!TestNotNull(TEXT("Shared instanced shop-window master is cooked"), Master)) return false;
    for (const auto* Business : Businesses)
    {
        const FString Key = FString(Business).ToLower();
        const FString MaterialPath = TEXT("/Game/CitySim/Art/Materials/MI_StorefrontRoom_") + Key;
        auto* Material = LoadObject<UMaterialInterface>(nullptr, *(MaterialPath + TEXT(".") + FPaths::GetBaseFilename(MaterialPath)));
        if (!TestNotNull(TEXT("Each business has its approved room material"), Material)) return false;
        Rooms.Add(FName(*Key), Material);
        const FString TexturePath = TEXT("/Game/CitySim/Art/Textures/T_StorefrontRoom_") + Key;
        auto* Texture = LoadObject<UTexture2D>(nullptr, *(TexturePath + TEXT(".") + FPaths::GetBaseFilename(TexturePath)));
        if (!TestNotNull(TEXT("Each approved room texture is cooked"), Texture)) return false;
#if WITH_EDITOR
        TArray<UTexture*> Textures = {Texture};
        FTextureCompilingManager::Get().FinishCompilation(Textures);
        FlushRenderingCommands();
#endif
        TestEqual(TEXT("Mip-safe room atlas width"), Texture->GetSizeX(), 512);
        TestEqual(TEXT("Mip-safe room atlas height"), Texture->GetSizeY(), 1024);
        TestTrue(TEXT("Room art retains mipmaps for city distance"), Texture->GetNumMips() > 1);
        for (const auto* Density : {TEXT("Low"), TEXT("Medium"), TEXT("High")})
        {
            const FString Name = FString::Printf(TEXT("/Game/CitySim/Art/Meshes/SM_Commercial%s_Store%s"), Density, Business);
            auto* Mesh = LoadObject<UStaticMesh>(nullptr, *(Name + TEXT(".") + FPaths::GetBaseFilename(Name)));
            if (!TestNotNull(TEXT("Every signed shop density is cooked"), Mesh)) return false;
            TestEqual(TEXT("Five semantic material slots retained"), Mesh->GetStaticMaterials().Num(), 5);
            const int32 Slot = Mesh->GetMaterialIndex(TEXT("BuildingWindowOpaque"));
            if (!TestTrue(TEXT("Window semantic slot retained"), Slot != INDEX_NONE)) return false;
            TestEqual(TEXT("Correct approved business room at every density"), Mesh->GetMaterial(Slot), Material);
            const FString DamagedName = Name + TEXT("_Damaged");
            auto* Damaged = LoadObject<UStaticMesh>(nullptr, *(DamagedName + TEXT(".") + FPaths::GetBaseFilename(DamagedName)));
            if (TestNotNull(TEXT("Signed damaged variant retained"), Damaged))
                for (const auto& DamageSlot : Damaged->GetStaticMaterials())
                    TestFalse(TEXT("Damaged shops never regain room displays"), DamageSlot.MaterialInterface && DamageSlot.MaterialInterface->GetName().StartsWith(TEXT("MI_StorefrontRoom_")));
        }
    }
    auto* Catalog = LoadObject<UCityBuildingCatalog>(nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
    if (!TestNotNull(TEXT("Runtime building catalog loads"), Catalog)) return false;
    int32 SignedAppearances = 0;
    for (const auto& Variant : Catalog->LowDensityPresentationVariants)
    {
        if (Variant.DefinitionId != TEXT("Shop")) continue;
        for (const auto* Business : Businesses)
        {
            const FName Key(*FString(Business).ToLower());
            const auto* Path = Variant.BusinessMeshes.Find(Key);
            if (!TestNotNull(TEXT("Each active appearance retains every signed business"), Path)) continue;
            auto* Mesh = Path->LoadSynchronous();
            if (!TestNotNull(TEXT("Actual runtime business mesh is cooked"), Mesh)) continue;
            TestEqual(TEXT("Active appearance uses corresponding room"), Mesh->GetMaterial(Mesh->GetMaterialIndex(TEXT("BuildingWindowOpaque"))), Rooms[Key]);
            ++SignedAppearances;
        }
        auto* Damaged = Variant.DamagedMesh.LoadSynchronous();
        if (TestNotNull(TEXT("Actual low-density damage remains cooked"), Damaged))
            for (const auto& Slot : Damaged->GetStaticMaterials())
                TestFalse(TEXT("Low-density damage remains charred"), Slot.MaterialInterface && Slot.MaterialInterface->GetName().StartsWith(TEXT("MI_StorefrontRoom_")));
    }
    TestEqual(TEXT("All four active appearances across all eight businesses"), SignedAppearances, 32);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNewspaperRenderingTest, "CitySim.Art.NewspaperRendering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityNewspaperRenderingTest::RunTest(const FString& Parameters)
{
    TArray<UTexture*> InkTextures;
    for (const auto* Name : {TEXT("Housing"), TEXT("Commerce"), TEXT("Automation")})
        InkTextures.Add(LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/CitySim/UI/Newspaper/T_%s.T_%s"), Name, Name)));
    for (auto* Texture : InkTextures)
        if (!TestNotNull(TEXT("Ink texture loads before rendering"), Texture)) return false;
#if WITH_EDITOR
    FTextureCompilingManager::Get().FinishCompilation(InkTextures);
#endif
    FlushRenderingCommands();
    // Prove actual GPU ink coverage, not just successful asset-path loads.
    for (auto* Texture : InkTextures)
    {
        FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageType = ESlateBrushImageType::FullColor;
        auto Image = SNew(SImage).Image(&Brush);
        FWidgetRenderer Renderer(false);
        auto* Target = Renderer.DrawWidget(Image, FVector2D(256,256));
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
        if (!Target || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))
        { AddError(TEXT("Native ink readback failed")); return false; }
        int32 InkPixels = 0, ClearPixels = 0;
        for (const auto& Pixel : Pixels)
        {
            if (Pixel.A > 32) ++InkPixels;
            if (Pixel.A < 8) ++ClearPixels;
        }
        TestTrue(TEXT("GPU ink image has visible strokes and transparent paper"), InkPixels > 1000 && ClearPixels > 1000);
    }
    auto Content = MakeShared<FCityUiNewspaper>();
    Content->Masthead = FText::FromString(TEXT("The City Chronicle"));
    Content->Dateline = FText::FromString(TEXT("ILLUSTRATION REVIEW | GENERIC EDITORIAL ART"));
    Content->Headline = FText::FromString(TEXT("Everyday life, in ink"));
    Content->Lead = FText::FromString(TEXT("Homes, trade and skilled work illustrate city life. Reporting continues to use recorded city facts."));
    Content->Footer = FText::FromString(TEXT("Newspaper native renderer review; this page contains no live city statistics."));
    Content->Articles.Add({FText::FromString(TEXT("HOUSING")), FText::FromString(TEXT("Homes and neighborhoods")), FText::FromString(TEXT("Housing reports describe actual recorded residents and movement."))});
    Content->Articles.Add({FText::FromString(TEXT("COMMERCE")), FText::FromString(TEXT("Work and trade")), FText::FromString(TEXT("Illustrations do not establish actual stock, purchases or technology adoption."))});
    auto* Font = LoadObject<UFont>(nullptr, TEXT("/Game/CitySim/UI/Fonts/F_CityNewsSerif.F_CityNewsSerif"));
    FString Directory;
    FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), Directory);
    if (Directory.IsEmpty())
        Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
    Directory /= TEXT("PresentationArt");
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const int32 Width : {920, 700, 1000})
    {
        const float FontScale = Width == 1000 ? .82f : 1.f;
        auto Sheet = CityNewspaper::MakeSheet(Content, Font, Width, FontScale);
        Sheet->SlatePrepass();
        const int32 Height = FMath::CeilToInt(Sheet->GetDesiredSize().Y);
        TestTrue(TEXT("Native paper layout remains bounded"), Height > 200 && Height < 1200);
        if (Width == 1000)
            TestTrue(TEXT("World-space review fits the existing fixed sheet"), Height <= 760);
        // Slate vertex colors already carry display-space ink. Avoid applying
        // the offscreen renderer's extra gamma conversion to the PNG proof.
        FWidgetRenderer Renderer(false);
        auto* Target = Renderer.DrawWidget(Sheet, FVector2D(Width, Height));
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags(RCM_UNorm); Flags.SetLinearToGamma(false);
        if (!Target || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))
        {
            AddError(TEXT("Newspaper render readback failed"));
            return false;
        }
        int32 DarkInkPixels = 0;
        for (const auto& Pixel : Pixels)
            if (Pixel.R < 120 && Pixel.G < 120 && Pixel.B < 120) ++DarkInkPixels;
        TestTrue(TEXT("Paper capture preserves dark ink contrast"), DarkInkPixels > 1000);
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
        TestTrue(TEXT("Native newspaper capture saved"), FFileHelper::SaveArrayToFile(Png, *(Directory / FString::Printf(TEXT("Newspaper-%d.png"), Width))));
    }
    return true;
}
class FCityStorefrontRoomsCommand : public IAutomationLatentCommand
{
public:
    explicit FCityStorefrontRoomsCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() < Next)
            return false;
        auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        if (!World)
        {
            if (++Attempts < 120) { Next = FPlatformTime::Seconds() + .5; return false; }
            Test->AddError(TEXT("Storefront capture requires an initialized game world"));
            return true;
        }
        if (!Camera)
        {
            // Match the existing packaged reflection fixture: entry mode
            // suspends the world until the controller returns to the city.
            for (TActorIterator<ACityVRToolController> It(World); It; ++It)
            { It->ReturnToCity(); break; }
            World->GetSubsystem<UCitySimulationSubsystem>()->SetEntryMenuSuspended(false);
            World->GetSubsystem<UCitySimulationSubsystem>()->SetSimulationPaused(true);
            FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), Directory);
            if (Directory.IsEmpty()) Directory = FPaths::ProjectSavedDir() / TEXT("Automation");
            Directory /= TEXT("PresentationArt");
            IFileManager::Get().MakeDirectory(*Directory, true);
            Camera = World->SpawnActor<ASceneCapture2D>();
            Shop = World->SpawnActor<AStaticMeshActor>();
            Shop->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Shop->SetActorLocation(FVector(0, 0, 20000));
            Shop->SetActorRotation(FRotator(0, 180, 0));
            auto* Capture = Camera->GetCaptureComponent2D();
            Capture->bCaptureEveryFrame = false;
            Capture->bCaptureOnMovement = false;
            Capture->ProjectionType = ECameraProjectionMode::Perspective;
            Capture->FOVAngle = 50;
            Capture->CaptureSource = SCS_FinalColorLDR;
            Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->ShowOnlyComponent(Shop->GetStaticMeshComponent());
            Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
            Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
            Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
            auto* Target = NewObject<UTextureRenderTarget2D>(Camera);
            Target->InitCustomFormat(1000, 900, PF_B8G8R8A8, false);
            Capture->TextureTarget = Target;
        }
        const TCHAR* Densities[] = {TEXT("Low"), TEXT("Medium"), TEXT("High")};
        const TCHAR* Businesses[] = {TEXT("market"), TEXT("bakery"), TEXT("cafe"), TEXT("books"),
            TEXT("pharmacy"), TEXT("repairs"), TEXT("outfitters"), TEXT("tech")};
        const int32 View = Phase / 3, Step = Phase % 3;
        const bool Close = View >= 24 && View < 40;
        const bool Night = View >= 40 && View < 43;
        const int32 Business = View < 24 ? View / 3 : (Close ? (View - 24) / 2 : 5);
        const int32 Density = View < 24 ? View % 3 : (Close ? (View - 24) % 2 + 1 : (Night ? View - 40 : 0));
        const TCHAR Variant = static_cast<TCHAR>('A' + (View >= 43 ? View - 42 : 0));
        const FString Key = Businesses[Business];
        FString Title = Key; Title[0] = FChar::ToUpper(Title[0]);
        const FString Label = FString::Printf(TEXT("%s-%s-%c-%s%s"), *Title, Densities[Density], Variant,
            Night ? TEXT("Night") : TEXT("Day"), Close ? TEXT("-Close") : TEXT(""));
        auto* Capture = Camera->GetCaptureComponent2D();
        if (Step == 0)
        {
            for (TActorIterator<ACitySandboxEnvironment> It(World); It; ++It)
                It->SetLightingPreset(Night ? ECityLightingPreset::Night : ECityLightingPreset::Day);
            const FString Name = Density > 0
                ? FString::Printf(TEXT("/Game/CitySim/Art/Meshes/SM_Commercial%s_Store%s"), Densities[Density], *Title)
                : FString::Printf(TEXT("/Game/CitySim/Art/LowDensity/Meshes/SM_LowDensity_Shop_%c%s"), Variant,
                    Key == TEXT("market") ? TEXT("") : *(TEXT("_") + Key));
            auto* Mesh = LoadObject<UStaticMesh>(nullptr, *(Name + TEXT(".") + FPaths::GetBaseFilename(Name)));
            if (!Test->TestNotNull(TEXT("Shop room gallery mesh loads"), Mesh)) return true;
            Shop->GetStaticMeshComponent()->SetStaticMesh(Mesh);
            // Scene captures sit far from the main player camera. Request the
            // real texture mips explicitly so this proof can show the artwork.
            Shop->GetStaticMeshComponent()->PrestreamTextures(30.f, true, 0);
            const auto Bounds = Shop->GetComponentsBoundingBox();
            const FVector Aim = Bounds.GetCenter();
            const float Size = FMath::Max3(Bounds.GetSize().X, Bounds.GetSize().Y, Bounds.GetSize().Z);
            Camera->SetActorLocation(Aim + FVector(Size, -Size * 1.8, Size * .85));
            Camera->SetActorRotation((Aim - Camera->GetActorLocation()).Rotation());
            if (Close)
            {
                const FVector Entrance = Shop->GetActorLocation() + FVector(0, -710, 250);
                Camera->SetActorLocation(Entrance + FVector(700, -1500, 380));
                Camera->SetActorRotation((Entrance - Camera->GetActorLocation()).Rotation());
            }
        }
        else if (Step == 1)
            Capture->CaptureScene();
        else
        {
            TArray<FColor> Pixels;
            if (!Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
            {
                Test->AddError(TEXT("Shop room gallery readback failed")); return true;
            }
            int32 Visible = 0;
            for (const auto& Pixel : Pixels)
                if (Pixel.R > 20 || Pixel.G > 20 || Pixel.B > 20) ++Visible;
            Test->TestTrue(TEXT("Shop room gallery contains rendered surfaces"), Visible > 5000);
            TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(1000, 900, Pixels, Png);
            Test->TestTrue(TEXT("Shop room capture saved"), FFileHelper::SaveArrayToFile(Png,
                *(Directory / (Label + TEXT(".png")))));
        }
        if (++Phase >= 46 * 3)
        {
            Camera->Destroy(); Shop->Destroy();
            for (TActorIterator<ACitySandboxEnvironment> It(World); It; ++It) It->SetLightingPreset(ECityLightingPreset::Day);
            return true;
        }
        Next = FPlatformTime::Seconds() + (Step == 0 ? 2.0 : .7);
        return false;
    }
private:
    FAutomationTestBase* Test;
    ASceneCapture2D* Camera = nullptr;
    AStaticMeshActor* Shop = nullptr;
    FString Directory;
    int32 Phase = 0, Attempts = 0;
    double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStorefrontRoomsTest, "CitySim.Desktop.StorefrontRooms",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityStorefrontRoomsTest::RunTest(const FString& Parameters)
{
    FString UserDir;
    if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir))
    { AddError(TEXT("Storefront review requires isolated Desktop test profile")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCityStorefrontRoomsCommand(this));
    return true;
}
#endif
