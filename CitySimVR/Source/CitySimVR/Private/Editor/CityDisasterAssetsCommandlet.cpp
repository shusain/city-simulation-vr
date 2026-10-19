#include "Editor/CityDisasterAssetsCommandlet.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Location/ParticleModuleLocation.h"
#include "Particles/Location/ParticleModuleLocationPrimitiveCylinder.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionVectorUniform.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Materials/MaterialInterface.h"
#endif
UCityDisasterAssetsCommandlet::UCityDisasterAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}
int32 UCityDisasterAssetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	auto Float = [](UObject* Outer, FRawDistributionFloat& Raw, float Value)
	{
		auto* D = NewObject<UDistributionFloatConstant>(Outer);
		D->Constant = Value;
		Raw.Distribution = D;
		Raw.Initialize();
	};
	auto Vector = [](UObject* Outer, FRawDistributionVector& Raw, FVector Min, FVector Max)
	{
		auto* D = NewObject<UDistributionVectorUniform>(Outer);
		D->Min = Min;
		D->Max = Max;
		Raw.Distribution = D;
		Raw.Initialize();
	};
	auto Create = [&](const TCHAR* Name, bool Tornado) -> bool
	{
		const FString Path = FString(TEXT("/Game/CitySim/Art/Effects/")) + Name;
		UPackage* Package = CreatePackage(*Path);
		Package->FullyLoad();
		auto* System = FindObject<UParticleSystem>(Package, Name);
		if (!System)
		{
			System = NewObject<UParticleSystem>(Package, Name, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(System);
		}
		System->Emitters.Reset();
		System->LODSettings.SetNum(1);
		System->LODDistances = {0.f};
		System->bUseFixedRelativeBoundingBox = true;
		System->FixedRelativeBoundingBox = FBox(FVector(-3500, -3500, -100), FVector(3500, 3500, 7000));
		for (int32 Index = 0; Index < (Tornado ? 8 : 1); ++Index)
		{
			const float Radius = 200.f + Index * 130.f;
			auto* Emitter = NewObject<UParticleSpriteEmitter>(System);
			Emitter->EmitterName = Tornado ? TEXT("FunnelDust") : TEXT("QuakeDust");
			Emitter->InitialAllocationCount = Tornado ? 64 : 96;
			auto* LOD = NewObject<UParticleLODLevel>(Emitter);
			LOD->Level = 0;
			LOD->bEnabled = true;
			Emitter->LODLevels.Add(LOD);
			System->Emitters.Add(Emitter);
			LOD->RequiredModule = NewObject<UParticleModuleRequired>(System);
			LOD->RequiredModule->LODValidity = 1;
			LOD->RequiredModule->bUseLocalSpace = true;
			LOD->RequiredModule->EmitterDuration = 2;
			LOD->RequiredModule->EmitterLoops = 0;
			LOD->RequiredModule->ScreenAlignment = PSA_Rectangle;
			const TCHAR* Material = TEXT("/Game/CitySim/Art/Materials/M_CitySmokeParticle");
			LOD->RequiredModule->Material = LoadObject<UMaterialInterface>(nullptr, Material);
			if (!LOD->RequiredModule->Material)
				return false;
			LOD->SpawnModule = NewObject<UParticleModuleSpawn>(System);
			LOD->SpawnModule->LODValidity = 1;
			Float(System, LOD->SpawnModule->Rate, Tornado ? 16 : 36);
			auto* Life = NewObject<UParticleModuleLifetime>(System);
			Life->LODValidity = 1;
			Float(System, Life->Lifetime, Tornado ? 3.f : 2.f);
			LOD->Modules.Add(Life);
			auto* Size = NewObject<UParticleModuleSize>(System);
			Size->LODValidity = 1;
			Vector(System, Size->StartSize, Tornado ? FVector(550 + Index * 90) : FVector(700),
			       Tornado ? FVector(800 + Index * 110) : FVector(1100));
			LOD->Modules.Add(Size);
			if (Tornado)
			{
				auto* Location = NewObject<UParticleModuleLocationPrimitiveCylinder>(System);
				Location->LODValidity = 1;
				Location->SurfaceOnly = true;
				Location->HeightAxis = PMLPC_HEIGHTAXIS_Z;
				Location->Velocity = false;
				Float(System, Location->StartRadius, Radius);
				Float(System, Location->StartHeight, 500);
				Vector(System, Location->StartLocation, FVector(0, 0, Index * 450 + 250),
				       FVector(0, 0, Index * 450 + 250));
				LOD->Modules.Add(Location);
			}
			else
			{
				auto* Location = NewObject<UParticleModuleLocation>(System);
				Location->LODValidity = 1;
				Vector(System, Location->StartLocation, FVector(-2200, -2200, 30), FVector(2200, 2200, 150));
				LOD->Modules.Add(Location);
			}
			auto* Velocity = NewObject<UParticleModuleVelocity>(System);
			Velocity->LODValidity = 1;
			Vector(System, Velocity->StartVelocity, FVector(-30, -30, 40), FVector(30, 30, Tornado ? 160 : 90));
			LOD->Modules.Add(Velocity);
			auto* Color = NewObject<UParticleModuleColorOverLife>(System);
			Color->LODValidity = 1;
			auto* RGB = NewObject<UDistributionVectorConstantCurve>(System);
			RGB->ConstantCurve.AddPoint(0, Tornado ? FVector(.35, .39, .45) : FVector(.5, .4, .25));
			RGB->ConstantCurve.AddPoint(1, Tornado ? FVector(.5, .53, .6) : FVector(.6, .5, .32));
			Color->ColorOverLife.Distribution = RGB;
			Color->ColorOverLife.Initialize();
			auto* Alpha = NewObject<UDistributionFloatConstantCurve>(System);
			Alpha->ConstantCurve.AddPoint(0, 0.f);
			Alpha->ConstantCurve.AddPoint(.15f, Tornado ? .9f : .75f);
			Alpha->ConstantCurve.AddPoint(.7f, Tornado ? .85f : .6f);
			Alpha->ConstantCurve.AddPoint(1, 0.f);
			Color->AlphaOverLife.Distribution = Alpha;
			Color->AlphaOverLife.Initialize();
			LOD->Modules.Add(Color);
			LOD->UpdateModuleLists();
		}
		System->BuildEmitters();
		System->PostEditChange();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Filename =
		    FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, System, *Filename, Args);
	};
	if (!Create(TEXT("PS_CityEarthquakeDust"), false) || !Create(TEXT("PS_CityTornado"), true))
		return 1;
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_DISASTER_PARTICLES_SAVED Quake=1 Tornado=8 CPU sprites bounded lifetime/rate"));
	return 0;
#else
	return 1;
#endif
}
