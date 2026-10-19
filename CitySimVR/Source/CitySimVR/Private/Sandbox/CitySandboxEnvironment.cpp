#include "Sandbox/CitySandboxEnvironment.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Engine/TextureCube.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/TextureRenderTarget.h"
#include "Presentation/CityBoardActor.h"
#include "EngineUtils.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

namespace
{
	const FLinearColor DayHorizon(0.35f, 0.60f, 0.80f), DuskHorizon(0.80f, 0.27f, 0.08f),
	    NightHorizon(0.025f, 0.04f, 0.095f);
	const FLinearColor DayZenith(0.045f, 0.18f, 0.42f), DuskZenith(0.085f, 0.045f, 0.16f),
	    NightZenith(0.004f, 0.007f, 0.025f);
	// A tilted circular orbit, with noon at phase 0, sunset at .25 and sunrise
	// at .75. Use the opposite body at night so one movable light serves sun/moon.
	FVector CelestialLightDirection(const double Phase)
	{
		const double Angle = Phase * UE_TWO_PI;
		const FVector Bearing = FRotator(0.0, -35.0, 0.0).Vector();
		const FVector Westward = FVector::CrossProduct(FVector::UpVector, Bearing);
		const double Height = FMath::Cos(Angle);
		const FVector SunRay = (Bearing - FVector::UpVector) * (UE_SQRT_2 * .5 * Height) + Westward * FMath::Sin(Angle);
		return Height >= 0.0 ? SunRay : -SunRay;
	}
} // namespace

ACitySandboxEnvironment::ACitySandboxEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMaterial(
	    TEXT("/Game/CitySim/Art/Materials/M_CityRoomFloor.M_CityRoomFloor"));

	Floor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Floor"));
	Floor->SetupAttachment(SceneRoot);
	Floor->SetStaticMesh(CubeMesh.Object);
	Floor->SetRelativeLocation(FVector(0.0, 0.0, -5.0));
	Floor->SetRelativeScale3D(FVector(40.0, 40.0, 0.1));
	Floor->SetMaterial(0, FloorMaterial.Object);
	Floor->SetCanEverAffectNavigation(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyAsset(
	    TEXT("/Game/CitySim/Art/Materials/M_CitySky.M_CitySky"));
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> LightingAsset(
	    TEXT("/Game/CitySim/Art/Materials/MPC_CityLighting.MPC_CityLighting"));
	LightingCollection = LightingAsset.Object;
	SkyDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyDome"));
	SkyDome->SetupAttachment(SceneRoot);
	SkyDome->SetStaticMesh(SphereMesh.Object);
	SkyDome->SetMaterial(0, SkyAsset.Object);
	// Ten-kilometer radius keeps the stationary horizon distant from tabletop locomotion.
	SkyDome->SetRelativeScale3D(FVector(20000.0));
	SkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyDome->SetGenerateOverlapEvents(false);
	SkyDome->SetCanEverAffectNavigation(false);
	SkyDome->SetCastShadow(false);
	SkyDome->bVisibleInReflectionCaptures = false;
	SkyDome->bVisibleInRealTimeSkyCaptures = false;

	DirectionalLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("DirectionalLight"));
	DirectionalLight->SetupAttachment(SceneRoot);
	DirectionalLight->SetRelativeRotation(FRotator(-45.0, -35.0, 0.0));
	DirectionalLight->SetIntensity(4.0f);
	DirectionalLight->Mobility = EComponentMobility::Movable;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetIntensity(1.0f);
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = false;
	static ConstructorHelpers::FObjectFinder<UTextureCube> DaylightCube(
	    TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
	SkyLight->SourceType = SLS_SpecifiedCubemap;
	SkyLight->Cubemap = DaylightCube.Object;

	// One precomputed neutral room reflection gives exposed metal a specular
	// environment. It does not change diffuse light or recapture moving city objects.
	static ConstructorHelpers::FObjectFinder<UTextureCube> RoomCube(
	    TEXT("/Game/CitySim/Art/Textures/T_CityRoomReflection.T_CityRoomReflection"));
	RoomReflection = CreateDefaultSubobject<USphereReflectionCaptureComponent>(TEXT("RoomReflection"));
	RoomReflection->SetupAttachment(SceneRoot);
	RoomReflection->ReflectionSourceType = EReflectionSourceType::SpecifiedCubemap;
	RoomReflection->Cubemap = RoomCube.Object;
	RoomReflection->InfluenceRadius = 5000.0f;
	RoomReflection->Brightness = 1.0f;
}

void ACitySandboxEnvironment::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// The sky dome is excluded from sky captures. A scene-captured skylight can
	// therefore bake an empty upper hemisphere and blacken shaded building walls.
	SkyLight->SourceType = SLS_SpecifiedCubemap;
	SkyLight->Cubemap = GetDefault<ACitySandboxEnvironment>()->SkyLight->Cubemap;
	SkyLight->RecaptureSky();
	// Replace the older lit floor override serialized in CitySandbox. An unlit
	// room surface cannot receive city shadows; disabling CastShadow alone cannot do that.
	Floor->SetMaterial(0, GetDefault<ACitySandboxEnvironment>()->Floor->GetMaterial(0));
	Floor->SetCastShadow(false);
#if PLATFORM_WINDOWS
	DesktopCascadeTransition = DirectionalLight->CascadeTransitionFraction;
	// The board spans 1-4m; the inherited 400m CSM range wastes texels on empty
	// space. Initialize 20m desktop coverage; XR pawn selection tightens it below.
	// Retain the cascade count and Quest profile;
	// the Windows atlas override below is scoped to the play lifecycle.
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows")))
	{
		DirectionalLight->SetDynamicShadowDistanceMovableLight(2000.0f);
		DirectionalLight->SetCascadeDistributionExponent(1.0f);
		// Miniature buildings need less caster separation than full-size geometry.
		DirectionalLight->SetShadowBias(.35f);
		DirectionalLight->SetShadowSlopeBias(.5f);
		DirectionalLight->ShadowSharpen = .5f;
		DirectionalLight->MarkRenderStateDirty();
	}
	SetHeadTrackedShadowCoverage(bHeadTrackedShadowView);
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "CITYSIM_WINDOWS_SHADOW_PROFILE DistanceCm=%.0f Cascades=%d Distribution=%.1f ResolutionScale=%.1f Bias=%.2f SlopeBias=%.2f Sharpen=%.2f"),
	    DirectionalLight->DynamicShadowDistanceMovableLight, DirectionalLight->DynamicShadowCascades,
	    DirectionalLight->CascadeDistributionExponent, DirectionalLight->ShadowResolutionScale,
	    DirectionalLight->ShadowBias, DirectionalLight->ShadowSlopeBias, DirectionalLight->ShadowSharpen);

	// Quest renders this stylized art with Mobile HDR disabled. Keep Windows
	// exposure/color response neutral without changing the shared lights/art.
	// Runtime ownership keeps platform presentation out of the cooked map schema.
	auto* WindowsColor = NewObject<UPostProcessComponent>(this, TEXT("WindowsColor"));
	AddInstanceComponent(WindowsColor);
	WindowsColor->SetupAttachment(SceneRoot);
	WindowsColor->bUnbound = true;
	WindowsColor->Priority = 10;
	WindowsColor->bEnabled = !FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsColor"));
	auto& Color = WindowsColor->Settings;
	Color.bOverride_AutoExposureMethod = true;
	Color.AutoExposureMethod = AEM_Manual;
	Color.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Color.AutoExposureApplyPhysicalCameraExposure = false;
	Color.bOverride_AutoExposureBias = true;
	Color.AutoExposureBias = 0;
	Color.bOverride_BloomIntensity = true;
	Color.BloomIntensity = 0;
	Color.bOverride_ToneCurveAmount = true;
	Color.ToneCurveAmount = 0;
	Color.bOverride_ExpandGamut = true;
	Color.ExpandGamut = 0;
	Color.bOverride_LocalExposureHighlightContrastScale = true;
	Color.LocalExposureHighlightContrastScale = 1;
	Color.bOverride_LocalExposureShadowContrastScale = true;
	Color.LocalExposureShadowContrastScale = 1;
	WindowsColor->RegisterComponent();
#endif
	SkyMaterial = SkyDome->CreateDynamicMaterialInstance(0);
	UpdateSkyMaterial();
}

void ACitySandboxEnvironment::SetHeadTrackedShadowCoverage(const bool bHeadTracked)
{
	bHeadTrackedShadowView = bHeadTracked;
#if PLATFORM_WINDOWS
	// Pawn selection can precede component initialization in a fresh world.
	if (!IsActorInitialized())
		return;
	if (FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows")))
		return;
	const bool bTabletopVR = bHeadTracked && !FParse::Param(FCommandLine::Get(), TEXT("CityPreviousVRShadows"));
	// Wide headset frusta spend many more shadow texels outside the miniature
	// board than the narrower desktop camera. Six metres covers normal 1-4m
	// tabletop manipulation; desktop retains its longer camera/zoom reach.
	DirectionalLight->SetDynamicShadowDistanceMovableLight(bTabletopVR ? 600.f : 2000.f);
	DirectionalLight->SetCascadeTransitionFraction(bTabletopVR ? .25f : DesktopCascadeTransition);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_SHADOW_VIEW_PROFILE Mode=%s DistanceCm=%.0f Transition=%.2f"),
	       bTabletopVR ? TEXT("PCVR") : TEXT("Desktop"), DirectionalLight->DynamicShadowDistanceMovableLight,
	       DirectionalLight->CascadeTransitionFraction);
#endif
}

void ACitySandboxEnvironment::BeginPlay()
{
	Super::BeginPlay();
#if PLATFORM_WINDOWS
	bDynamicReflectionsEnabled = !FParse::Param(FCommandLine::Get(), TEXT("CityStaticReflections"));
	if (bDynamicReflectionsEnabled)
	{
		// Retain the authored diffuse irradiance. Only its specular cube is
		// refreshed from a GPU scene capture; no recurring SH readback/hitch.
		SkyLight->CubemapResolution = 64;
		SkyLight->RecaptureSky();
		RoomReflection->SetVisibility(false);
		LiveReflectionTarget = NewObject<UTextureRenderTargetCube>(this);
		LiveReflectionTarget->bHDR = true;
		LiveReflectionTarget->bAutoGenerateMips = true;
		LiveReflectionTarget->Init(64, PF_FloatRGBA);
		LiveReflectionActor = GetWorld()->SpawnActor<ASceneCaptureCube>();
		LiveReflectionActor->SetActorTickEnabled(false);
		LiveReflectionCapture = LiveReflectionActor->GetCaptureComponentCube();
		LiveReflectionCapture->TextureTarget = LiveReflectionTarget;
		LiveReflectionCapture->CaptureSource = SCS_SceneColorHDR;
		LiveReflectionCapture->bCaptureEveryFrame = false;
		LiveReflectionCapture->bCaptureOnMovement = false;
		// Avoid reflecting a previous reflection into itself. Capture the
		// actual diffuse city, sky and direct light, with native roughness mips.
		LiveReflectionCapture->ShowFlags.SetReflectionEnvironment(false);
		// Main-view shadows remain intact. At64px, recomputing their cascades
		// in six cube views caused avoidable capture-frame GPU peaks.
		LiveReflectionCapture->ShowFlags.SetDynamicShadows(false);
		LiveReflectionCapture->ShowFlags.SetMotionBlur(false);
		LiveReflectionCapture->ShowFlags.SetTemporalAA(false);
		LiveReflectionCapture->ShowFlags.SetParticles(false);
		LiveReflectionCapture->ShowFlags.SetOnScreenDebug(false);
		LiveReflectionCapture->SetComponentTickEnabled(false);
		NextReflectionCaptureSeconds = FPlatformTime::Seconds() + 2.0;
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_DYNAMIC_REFLECTIONS enabled=%d resolution=64 interval=0.5 GPU-only"),
	       bDynamicReflectionsEnabled);
#endif
	// This dynamically lit sandbox has no baked indirect light to mix with the
	// room reflection. Black legacy indirect data otherwise suppresses rough metal.
	// Scope the specular policy to this scene and preserve explicit user overrides.
	if (IConsoleVariable* Mix =
	        IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionEnvironmentLightmapMixing")))
	{
		if ((Mix->GetFlags() & ECVF_SetByMask) < ECVF_SetByCommandline)
		{
			ReflectionMixTag = FName(*GetPathName());
			// Preview is a tagged array priority below explicit command-line/console
			// choices, allowing independent scene layers to be removed safely.
			Mix->Set(0, ECVF_SetByPreview, ReflectionMixTag);
		}
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_ROOM_REFLECTION_LIGHTMAP_MIXING=%d"), Mix->GetInt());
	}
#if PLATFORM_WINDOWS
	// CSM ignores per-light resolution scale. Scope the atlas override to gameplay
	// and restore it after PIE; do not write project/device-profile settings.
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityLegacyWindowsShadows")))
	{
		if (IConsoleVariable* Resolution =
		        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxCSMResolution")))
		{
			if (Resolution->GetInt() < 4096 && (Resolution->GetFlags() & ECVF_SetByMask) < ECVF_SetByCommandline)
			{
				PreviousCSMResolution = Resolution->GetInt();
				PreviousCSMFlags = Resolution->GetFlags();
				Resolution->SetWithCurrentPriority(4096);
			}
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_WINDOWS_CSM_RESOLUTION=%d"), Resolution->GetInt());
		}
	}
#endif
}

void ACitySandboxEnvironment::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LiveReflectionActor) LiveReflectionActor->Destroy();
	if (!ReflectionMixTag.IsNone())
	{
		if (IConsoleVariable* Mix =
		        IConsoleManager::Get().FindConsoleVariable(TEXT("r.ReflectionEnvironmentLightmapMixing")))
			Mix->Unset(ECVF_SetByPreview, ReflectionMixTag);
		ReflectionMixTag = NAME_None;
	}
#if PLATFORM_WINDOWS
	if (PreviousCSMResolution >= 0)
	{
		if (IConsoleVariable* Resolution =
		        IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxCSMResolution")))
		{
			// Respect a quality/console change made while this city was running.
			if (Resolution->GetInt() == 4096 && Resolution->GetFlags() == PreviousCSMFlags)
				Resolution->SetWithCurrentPriority(PreviousCSMResolution);
		}
		PreviousCSMResolution = -1;
	}
#endif
	Super::EndPlay(EndPlayReason);
}

void ACitySandboxEnvironment::UpdateSkyMaterial()
{
	if (SkyMaterial)
	{
		const float Daylight = 1.0f - DisplayedWindowGlow * .90f;
		SkyMaterial->SetVectorParameterValue(
		    TEXT("HorizonColor"),
		    FMath::Lerp(DisplayedHorizon, FLinearColor(.25, .29, .33) * Daylight, DisplayedCloudiness * .85f));
		SkyMaterial->SetVectorParameterValue(
		    TEXT("ZenithColor"),
		    FMath::Lerp(DisplayedZenith, FLinearColor(.13, .16, .20) * Daylight, DisplayedCloudiness));
	}
	if (LightingCollection && GetWorld())
		GetWorld()
		    ->GetParameterCollectionInstance(LightingCollection)
		    ->SetScalarParameterValue(TEXT("NightGlow"), DisplayedWindowGlow);
}

void ACitySandboxEnvironment::SetLightingPreset(const ECityLightingPreset Preset)
{
	if (Preset != ECityLightingPreset::Day && Preset != ECityLightingPreset::Dusk &&
	    Preset != ECityLightingPreset::Night && Preset != ECityLightingPreset::Automatic)
		return;
	LightingPreset = Preset;
	if (Preset == ECityLightingPreset::Automatic)
		return; // Ease from the current manual lighting on the next running frame.
	// One existing movable light and fixed ambient capture; no new shadow casters or recapture.
	const bool bNight = Preset == ECityLightingPreset::Night;
	const bool bDusk = Preset == ECityLightingPreset::Dusk;
	DisplayedLightingPhase =
	    bNight ? .5
	           : (bDusk ? FMath::Acos(FMath::Sin(FMath::DegreesToRadians(18.0)) / (UE_SQRT_2 * .5)) / UE_TWO_PI : 0.0);
	DirectionalLight->SetRelativeRotation(CelestialLightDirection(DisplayedLightingPhase).Rotation());
	DirectionalLight->SetIntensity(bNight ? 0.8f : (bDusk ? 2.4f : 4.0f));
	DisplayedLightColor =
	    bNight ? FLinearColor(0.48f, 0.64f, 1.0f) : (bDusk ? FLinearColor(1.0f, 0.62f, 0.35f) : FLinearColor::White);
	DirectionalLight->SetLightColor(DisplayedLightColor);
	SkyLight->SetIntensity(bNight ? 0.45f : (bDusk ? 0.75f : 1.0f));
	DisplayedWindowGlow = bNight ? 1.0f : (bDusk ? 0.35f : 0.0f);
	DisplayedHorizon = bNight ? NightHorizon : (bDusk ? DuskHorizon : DayHorizon);
	DisplayedZenith = bNight ? NightZenith : (bDusk ? DuskZenith : DayZenith);
	UpdateSkyMaterial();
}

void ACitySandboxEnvironment::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const UCitySimulationSubsystem* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		if (Simulation->IsEntryMenuSuspended())
			return;
		const auto& Weather = Simulation->GetEnvironmentSnapshot();
		const float CloudTarget = Weather.bPrecipitation ? (Weather.WindStrength == 2 ? 1.0f : .75f) : .05f;
		if (WeatherCitySession != Simulation->GetCitySessionGeneration())
		{
			WeatherCitySession = Simulation->GetCitySessionGeneration();
			DisplayedCloudiness = CloudTarget;
		}
		if (!Simulation->IsSimulationPaused() && FMath::IsFinite(DeltaSeconds))
		{
			DisplayedCloudiness =
			    FMath::Lerp(DisplayedCloudiness, CloudTarget, 1.0f - FMath::Exp(-FMath::Min(DeltaSeconds, .1f) / 4.0f));
		}
		UpdateLightingClock(Simulation->GetEconomySnapshot().FixedStepId, Simulation->GetSimulationStepProgress(),
		                    Simulation->GetEconomyTuning().FixedStepsPerMonth, Simulation->IsSimulationPaused(),
		                    DeltaSeconds);
		UpdateSkyMaterial();
		UpdateDynamicReflections();
	}
}

void ACitySandboxEnvironment::UpdateDynamicReflections()
{
#if PLATFORM_WINDOWS
	const double Now = FPlatformTime::Seconds();
	if (!bDynamicReflectionsEnabled || !LiveReflectionCapture || Now < NextReflectionCaptureSeconds)
		return;
	const FTexture* SkyTexture = SkyLight->GetProcessedSkyTexture();
	if (!SkyTexture || !SkyTexture->TextureRHI.IsValid() || SkyTexture->GetSizeX() != 64)
		return;
	for (TActorIterator<ACityBoardActor> Board(GetWorld()); Board; ++Board)
	{
		LiveReflectionCapture->SetWorldLocation(Board->GetActorLocation() + FVector(0, 0, 40));
		break;
	}
	LiveReflectionCapture->CaptureScene();
	if (!DynamicReflectionCaptureCount)
		UE_LOG(LogTemp,Display,TEXT("CITYSIM_CUBE_DIAGNOSTIC visible=%d world=%s at=%s lighting=%d specular=%d sky=%d"),LiveReflectionCapture->IsVisible(),*LiveReflectionCapture->GetWorld()->GetName(),*LiveReflectionCapture->GetComponentLocation().ToString(),LiveReflectionCapture->ShowFlags.Lighting,LiveReflectionCapture->ShowFlags.Specular,LiveReflectionCapture->ShowFlags.SkyLighting);
	FTextureRenderTargetResource* SourceResource = LiveReflectionTarget->GameThread_GetRenderTargetResource();
	const FTextureRHIRef Destination = SkyTexture->TextureRHI;
	ENQUEUE_RENDER_COMMAND(CityCopyLiveReflection)(
	    [SourceResource, Destination](FRHICommandListImmediate& RHICmdList)
	    {
		// Cube resources also own a scratch 2D render target. The resolved
		// six-face cube is TextureRHI, not GetRenderTargetTexture().
		const FTextureRHIRef Source = SourceResource->TextureRHI;
		if (!Source.IsValid() || !Destination.IsValid()) return;
		FRHICopyTextureInfo Copy;
		Copy.NumSlices = 6;
		Copy.NumMips = FMath::Min(Source->GetNumMips(), Destination->GetNumMips());
		RHICmdList.Transition(FRHITransitionInfo(Source, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		RHICmdList.Transition(FRHITransitionInfo(Destination, ERHIAccess::SRVMask, ERHIAccess::CopyDest));
		RHICmdList.CopyTexture(Source, Destination, Copy);
		RHICmdList.Transition(FRHITransitionInfo(Source, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		RHICmdList.Transition(FRHITransitionInfo(Destination, ERHIAccess::CopyDest, ERHIAccess::SRVMask));
	    });
	++DynamicReflectionCaptureCount;
	NextReflectionCaptureSeconds = Now + .5;
#endif
}

void ACitySandboxEnvironment::UpdateLightingClock(const int64 Step, const double StepProgress,
                                                  const int32 StepsPerMonth, const bool /*bPaused*/,
                                                  const float DeltaSeconds)
{
	if (LightingPreset != ECityLightingPreset::Automatic || DeltaSeconds <= 0.0f ||
	    !FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(StepProgress) || Step < 0 || StepsPerMonth <= 0)
		return;
	// One calendar day per cycle: 360/120/72 seconds at the default 1x/3x/5x rates.
	// Reduce the integer first to retain fractional precision even in very old cities.
	const int64 CycleSteps = StepsPerMonth; // One complete cycle per calendar day.
	const double Phase = FMath::Fmod((Step % CycleSteps + FMath::Clamp(StepProgress, 0.0, 1.0)) / CycleSteps + 0.5, 1.0);
	// Keep direction, horizon and brightness on the same calendar orbit after load.
	// Day/night is a pure presentation of the authoritative calendar phase.
	// The interpolated authoritative clock already supplies smooth motion at every speed.
	DisplayedLightingPhase = Phase;
	constexpr double Times[] = {0.0, 0.15, 0.25, 0.35, 0.65, 0.75, 0.85, 1.0};
	constexpr float Ambient[] = {1.0f, 1.0f, 0.75f, 0.45f, 0.45f, 0.75f, 1.0f, 1.0f};
	const FLinearColor Warm(1.0f, 0.62f, 0.35f), Cool(0.48f, 0.64f, 1.0f);
	const FLinearColor Colors[] = {FLinearColor::White, FLinearColor::White, Warm, Cool, Cool, Warm,
	                               FLinearColor::White, FLinearColor::White};
	int32 Segment = 0;
	while (Segment < 6 && DisplayedLightingPhase > Times[Segment + 1])
		++Segment;
	const float T =
	    static_cast<float>((DisplayedLightingPhase - Times[Segment]) / (Times[Segment + 1] - Times[Segment]));
	const float Blend = T * T * (3.0f - 2.0f * T);
	// A paused clock has an unchanged phase; a paused reload presents its saved phase immediately.
	constexpr float WindowGlow[] = {0.0f, 0.0f, 0.35f, 1.0f, 1.0f, 0.35f, 0.0f, 0.0f};
	DisplayedWindowGlow =
	    FMath::Lerp(WindowGlow[Segment], WindowGlow[Segment + 1], Blend);
	const FLinearColor Horizons[] = {DayHorizon,   DayHorizon,  DuskHorizon, NightHorizon,
	                                 NightHorizon, DuskHorizon, DayHorizon,  DayHorizon};
	const FLinearColor Zeniths[] = {DayZenith,   DayZenith,  DuskZenith, NightZenith,
	                                NightZenith, DuskZenith, DayZenith,  DayZenith};
	DisplayedHorizon =
	    FMath::Lerp(Horizons[Segment], Horizons[Segment + 1], Blend);
	DisplayedZenith = FMath::Lerp(Zeniths[Segment], Zeniths[Segment + 1], Blend);
	UpdateSkyMaterial();
	const double SunHeight = FMath::Cos(DisplayedLightingPhase * UE_TWO_PI);
	const float HorizonFade = FMath::Clamp(static_cast<float>(FMath::Abs(SunHeight) / .50), 0.0f, 1.0f);
	// Both bodies fade to zero at the horizon before swapping. No bright 180-degree
	// shadow flip, upward sun illumination or second mobile directional light.
	DirectionalLight->SetIntensity((SunHeight >= 0.0 ? 4.0f : .8f) * HorizonFade * HorizonFade *
	                               (3.0f - 2.0f * HorizonFade) * (1.0f - DisplayedCloudiness * .35f));
	SkyLight->SetIntensity(
	    FMath::Lerp(Ambient[Segment], Ambient[Segment + 1], Blend));
	DisplayedLightColor =
	    FMath::Lerp(Colors[Segment], Colors[Segment + 1], Blend);
	DirectionalLight->SetLightColor(DisplayedLightColor);
	DirectionalLight->SetRelativeRotation(CelestialLightDirection(DisplayedLightingPhase).Rotation());
}
