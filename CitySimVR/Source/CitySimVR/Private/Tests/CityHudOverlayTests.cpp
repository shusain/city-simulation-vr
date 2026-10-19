#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityVRToolController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHudOverlayTest, "CitySim.Presentation.HudOverlay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityHudOverlayTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityHudOverlayTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACityVRToolController* Hud = World->SpawnActor<ACityVRToolController>();
	World->InitializeActorsForPlay(FURL());
	Hud->DispatchBeginPlay();
	for (int32 Index = 0; Index < Hud->PaletteIconDynamics.Num(); ++Index)
	{
		auto* Icon = Hud->PaletteIconDynamics[Index].Get();
		if (TestNotNull(TEXT("Startup retains a usable icon instance"), Icon))
		{
			TestNotNull(TEXT("Icon instance has a valid parent after BeginPlay"), Icon->Parent.Get());
			TestTrue(TEXT("Startup reuses the overlay instance"), Icon == Hud->PaletteIconMaterials[Index]);
			UTexture* Texture = nullptr;
			TestTrue(TEXT("Icon texture survives overlay conversion and startup"),
			         Icon->GetTextureParameterValue(FMaterialParameterInfo(TEXT("IconTexture")), Texture) && Texture);
		}
	}
	Hud->ReturnToCity();
	Hud->PaletteAnchorMode = 2;
	Hud->SetPalettePage(ECityPalettePage::Root);
	Hud->SetPaletteOpen(true);
	int32 HandIcons = 0;
	for (int32 Index = 0; Index < Hud->VisiblePaletteEntries.Num(); ++Index)
	{
		if (!Hud->VisiblePaletteEntries[Index].bIconOnly)
			continue;
		++HandIcons;
		TestTrue(TEXT("Grip drawer icon is visible"), Hud->PaletteIcons[Index]->IsVisible());
		TestTrue(TEXT("Grip drawer icon keeps HUD master after refresh"),
		         Hud->PaletteIcons[Index]->GetMaterial(0)->GetMaterial()->GetName() == TEXT("M_CityHudIcon"));
	}
	TestTrue(TEXT("Grip drawer exercises icon-only commands"), HandIcons > 0);
	TestTrue(TEXT("Pooled preview overlays are prepared before geometry exists"),
	         Hud->PaletteTerrainPreview->GetMaterial(0)->GetMaterial()->bDisableDepthTest);
	for (const bool bOpen : {false, true, false, true})
	{
		Hud->SetPaletteOpen(bOpen);
		Hud->RefreshPaletteText();
		Hud->UpdateNeedsStatusBars();
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Hud->GetComponents(Primitives);
		int32 OverlayCount = 0;
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			// Inactive pooled icons/fills may clear their material override. Only
			// rendered pieces need the overlay; refresh rebinds before showing them.
			if (!Primitive->IsVisible())
				continue;
			bool bWorld = Primitive == Hud->EntrySky || Primitive == Hud->ConditionLeaderInstances;
			for (const auto& Root : Hud->ConditionRoots)
				bWorld |= Primitive->IsAttachedTo(Root);
			for (int32 Slot = 0; Slot < Primitive->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Primitive->GetMaterial(Slot);
				if (!Material)
					continue;
				TestEqual(*FString::Printf(TEXT("%s keeps its intended depth policy"), *Primitive->GetName()),
				          bool(Material->GetMaterial()->bDisableDepthTest), !bWorld);
				if (!bWorld)
				{
					++OverlayCount;
					TestEqual(TEXT("Overlay uses translucency"), Material->GetBlendMode(), BLEND_Translucent);
					TestFalse(TEXT("Overlay has no shadow"), bool(Primitive->CastShadow));
				}
			}
		}
		TestTrue(TEXT("Visible HUD panels/text/icons are covered after menu refresh"), OverlayCount > 10);
	}
	TestTrue(TEXT("Labels render after their panel"),
	         Hud->CalendarText->TranslucencySortPriority > Hud->CityTotalsBackground->TranslucencySortPriority);
	TestTrue(TEXT("Objective fill renders after rail"),
	         Hud->ObjectiveFill->TranslucencySortPriority > Hud->ObjectiveRail->TranslucencySortPriority);
	TestTrue(TEXT("HUD draws in the same final translucency pass as world water"),
	         Hud->CalendarText->GetMaterial(0)->GetMaterial()->TranslucencyPass == MTP_AfterDOF);
	UMaterialInstanceDynamic* Arrow = Cast<UMaterialInstanceDynamic>(Hud->CityTrendArrows[0]->GetMaterial(0));
	UMaterialInterface* Card = Hud->CityInfoBackgrounds[0]->GetMaterial(0);
	FLinearColor Before, After;
	Card->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BarColor")), Before);
	if (TestNotNull(TEXT("Trend keeps its own dynamic material"), Arrow))
		Arrow->SetVectorParameterValue(TEXT("BarColor"), FLinearColor::Red);
	Card->GetVectorParameterValue(FMaterialParameterInfo(TEXT("BarColor")), After);
	TestTrue(TEXT("Updating a trend never recolors an unrelated information card"), Before.Equals(After));
	UStaticMeshComponent* Button = Hud->PaletteButtons[0];
	const FVector Direction = -Button->GetForwardVector();
	TestEqual(TEXT("HUD hit testing remains available independent of world depth"),
	          Hud->FindPaletteButton(Button->GetComponentLocation() - Direction * 200, Direction), 0);
	// Isolate two real HUD surfaces to exercise nearest-hit clipping independent
	// of actor/component order, including a transformed/scaled panel.
	TInlineComponentArray<UPrimitiveComponent*> AllPrimitives;
	Hud->GetComponents(AllPrimitives);
	for (auto* Primitive : AllPrimitives)
		Primitive->SetVisibility(false);
	Hud->SetActorHiddenInGame(false);
	auto* Near = Hud->ObjectiveBackground.Get();
	auto* Far = Hud->PaletteBackground.Get();
	const FRotator Rotation(15, 35, 0);
	const FVector Ray = Rotation.Vector();
	const FVector Origin(12, 34, 56);
	Near->SetWorldLocationAndRotation(Origin + Ray * 80, Rotation);
	Far->SetWorldLocationAndRotation(Origin + Ray * 160, Rotation);
	Near->SetWorldScale3D(FVector(.02, .4, .3));
	Far->SetWorldScale3D(FVector(.02, .4, .3));
	Near->SetVisibility(true);
	Far->SetVisibility(true);
	Near->SetHiddenInGame(false);
	Far->SetHiddenInGame(false);
	TestEqual(TEXT("Ray stops at nearest rotated HUD front face"), Hud->GetHudRayDistance(Origin, Ray, 1000), 79.f);
	Near->SetVisibility(false);
	TestEqual(TEXT("Hidden panel does not stop ray"), Hud->GetHudRayDistance(Origin, Ray, 1000), 159.f);
	Far->SetHiddenInGame(true);
	TestEqual(TEXT("Game-hidden panel does not stop ray"), Hud->GetHudRayDistance(Origin, Ray, 1000), 1000.f);
	Far->SetHiddenInGame(false);
	TestEqual(TEXT("Ray pointing away ignores HUD"), Hud->GetHudRayDistance(Origin, -Ray, 1000), 1000.f);
	TestEqual(TEXT("Ray outside panel leaves full reach"),
	          Hud->GetHudRayDistance(Origin + Rotation.RotateVector(FVector(0, 100, 0)), Ray, 1000), 1000.f);
	TestEqual(TEXT("Parallel ray leaves full reach"),
	          Hud->GetHudRayDistance(Origin, Rotation.RotateVector(FVector(0, 1, 0)), 1000), 1000.f);
	TestEqual(TEXT("HUD beyond pointer range is ignored"), Hud->GetHudRayDistance(Origin, Ray, 100), 100.f);
	for (const TCHAR* Path : {TEXT("/Game/CitySim/UI/M_CityTextUnlit"), TEXT("/Game/CitySim/UI/M_CityUiBar"),
	                          TEXT("/Game/CitySim/UI/Icons/M_CityUiIcon"), TEXT("/Game/CitySim/UI/M_CityHeatMap")})
	{
		UMaterial* Source = LoadObject<UMaterial>(nullptr, Path);
		if (TestNotNull(TEXT("Original world material remains present"), Source))
			TestFalse(TEXT("Original world material retains depth test"), bool(Source->bDisableDepthTest));
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
