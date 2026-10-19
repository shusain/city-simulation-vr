#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CitySandboxEnvironment.generated.h"

class UMaterialParameterCollection;
class UMaterialInstanceDynamic;
class UDirectionalLightComponent;
class USceneComponent;
class USkyLightComponent;
class USphereReflectionCaptureComponent;
class UStaticMeshComponent;
class USceneCaptureComponentCube;
class UTextureRenderTargetCube;
class ASceneCaptureCube;

UENUM(BlueprintType)
enum class ECityLightingPreset : uint8
{
	Day,
	Dusk,
	Night,
	Automatic
};

UCLASS()
class CITYSIMVR_API ACitySandboxEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ACitySandboxEnvironment();
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// Read-only simulation clock input; fractional steps keep lighting smooth between days.
	void UpdateLightingClock(int64 Step, double StepProgress, int32 StepsPerMonth, bool bPaused, float DeltaSeconds);
	void SetLightingPreset(ECityLightingPreset Preset);
	// Called by pawn selection; retain desktop reach and the Quest shadow budget.
	void SetHeadTrackedShadowCoverage(bool bHeadTracked);
	ECityLightingPreset GetLightingPreset() const
	{
		return LightingPreset;
	}
	float GetParkDaylightFactor() const
	{
		return FMath::Clamp((.35f - DisplayedWindowGlow) / .35f, 0.0f, 1.0f);
	}
	bool AreDynamicReflectionsEnabled() const { return bDynamicReflectionsEnabled; }
	uint64 GetDynamicReflectionCaptureCount() const { return DynamicReflectionCaptureCount; }

private:
	friend class FCityDynamicReflectionCommand;
	void UpdateDynamicReflections();
	bool bDynamicReflectionsEnabled = false;
	double NextReflectionCaptureSeconds = 0.0;
	uint64 DynamicReflectionCaptureCount = 0;
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponentCube> LiveReflectionCapture;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetCube> LiveReflectionTarget;
	UPROPERTY(Transient)
	TObjectPtr<ASceneCaptureCube> LiveReflectionActor;
	void UpdateSkyMaterial();
	double DisplayedLightingPhase = 0.0;
	int32 PreviousCSMResolution = -1;
	uint32 PreviousCSMFlags = 0;
	FName ReflectionMixTag;
	float DesktopCascadeTransition = .1f;
	bool bHeadTrackedShadowView = false;
	float DisplayedWindowGlow = 0.0f;
	float DisplayedCloudiness = 0.0f;
	uint64 WeatherCitySession = MAX_uint64;
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> LightingCollection;
	FLinearColor DisplayedHorizon = FLinearColor(0.35f, 0.60f, 0.80f);
	FLinearColor DisplayedZenith = FLinearColor(0.045f, 0.18f, 0.42f);
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;
	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<UStaticMeshComponent> SkyDome;
	FLinearColor DisplayedLightColor = FLinearColor::White;
	ECityLightingPreset LightingPreset = ECityLightingPreset::Automatic;
	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<UDirectionalLightComponent> DirectionalLight;

	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Sandbox")
	TObjectPtr<USphereReflectionCaptureComponent> RoomReflection;
};
