#pragma once
#include "CoreMinimal.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleModule.h"
#include "Simulation/CityTerrain.h"
#include "CityWeatherParticles.generated.h"

// Two batched native sprite systems, attached in the board's logical space.
// A private heightfield snapshot allows particle simulation to run asynchronously
// without reading mutable city authority or tracing against scene collision.
UCLASS()
class CITYSIMVR_API UCityWeatherParticleComponent : public UParticleSystemComponent
{
	GENERATED_BODY()
public:
	void SetTerrain(const FCityTerrainHeightfield& Terrain);
	void SetWeather(const FVector& Velocity, float Density);
	float SurfaceHeight(float X, float Y) const;
	FVector2D GetBoardSize() const;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	FVector3f WeatherVelocity = FVector3f::ZeroVector;
	static constexpr int32 MaxDrops = 320;
	static constexpr int32 MaxSplashes = 48;
	// Cross the 19 m precipitation layer in 0.35 s. Asset spawn rate follows this
	// lifetime so faster rain retains its density and the existing live cap.
	static constexpr float RainLifetime = .35f;
	static constexpr float SnowLifetime = 5.0f;
	static constexpr float SplashLifetime = .4f;

	// Repeatable editor asset authoring, exposed to the project Python pipeline.
	UFUNCTION(BlueprintCallable, Category = "CitySim|Art")
	static bool CreateWeatherAssets(bool bRainOnly = false);

private:
	FCityTerrainHeightfield Surface;
	FBox WeatherBounds = FBox(ForceInit);
	float PreviousDensity = -1.0f;
};

// Cascade owns spawning, lifetime, packed particle buffers and sprite rendering.
// This small module supplies the terrain surface and shared progressive wind.
UCLASS(editinlinenew, meta = (DisplayName = "City tabletop weather"))
class CITYSIMVR_API UCityWeatherParticleModule : public UParticleModule
{
	GENERATED_BODY()
public:
	UCityWeatherParticleModule();
	UPROPERTY(EditAnywhere, Category = "Weather")
	uint8 Kind = 0; // rain, snow, splash
	virtual void Spawn(const FSpawnContext& Context) override;
	virtual void Update(const FUpdateContext& Context) override;
	virtual void CompileModule(FParticleEmitterBuildInfo& EmitterInfo) override;
};
