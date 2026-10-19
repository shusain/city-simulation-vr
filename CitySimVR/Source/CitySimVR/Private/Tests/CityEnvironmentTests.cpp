#if WITH_DEV_AUTOMATION_TESTS
#include "Sandbox/CitySandboxEnvironment.h"
#include "Presentation/CityBoardActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Engine/TextureCube.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEnvironmentTest, "CitySim.Presentation.LightingPresets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityEnvironmentTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityEnvironmentTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACitySandboxEnvironment* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	ACityBoardActor* Board = World->SpawnActor<ACityBoardActor>();
	auto* Sun = Environment->FindComponentByClass<UDirectionalLightComponent>();
	const float InheritedShadowDistance = Sun->DynamicShadowDistanceMovableLight;
	const float InheritedDistribution = Sun->CascadeDistributionExponent;
	const int32 InheritedCascades = Sun->DynamicShadowCascades;
	const float InheritedResolutionScale = Sun->ShadowResolutionScale;
	const float InheritedBias = Sun->ShadowBias, InheritedSlopeBias = Sun->ShadowSlopeBias;
	const float InheritedSharpen = Sun->ShadowSharpen;
	// Runtime components are created when the test world enters its play lifecycle.
	World->InitializeActorsForPlay(FURL());
	TArray<UStaticMeshComponent*> Surfaces;
	Environment->GetComponents(Surfaces);
	const auto* Floor =
	    Surfaces.FindByPredicate([](const auto* Component) { return Component->GetFName() == TEXT("Floor"); });
	if (TestNotNull(TEXT("sandbox retains its room floor"), Floor))
	{
		TestFalse(TEXT("room floor casts no shadows"), (*Floor)->CastShadow);
		TestTrue(TEXT("unlit opaque room floor cannot receive city shadows"),
		         (*Floor)->GetMaterial(0)->GetMaterial()->GetShadingModels().HasShadingModel(MSM_Unlit) &&
		             (*Floor)->GetMaterial(0)->GetBlendMode() == BLEND_Opaque);
	}
	auto* Sky = Environment->FindComponentByClass<USkyLightComponent>();
	auto* Reflection = Environment->FindComponentByClass<USphereReflectionCaptureComponent>();
	if (TestNotNull(TEXT("Metal has a precomputed room reflection"), Reflection))
	{
		TestTrue(TEXT("Room reflection uses the fixed authored cube"),
		         Reflection->ReflectionSourceType == EReflectionSourceType::SpecifiedCubemap && Reflection->Cubemap);
		TestTrue(TEXT("Reflection has no per-frame component work"),
		         Reflection->Mobility == EComponentMobility::Static && !Reflection->PrimaryComponentTick.bCanEverTick);
		if (Reflection->Cubemap)
		{
#if WITH_EDITORONLY_DATA
			TestEqual(TEXT("Room cube retains a small native size cap"), Reflection->Cubemap->MaxTextureSize, 64);
#endif
			TestFalse(TEXT("Room HDR values are linear"), Reflection->Cubemap->SRGB);
		}
	}
	TestTrue(TEXT("Fresh environment defaults to automatic"),
	         Environment->GetLightingPreset() == ECityLightingPreset::Automatic);
	const float DayIntensity = Sun->Intensity;
	const FRotator DayRotation = Sun->GetRelativeRotation();
	const int32 ComponentCount = Environment->GetComponents().Num();
#if PLATFORM_WINDOWS
	if (FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows")))
	{
		TestEqual(TEXT("Shadow comparison preserves inherited coverage"), Sun->DynamicShadowDistanceMovableLight,
		          InheritedShadowDistance);
		TestEqual(TEXT("Shadow comparison preserves inherited distribution"), Sun->CascadeDistributionExponent,
		          InheritedDistribution);
		TestEqual(TEXT("Shadow comparison retains caster bias"), Sun->ShadowBias, InheritedBias);
		TestEqual(TEXT("Shadow comparison retains filtering"), Sun->ShadowSharpen, InheritedSharpen);
	}
	else
	{
		TestTrue(TEXT("Windows shadow texels cover tabletop viewing distances rather than an outdoor world"),
		         Sun->DynamicShadowDistanceMovableLight >= 1900.f &&
		             Sun->DynamicShadowDistanceMovableLight < InheritedShadowDistance * .1f);
		TestTrue(TEXT("Normal tabletop view fits in a uniformly spaced near cascade"),
		         Sun->CascadeDistributionExponent == 1.f &&
		             Sun->DynamicShadowDistanceMovableLight / Sun->DynamicShadowCascades >= 500.f);
		TestTrue(TEXT("Miniature caster bias retains acne protection with closer contact"),
		         Sun->ShadowBias > 0.f && Sun->ShadowBias < InheritedBias && Sun->ShadowSlopeBias > 0.f &&
		             Sun->ShadowSlopeBias <= InheritedSlopeBias);
		TestTrue(TEXT("Windows miniature silhouettes use crisper shadow filtering"),
		         Sun->ShadowSharpen > InheritedSharpen && Sun->ShadowSharpen <= 1.f);
	}
#else
	TestEqual(TEXT("Mobile shadow coverage is retained"), Sun->DynamicShadowDistanceMovableLight,
	          InheritedShadowDistance);
	TestEqual(TEXT("Mobile shadow distribution is retained"), Sun->CascadeDistributionExponent, InheritedDistribution);
	TestEqual(TEXT("Mobile caster bias retained"), Sun->ShadowBias, InheritedBias);
	TestEqual(TEXT("Mobile slope bias retained"), Sun->ShadowSlopeBias, InheritedSlopeBias);
	TestEqual(TEXT("Mobile filtering retained"), Sun->ShadowSharpen, InheritedSharpen);
#endif
	TestEqual(TEXT("Shadow profile retains the cascade budget"), Sun->DynamicShadowCascades, InheritedCascades);
	TestEqual(TEXT("Shadow profile retains the atlas resolution scale"), Sun->ShadowResolutionScale,
	          InheritedResolutionScale);
	const float RuntimeShadowDistance = Sun->DynamicShadowDistanceMovableLight;
#if PLATFORM_WINDOWS
	const auto* Color = Environment->FindComponentByClass<UPostProcessComponent>();
	TestNotNull(TEXT("Windows sandbox owns its color profile"), Color);
	if (Color)
	{
		TestTrue(TEXT("Color profile is unbound and independent of headset mode"), Color->bUnbound && Color->bEnabled);
		TestEqual(TEXT("Exposure is neutral rather than camera-adaptive"), Color->Settings.AutoExposureMethod,
		          AEM_Manual);
		TestEqual(TEXT("Fixed exposure bias matches the LDR art baseline"), Color->Settings.AutoExposureBias, 0.f);
		TestEqual(TEXT("Bright art retains its colors without the film curve"), Color->Settings.ToneCurveAmount, 0.f);
		TestEqual(TEXT("Mobile-compatible color avoids bloom haze"), Color->Settings.BloomIntensity, 0.f);
	}
#endif
	Board->SetLightingPreset(ECityLightingPreset::Night);
	TestTrue(TEXT("Board routes selection to actual sandbox light component"),
	         Board->GetLightingPreset() == ECityLightingPreset::Night);
	TestTrue(TEXT("Night retains dimmer directional and ambient light"),
	         Sun->Intensity > 0 && Sun->Intensity < DayIntensity && Sky->Intensity > 0);
	TestTrue(TEXT("Night tint is cool"), Sun->GetLightColor().B > Sun->GetLightColor().R);
	Board->SetLightingPreset(ECityLightingPreset::Dusk);
	TestFalse(TEXT("Dusk changes sun angle"), Sun->GetRelativeRotation().Equals(DayRotation));
	TestTrue(TEXT("Dusk tint is warm"), Sun->GetLightColor().R > Sun->GetLightColor().B);
	Board->SetLightingPreset(ECityLightingPreset::Day);
	TestEqual(TEXT("Day restores baseline intensity"), Sun->Intensity, DayIntensity);
	TestTrue(TEXT("Day restores baseline angle"), Sun->GetRelativeRotation().Equals(DayRotation));
	TestTrue(TEXT("Day restores neutral light"), Sun->GetLightColor().Equals(FLinearColor::White));
	TestEqual(TEXT("Preset switches allocate no extra components"), Environment->GetComponents().Num(), ComponentCount);
	TestFalse(TEXT("Ambient capture stays fixed"), Sky->bRealTimeCapture);
	TestEqual(TEXT("Lighting preset transitions retain shadow coverage"), Sun->DynamicShadowDistanceMovableLight,
	          RuntimeShadowDistance);
	Board->SetLightingPreset(static_cast<ECityLightingPreset>(255));
	TestTrue(TEXT("Invalid preset preserves current environment"),
	         Board->GetLightingPreset() == ECityLightingPreset::Day);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLightingCycleTest, "CitySim.Presentation.LightingCycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityLightingCycleTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityLightingCycleTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACitySandboxEnvironment* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	auto* Sun = Environment->FindComponentByClass<UDirectionalLightComponent>();
	auto* Sky = Environment->FindComponentByClass<USkyLightComponent>();
	const FCityEconomyTuning Tuning;
	constexpr float Frame = 1.0f / 72.0f;
	for (const int32 Speed : {1, 3, 5})
	{
		FCityPlacementModel City;
		FCityEconomySimulation Clock;
		Clock.SetPaused(false);
		Clock.SetSpeedMultiplier(Speed);
		Environment->SetLightingPreset(ECityLightingPreset::Day);
		Environment->SetLightingPreset(ECityLightingPreset::Automatic);
		Environment->UpdateLightingClock(0, 0, 150, false, Frame);
		float Minimum = 4.0f, Maximum = 0.0f, LargestFrameChange = 0.0f;
		const int32 Frames = FMath::RoundToInt(360.0f / Speed / Frame);
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			Clock.Advance(Frame, City, Tuning);
			const float Previous = Sun->Intensity;
			Environment->UpdateLightingClock(Clock.GetSnapshot().FixedStepId, Clock.GetFixedStepProgress(Tuning),
			                                 Tuning.FixedStepsPerMonth, Clock.IsPaused(), Frame);
			Minimum = FMath::Min(Minimum, Sun->Intensity);
			Maximum = FMath::Max(Maximum, Sun->Intensity);
			LargestFrameChange = FMath::Max(LargestFrameChange, FMath::Abs(Sun->Intensity - Previous));
		}
		TestTrue(TEXT("Each speed completes night and daylight in one day"),
		         Minimum < 0.85f && Maximum > 3.8f && Sun->Intensity < 1.0f);
		TestTrue(TEXT("Continuous fractional-day lighting including cycle wrap"), LargestFrameChange < 0.05f);
		TestEqual(TEXT("Clock completes one calendar day at each speed"), Clock.GetSnapshot().FixedStepId,
		          static_cast<int64>(150));
		Environment->UpdateLightingClock(75, 0, 150, false, 20.0f);
		TestTrue(TEXT("Reload or catch-up presents the actual calendar phase"), Sun->Intensity > 3.99f);
		const float PausedSun = Sun->Intensity, PausedSky = Sky->Intensity;
		const FRotator PausedRotation = Sun->GetRelativeRotation();
		const FLinearColor PausedColor = Sun->GetLightColor();
		for (int32 Index = 0; Index < 72; ++Index)
			Environment->UpdateLightingClock(75, 0.0, 150, true, Frame);
		TestEqual(TEXT("Pause freezes sunlight even while easing"), Sun->Intensity, PausedSun);
		TestEqual(TEXT("Pause freezes ambient"), Sky->Intensity, PausedSky);
		TestTrue(TEXT("Pause freezes angle and color"),
		         Sun->GetRelativeRotation().Equals(PausedRotation) && Sun->GetLightColor().Equals(PausedColor));
	}
	Environment->SetLightingPreset(ECityLightingPreset::Night);
	for (int32 Index = 0; Index < 360; ++Index)
		Environment->UpdateLightingClock(0, 0.0, 25, false, Frame);
	TestEqual(TEXT("Manual preset holds while clock advances"), Sun->Intensity, 0.8f);
	Environment->SetLightingPreset(ECityLightingPreset::Automatic);
	TestEqual(TEXT("Returning to auto does not snap immediately"), Sun->Intensity, 0.8f);
	for (int32 Index = 0; Index < 360; ++Index)
		Environment->UpdateLightingClock(0, 0.0, 25, false, Frame);
	TestTrue(TEXT("Reset or loaded clock presents midnight"), Sun->Intensity < 1.0f);
	TestTrue(TEXT("Ambient remains bounded"), Sky->Intensity >= 0.45f && Sky->Intensity <= 1.0f);
	// Inspect the actual light through a complete fast cycle. The only angular
	// discontinuities allowed are horizon sun/moon handoffs at negligible intensity.
	Environment->SetLightingPreset(ECityLightingPreset::Day);
	Environment->SetLightingPreset(ECityLightingPreset::Automatic);
	Environment->UpdateLightingClock(0, 0, 150, false, Frame);
	FVector Sunset, Sunrise;
	float LargestVisibleAngle = 0;
	int32 HorizonHandoffs = 0;
	for (int32 Index = 0; Index <= 1728; ++Index)
	{
		const FVector BeforeDirection = Sun->GetForwardVector();
		const float BeforeIntensity = Sun->Intensity;
		const double StepTime = Index * 150.0 / 1728.0;
		Environment->UpdateLightingClock(static_cast<int64>(StepTime), FMath::Frac(StepTime), 150, false, Frame);
		const double Angle = FMath::RadiansToDegrees(
		    FMath::Acos(FMath::Clamp(FVector::DotProduct(BeforeDirection, Sun->GetForwardVector()), -1.0, 1.0)));
		if (Angle > 90.0)
		{
			++HorizonHandoffs;
			TestTrue(TEXT("Horizon handoff has no bright shadow flip"),
			         BeforeIntensity < .01f && Sun->Intensity < .01f);
		}
		else if (Sun->Intensity > .01f && BeforeIntensity > .01f)
			LargestVisibleAngle = FMath::Max(LargestVisibleAngle, static_cast<float>(Angle));
		TestTrue(TEXT("Sun and moon rays always point down toward the city"), Sun->GetForwardVector().Z <= .001);
		if (Index == 380)
			Sunset = Sun->GetForwardVector().GetSafeNormal2D();
		if (Index == 1382)
			Sunrise = Sun->GetForwardVector().GetSafeNormal2D();
	}
	TestEqual(TEXT("One sun/moon handoff per horizon"), HorizonHandoffs, 2);
	TestTrue(TEXT("Low morning and evening shadows point toward opposite sides"),
	         FVector::DotProduct(Sunset, Sunrise) < -.75);
	TestTrue(TEXT("Shadow direction follows a smooth arc at 5x"), LargestVisibleAngle < 1.0f);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShadowProfileLifecycleTest, "CitySim.Presentation.ShadowProfileLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityShadowProfileLifecycleTest::RunTest(const FString& Parameters)
{
	auto* ReflectionMix = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionEnvironmentLightmapMixing"));
	if (!TestNotNull(TEXT("Native reflection mixing setting exists"), ReflectionMix))
		return false;
	const int32 BeforeMix = ReflectionMix->GetInt();
	const uint32 BeforeMixFlags = ReflectionMix->GetFlags();
	auto* Resolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxCSMResolution"));
	if (!TestNotNull(TEXT("Native CSM resolution setting exists"), Resolution))
		return false;
	const int32 Before = Resolution->GetInt();
	const uint32 BeforeFlags = Resolution->GetFlags();
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityShadowProfileLifecycleTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	World->InitializeActorsForPlay(FURL());
	Environment->DispatchBeginPlay();
	TestEqual(TEXT("Dynamic city reflection avoids black indirect-light mixing unless explicitly overridden"),
	          ReflectionMix->GetInt(), (BeforeMixFlags & ECVF_SetByMask) < ECVF_SetByCommandline ? 0 : BeforeMix);
#if PLATFORM_WINDOWS
	const bool bOverride = !FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows")) && Before < 4096 &&
	                       (BeforeFlags & ECVF_SetByMask) < ECVF_SetByCommandline;
	TestEqual(TEXT("Windows city uses sharper CSM unless explicitly overridden"), Resolution->GetInt(),
	          bOverride ? 4096 : Before);
#else
	TestEqual(TEXT("Mobile keeps inherited CSM resolution"), Resolution->GetInt(), Before);
#endif
	Environment->EndPlay(EEndPlayReason::EndPlayInEditor);
	TestEqual(TEXT("Ending sandbox restores other scenes' reflection policy"), ReflectionMix->GetInt(), BeforeMix);
	TestEqual(TEXT("Ending sandbox preserves reflection setting priority"),
	          static_cast<uint32>(ReflectionMix->GetFlags()), BeforeMixFlags);
	TestEqual(TEXT("Ending city play restores prior atlas budget"), Resolution->GetInt(), Before);
	TestEqual(TEXT("Ending city play preserves setting priority"), static_cast<uint32>(Resolution->GetFlags()),
	          BeforeFlags);
	// A live user console choice must survive removal of the scene's tagged layer.
	auto* SecondEnvironment = World->SpawnActor<ACitySandboxEnvironment>();
	SecondEnvironment->DispatchBeginPlay();
	const FName LiveOverride(TEXT("CityReflectionLifecycleTestOverride"));
	ReflectionMix->Set(1, ECVF_SetByConsole, LiveOverride);
	SecondEnvironment->EndPlay(EEndPlayReason::EndPlayInEditor);
	TestEqual(TEXT("User's live reflection choice survives sandbox cleanup"), ReflectionMix->GetInt(), 1);
	TestEqual(TEXT("User console priority survives sandbox cleanup"), ReflectionMix->GetFlags() & ECVF_SetByMask,
	          uint32(ECVF_SetByConsole));
	ReflectionMix->Unset(ECVF_SetByConsole, LiveOverride);
	TestEqual(TEXT("Temporary console fixture restores original reflection value"), ReflectionMix->GetInt(), BeforeMix);
	TestEqual(TEXT("Temporary console fixture restores original priority"),
	          static_cast<uint32>(ReflectionMix->GetFlags()), BeforeMixFlags);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityShadowViewCoverageTest, "CitySim.Presentation.ShadowViewCoverage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityShadowViewCoverageTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityShadowViewCoverageTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	auto* Sun = Environment->FindComponentByClass<UDirectionalLightComponent>();
	const float InheritedDistance = Sun->DynamicShadowDistanceMovableLight;
	const float InheritedTransition = Sun->CascadeTransitionFraction;
	const int32 Cascades = Sun->DynamicShadowCascades;
	Environment->SetHeadTrackedShadowCoverage(true);
	World->InitializeActorsForPlay(FURL());
#if PLATFORM_WINDOWS
	const bool bLegacy = FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows"));
	const bool bPrevious = FParse::Param(FCommandLine::Get(), TEXT("CityPreviousVRShadows"));
	TestEqual(TEXT("early pawn selection survives component initialization"), Sun->DynamicShadowDistanceMovableLight,
	          bLegacy     ? InheritedDistance
	          : bPrevious ? 2000.f
	                      : 600.f);
	TestEqual(TEXT("PCVR widens cascade blends rather than exposing hard detail changes"),
	          Sun->CascadeTransitionFraction, bLegacy || bPrevious ? InheritedTransition : .25f);
	const float Bias = Sun->ShadowBias, Sharpen = Sun->ShadowSharpen;
	Environment->SetHeadTrackedShadowCoverage(false);
	TestEqual(TEXT("desktop/fallback retains twenty-metre camera reach"), Sun->DynamicShadowDistanceMovableLight,
	          bLegacy ? InheritedDistance : 2000.f);
	TestEqual(TEXT("desktop restores its inherited transition"), Sun->CascadeTransitionFraction, InheritedTransition);
	Environment->SetHeadTrackedShadowCoverage(true);
	TestEqual(TEXT("headtracked re-entry restores its coverage"), Sun->DynamicShadowDistanceMovableLight,
	          bLegacy     ? InheritedDistance
	          : bPrevious ? 2000.f
	                      : 600.f);
	TestEqual(TEXT("view profile preserves conservative caster bias"), Sun->ShadowBias, Bias);
	TestEqual(TEXT("view profile preserves filter treatment"), Sun->ShadowSharpen, Sharpen);
#else
	Environment->SetHeadTrackedShadowCoverage(false);
	Environment->SetHeadTrackedShadowCoverage(true);
	TestEqual(TEXT("Quest retains shadow coverage across mode routing"), Sun->DynamicShadowDistanceMovableLight,
	          InheritedDistance);
	TestEqual(TEXT("Quest retains transition budget"), Sun->CascadeTransitionFraction, InheritedTransition);
#endif
	TestEqual(TEXT("view profile does not increase cascade count"), Sun->DynamicShadowCascades, Cascades);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySkyTest, "CitySim.Presentation.SkyCycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCitySkyTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CitySkyTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	ACitySandboxEnvironment* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	// This isolated test world does not initialize actors through gameplay startup.
	if (!Environment->IsActorInitialized())
		Environment->PostInitializeComponents();
	TArray<UStaticMeshComponent*> Meshes;
	Environment->GetComponents(Meshes);
	UStaticMeshComponent* Dome = nullptr;
	for (auto* Mesh : Meshes)
		if (Mesh->GetFName() == TEXT("SkyDome"))
			Dome = Mesh;
	if (!TestNotNull(TEXT("Environment owns sky dome"), Dome))
	{
		World->DestroyWorld(false);
		return false;
	}
	auto* Material = Cast<UMaterialInstanceDynamic>(Dome->GetMaterial(0));
	if (!TestNotNull(TEXT("Sky has its own dynamic material"), Material))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Sky sphere is loaded"), Dome->GetStaticMesh() != nullptr);
	const UMaterial* Base = Material->GetMaterial();
	TestTrue(TEXT("Sky is opaque, unlit, two-sided and marked sky"),
	         Base->BlendMode == BLEND_Opaque && Base->TwoSided && Base->bIsSky &&
	             Base->GetShadingModels().HasShadingModel(MSM_Unlit));
	TestTrue(TEXT("Dome cannot intercept tools, cast shadows or enter ambient captures"),
	         Dome->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Dome->CastShadow &&
	             !Dome->bVisibleInReflectionCaptures && !Dome->bVisibleInRealTimeSkyCaptures);
	const auto Horizon = [Material]() { return Material->K2_GetVectorParameterValue(TEXT("HorizonColor")); };
	const auto Zenith = [Material]() { return Material->K2_GetVectorParameterValue(TEXT("ZenithColor")); };
	Environment->SetLightingPreset(ECityLightingPreset::Day);
	const FLinearColor Day = Horizon(), DayTop = Zenith();
	Environment->SetLightingPreset(ECityLightingPreset::Dusk);
	TestTrue(TEXT("Dusk horizon warms while top remains cool"), Horizon().R > Horizon().B && Zenith().B > Zenith().R);
	Environment->SetLightingPreset(ECityLightingPreset::Night);
	const FLinearColor Night = Horizon(), NightTop = Zenith();
	TestTrue(TEXT("Night is darker and blue"), Night.GetLuminance() < Day.GetLuminance() &&
	                                               NightTop.GetLuminance() < DayTop.GetLuminance() &&
	                                               Night.B > Night.R);
	Environment->SetLightingPreset(ECityLightingPreset::Automatic);
	constexpr float Frame = 1.0f / 72.0f;
	Environment->UpdateLightingClock(0, 0, 25, true, Frame);
	TestTrue(TEXT("Paused return to auto holds sky"), Horizon().Equals(Night) && Zenith().Equals(NightTop));
	for (int32 Index = 0; Index < 360; ++Index)
		Environment->UpdateLightingClock(0, 0, 25, false, Frame);
	TestTrue(TEXT("Auto converges to daytime sky"), Horizon().Equals(Day, 0.001f) && Zenith().Equals(DayTop, 0.001f));
	// Complete a 5x cycle including the wrap; compare actual material values every headset frame.
	float LargestChange = 0;
	for (int32 Index = 0; Index <= 1728; ++Index)
	{
		const double StepTime = Index * 150.0 / 1728.0;
		const FLinearColor Before = Horizon();
		Environment->UpdateLightingClock(static_cast<int64>(StepTime), FMath::Frac(StepTime), 150, false, Frame);
		const FLinearColor Delta = Horizon() - Before;
		LargestChange =
		    FMath::Max(LargestChange, FMath::Max3(FMath::Abs(Delta.R), FMath::Abs(Delta.G), FMath::Abs(Delta.B)));
		if (Index == 1080)
			TestTrue(TEXT("Automatic night matches the manual sky"), Horizon().Equals(Night, 0.02f));
	}
	TestTrue(TEXT("Sky has no color snap across phases or wrap at 5x"), LargestChange < 0.02f);
	const FLinearColor BeforePause = Horizon();
	Environment->UpdateLightingClock(30, 0, 25, true, 10.0f);
	TestTrue(TEXT("Pause holds sky during unfinished dawn easing"), Horizon().Equals(BeforePause));
	TestEqual(TEXT("Sky component count remains fixed"), Meshes.Num(), 2);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWindowGlowTest, "CitySim.Presentation.WindowGlow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWindowGlowTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues()
	                                                .AllowAudioPlayback(false)
	                                                .CreatePhysicsScene(false)
	                                                .CreateNavigation(false)
	                                                .CreateAISystem(false)
	                                                .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityWindowGlowTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	auto* Collection = LoadObject<UMaterialParameterCollection>(
	    nullptr, TEXT("/Game/CitySim/Art/Materials/MPC_CityLighting.MPC_CityLighting"));
	if (!TestNotNull(TEXT("Shared lighting collection loads"), Collection))
	{
		World->DestroyWorld(false);
		return false;
	}
	auto* Instance = World->GetParameterCollectionInstance(Collection);
	const auto Glow = [Instance]()
	{
		float Value = -1;
		Instance->GetScalarParameterValue(TEXT("NightGlow"), Value);
		return Value;
	};
	Environment->SetLightingPreset(ECityLightingPreset::Day);
	TestEqual(TEXT("Day windows unlit"), Glow(), 0.0f);
	Environment->SetLightingPreset(ECityLightingPreset::Dusk);
	TestEqual(TEXT("Dusk windows gently lit"), Glow(), 0.35f);
	Environment->SetLightingPreset(ECityLightingPreset::Night);
	TestEqual(TEXT("Night enables masked glow"), Glow(), 1.0f);
	Environment->SetLightingPreset(ECityLightingPreset::Automatic);
	Environment->UpdateLightingClock(0, 0, 25, true, 10);
	TestEqual(TEXT("Pause holds window glow"), Glow(), 1.0f);
	for (int32 I = 0; I < 360; ++I)
		Environment->UpdateLightingClock(0, 0, 25, false, 1.0f / 72);
	TestTrue(TEXT("Automatic dawn fades glow away"), Glow() < 0.001f);
	Environment->UpdateLightingClock(30, 0, 25, false, 30);
	TestTrue(TEXT("Clock jump cannot flash windows on"), Glow() >= 0.0f && Glow() < 0.1f);
	World->DestroyWorld(false);
	return true;
}
#endif
