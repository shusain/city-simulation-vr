#include "Editor/CitySafetyAssetsCommandlet.h"
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
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionVectorUniform.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Materials/MaterialInterface.h"
#endif
UCitySafetyAssetsCommandlet::UCitySafetyAssetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}
int32 UCitySafetyAssetsCommandlet::Main(const FString& Params)
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
	auto Create = [&](const TCHAR* Name, bool Water) -> bool
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
		System->FixedRelativeBoundingBox = FBox(FVector(-600, -600, -100), FVector(800, 600, 1800));
		for (int32 Index = 0; Index < (Water ? 1 : 2); ++Index)
		{
			const bool Smoke = !Water && Index == 1;
			auto* Emitter = NewObject<UParticleSpriteEmitter>(System);
			Emitter->EmitterName = Water ? TEXT("WaterSpray") : Smoke ? TEXT("Smoke") : TEXT("Flame");
			Emitter->InitialAllocationCount = Smoke ? 64 : 96;
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
			LOD->RequiredModule->ScreenAlignment = Water ? PSA_Velocity : PSA_Rectangle;
			const TCHAR* Material = Water   ? TEXT("/Game/CitySim/Art/Materials/M_CityWaterParticle")
			                        : Smoke ? TEXT("/Game/CitySim/Art/Materials/M_CitySmokeParticle")
			                                : TEXT("/Game/CitySim/Art/Materials/M_CityFlameParticle");
			LOD->RequiredModule->Material = LoadObject<UMaterialInterface>(nullptr, Material);
			if (!LOD->RequiredModule->Material)
				return false;
			LOD->SpawnModule = NewObject<UParticleModuleSpawn>(System);
			LOD->SpawnModule->LODValidity = 1;
			Float(System, LOD->SpawnModule->Rate, Water ? 80 : Smoke ? 12 : 36);
			auto* Life = NewObject<UParticleModuleLifetime>(System);
			Life->LODValidity = 1;
			Float(System, Life->Lifetime, Water ? .9f : Smoke ? 4.f : 2.f);
			LOD->Modules.Add(Life);
			auto* Size = NewObject<UParticleModuleSize>(System);
			Size->LODValidity = 1;
			Vector(System, Size->StartSize,
			       Water   ? FVector(18, 48, 18)
			       : Smoke ? FVector(160, 160, 160)
			               : FVector(140, 230, 140),
			       Water   ? FVector(28, 72, 28)
			       : Smoke ? FVector(300, 300, 300)
			               : FVector(250, 390, 250));
			LOD->Modules.Add(Size);
			auto* Location = NewObject<UParticleModuleLocation>(System);
			Location->LODValidity = 1;
			Vector(System, Location->StartLocation, Water ? FVector(-5, -4, -4) : FVector(-220, -220, 0),
			       Water ? FVector(5, 4, 4) : FVector(220, 220, 30));
			LOD->Modules.Add(Location);
			auto* Velocity = NewObject<UParticleModuleVelocity>(System);
			Velocity->LODValidity = 1;
			Vector(System, Velocity->StartVelocity,
			       Water   ? FVector(650, -20, -20)
			       : Smoke ? FVector(30, -20, 120)
			               : FVector(-20, -20, 150),
			       Water   ? FVector(800, 20, 20)
			       : Smoke ? FVector(70, 20, 200)
			               : FVector(20, 20, 300));
			LOD->Modules.Add(Velocity);
			auto* Color = NewObject<UParticleModuleColorOverLife>(System);
			Color->LODValidity = 1;
			auto* RGB = NewObject<UDistributionVectorConstantCurve>(System);
			RGB->ConstantCurve.AddPoint(0, Water   ? FVector(.25, .7, 1)
			                               : Smoke ? FVector(.12, .12, .13)
			                                       : FVector(1, .65, .08));
			RGB->ConstantCurve.AddPoint(1, Water   ? FVector(.65, .9, 1)
			                               : Smoke ? FVector(.22, .22, .24)
			                                       : FVector(.9, .08, .01));
			Color->ColorOverLife.Distribution = RGB;
			Color->ColorOverLife.Initialize();
			auto* Alpha = NewObject<UDistributionFloatConstantCurve>(System);
			Alpha->ConstantCurve.AddPoint(0, 0.f);
			Alpha->ConstantCurve.AddPoint(.15f, Smoke ? .45f : .8f);
			Alpha->ConstantCurve.AddPoint(.7f, Smoke ? .4f : .9f);
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
	if (!Create(TEXT("PS_CityFire"), false) || !Create(TEXT("PS_CityWaterSpray"), true))
		return 1;
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_SAFETY_PARTICLES_SAVED FireSmoke=2 Water=1 CPU sprites bounded lifetime/rate"));
	return 0;
#else
	return 1;
#endif
}
