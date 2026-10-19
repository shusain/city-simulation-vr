#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presentation/CityAtmosphereMotion.h"
#include "Presentation/CityScenery.h"
#include "Presentation/CityBeachScenery.h"
#include "Presentation/CitySeasonalScenery.h"
#include "Simulation/CityTerrain.h"
#include "CityAtmosphereComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCitySimulationSubsystem;
class UParticleSystem;
class UCityWeatherParticleComponent;

// Instanced scenery and native sprite precipitation, driven by the board tick.
UCLASS()
class CITYSIMVR_API UCityAtmosphereComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCityAtmosphereComponent();
	void Update(UCitySimulationSubsystem* Simulation, float DeltaSeconds);
	const FCityAtmosphereMotion& GetMotion() const
	{
		return Motion;
	}
	static constexpr int32 MaxTrees = 420;
	static constexpr int32 MaxRocks = 100;
	static constexpr int32 MaxPrecipitation = 320;
	static constexpr int32 MaxSplashes = 48;
	static constexpr int32 SceneryPoolCount = 5;
	int32 GetTreeCount() const;
	int32 GetPrecipitationCount() const;
	int32 GetBeachDecorationCount() const;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	friend class FCityAtmospherePresentationTest;
	friend class FCitySeasonalSceneryPresentationTest;
	friend class FCityBeachPresentationTest;
	friend class FCityBeachCaptureCommand;
	void EnsurePools();
	void RebuildScenery(const UCitySimulationSubsystem& Simulation);
	void RebuildBeachScenery(const UCitySimulationSubsystem& Simulation);
	void UpdateSeasonalScenery(const FCityEnvironmentSnapshot& Weather);
	void UpdatePrecipitation(const UCitySimulationSubsystem& Simulation);
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> Meshes;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> SeasonalMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> BeachMeshes;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ColorMaterial;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BeachPools;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	UPROPERTY()
	TArray<TObjectPtr<UParticleSystem>> WeatherTemplates;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCityWeatherParticleComponent>> WeatherSystems;
	FCityAtmosphereMotion Motion;
	TOptional<FCityTerrainGenerationParameters> TerrainParameters;
	TArray<FCitySceneryCandidate> SceneryCandidates;
	TArray<FCityBeachCandidate> BeachCandidates;
	TArray<int32> SceneryWarmth;
	uint64 CitySession = MAX_uint64;
	uint64 PlacementRevision = MAX_uint64;
	FLinearColor DisplayedTreeColor = FLinearColor::Transparent;
	FLinearColor DisplayedConiferColor = FLinearColor::Transparent;
	bool bSnowyConiferMesh = false;
};
