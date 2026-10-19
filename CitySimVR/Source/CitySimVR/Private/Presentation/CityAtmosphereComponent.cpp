#include "Presentation/CityAtmosphereComponent.h"
#include "Presentation/CityWeatherParticles.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Interaction/CityBoardPicking.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "ParticleEmitterInstances.h"

UCityAtmosphereComponent::UCityAtmosphereComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Broadleaf(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Broadleaf_A.SM_Scenery_Broadleaf_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Conifer(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Conifer_A.SM_Scenery_Conifer_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Rock_A.SM_Scenery_Rock_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Palm(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Palm_A.SM_Scenery_Palm_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Autumn(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Broadleaf_Autumn_A.SM_Scenery_Broadleaf_Autumn_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bare(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Broadleaf_Winter_A.SM_Scenery_Broadleaf_Winter_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SnowyConifer(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Scenery_Conifer_Winter_A.SM_Scenery_Conifer_Winter_A"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Rain(
	    TEXT("/Game/CitySim/Art/Weather/P_CityRain.P_CityRain"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> Snow(
	    TEXT("/Game/CitySim/Art/Weather/P_CitySnow.P_CitySnow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Color(
	    TEXT("/Game/CitySim/Art/Materials/M_CityColor.M_CityColor"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FanShell(
	    TEXT("/Game/CitySim/Art/Beach/Meshes/SM_Scenery_FanShell_A.SM_Scenery_FanShell_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SpiralShell(
	    TEXT("/Game/CitySim/Art/Beach/Meshes/SM_Scenery_SpiralShell_A.SM_Scenery_SpiralShell_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Starfish(
	    TEXT("/Game/CitySim/Art/Beach/Meshes/SM_Scenery_Starfish_A.SM_Scenery_Starfish_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Driftwood(
	    TEXT("/Game/CitySim/Art/Beach/Meshes/SM_Scenery_Driftwood_A.SM_Scenery_Driftwood_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DuneGrass(
	    TEXT("/Game/CitySim/Art/Beach/Meshes/SM_Scenery_DuneGrass_A.SM_Scenery_DuneGrass_A"));
	BeachMeshes = {FanShell.Object, SpiralShell.Object, Starfish.Object, Driftwood.Object, DuneGrass.Object};
	// Keep the existing warm broadleaf pool assignment and frozen instances.
	Meshes = {Broadleaf.Object, Conifer.Object, Rock.Object, Palm.Object, Broadleaf.Object};
	ColorMaterial = Color.Object;
	SeasonalMeshes = {Autumn.Object, Bare.Object, SnowyConifer.Object};
	WeatherTemplates = {Rain.Object, Snow.Object};
}

void UCityAtmosphereComponent::EnsurePools()
{
	if (!Pools.IsEmpty() || !GetOwner() || !GetOwner()->GetRootComponent())
		return;
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		auto* Pool =
		    NewObject<UInstancedStaticMeshComponent>(GetOwner(), *FString::Printf(TEXT("CityAtmospherePool%d"), Index));
		Pool->SetupAttachment(GetOwner()->GetRootComponent());
		Pool->SetStaticMesh(Meshes[Index]);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetGenerateOverlapEvents(false);
		Pool->SetCanEverAffectNavigation(false);
		Pool->SetCastShadow(false);
		Pool->SetMobility(EComponentMobility::Movable);
		GetOwner()->AddInstanceComponent(Pool);
		Pool->RegisterComponent();
		Pools.Add(Pool);
		auto* Material = UMaterialInstanceDynamic::Create(ColorMaterial, this);
		Materials.Add(Material);
		// Tree meshes retain their authored shared bark material in slot 0.
		Pool->SetMaterial(Index == 2 ? 0 : 1, Material);
	}
	Materials[2]->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(.30, .29, .26));
	Materials[3]->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(.11, .38, .13));
	Materials[4]->SetVectorParameterValue(TEXT("CityColor"),
	                                      FCitySeasonalScenery::BroadleafColor(ECitySeasonalTreeVariant::Leafy));
	for (int32 Index = 0; Index < BeachMeshes.Num(); ++Index)
	{
		auto* Pool = NewObject<UInstancedStaticMeshComponent>(
		    GetOwner(), *FString::Printf(TEXT("CityBeachPool%d"), Index));
		Pool->SetupAttachment(GetOwner()->GetRootComponent());
		Pool->SetStaticMesh(BeachMeshes[Index]);
		Pool->PrimaryComponentTick.bCanEverTick = false;
		Pool->SetComponentTickEnabled(false);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetGenerateOverlapEvents(false);
		Pool->SetCanEverAffectNavigation(false);
		Pool->SetCastShadow(false);
		Pool->SetMobility(EComponentMobility::Movable);
		GetOwner()->AddInstanceComponent(Pool);
		Pool->RegisterComponent();
		BeachPools.Add(Pool);
	}
	for (int32 Index = 0; Index < WeatherTemplates.Num(); ++Index)
	{
		auto* System = NewObject<UCityWeatherParticleComponent>(
		    GetOwner(), *FString::Printf(TEXT("CityWeatherParticles%d"), Index));
		System->bAutoActivate = false;
		System->bAutoDestroy = false;
		System->SetupAttachment(GetOwner()->GetRootComponent());
		System->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		System->SetGenerateOverlapEvents(false);
		System->SetCanEverAffectNavigation(false);
		System->SetCastShadow(false);
		System->SetTemplate(WeatherTemplates[Index]);
		GetOwner()->AddInstanceComponent(System);
		System->RegisterComponent();
		System->AddTickPrerequisiteActor(GetOwner());
		System->SetComponentTickEnabled(false);
		WeatherSystems.Add(System);
	}
}

int32 UCityAtmosphereComponent::GetTreeCount() const
{
	return Pools.Num() == SceneryPoolCount ? Pools[0]->GetInstanceCount() + Pools[1]->GetInstanceCount() +
	                                             Pools[3]->GetInstanceCount() + Pools[4]->GetInstanceCount()
	                                       : 0;
}
int32 UCityAtmosphereComponent::GetPrecipitationCount() const
{
	int32 Count = 0;
	for (const auto& System : WeatherSystems)
		if (System)
		{
			System->ForceAsyncWorkCompletion(UParticleSystemComponent::STALL);
			for (const auto* Emitter : System->EmitterInstances)
				if (Emitter && Emitter->SpriteTemplate->EmitterName != TEXT("Splashes"))
					Count += Emitter->ActiveParticles;
		}
	return Count;
}

int32 UCityAtmosphereComponent::GetBeachDecorationCount() const
{
	int32 Count = 0;
	for (const auto& Pool : BeachPools)
		if (Pool)
			Count += Pool->GetInstanceCount();
	return Count;
}

void UCityAtmosphereComponent::Update(UCitySimulationSubsystem* Simulation, const float DeltaSeconds)
{
	if (!Simulation)
		return;
	EnsurePools();
	if (Pools.Num() != SceneryPoolCount || BeachPools.Num() != FCityBeachScenery::KindCount || WeatherSystems.Num() != 2)
		return;
	const auto& Weather = Simulation->GetEnvironmentSnapshot();
	const auto& Parameters = Simulation->GetTerrain().GetParameters();
	const bool bNewCity = CitySession != Simulation->GetCitySessionGeneration() || !TerrainParameters.IsSet() ||
	                      TerrainParameters.GetValue() != Parameters;
	if (bNewCity)
	{
		Motion.Reset(Weather);
		TerrainParameters = Parameters;
		CitySession = Simulation->GetCitySessionGeneration();
		PlacementRevision = MAX_uint64;
		SceneryCandidates = FCitySceneryField::SelectCandidates(
		    Simulation->GetTerrain(), Simulation->GetPlacementModel().GetEntranceCell(), MaxTrees, MaxRocks);
		BeachCandidates = FCityBeachScenery::SelectCandidates(Simulation->GetTerrain());
		FCitySceneryField Climate;
		Climate.Build(Simulation->GetTerrain());
		SceneryWarmth.Reset(SceneryCandidates.Num());
		for (const auto& Candidate : SceneryCandidates)
			SceneryWarmth.Add(Climate.GetSample(Candidate.Cell).WarmthPercent);
		bSnowyConiferMesh = false;
		for (const auto& Pool : Pools)
			Pool->ClearInstances();
		for (const auto& Pool : BeachPools)
			Pool->ClearInstances();
		for (const auto& System : WeatherSystems)
		{
			System->DeactivateSystem();
			System->ResetParticles(true);
			System->SetTerrain(Simulation->GetTerrain());
		}
	}
	if (Simulation->IsEntryMenuSuspended())
	{
		for (const auto& Pool : Pools)
			Pool->SetVisibility(false);
		for (const auto& Pool : BeachPools)
			Pool->SetVisibility(false);
		for (const auto& System : WeatherSystems)
		{
			System->SetVisibility(false);
			System->SetComponentTickEnabled(false);
		}
		return;
	}
	for (const auto& Pool : Pools)
		if (!Pool->IsVisible())
			Pool->SetVisibility(true);
	for (const auto& Pool : BeachPools)
		if (!Pool->IsVisible())
			Pool->SetVisibility(true);
	if (!Simulation->IsSimulationPaused())
		Motion.Advance(FMath::Min(DeltaSeconds, .1f), Weather);
	UpdateSeasonalScenery(Weather);
	if (PlacementRevision != Simulation->GetPlacementModel().GetRevision())
	{
		RebuildScenery(*Simulation);
		PlacementRevision = Simulation->GetPlacementModel().GetRevision();
	}
	const FLinearColor ConiferColor(.055, .22, .105);
	if (!ConiferColor.Equals(DisplayedConiferColor, .001f))
	{
		Materials[1]->SetVectorParameterValue(TEXT("CityColor"), ConiferColor);
		DisplayedConiferColor = ConiferColor;
	}
	UpdatePrecipitation(*Simulation);
}

void UCityAtmosphereComponent::UpdateSeasonalScenery(const FCityEnvironmentSnapshot& Weather)
{
	const auto Variant = FCitySeasonalScenery::BroadleafVariant(0, Weather.Season);
	UStaticMesh* Broadleaf = Meshes[0].Get();
	if (Variant == ECitySeasonalTreeVariant::Autumn && SeasonalMeshes[0])
		Broadleaf = SeasonalMeshes[0].Get();
	if (Variant == ECitySeasonalTreeVariant::Bare && SeasonalMeshes[1])
		Broadleaf = SeasonalMeshes[1].Get();
	for (const int32 PoolIndex : {0, 4})
		if (Pools[PoolIndex]->GetStaticMesh() != Broadleaf)
		{
			Pools[PoolIndex]->SetStaticMesh(Broadleaf);
			// The bare mesh only has Bark; slot0 remains the authored shared bark.
			Pools[PoolIndex]->SetMaterial(1, Variant == ECitySeasonalTreeVariant::Bare && SeasonalMeshes[1]
			                                     ? nullptr
			                                     : Materials[PoolIndex].Get());
		}
	const FLinearColor TreeColor = FCitySeasonalScenery::BroadleafColor(Variant);
	if (!TreeColor.Equals(DisplayedTreeColor, .001f))
	{
		for (const int32 PoolIndex : {0, 4})
			Materials[PoolIndex]->SetVectorParameterValue(TEXT("CityColor"), TreeColor);
		DisplayedTreeColor = TreeColor;
	}
	bSnowyConiferMesh = FCitySeasonalScenery::ConiferSnowMesh(Weather, Motion.SnowCover, bSnowyConiferMesh);
	UStaticMesh* Conifer = bSnowyConiferMesh && SeasonalMeshes[2] ? SeasonalMeshes[2].Get() : Meshes[1].Get();
	if (Pools[1]->GetStaticMesh() != Conifer)
		Pools[1]->SetStaticMesh(Conifer);
}

void UCityAtmosphereComponent::RebuildScenery(const UCitySimulationSubsystem& Simulation)
{
	for (const auto& Pool : Pools)
		Pool->ClearInstances();
	const auto& Terrain = Simulation.GetTerrain();
	const auto& City = Simulation.GetPlacementModel();
	for (int32 CandidateIndex = 0; CandidateIndex < SceneryCandidates.Num(); ++CandidateIndex)
	{
		const auto& Candidate = SceneryCandidates[CandidateIndex];
		const int32 X = Candidate.Cell.X, Y = Candidate.Cell.Y;
		// Clear a full cell of margin around any road (including bridges), building,
		// or zone. Scenery never participates in authority or placement rejection.
		bool bClear = true;
		for (int32 Dy = -1; Dy <= 1; ++Dy)
			for (int32 Dx = -1; Dx <= 1; ++Dx)
			{
				const FCityCellCoord Neighbor(X + Dx, Y + Dy);
				bClear &= City.GetGrid().GetOccupant(Neighbor) == 0 && !City.IsRoad(Neighbor) &&
				          !City.IsRoad({Neighbor.X, Neighbor.Y, 1}) && City.GetZone(Neighbor) == ECityZoneType::None;
			}
		if (!bClear)
			continue;
		const uint32 Hash = Candidate.Hash;
		const double Cx = X + .3 + ((Hash >> 8) % 40) / 100.0, Cy = Y + .3 + ((Hash >> 16) % 40) / 100.0;
		const FVector Position(Cx * CityBoardPicking::LogicalCellSizeCm, Cy * CityBoardPicking::LogicalCellSizeCm,
		                       Terrain.SampleHeightCm(Cx, Cy));
		const double Scale = .65 + (Hash >> 24) / 255.0 * .65;
		const int32 PoolIndex =
		    Candidate.Kind == ECitySceneryKind::Broadleaf &&
		            SceneryWarmth[CandidateIndex] >= FCitySeasonalScenery::WarmBroadleafPoolThreshold
		        ? 4
		        : static_cast<int32>(Candidate.Kind);
		Pools[PoolIndex]->AddInstance(FTransform(FRotator(0, Hash % 360, 0), Position, FVector(Scale)));
	}
	RebuildBeachScenery(Simulation);
}

void UCityAtmosphereComponent::RebuildBeachScenery(const UCitySimulationSubsystem& Simulation)
{
	for (const auto& Pool : BeachPools)
		Pool->ClearInstances();
	for (const auto& Candidate : BeachCandidates)
		if (FCityBeachScenery::IsClear(Simulation.GetPlacementModel(), Candidate.Cell))
			BeachPools[static_cast<int32>(Candidate.Kind)]->AddInstance(Candidate.Transform);
}

void UCityAtmosphereComponent::UpdatePrecipitation(const UCitySimulationSubsystem& Simulation)
{
	const bool bSnow = FCityAtmosphereMotion::IsSnow(Simulation.GetEnvironmentSnapshot());
	const bool bVisible = Motion.Precipitation > .02;
	const FVector Wind = FRotator(0, Motion.WindYaw, 0).Vector();
	const float Density = FMath::Clamp(
	    static_cast<float>(Motion.Precipitation * (160 + Motion.WindStrength * 80) / MaxPrecipitation), 0.0f, 1.0f);
	for (int32 Index = 0; Index < WeatherSystems.Num(); ++Index)
	{
		auto* System = WeatherSystems[Index].Get();
		const bool bSelected = bVisible && Index == (bSnow ? 1 : 0);
		if (!bSelected)
		{
			if (System->IsActive())
			{
				System->DeactivateSystem();
				System->ResetParticles(true);
			}
			if (System->IsVisible())
				System->SetVisibility(false);
			System->SetComponentTickEnabled(false);
			continue;
		}
		if (!System->IsVisible())
			System->SetVisibility(true);
		const double Drift = (bSnow ? 110.0 : 160.0) * Motion.WindStrength;
		System->SetWeather(FVector(Wind.X * Drift, Wind.Y * Drift,
		                           -1900.0 / (bSnow ? UCityWeatherParticleComponent::SnowLifetime
		                                            : UCityWeatherParticleComponent::RainLifetime)),
		                   Density);
		if (!System->IsActive())
			System->ActivateSystem(true);
		System->SetComponentTickEnabled(!Simulation.IsSimulationPaused());
	}
}
void UCityAtmosphereComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const auto& System : WeatherSystems)
		if (System)
			System->DestroyComponent();
	WeatherSystems.Reset();
	for (const auto& Pool : Pools)
		if (Pool)
			Pool->DestroyComponent();
	Pools.Reset();
	for (const auto& Pool : BeachPools)
		if (Pool)
			Pool->DestroyComponent();
	BeachPools.Reset();
	BeachCandidates.Reset();
	Materials.Reset();
	SceneryCandidates.Reset();
	SceneryWarmth.Reset();
	Super::EndPlay(Reason);
}
