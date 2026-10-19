#include "Presentation/CityWeatherParticles.h"
#include "Interaction/CityBoardPicking.h"
#include "ParticleEmitterInstances.h"
#include "ParticleEmitterInstanceOwner.h"
#include "ParticleHelper.h"
#include "Particles/ParticleSystem.h"

namespace
{
	constexpr float FallHeight = 1900.0f;
} // namespace

void UCityWeatherParticleComponent::SetTerrain(const FCityTerrainHeightfield& Terrain)
{
	ForceAsyncWorkCompletion(STALL);
	Surface = Terrain;
	const auto Size = GetBoardSize();
	int32 Minimum = Surface.GetSeaLevelCm(), Maximum = Minimum;
	for (const int32 Height : Surface.GetQuantizedHeightsCm())
	{
		Minimum = FMath::Min(Minimum, Height);
		Maximum = FMath::Max(Maximum, Height);
	}
	WeatherBounds =
	    FBox(FVector(-400, -400, Minimum - 400), FVector(Size.X + 400, Size.Y + 400, Maximum + FallHeight + 400));
	UpdateBounds();
	MarkRenderTransformDirty();
}

FVector2D UCityWeatherParticleComponent::GetBoardSize() const
{
	return CityBoardPicking::GetLogicalBoardSize(Surface.GetDimensions());
}

FBoxSphereBounds UCityWeatherParticleComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return WeatherBounds.IsValid ? FBoxSphereBounds(WeatherBounds.TransformBy(LocalToWorld))
	                             : Super::CalcBounds(LocalToWorld);
}

void UCityWeatherParticleComponent::SetWeather(const FVector& Velocity, const float Density)
{
	ForceAsyncWorkCompletion(STALL);
	WeatherVelocity = FVector3f(Velocity);
	if (!FMath::IsNearlyEqual(Density, PreviousDensity, .001f))
	{
		SetFloatParameter(TEXT("WeatherDensity"), Density);
		PreviousDensity = Density;
	}
}

float UCityWeatherParticleComponent::SurfaceHeight(const float X, const float Y) const
{
	return FMath::Max(static_cast<float>(Surface.SampleHeightCm(X / CityBoardPicking::LogicalCellSizeCm,
	                                                            Y / CityBoardPicking::LogicalCellSizeCm)),
	                  static_cast<float>(Surface.GetSeaLevelCm()));
}

UCityWeatherParticleModule::UCityWeatherParticleModule()
{
	bSpawnModule = true;
	bUpdateModule = true;
	LODValidity = 1;
}

void UCityWeatherParticleModule::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	EmitterInfo.MaxLifetime = Kind == 0   ? UCityWeatherParticleComponent::RainLifetime
	                          : Kind == 1 ? UCityWeatherParticleComponent::SnowLifetime
	                                      : UCityWeatherParticleComponent::SplashLifetime;
	EmitterInfo.MaxSize = Kind == 0 ? FVector2D(60, 360) : FVector2D(320, 320);
	EmitterInfo.SpawnModules.Add(this);
}

void UCityWeatherParticleModule::Spawn(const FSpawnContext& Context)
{
	auto* Weather = Cast<UCityWeatherParticleComponent>(Context.Owner.Component.GetDistributionData());
	FBaseParticle& Particle = *Context.ParticleBase;
	const int32 Limit =
	    Kind == 2 ? UCityWeatherParticleComponent::MaxSplashes : UCityWeatherParticleComponent::MaxDrops;
	if (!Weather || Context.Owner.ActiveParticles > Limit)
	{
		// The engine rejects this spawn immediately, before it enters the live buffer.
		Particle.RelativeTime = 2.0f;
		return;
	}
	auto& Random = GetRandomStream(Context);
	const auto BoardSize = Weather->GetBoardSize();
	const float X = Random.FRand() * BoardSize.X, Y = Random.FRand() * BoardSize.Y;
	const float Lifetime = Kind == 0   ? UCityWeatherParticleComponent::RainLifetime
	                       : Kind == 1 ? UCityWeatherParticleComponent::SnowLifetime
	                                   : UCityWeatherParticleComponent::SplashLifetime;
	Particle.OneOverMaxLifetime = 1.0f / Lifetime;
	Particle.RelativeTime = Context.SpawnTime / Lifetime;
	Particle.Location = FVector(X, Y, Weather->SurfaceHeight(X, Y) + (Kind == 2 ? 12.0f : FallHeight));
	Particle.OldLocation = Particle.Location;
	Particle.Velocity = Particle.BaseVelocity = Kind == 2 ? FVector3f::ZeroVector : Weather->WeatherVelocity;
	const float Size = Kind == 1 ? Random.FRandRange(100.0f, 145.0f) : 60.0f;
	Particle.Size = Particle.BaseSize = Kind == 0 ? FVector3f(Size, 360, 1) : FVector3f(Size, Size, 1);
	Particle.Color = Particle.BaseColor =
	    Kind == 1 ? FLinearColor(.86, .92, .97, .85) : FLinearColor(.38, .58, .70, .65);
	Particle.Rotation = Kind == 1 ? Random.FRand() * UE_TWO_PI : 0.0f;
}

void UCityWeatherParticleModule::Update(const FUpdateContext& Context)
{
	const auto* Weather = Cast<UCityWeatherParticleComponent>(Context.Owner.Component.GetDistributionData());
	if (!Weather)
		return;
	const auto BoardSize = Weather->GetBoardSize();
	// Walk the engine's packed buffer directly: no transform arrays, instance
	// updates, render-state rebuilds, scene traces, events or particle Actors.
	for (int32 Index = Context.Owner.ActiveParticles - 1; Index >= 0; --Index)
	{
		auto& Particle = *reinterpret_cast<FBaseParticle*>(
		    Context.Owner.ParticleData + Context.Owner.ParticleStride * Context.Owner.ParticleIndices[Index]);
		const float Age = FMath::Clamp(Particle.RelativeTime, 0.0f, 1.0f);
		if (Kind == 2)
		{
			Particle.Size = Particle.BaseSize * (1.0f + Age * 4.0f);
			Particle.Color.A = Particle.BaseColor.A * (1.0f - Age);
			continue;
		}
		Particle.Velocity = Particle.BaseVelocity = Weather->WeatherVelocity;
		if (Kind == 1)
		{
			Particle.Velocity.Y += FMath::Sin(Age * UE_TWO_PI + Particle.Rotation) * 55.0f;
			Particle.Rotation += Context.DeltaTime * .25f;
		}
		const FVector Next = Particle.Location + FVector(Particle.Velocity) * Context.DeltaTime;
		if (Next.X < 0 || Next.Y < 0 || Next.X >= BoardSize.X || Next.Y >= BoardSize.Y ||
		    Next.Z <= Weather->SurfaceHeight(Next.X, Next.Y) + 8.0f)
			Context.Owner.KillParticle(Index);
		else
			Particle.Color.A = Particle.BaseColor.A * FMath::Min(1.0f, (1.0f - Age) * 8.0f);
	}
}

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatParticleParameter.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

bool UCityWeatherParticleComponent::CreateWeatherAssets(const bool bRainOnly)
{
#if WITH_EDITOR
	for (int32 SystemIndex = 0; SystemIndex < (bRainOnly ? 1 : 2); ++SystemIndex)
	{
		const FString Name = SystemIndex == 0 ? TEXT("P_CityRain") : TEXT("P_CitySnow");
		const FString Path = TEXT("/Game/CitySim/Art/Weather/") + Name;
		UPackage* Package = CreatePackage(*Path);
		auto* System = LoadObject<UParticleSystem>(nullptr, *(Path + TEXT(".") + Name));
		const bool bNew = !System;
		if (!System)
			System = NewObject<UParticleSystem>(Package, *Name, RF_Public | RF_Standalone);
		System->Emitters.Reset();
		System->bUseFixedRelativeBoundingBox = true;
		const auto DefaultSize = CityBoardPicking::GetLogicalBoardSize(FCityGridDimensions());
		System->FixedRelativeBoundingBox =
		    FBox(FVector(-400, -400, -2000), FVector(DefaultSize.X + 400, DefaultSize.Y + 400, 12000));
		System->WarmupTime = 0; // Spread initial filling across frames; no activation hitch.
		System->SecondsBeforeInactive = .5f;
		System->bAutoDeactivate = false;
		System->LODMethod = PARTICLESYSTEMLODMETHOD_DirectSet;
		System->LODDistances = {0};
		for (int32 EmitterIndex = 0; EmitterIndex < (SystemIndex == 0 ? 2 : 1); ++EmitterIndex)
		{
			const uint8 Kind = EmitterIndex == 1 ? 2 : SystemIndex;
			const int32 Limit = Kind == 2 ? MaxSplashes : MaxDrops;
			const float Lifetime = Kind == 0 ? RainLifetime : Kind == 1 ? SnowLifetime : SplashLifetime;
			auto* Emitter = NewObject<UParticleSpriteEmitter>(System);
			Emitter->SetEmitterName(Kind == 0 ? TEXT("Rain") : Kind == 1 ? TEXT("Snow") : TEXT("Splashes"));
			Emitter->InitialAllocationCount = Limit;
			Emitter->CreateLODLevel(0);
			auto* LOD = Emitter->LODLevels[0].Get();
			auto* Required = LOD->RequiredModule.Get();
			Required->Material = LoadObject<UMaterialInterface>(
			    nullptr, Kind == 2 ? TEXT("/Game/CitySim/Art/Weather/M_CityWeatherSplash.M_CityWeatherSplash")
			                       : TEXT("/Game/CitySim/Art/Weather/M_CityWeatherDrop.M_CityWeatherDrop"));
			if (!Required->Material)
				return false;
			Required->bUseLocalSpace = true;
			Required->bKillOnDeactivate = true;
			Required->ScreenAlignment = Kind == 0 ? PSA_Velocity : PSA_FacingCameraPosition;
			Required->SortMode = PSORTMODE_None;
			Required->bUseMaxDrawCount = true;
			Required->MaxDrawCount = Limit;
			auto* Rate = CastChecked<UDistributionFloatConstant>(LOD->SpawnModule->Rate.Distribution);
			Rate->Constant = Limit / Lifetime;
			Rate->bIsDirty = true;
			auto* Density = NewObject<UDistributionFloatParticleParameter>(LOD->SpawnModule);
			Density->ParameterName = TEXT("WeatherDensity");
			Density->ParamMode = DPM_Direct;
			Density->Constant = 0;
			Density->MinInput = Density->MinOutput = 0;
			Density->MaxInput = Density->MaxOutput = 1;
			LOD->SpawnModule->RateScale.Distribution = Density;
			auto* Module = NewObject<UCityWeatherParticleModule>(System);
			Module->Kind = Kind;
			LOD->Modules = {Module};
			Emitter->UpdateModuleLists();
			System->Emitters.Add(Emitter);
		}
		System->BuildEmitters();
		System->PostEditChange();
		if (bNew)
			FAssetRegistryModule::AssetCreated(System);
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(
		        Package, System,
		        *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), Args))
			return false;
	}
	return true;
#else
	return false;
#endif
}
