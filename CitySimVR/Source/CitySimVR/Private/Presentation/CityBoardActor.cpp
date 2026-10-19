#include "Presentation/CityBoardActor.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityTechnology.h"
#include "Simulation/CityCommercialAi.h"
#include "Presentation/CityUiContent.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Presentation/CityCitizenPresentation.h"
#include "Animation/BlendSpace1D.h"
#include "Presentation/CityAtmosphereComponent.h"
#include "Misc/Paths.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityUtilitySimulation.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Presentation/CityAudioSettings.h"
#include "Presentation/CitySoundscapeComponent.h"
#include "Presentation/CityMusicComponent.h"
#include "Misc/CoreDelegates.h"
#include "Presentation/CityParkActivity.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Presentation/CityConditionIndicators.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Presentation/CityRoadPresentation.h"
#include "Presentation/CityRoadRibbon.h"
#include "Presentation/CityBuildingSupports.h"
#include "Presentation/CityUtilityFoundations.h"
#include "Presentation/CityHomeWalk.h"
#include "Presentation/CityIndustrialDriveway.h"
#include "Presentation/CityPrivateEntrances.h"
#include "Presentation/CityEmergencyPresentation.h"
#include "Presentation/CityWeatherCoverage.h"
#include "Presentation/CityWaterMesh.h"
#include "Presentation/CityTerrainSurface.h"

#include "Algo/Reverse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Containers/Queue.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "IXRTrackingSystem.h"
#include "Interaction/CityAreaSelection.h"
#include "Presentation/CitySelectionHighlight.h"
#include "Interaction/CityBoardPicking.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityServiceGuidance.h"
#include "Presentation/CityNeedsPresentation.h"
#include "ProceduralMeshComponent.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double PlaneMeshSizeCm = 100.0;
	constexpr double DisplayedBoardWidthCm = 200.0;

	FCityRoadDefinition ResolveRoadDefinition(const FName Id)
	{
		if (Id == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt();
		if (Id == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue();
		if (Id == TEXT("Highway"))
			return FCityRoadDefinition::Highway();
		return FCityRoadDefinition::Local();
	}

	const TCHAR* RoadDisplayName(const FName Id)
	{
		if (Id == TEXT("DirtRoad"))
			return TEXT("Dirt Road");
		if (Id == TEXT("Avenue"))
			return TEXT("Avenue");
		if (Id == TEXT("Highway"))
			return TEXT("Highway");
		return TEXT("Local Road");
	}
} // namespace

ACityBoardActor::ACityBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	BoardRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BoardRoot"));
	SetRootComponent(BoardRoot);
	Atmosphere = CreateDefaultSubobject<UCityAtmosphereComponent>(TEXT("CityAtmosphere"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BoardMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTerrain.MI_CityTerrain"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityWater.MI_CityWater"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HoverMaterial(
	    TEXT("/Game/XRFramework/Materials/MI_Grid_Accent.MI_Grid_Accent"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrafficCarMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_Car_A.SM_Traffic_Car_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrafficTruckMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_Truck_A.SM_Traffic_Truck_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CitizenMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Citizen_A.SM_Citizen_A"));
	for (auto& Mesh : CitizenPresentationMeshes)
		Mesh = CitizenMesh.Object;
#if PLATFORM_WINDOWS
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CasualCitizen(
	    TEXT("/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Casual.SM_Citizen_PC_Casual"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> OfficeCitizen(
	    TEXT("/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Office.SM_Citizen_PC_Office"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WorkerCitizen(
	    TEXT("/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Worker.SM_Citizen_PC_Worker"));
	bRefinedCitizens = !FParse::Param(FCommandLine::Get(), TEXT("CityLegacyCitizens")) && CasualCitizen.Object &&
	                   OfficeCitizen.Object && WorkerCitizen.Object;
	if (bRefinedCitizens)
	{
		CitizenPresentationMeshes[0] = CasualCitizen.Object;
		CitizenPresentationMeshes[1] = OfficeCitizen.Object;
		CitizenPresentationMeshes[2] = WorkerCitizen.Object;
	}
#endif
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadStraightMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Road_Straight.SM_Road_Straight"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadCornerMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Road_Corner.SM_Road_Corner"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadTMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Road_T.SM_Road_T"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadCrossMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Road_Cross.SM_Road_Cross"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ResidentialLowMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_A.SM_Building_Residential_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ResidentialMediumMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A.SM_Building_Residential_Medium_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ResidentialHighMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A.SM_Building_Residential_High_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ResidentialConstructionMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConstructionJibMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Construction_Crane_Jib_A.SM_Construction_Crane_Jib_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConstructionHookMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Construction_Crane_Hook_A.SM_Construction_Crane_Hook_A"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InvalidMaterial(
	    TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray_02.MI_PrototypeGrid_Gray_02"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlacedMaterial(
	    TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CityColorMaterial(
	    TEXT("/Game/CitySim/Art/Materials/M_CityColor.M_CityColor"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityDefault_OverlayFade.MI_CityDefault_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FacadeFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityFacade_OverlayFade.MI_CityFacade_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ResidentialFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityResidential_OverlayFade.MI_CityResidential_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CommercialFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityCommercial_OverlayFade.MI_CityCommercial_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PowerFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityPower_OverlayFade.MI_CityPower_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ServiceFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityService_OverlayFade.MI_CityService_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoofFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityRoof_OverlayFade.MI_CityRoof_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoadFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityRoad_OverlayFade.MI_CityRoad_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrimFadeMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTrim_OverlayFade.MI_CityTrim_OverlayFade"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoadSupportMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTrim.MI_CityTrim"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoadMarkingMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityRoadMarking.MI_CityRoadMarking"));
	PlaceholderBuildingMesh = CubeMesh.Object;
	ValidPreviewMaterial = HoverMaterial.Object;
	InvalidPreviewMaterial = InvalidMaterial.Object;
	PlacedBuildingMaterial = PlacedMaterial.Object;
	ZoneBaseMaterial = CityColorMaterial.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PavementAsset(
	    TEXT("/Game/CitySim/Art/Materials/M_CityPavement.M_CityPavement"));
	PavementBaseMaterial = PavementAsset.Object;
	OverlayFadeMaterials.Add(NAME_None, DefaultFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityFacade"), FacadeFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityResidential"), ResidentialFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityCommercial"), CommercialFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityPower"), PowerFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityService"), ServiceFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityRoof"), RoofFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityRoad"), RoadFadeMaterial.Object);
	OverlayFadeMaterials.Add(TEXT("MI_CityTrim"), TrimFadeMaterial.Object);

	const double CellMeshScale = CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm;

	HeatMapSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeatMapSurface"));
	HeatMapSurface->SetupAttachment(BoardRoot);
	HeatMapSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeatMapSurface->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HeatMaterial(
	    TEXT("/Game/CitySim/UI/M_CityHeatMap.M_CityHeatMap"));
	HeatMapSurface->SetMaterial(0, HeatMaterial.Object);
	WaterPlacementSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterPlacementSurface"));
	WaterPlacementSurface->SetupAttachment(BoardRoot);
	WaterPlacementSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterPlacementSurface->SetCastShadow(false);
	WaterPlacementSurface->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterPlacementMaterial(
	    TEXT("/Game/CitySim/UI/M_CityWaterPlacementOverlay.M_CityWaterPlacementOverlay"));
	WaterPlacementSurface->SetMaterial(0, WaterPlacementMaterial.Object);
	WaterPlacementSurface->SetMaterial(1, WaterPlacementMaterial.Object);
	BoardSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoardSurface"));
	BoardSurface->SetupAttachment(BoardRoot);
	BoardSurface->SetMaterial(0, BoardMaterial.Object);
	BoardSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoardSurface->SetCastShadow(false);
	TerrainBase = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainBase"));
	TerrainBase->SetupAttachment(BoardRoot);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TerrainBaseMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTerrainBase.MI_CityTerrainBase"));
	TerrainBase->SetMaterial(0, TerrainBaseMaterial.Object);
	TerrainBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerrainBase->SetGenerateOverlapEvents(false);
	TerrainBase->SetCanEverAffectNavigation(false);
	TerrainBase->SetCastShadow(false);
	WaterSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterSurface"));
	WaterSurface->SetupAttachment(BoardRoot);
	WaterSurface->SetMaterial(0, WaterMaterial.Object);
	WaterSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WaterSurface->SetCastShadow(false);

	FoundationInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FoundationInstances"));
	FoundationInstances->SetupAttachment(BoardRoot);
	FoundationInstances->SetStaticMesh(CubeMesh.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FoundationConcrete(
	    TEXT("/Game/CitySim/Art/HomeGroundPilot/M_Concrete.M_Concrete"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GroundPilotHome(
	    TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot.SM_CompactHome_GroundPilot"));
	GroundPilotResidentialMesh = GroundPilotHome.Object;
    for (const auto& Entry : CityResidentialEntrances::All())
    {
        const FString GroundObjectPath = CityResidentialEntrances::ObjectPath(Entry);
        ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(*GroundObjectPath);
        if (GroundMesh.Succeeded()) BuildingGroundMeshes.Add(FName(Entry.OriginalMesh), GroundMesh.Object);
    }
    for (const auto& Entry : CityCommercialEntrances::All())
    {
        const FString GroundObjectPath = CityResidentialEntrances::ObjectPath(Entry);
        ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(*GroundObjectPath);
        if (GroundMesh.Succeeded()) BuildingGroundMeshes.Add(FName(Entry.OriginalMesh), GroundMesh.Object);
    }
    for(const auto& Entry:CityIndustrialEntrances::All())
    {
        const FString GroundObjectPath=CityResidentialEntrances::ObjectPath(Entry);
        ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(*GroundObjectPath);
        if(GroundMesh.Succeeded()) BuildingGroundMeshes.Add(FName(Entry.OriginalMesh),GroundMesh.Object);
    }
    for(const auto& Entry:CityCivicEntrances::All())
    {
        const FString GroundObjectPath=CityResidentialEntrances::ObjectPath(Entry);
        ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(*GroundObjectPath);
        if(GroundMesh.Succeeded()) BuildingGroundMeshes.Add(FName(Entry.OriginalMesh),GroundMesh.Object);
    }
    for(const auto& Entry:CityUtilityEntrances::All())
    {
        const FString GroundObjectPath=CityResidentialEntrances::ObjectPath(Entry);
        ConstructorHelpers::FObjectFinder<UStaticMesh> GroundMesh(*GroundObjectPath);
        if(GroundMesh.Succeeded()) BuildingGroundMeshes.Add(FName(Entry.OriginalMesh),GroundMesh.Object);
    }
	FoundationInstances->SetMaterial(0, FoundationConcrete.Object);
	FoundationInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResidentialFoundationInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ResidentialFoundationInstances"));
	ResidentialFoundationInstances->SetupAttachment(BoardRoot);
	ResidentialFoundationInstances->SetStaticMesh(CubeMesh.Object);
	ResidentialFoundationInstances->SetMaterial(0, FoundationConcrete.Object);
	ResidentialFoundationInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResidentialFoundationInstances->SetCanEverAffectNavigation(false);
	for(const auto& Group:CityUtilityFoundations::All())
	{
		const FString Package=Group.Mesh;
		const FString Name=FPaths::GetBaseFilename(Package);
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*(Package+TEXT(".")+Name));
		auto* Pool=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*Name);
		Pool->SetupAttachment(BoardRoot);
		Pool->SetStaticMesh(Mesh.Object);
		Pool->SetMaterial(0,FoundationConcrete.Object);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCanEverAffectNavigation(false);
		UtilityFoundationInstances.Add(FName(Group.Mesh),Pool);
	}
	BuildingEntranceProfiles = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingEntranceProfiles"));
	BuildingEntranceProfiles->SetupAttachment(BoardRoot);
	BuildingEntranceProfiles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BuildingEntranceProfiles->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FireGlow(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarIndustrial.MI_CityUiBarIndustrial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DamageGlow(
	    TEXT("/Game/CitySim/UI/MI_CityUiBarCommercial.MI_CityUiBarCommercial"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> FireParticles(
	    TEXT("/Game/CitySim/Art/Effects/PS_CityFire.PS_CityFire"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> WaterParticles(
	    TEXT("/Game/CitySim/Art/Effects/PS_CityWaterSpray.PS_CityWaterSpray"));
	FireParticleTemplate = FireParticles.Object;
	WaterParticleTemplate = WaterParticles.Object;
	static ConstructorHelpers::FObjectFinder<UParticleSystem> QuakeTemplate(
	    TEXT("/Game/CitySim/Art/Effects/PS_CityEarthquakeDust.PS_CityEarthquakeDust"));
	static ConstructorHelpers::FObjectFinder<UParticleSystem> TornadoTemplate(
	    TEXT("/Game/CitySim/Art/Effects/PS_CityTornado.PS_CityTornado"));
	DisasterAlarmAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DisasterAlarmAudio"));
	Soundscape = CreateDefaultSubobject<UCitySoundscapeComponent>(TEXT("CitySoundscape"));
	Music = CreateDefaultSubobject<UCityMusicComponent>(TEXT("CityMusic"));
	DisasterAlarmAudio->SetupAttachment(BoardRoot);
	DisasterAlarmAudio->bAutoActivate = false;
	DisasterAlarmAudio->bAutoDestroy = false;
	DisasterAlarmAudio->bAllowSpatialization = false;
	DisasterAlarmAudio->bIsUISound = true;
	static ConstructorHelpers::FObjectFinder<USoundBase> Alarm(
	    TEXT("/Game/CitySim/Audio/MS_CityDisasterAlarm.MS_CityDisasterAlarm"));
	if (Alarm.Succeeded())
		DisasterAlarmAudio->SetSound(Alarm.Object);
	EarthquakeDust = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("EarthquakeDust"));
	EarthquakeDust->SetupAttachment(BoardRoot);
	EarthquakeDust->SetTemplate(QuakeTemplate.Object);
	EarthquakeDust->bAutoActivate = false;
	TornadoParticles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TornadoParticles"));
	TornadoParticles->SetupAttachment(BoardRoot);
	TornadoParticles->SetTemplate(TornadoTemplate.Object);
	TornadoParticles->bAutoActivate = false;
	for (auto* P : {EarthquakeDust.Get(), TornadoParticles.Get()})
	{
		P->SetCastShadow(false);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	ParticleFireIds.Init(0, 3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto* FirePool =
		    CreateDefaultSubobject<UParticleSystemComponent>(*FString::Printf(TEXT("FireParticles%d"), Index));
		FirePool->SetupAttachment(BoardRoot);
		FirePool->SetTemplate(FireParticleTemplate);
		FirePool->bAutoActivate = false;
		FirePool->SetCastShadow(false);
		FirePool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FireParticlePools.Add(FirePool);
		auto* WaterPool =
		    CreateDefaultSubobject<UParticleSystemComponent>(*FString::Printf(TEXT("WaterParticles%d"), Index));
		WaterPool->SetupAttachment(BoardRoot);
		WaterPool->SetTemplate(WaterParticleTemplate);
		WaterPool->bAutoActivate = false;
		WaterPool->SetCastShadow(false);
		WaterPool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaterParticlePools.Add(WaterPool);
	}
	FireDamageMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FireDamageMarkers"));
	FireDamageMarkers->SetupAttachment(BoardRoot);
	FireDamageMarkers->SetStaticMesh(CubeMesh.Object);
	FireDamageMarkers->SetMaterial(0, DamageGlow.Object);
	FireDamageMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FireDamageMarkers->SetCastShadow(false);

	HoverHighlight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HoverHighlight"));
	HoverHighlight->SetupAttachment(BoardRoot);
	HoverHighlight->SetStaticMesh(PlaneMesh.Object);
	HoverHighlight->SetRelativeScale3D(FVector(CellMeshScale, CellMeshScale, 1.0));
	HoverHighlight->SetRelativeLocation(FVector(0.0, 0.0, 40.0));
	HoverHighlight->SetMaterial(0, HoverMaterial.Object);
	HoverHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HoverHighlight->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SelectionMaterial(
	    TEXT("/Game/CitySim/UI/M_CityUiBar.M_CityUiBar"));
	SelectionOutlineInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SelectionOutlineInstances"));
	SelectionRetainedInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SelectionRetainedInstances"));
	for (auto* Layer : {SelectionOutlineInstances.Get(), SelectionRetainedInstances.Get()})
	{
		Layer->SetupAttachment(BoardRoot);
		Layer->SetStaticMesh(CubeMesh.Object);
		Layer->SetMaterial(0, SelectionMaterial.Object);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
	}

	EntranceMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EntranceMarker"));
	EntranceMarker->SetupAttachment(BoardRoot);
	EntranceMarker->SetStaticMesh(CubeMesh.Object);
	EntranceMarker->SetRelativeLocation(CityBoardPicking::GetCellCenterLocal(FCityPlacementModel::EntranceCell) +
	                                    FVector(0.0, 0.0, 15.0));
	EntranceMarker->SetRelativeScale3D(FVector(CellMeshScale * 0.85, CellMeshScale * 0.85, 0.3));
	EntranceMarker->SetMaterial(0, HoverMaterial.Object);
	EntranceMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A floating, center-map compass deliberately has no pole or terrain contact.
	const FVector WindVaneLocation = CityBoardPicking::GetCellCenterLocal(FCityCellCoord(
	                                     GetDimensions().GetWidth() / 2, GetDimensions().GetHeight() / 2)) +
	                                 FVector(0.0, 0.0, 11000.0);
	WindVanePost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalNorth"));
	WindVanePost->SetupAttachment(BoardRoot);
	WindVanePost->SetStaticMesh(CubeMesh.Object);
	WindVanePost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WindVanePost->SetCastShadow(false);
	WindVanePost->SetRelativeLocation(WindVaneLocation + FVector(0.0, 950.0, 0.0));
	WindVanePost->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	WindVanePost->SetRelativeScale3D(FVector(19.0, 1.8, 0.8));
	WindCardinalEast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalEast"));
	WindCardinalSouth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalSouth"));
	WindCardinalWest = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalWest"));
	for (UStaticMeshComponent* Cardinal : {WindCardinalEast.Get(), WindCardinalSouth.Get(), WindCardinalWest.Get()})
	{
		Cardinal->SetupAttachment(BoardRoot);
		Cardinal->SetStaticMesh(CubeMesh.Object);
		Cardinal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cardinal->SetCastShadow(false);
		Cardinal->SetRelativeScale3D(FVector(19.0, 1.8, 0.8));
	}
	WindCardinalEast->SetRelativeLocation(WindVaneLocation + FVector(950.0, 0.0, 0.0));
	WindCardinalEast->SetRelativeRotation(FRotator::ZeroRotator);
	WindCardinalSouth->SetRelativeLocation(WindVaneLocation + FVector(0.0, -950.0, 0.0));
	WindCardinalSouth->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	WindCardinalWest->SetRelativeLocation(WindVaneLocation + FVector(-950.0, 0.0, 0.0));
	WindCardinalWest->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	WindVaneArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindVaneArrow"));
	WindVaneArrow->SetupAttachment(BoardRoot);
	WindVaneArrow->SetStaticMesh(CubeMesh.Object);
	WindVaneArrow->SetMaterial(0, HoverMaterial.Object);
	WindVaneArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WindVaneArrow->SetCastShadow(false);
	WindVaneArrow->SetRelativeLocation(WindVaneLocation + FVector(0.0, 0.0, 1800.0));
	WindVaneArrow->SetRelativeScale3D(FVector(24.0, 2.4, 1.1));
	WindVaneHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindVaneHead"));
	WindCardinalNorthHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalNorthHead"));
	WindCardinalEastHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalEastHead"));
	WindCardinalSouthHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalSouthHead"));
	WindCardinalWestHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindCardinalWestHead"));
	for (UStaticMeshComponent* Head : {WindVaneHead.Get(), WindCardinalNorthHead.Get(), WindCardinalEastHead.Get(),
	                                   WindCardinalSouthHead.Get(), WindCardinalWestHead.Get()})
	{
		Head->SetupAttachment(BoardRoot);
		Head->SetStaticMesh(ConeMesh.Object);
		Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Head->SetCastShadow(false);
		Head->SetRelativeScale3D(FVector(3.2, 3.2, 10.0));
	}
	WindCardinalNorthHead->SetRelativeLocation(WindVaneLocation + FVector(0.0, 2150.0, 0.0));
	WindCardinalNorthHead->SetRelativeRotation(FRotator(-90.0, 90.0, 0.0));
	WindCardinalEastHead->SetRelativeLocation(WindVaneLocation + FVector(2150.0, 0.0, 0.0));
	WindCardinalEastHead->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	WindCardinalSouthHead->SetRelativeLocation(WindVaneLocation + FVector(0.0, -2150.0, 0.0));
	WindCardinalSouthHead->SetRelativeRotation(FRotator(-90.0, -90.0, 0.0));
	WindCardinalWestHead->SetRelativeLocation(WindVaneLocation + FVector(-2150.0, 0.0, 0.0));
	WindCardinalWestHead->SetRelativeRotation(FRotator(-90.0, 180.0, 0.0));
	// Use two uninterrupted beams for the compass body. The remaining two arm
	// components are retained only to avoid recreating runtime components, and
	// their endpoint cones carry the individual cardinal direction meaning.
	WindVanePost->SetRelativeLocation(WindVaneLocation);
	WindVanePost->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
	WindVanePost->SetRelativeScale3D(FVector(42.0, 1.8, 0.8));
	WindCardinalEast->SetRelativeLocation(WindVaneLocation);
	WindCardinalEast->SetRelativeRotation(FRotator::ZeroRotator);
	WindCardinalEast->SetRelativeScale3D(FVector(42.0, 1.8, 0.8));
	WindCardinalSouth->SetVisibility(false);
	WindCardinalWest->SetVisibility(false);
	WindCardinalNorthHead->SetRelativeLocation(WindVaneLocation + FVector(0.0, 2450.0, 0.0));
	WindCardinalEastHead->SetRelativeLocation(WindVaneLocation + FVector(2450.0, 0.0, 0.0));
	WindCardinalSouthHead->SetRelativeLocation(WindVaneLocation + FVector(0.0, -2450.0, 0.0));
	WindCardinalWestHead->SetRelativeLocation(WindVaneLocation + FVector(-2450.0, 0.0, 0.0));
	WindOverlayInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WindOverlayInstances"));
	WindOverlayInstances->SetupAttachment(BoardRoot);
	WindOverlayInstances->SetStaticMesh(CubeMesh.Object);
	WindOverlayInstances->SetMaterial(0, HoverMaterial.Object);
	WindOverlayInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WindOverlayInstances->SetCastShadow(false);

	PlacementGhost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlacementGhost"));
	PlacementGhost->SetupAttachment(BoardRoot);
	PlacementGhost->SetStaticMesh(PlaceholderBuildingMesh);
	PlacementGhost->SetMaterial(0, ValidPreviewMaterial);
	PlacementGhost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementGhost->SetVisibility(false);

	ProgressionConstructionSite = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionConstructionSite"));
	ProgressionCraneJib = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionCraneJib"));
	ProgressionCraneHook = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionCraneHook"));
	ProgressionResidentialLow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionResidentialLow"));
	ProgressionResidentialMedium = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionResidentialMedium"));
	ProgressionResidentialHigh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressionResidentialHigh"));
	for (UStaticMeshComponent* ShowcaseComponent :
	     {ProgressionConstructionSite.Get(), ProgressionCraneJib.Get(), ProgressionCraneHook.Get(),
	      ProgressionResidentialLow.Get(), ProgressionResidentialMedium.Get(), ProgressionResidentialHigh.Get()})
	{
		ShowcaseComponent->SetupAttachment(BoardRoot);
		ShowcaseComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ShowcaseComponent->SetCastShadow(false);
	}
	ProgressionConstructionSite->SetStaticMesh(ResidentialConstructionMesh.Object);
	ProgressionCraneJib->SetStaticMesh(ConstructionJibMesh.Object);
	ProgressionCraneHook->SetStaticMesh(ConstructionHookMesh.Object);
	ProgressionResidentialLow->SetStaticMesh(ResidentialLowMesh.Object);
	ProgressionResidentialMedium->SetStaticMesh(ResidentialMediumMesh.Object);
	ProgressionResidentialHigh->SetStaticMesh(ResidentialHighMesh.Object);
	ProgressionConstructionSite->SetRelativeLocation(FVector(7000.0, -6000.0, 40.0));
	ProgressionConstructionSite->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ProgressionResidentialLow->SetRelativeLocation(FVector(19000.0, -6000.0, 40.0));
	ProgressionResidentialLow->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ProgressionResidentialMedium->SetRelativeLocation(FVector(31000.0, -6000.0, 40.0));
	ProgressionResidentialMedium->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ProgressionResidentialHigh->SetRelativeLocation(FVector(43000.0, -6000.0, 40.0));
	ProgressionResidentialHigh->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	ProgressionCraneJib->SetMobility(EComponentMobility::Movable);
	ProgressionCraneHook->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WindBody(
	    TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/Wind/SM_Wind_Stationary_v1.SM_Wind_Stationary_v1"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WindRotor(
	    TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/Wind/SM_Wind_SpinRotor_v1.SM_Wind_SpinRotor_v1"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WindNacelle(
	    TEXT("/Game/CitySim/Art/Utilities/ApprovedBatch/Wind/SM_Wind_YawNacelle_v1.SM_Wind_YawNacelle_v1"));
	WindBodyMesh = WindBody.Object;
	WindNacelleInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WindNacelles"));
	WindNacelleInstances->SetupAttachment(BoardRoot);
	WindNacelleInstances->SetStaticMesh(WindNacelle.Object);
	WindNacelleInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WindNacelleInstances->SetMobility(EComponentMobility::Movable);
	WindRotorInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WindRotors"));
	WindRotorInstances->SetupAttachment(BoardRoot);
	WindRotorInstances->SetStaticMesh(WindRotor.Object);
	WindRotorInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WindRotorInstances->SetCastShadow(false);
	WindRotorInstances->SetMobility(EComponentMobility::Movable);
	LiveConstructionCraneJibs =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LiveConstructionCraneJibs"));
	LiveConstructionCraneJibs->SetupAttachment(BoardRoot);
	LiveConstructionCraneJibs->SetStaticMesh(ConstructionJibMesh.Object);
	LiveConstructionCraneJibs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiveConstructionCraneJibs->SetCastShadow(false);
	LiveConstructionCraneJibs->SetMobility(EComponentMobility::Movable);
	LiveConstructionCraneHooks =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LiveConstructionCraneHooks"));
	LiveConstructionCraneHooks->SetupAttachment(BoardRoot);
	LiveConstructionCraneHooks->SetStaticMesh(ConstructionHookMesh.Object);
	LiveConstructionCraneHooks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiveConstructionCraneHooks->SetCastShadow(false);
	LiveConstructionCraneHooks->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CableMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityRoof.MI_CityRoof"));
	LiveConstructionCraneCables =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LiveConstructionCraneCables"));
	LiveConstructionCraneCables->SetupAttachment(BoardRoot);
	LiveConstructionCraneCables->SetStaticMesh(CubeMesh.Object);
	LiveConstructionCraneCables->SetMaterial(0, CableMaterial.Object);
	LiveConstructionCraneCables->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LiveConstructionCraneCables->SetCastShadow(false);
	LiveConstructionCraneCables->SetMobility(EComponentMobility::Movable);

	RoadInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadInstances"));
	RoadInstances->SetupAttachment(BoardRoot);
	RoadInstances->SetStaticMesh(RoadStraightMesh.Object);
	RoadInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadSurfaceProfile = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadSurfaceProfile"));
	RoadSurfaceProfile->SetupAttachment(BoardRoot);
	RoadSurfaceProfile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadSurfaceProfile->SetCastShadow(false);
	RoadSupportInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadSupportInstances"));
	RoadSupportInstances->SetupAttachment(BoardRoot);
	RoadSupportInstances->SetStaticMesh(CubeMesh.Object);
	RoadSupportInstances->SetMaterial(0, RoadSupportMaterial.Object);
	RoadSupportInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadCornerInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadCornerInstances"));
	RoadCornerInstances->SetupAttachment(BoardRoot);
	RoadCornerInstances->SetStaticMesh(RoadCornerMesh.Object);
	RoadCornerInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadTInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadTInstances"));
	RoadTInstances->SetupAttachment(BoardRoot);
	RoadTInstances->SetStaticMesh(RoadTMesh.Object);
	RoadTInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadCrossInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadCrossInstances"));
	RoadCrossInstances->SetupAttachment(BoardRoot);
	RoadCrossInstances->SetStaticMesh(RoadCrossMesh.Object);
	RoadCrossInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Topology meshes contain only asphalt; graph-edge instances own lane markings.
	UStaticMesh* RoadSurfaceMesh = CubeMesh.Object;
	auto CreateRoadSurfacePool = [this, RoadSurfaceMesh](const FName Name)
	{
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(BoardRoot);
		Component->SetStaticMesh(RoadSurfaceMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		return Component;
	};
	DirtRoadSurfaceInstances = CreateRoadSurfacePool(TEXT("DirtRoadSurfaceInstances"));
	LocalRoadSurfaceInstances = CreateRoadSurfacePool(TEXT("LocalRoadSurfaceInstances"));
	AvenueRoadSurfaceInstances = CreateRoadSurfacePool(TEXT("AvenueRoadSurfaceInstances"));
	HighwayRoadSurfaceInstances = CreateRoadSurfacePool(TEXT("HighwayRoadSurfaceInstances"));
	RoadMarkingInstances = CreateRoadSurfacePool(TEXT("RoadMarkingInstances"));
	RoadMarkingInstances->SetMaterial(0, RoadMarkingMaterial.Object);
	SidewalkSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SidewalkSurface"));
	SidewalkSurface->SetupAttachment(BoardRoot);
	SidewalkSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SidewalkSurface->SetCastShadow(false);
	SidewalkSurface->SetMaterial(0, RoadMarkingMaterial.Object);
	RoadDetailSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadDetailSurface"));
	RoadDetailSurface->SetupAttachment(BoardRoot);
	RoadDetailSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoadDetailSurface->SetCastShadow(false);
	BenchmarkVehicleInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BenchmarkVehicleInstances"));
	BenchmarkVehicleInstances->SetupAttachment(BoardRoot);
	BenchmarkVehicleInstances->SetStaticMesh(CubeMesh.Object);
	BenchmarkVehicleInstances->SetMaterial(0, HoverMaterial.Object);
	BenchmarkVehicleInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CitizenInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CitizenInstances"));
	CitizenInstances->SetupAttachment(BoardRoot);
	CitizenInstances->SetStaticMesh(CitizenMesh.Object ? CitizenMesh.Object : CubeMesh.Object);
	CitizenInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CitizenInstances->SetCastShadow(false);
	CitizenInstances->SetMobility(EComponentMobility::Movable);
	OfficeCitizenInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OfficeCitizenInstances"));
	WorkerCitizenInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WorkerCitizenInstances"));
	for (auto* Pool : {OfficeCitizenInstances.Get(), WorkerCitizenInstances.Get()})
	{
		Pool->SetupAttachment(BoardRoot);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCastShadow(false);
		Pool->SetMobility(EComponentMobility::Movable);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ParkBallMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Park_Ball_A.SM_Park_Ball_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SkateboardMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Park_Skateboard_A.SM_Park_Skateboard_A"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ParkBallMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTrim.MI_CityTrim"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FieldBallMaterial(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityTraffic.MI_CityTraffic"));
	auto SetupParkPool = [&](UInstancedStaticMeshComponent* Pool, UStaticMesh* Mesh)
	{
		Pool->SetupAttachment(BoardRoot);
		Pool->SetStaticMesh(Mesh);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCastShadow(false);
		Pool->SetMobility(EComponentMobility::Movable);
	};
	ParkVisitorInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkVisitorInstances"));
	SetupParkPool(ParkVisitorInstances, CitizenMesh.Object);
	ParkOfficeVisitorInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkOfficeVisitorInstances"));
	ParkWorkerVisitorInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkWorkerVisitorInstances"));
	SetupParkPool(ParkOfficeVisitorInstances, CitizenMesh.Object);
	SetupParkPool(ParkWorkerVisitorInstances, CitizenMesh.Object);
	const TCHAR* ParkStyles[] = {TEXT("Casual"), TEXT("Office"), TEXT("Worker")};
	for (int32 Family = 0; Family < 2; ++Family)
		for (int32 Style = 0; Style < 3; ++Style)
		{
			const FString Name =
			    FString::Printf(TEXT("Park_%s_%s"), Family ? TEXT("Skate") : TEXT("Field"), ParkStyles[Style]);
			ConstructorHelpers::FObjectFinder<UStaticMesh> Static(
			    *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Meshes/SM_%s.SM_%s"), *Name, *Name));
			ConstructorHelpers::FObjectFinder<USkeletalMesh> Skinned(
			    *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Meshes/SK_%s.SK_%s"), *Name, *Name));
			ParkCitizenMeshes[Family * 3 + Style] = Static.Object;
			ParkCitizenSkeletonMeshes[Family * 3 + Style] = Skinned.Object;
			if (Family)
			{
				auto* Pool = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
				    *FString::Printf(TEXT("ParkSkateVisitor%d"), Style));
				SetupParkPool(Pool, Static.Object);
				ParkSkateVisitorPools.Add(Pool);
			}
		}
	const TCHAR* ParkClips[] = {TEXT("Idle"),
	                            TEXT("Walk"),
	                            TEXT("SkateCarve"),
	                            TEXT("SkatePush"),
	                            TEXT("SkateKickflip"),
	                            TEXT("Skate180"),
	                            TEXT("SkateLipStall"),
	                            TEXT("BaseballPitch"),
	                            TEXT("BaseballSwing"),
	                            TEXT("SoccerKick"),
	                            TEXT("SoccerRun"),
	                            TEXT("GoalkeeperSave"),
	                            TEXT("BasketballDribble"),
	                            TEXT("BasketballShot")};
	for (int32 Index = 0; Index < FCityParkMotion::ClipCount; ++Index)
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> Animation(
		    *FString::Printf(TEXT("/Game/CitySim/Art/Parks/Animations/AS_Park_%s.AS_Park_%s"), ParkClips[Index],
		                     ParkClips[Index]));
		ParkCitizenAnimations[Index] = Animation.Object;
	}
	static ConstructorHelpers::FObjectFinder<UBlendSpace1D> StreetBlend(TEXT("/Game/CitySim/Art/Citizens/BS_Citizen_StreetWalk.BS_Citizen_StreetWalk"));
	StreetWalkBlend=StreetBlend.Object;
	bStreetSkeletalAnimation=!FParse::Param(FCommandLine::Get(),TEXT("CityStaticStreetCitizens"));
	bParkSkeletalAnimation = !FParse::Param(FCommandLine::Get(), TEXT("CityStaticParkCitizens"));
	bParkBenchmarkMotion = FParse::Param(FCommandLine::Get(), TEXT("CityBenchmark")) &&
	                       FParse::Param(FCommandLine::Get(), TEXT("CityBenchmarkParkMotion"));
	ConfigureCitizenPools();
	ParkBasketballInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkBasketballInstances"));
	SetupParkPool(ParkBasketballInstances, ParkBallMesh.Object);
	ParkBasketballInstances->SetMaterial(0, ParkBallMaterial.Object);
	ParkFieldBallInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkFieldBallInstances"));
	SetupParkPool(ParkFieldBallInstances, ParkBallMesh.Object);
	ParkFieldBallInstances->SetMaterial(0, FieldBallMaterial.Object);
	ParkSkateboardInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkSkateboardInstances"));
	SetupParkPool(ParkSkateboardInstances, SkateboardMesh.Object);
	ParkBatInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkBatInstances"));
	SetupParkPool(ParkBatInstances, CubeMesh.Object);
	ParkBatInstances->SetMaterial(0, ParkBallMaterial.Object);
	ParkBaseballInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ParkBaseballInstances"));
	SetupParkPool(ParkBaseballInstances, ParkBallMesh.Object);
	struct FParkPropMesh
	{
		UInstancedStaticMeshComponent* Pool;
		const TCHAR* Name;
	};
	for (const auto& Prop :
	     {FParkPropMesh{ParkBasketballInstances, TEXT("Basketball")},
	      FParkPropMesh{ParkFieldBallInstances, TEXT("Soccer")}, FParkPropMesh{ParkBaseballInstances, TEXT("Baseball")},
	      FParkPropMesh{ParkSkateboardInstances, TEXT("Skateboard")}, FParkPropMesh{ParkBatInstances, TEXT("Bat")}})
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(*FString::Printf(
		    TEXT("/Game/CitySim/Art/Parks/Meshes/SM_ParkNative_%s.SM_ParkNative_%s"), Prop.Name, Prop.Name));
		if (Mesh.Object)
		{
			Prop.Pool->SetStaticMesh(Mesh.Object);
			Prop.Pool->SetMaterial(0, Mesh.Object->GetMaterial(0));
		}
	}
	CreateSeasonalDecorationPools();
	TrafficVehicleInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrafficVehicleInstances"));
	TrafficVehicleInstances->SetupAttachment(BoardRoot);
	TrafficVehicleInstances->SetStaticMesh(TrafficCarMesh.Object ? TrafficCarMesh.Object : CubeMesh.Object);
	TrafficVehicleInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrafficVehicleInstances->SetCastShadow(false);
	TrafficVehicleInstances->SetMobility(EComponentMobility::Movable);
	GarbageTruckInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GarbageTruckInstances"));
	GarbageTruckInstances->SetupAttachment(BoardRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GarbageMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_GarbageTruck_A.SM_Traffic_GarbageTruck_A"));
	GarbageTruckInstances->SetStaticMesh(GarbageMesh.Object);
	GarbageTruckInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GarbageTruckInstances->SetCastShadow(false);
	RecyclingTruckInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RecyclingTruckInstances"));
	RecyclingTruckInstances->SetupAttachment(BoardRoot);
	RecyclingTruckInstances->SetStaticMesh(GarbageMesh.Object);
	RecyclingTruckInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RecyclingTruckInstances->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WasteBody(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityIndustrial.MI_CityIndustrial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RecyclingBody(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityService.MI_CityService"));
	if (GarbageMesh.Object)
	{
		const int32 BodySlot = GarbageMesh.Object->GetMaterialIndex(TEXT("Service"));
		if (BodySlot != INDEX_NONE)
		{
			GarbageTruckInstances->SetMaterial(BodySlot, WasteBody.Object);
			RecyclingTruckInstances->SetMaterial(BodySlot, RecyclingBody.Object);
		}
	}
	GarbageTruckInstances->SetMobility(EComponentMobility::Movable);
	RecyclingTruckInstances->SetMobility(EComponentMobility::Movable);
	TrafficTruckInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrafficTruckInstances"));
	TrafficTruckInstances->SetupAttachment(BoardRoot);
	TrafficTruckInstances->SetStaticMesh(TrafficTruckMesh.Object ? TrafficTruckMesh.Object : CubeMesh.Object);
	TrafficTruckInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrafficTruckInstances->SetCastShadow(false);
	TrafficTruckInstances->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PoliceMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_PoliceCar_A.SM_Traffic_PoliceCar_A"));
	AmbulanceResponseInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("AmbulanceResponseInstances"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> AmbulanceMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_Ambulance_A.SM_Traffic_Ambulance_A"));
	AmbulanceResponseInstances->SetStaticMesh(AmbulanceMesh.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> EngineMesh(
	    TEXT("/Game/CitySim/Art/Meshes/SM_Traffic_FireEngine_A.SM_Traffic_FireEngine_A"));
	PoliceResponseInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PoliceResponseInstances"));
	FireResponseInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FireResponseInstances"));
	PoliceResponseInstances->SetStaticMesh(PoliceMesh.Object);
	FireResponseInstances->SetStaticMesh(EngineMesh.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PoliceLights(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityPoliceResponseDetail.MI_CityPoliceResponseDetail"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FireLights(
	    TEXT("/Game/CitySim/Art/Materials/MI_CityFireResponseDetail.MI_CityFireResponseDetail"));
	auto SetResponseLights = [](UInstancedStaticMeshComponent* Pool, UMaterialInterface* Material)
	{
		if (!Pool->GetStaticMesh() || !Material)
			return;
		const int32 RoofSlot = Pool->GetStaticMesh()->GetMaterialIndex(TEXT("Roof"));
		if (RoofSlot != INDEX_NONE)
			Pool->SetMaterial(RoofSlot, Material);
	};
	SetResponseLights(AmbulanceResponseInstances, PoliceLights.Object);
	SetResponseLights(PoliceResponseInstances, PoliceLights.Object);
	SetResponseLights(FireResponseInstances, FireLights.Object);
	for (auto* Pool : {PoliceResponseInstances.Get(), FireResponseInstances.Get(), AmbulanceResponseInstances.Get()})
	{
		Pool->SetupAttachment(BoardRoot);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCastShadow(false);
		Pool->SetMobility(EComponentMobility::Movable);
	}

	ResidentialZoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ResidentialZoneInstances"));
	ResidentialZoneInstances->SetupAttachment(BoardRoot);
	ResidentialZoneInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ResidentialZoneInstances->SetMaterial(0, ValidPreviewMaterial);
	ResidentialZoneInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResidentialZoneInstances->SetVisibility(false);

	CommercialZoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CommercialZoneInstances"));
	CommercialZoneInstances->SetupAttachment(BoardRoot);
	CommercialZoneInstances->SetStaticMesh(PlaceholderBuildingMesh);
	CommercialZoneInstances->SetMaterial(0, InvalidPreviewMaterial);
	CommercialZoneInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CommercialZoneInstances->SetVisibility(false);

	IndustrialZoneInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("IndustrialZoneInstances"));
	IndustrialZoneInstances->SetupAttachment(BoardRoot);
	IndustrialZoneInstances->SetStaticMesh(PlaceholderBuildingMesh);
	IndustrialZoneInstances->SetMaterial(0, InvalidPreviewMaterial);
	IndustrialZoneInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IndustrialZoneInstances->SetVisibility(false);

	ZonePreviewInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ZonePreviewInstances"));
	ZonePreviewInstances->SetupAttachment(BoardRoot);
	ZonePreviewInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ZonePreviewInstances->SetMaterial(0, ValidPreviewMaterial);
	ZonePreviewInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZoneSkippedPreviewInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ZoneSkippedPreviewInstances"));
	ZoneSkippedPreviewInstances->SetupAttachment(BoardRoot);
	ZoneSkippedPreviewInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ZoneSkippedPreviewInstances->SetMaterial(0, InvalidPreviewMaterial);
	ZoneSkippedPreviewInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RoadPreviewInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RoadPreviewInstances"));
	RoadPreviewInstances->SetupAttachment(BoardRoot);
	RoadPreviewInstances->SetStaticMesh(PlaceholderBuildingMesh);
	RoadPreviewInstances->SetMaterial(0, ValidPreviewMaterial);
	RoadPreviewInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ServiceCoverageInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ServiceCoverageInstances"));
	ServiceCoverageInstances->SetupAttachment(BoardRoot);
	ServiceCoverageInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ServiceCoverageInstances->SetMaterial(0, ValidPreviewMaterial);
	ServiceCoverageInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ServiceServedParcelInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ServiceServedParcelInstances"));
	ServiceServedParcelInstances->SetupAttachment(BoardRoot);
	ServiceServedParcelInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ServiceServedParcelInstances->SetMaterial(0, ValidPreviewMaterial);
	ServiceServedParcelInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ServiceUnservedParcelInstances =
	    CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ServiceUnservedParcelInstances"));
	ServiceUnservedParcelInstances->SetupAttachment(BoardRoot);
	ServiceUnservedParcelInstances->SetStaticMesh(PlaceholderBuildingMesh);
	ServiceUnservedParcelInstances->SetMaterial(0, InvalidPreviewMaterial);
	ServiceUnservedParcelInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const double DisplayScale = DisplayedBoardWidthCm / CityBoardPicking::GetLogicalBoardSize(FCityGridDimensions()).X;
	SetActorScale3D(FVector(DisplayScale));
}

void ACityBoardActor::StopDisasterAlarm()
{
	DisasterAlarmAudio->Stop();
	bAlarmPreview = false;
	AlarmPreviewEndSeconds = 0;
}
void ACityBoardActor::EndPlay(const EEndPlayReason::Type Reason)
{
	Music->Suspend();
	StopDisasterAlarm();
	Soundscape->StopAll();
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(AlarmBackgroundHandle);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(AlarmForegroundHandle);
	Super::EndPlay(Reason);
}
bool ACityBoardActor::PreviewDisasterAlarm()
{
	if (!SimulationSubsystem || SimulationSubsystem->IsEntryMenuSuspended() || bAlarmBackgrounded ||
	    GetDefault<UCityAudioSettings>()->GetAlarmVolume() <= 0 || !DisasterAlarmAudio->Sound)
		return false;
	StopDisasterAlarm();
	bAlarmPreview = true;
	if (auto* Town = Soundscape->GetCueSound(ECityAudioCue::TownLoop))
	{
		DisasterAlarmAudio->SetSound(Town);
		AlarmPreviewEndSeconds = FPlatformTime::Seconds() + 5;
	}
	DisasterAlarmAudio->SetVolumeMultiplier(GetDefault<UCityAudioSettings>()->GetAlarmVolume());
	DisasterAlarmAudio->Play();
	++DisasterAlarmPlayCount;
	return true;
}
void ACityBoardActor::UpdateDisasterAlarm()
{
	if (!SimulationSubsystem)
		return;
	const float Volume = GetDefault<UCityAudioSettings>()->GetAlarmVolume();
	DisasterAlarmAudio->SetVolumeMultiplier(Volume);
	const bool Blocked = bAlarmBackgrounded || SimulationSubsystem->IsEntryMenuSuspended() ||
	                     SimulationSubsystem->IsSimulationPaused() || Volume <= 0;
	if (bAlarmPreview && AlarmPreviewEndSeconds > 0 && FPlatformTime::Seconds() >= AlarmPreviewEndSeconds)
		StopDisasterAlarm();
	if (bAlarmPreview && !DisasterAlarmAudio->IsPlaying())
		bAlarmPreview = false;
	if (bAlarmBackgrounded || SimulationSubsystem->IsEntryMenuSuspended() || Volume <= 0 ||
	    (SimulationSubsystem->IsSimulationPaused() && !bAlarmPreview))
		StopDisasterAlarm();
	const uint64 Session = SimulationSubsystem->GetCitySessionGeneration();
	// A city replacement stops an old cue; the cursor baselines restored active events.
	if (Session != ObservedAlarmSession)
	{
		StopDisasterAlarm();
		ObservedAlarmSession = Session;
	}
	if (DisasterAlarmGate.Observe(SimulationSubsystem->GetDisasterSnapshot(), Session, Blocked) &&
	    !Soundscape->IsReady() && DisasterAlarmAudio->Sound)
	{
		StopDisasterAlarm();
		DisasterAlarmAudio->Play();
		++DisasterAlarmPlayCount;
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_DISASTER_ALARM_ONSET"));
	}
}
void ACityBoardActor::ConfigureCitizenPools()
{
	UInstancedStaticMeshComponent* TrafficPools[] = {CitizenInstances, OfficeCitizenInstances, WorkerCitizenInstances};
	UInstancedStaticMeshComponent* ParkPools[] = {ParkVisitorInstances, ParkOfficeVisitorInstances,
	                                              ParkWorkerVisitorInstances};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		for (auto* Pool : {TrafficPools[Index]})
		{
			Pool->SetStaticMesh(CitizenPresentationMeshes[Index]);
			if (CitizenPresentationMeshes[Index])
				Pool->SetMaterial(0, CitizenPresentationMeshes[Index]->GetMaterial(0));
		}
		ParkPools[Index]->SetStaticMesh(ParkCitizenMeshes[Index] ? ParkCitizenMeshes[Index]
		                                                         : CitizenPresentationMeshes[Index]);
		if (ParkPools[Index]->GetStaticMesh())
			ParkPools[Index]->SetMaterial(0, ParkPools[Index]->GetStaticMesh()->GetMaterial(0));
		ParkPools[Index]->SetForcedLodModel(PLATFORM_ANDROID ? 2 : 0);
	}
	for (int32 Index = 0; Index < ParkSkateVisitorPools.Num(); ++Index)
	{
		auto* Pool = ParkSkateVisitorPools[Index].Get();
		Pool->SetStaticMesh(ParkCitizenMeshes[Index + 3]);
		if (Pool->GetStaticMesh())
			Pool->SetMaterial(0, Pool->GetStaticMesh()->GetMaterial(0));
		Pool->SetForcedLodModel(PLATFORM_ANDROID ? 2 : 0);
	}
}

void ACityBoardActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Existing maps serialize the baseline mesh on these default subobjects.
	// Runtime platform policy owns their presentation without changing the map.
	ConfigureCitizenPools();
}

void ACityBoardActor::BeginPlay()
{
	Super::BeginPlay();
	AlarmBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda(
	    [this]()
	    {
		    bAlarmBackgrounded = true;
		    Music->Suspend();
		    StopDisasterAlarm();
		    Soundscape->StopAll();
	    });
	AlarmForegroundHandle =
	    FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda([this]() { bAlarmBackgrounded = false; });
	for (UStaticMeshComponent* ShowcaseComponent :
	     {ProgressionConstructionSite.Get(), ProgressionCraneJib.Get(), ProgressionCraneHook.Get(),
	      ProgressionResidentialLow.Get(), ProgressionResidentialMedium.Get(), ProgressionResidentialHigh.Get()})
	{
		ShowcaseComponent->SetVisibility(bShowProgressionShowcase, true);
	}
	if (bShowProgressionShowcase)
	{
		UpdateProgressionShowcase(0.0f);
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_PROGRESSION_SHOWCASE Stages=Construction,Low,Medium,High Animation=CraneJibAndHook"));
	}
	if (ZoneBaseMaterial)
	{
		ResidentialZoneMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_ResidentialZone"));
		CommercialZoneMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_CommercialZone"));
		IndustrialZoneMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_IndustrialZone"));
		ServiceCoverageMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_ServiceCoverage"));
		UMaterialInterface* Pavement = PavementBaseMaterial;
		if (!Pavement)
			Pavement = ZoneBaseMaterial;
		DirtRoadMaterial = UMaterialInstanceDynamic::Create(Pavement, this, TEXT("MID_DirtRoad"));
		LocalRoadMaterial = UMaterialInstanceDynamic::Create(Pavement, this, TEXT("MID_LocalRoad"));
		AvenueRoadMaterial = UMaterialInstanceDynamic::Create(Pavement, this, TEXT("MID_AvenueRoad"));
		HighwayRoadMaterial = UMaterialInstanceDynamic::Create(Pavement, this, TEXT("MID_HighwayRoad"));
		SidewalkMaterial = UMaterialInstanceDynamic::Create(Pavement, this, TEXT("MID_Sidewalk"));
		SidewalkMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.48f, 0.47f, 0.42f, 1.0f));
		SidewalkMaterial->SetScalarParameterValue(TEXT("SeamStrength"), 0.7f);
		SidewalkSurface->SetMaterial(0, SidewalkMaterial);
		RoadDetailSurface->SetMaterial(0, SidewalkMaterial);
		if (ResidentialZoneMaterial && CommercialZoneMaterial && IndustrialZoneMaterial && ServiceCoverageMaterial)
		{
			ResidentialZoneMaterial->SetVectorParameterValue(TEXT("CityColor"),
			                                                 FLinearColor(0.12f, 0.72f, 0.18f, 1.0f));
			CommercialZoneMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.08f, 0.32f, 0.90f, 1.0f));
			IndustrialZoneMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.95f, 0.72f, 0.08f, 1.0f));
			ServiceCoverageMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(1.0f, 0.55f, 0.05f, 1.0f));
			ResidentialZoneInstances->SetMaterial(0, ResidentialZoneMaterial);
			CommercialZoneInstances->SetMaterial(0, CommercialZoneMaterial);
			IndustrialZoneInstances->SetMaterial(0, IndustrialZoneMaterial);
		}
		if (DirtRoadMaterial && LocalRoadMaterial && AvenueRoadMaterial && HighwayRoadMaterial)
		{
			DirtRoadMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.32f, 0.17f, 0.07f, 1.0f));
			LocalRoadMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.12f, 0.14f, 0.17f, 1.0f));
			AvenueRoadMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.19f, 0.24f, 0.31f, 1.0f));
			HighwayRoadMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.055f, 0.07f, 0.09f, 1.0f));
			DirtRoadSurfaceInstances->SetMaterial(0, DirtRoadMaterial);
			LocalRoadSurfaceInstances->SetMaterial(0, LocalRoadMaterial);
			AvenueRoadSurfaceInstances->SetMaterial(0, AvenueRoadMaterial);
			HighwayRoadSurfaceInstances->SetMaterial(0, HighwayRoadMaterial);
		}
		WindNorthMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_WindNorth"));
		WindCardinalMaterial = UMaterialInstanceDynamic::Create(ZoneBaseMaterial, this, TEXT("MID_WindCardinal"));
		if (WindNorthMaterial && WindCardinalMaterial)
		{
			WindNorthMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.92f, 0.08f, 0.06f, 1.0f));
			WindCardinalMaterial->SetVectorParameterValue(TEXT("CityColor"), FLinearColor(0.80f, 0.84f, 0.88f, 1.0f));
			WindVanePost->SetMaterial(0, WindCardinalMaterial);
			WindCardinalEast->SetMaterial(0, WindCardinalMaterial);
			WindCardinalSouth->SetMaterial(0, WindCardinalMaterial);
			WindCardinalWest->SetMaterial(0, WindCardinalMaterial);
			WindCardinalNorthHead->SetMaterial(0, WindNorthMaterial);
			WindCardinalEastHead->SetMaterial(0, WindCardinalMaterial);
			WindCardinalSouthHead->SetMaterial(0, WindCardinalMaterial);
			WindCardinalWestHead->SetMaterial(0, WindCardinalMaterial);
			WindVaneHead->SetMaterial(0, ValidPreviewMaterial.Get());
		}
	}
	SimulationSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	UpdateDisasterAlarm();
	PlacementModel = SimulationSubsystem ? &SimulationSubsystem->GetPlacementModel() : nullptr;
	if (!PlacementModel)
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_PLACEMENT_DISABLED Missing city simulation subsystem on %s"), *GetName());
		SetActorTickEnabled(false);
		return;
	}
	SimulationSubsystem->RegisterBuildingCatalog(BuildingCatalog);
	// Benchmark initialization can replace the model while registering the catalog.
	PlacementModel = &SimulationSubsystem->GetPlacementModel();
	RefreshTerrainPresentation();
	EntranceMarker->SetRelativeLocation(GetCellSurfaceCenter(PlacementModel->GetEntranceCell(), 15.0));

	if (!BuildingCatalog || BuildingCatalog->Definitions.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CITYSIM_PLACEMENT_DISABLED Missing building catalog on %s"), *GetName());
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		SetActorTickEnabled(false);
		return;
	}

	const bool bHasActiveHeadTracking =
	    GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowedForWorld(*GetWorld());
	if (!bHasActiveHeadTracking)
	{
		PlayerController->bShowMouseCursor = true;
		PlayerController->SetIgnoreLookInput(true);
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
	}
}

void ACityBoardActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateConcreteMaterial();
	UpdateDisasterAlarm();
	if (SimulationSubsystem)
		Soundscape->Update(*this, *SimulationSubsystem, DeltaSeconds, bAlarmBackgrounded);
	const float MusicDuck = DisasterAlarmAudio->IsPlaying() && GetDefault<UCityAudioSettings>()->GetAlarmVolume() > 0
	                            ? .35f
	                            : Soundscape->GetMusicDuckGain();
	Music->Update(bAlarmBackgrounded, MusicDuck);
	Atmosphere->Update(SimulationSubsystem, DeltaSeconds);
	if (TerrainMaterial)
	{
		TerrainMaterial->SetScalarParameterValue(TEXT("WeatherWet"), Atmosphere->GetMotion().Wetness);
		TerrainMaterial->SetScalarParameterValue(TEXT("WeatherSnow"), Atmosphere->GetMotion().SnowCover);
	}
	if (SimulationSubsystem && SimulationSubsystem->IsEntryMenuSuspended())
	{
		ClearWaterPlacementOverlay();
		ClearSeasonalDecorations();
		return;
	}
	// A load replaces the placement model. Rebind before any animation or
	// presentation reads entities from the previous city's model.
	if (SimulationSubsystem && ObservedPresentationGeneration != SimulationSubsystem->GetPresentationGeneration())
	{
		PrepareCityView();
	}
	UpdateWindPresentation();
	UpdateProgressionShowcase(DeltaSeconds);
	UpdateLiveConstructionAnimations(DeltaSeconds);
	RenderPendingGrowth();
	UpdatePopulationTraffic();
	UpdateParkActivity(DeltaSeconds);
	UpdateSeasonalDecorations();
	UpdateWindRotors(DeltaSeconds);
	UpdateFirePresentation();
	if (bManipulationActive)
	{
		SetHoveredCell(FCityCellCoord(), false);
		ClearServiceCoverage();
		ClearWaterPlacementOverlay();
		PlacementGhost->SetVisibility(false);
		bConfirmRequested = false;
		bCancelRequested = false;
		bRotateRequested = false;
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	FCityCellCoord HoveredCell;
	const bool bHasHoveredCell =
	    bControllerRayActive ? PickCellFromWorldRay(ControllerRayOrigin, ControllerRayDirection, HoveredCell)
	                         : (!bSuppressDesktopRay && PickCellFromPlayerController(PlayerController, HoveredCell));
	SetHoveredCell(HoveredCell, bHasHoveredCell);

	const bool bFallbackInput = !bCommandInputBound && PlayerController;
	const bool bConfirm =
	    bConfirmRequested || (bFallbackInput && PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton));
	const bool bCancel =
	    bCancelRequested || (bFallbackInput && (PlayerController->WasInputKeyJustPressed(EKeys::RightMouseButton) ||
	                                            PlayerController->WasInputKeyJustPressed(EKeys::Escape)));
	const bool bRotate = bRotateRequested || (bFallbackInput && PlayerController->WasInputKeyJustPressed(EKeys::R));
	UpdatePlacementPreview(PlayerController, HoveredCell, bHasHoveredCell, bConfirm, bCancel, bRotate);
	bConfirmRequested = false;
	bCancelRequested = false;
	bRotateRequested = false;
}

void ACityBoardActor::PrepareCityView()
{
	UpdateConcreteMaterial();
	if (!SimulationSubsystem)
		return;
	// A load replaces the model. Rebuild it while hidden before revealing the city.
	PlacementModel = &SimulationSubsystem->GetPlacementModel();
	if (ObservedPresentationGeneration != SimulationSubsystem->GetPresentationGeneration())
	{
		ObservedPresentationGeneration = SimulationSubsystem->GetPresentationGeneration();
		DisplayedDisasterId = -2;
		RebuildAllPresentation();
	}
	bConfirmRequested = bRotateRequested = bCancelRequested = false;
}

bool ACityBoardActor::TryToggleWindOverlay()
{
	if (bManipulationActive || !IsPointingAtWindVane())
		return false;
	bWindOverlayRequested = !bWindOverlayRequested;
	UpdateWindPresentation();
	return true;
}

bool ACityBoardActor::IsPointingAtWindVane() const
{
	if (!bControllerRayActive)
		return false;
	const FVector End = ControllerRayOrigin + ControllerRayDirection.GetSafeNormal() * 1000.0;
	// Test each visible piece in its own mesh space, rather than a 42 cm sphere
	// which swallowed rays aimed at the far side of the tabletop.
	for (const UStaticMeshComponent* Piece :
	     {WindVanePost.Get(), WindCardinalEast.Get(), WindVaneArrow.Get(), WindVaneHead.Get(),
	      WindCardinalNorthHead.Get(), WindCardinalEastHead.Get(), WindCardinalSouthHead.Get(),
	      WindCardinalWestHead.Get()})
	{
		if (!Piece || !Piece->IsVisible() || !Piece->GetStaticMesh())
			continue;
		const FTransform Transform = Piece->GetComponentTransform();
		const FBox Bounds = Piece->GetStaticMesh()->GetBoundingBox().ExpandBy(3.0);
		FVector Hit, Normal;
		float Time = 0.0f;
		if (FMath::LineExtentBoxIntersection(Bounds, Transform.InverseTransformPosition(ControllerRayOrigin),
		                                     Transform.InverseTransformPosition(End), FVector::ZeroVector, Hit, Normal,
		                                     Time))
			return true;
	}
	return false;
}

void ACityBoardActor::UpdateWindPresentation()
{
	if (!SimulationSubsystem || !WindVaneArrow || !WindOverlayInstances)
	{
		return;
	}
	const FCityEnvironmentSnapshot& Weather = SimulationSubsystem->GetEnvironmentSnapshot();
	if (WindVaneDimensions != GetDimensions())
	{
		const auto PreviousCenter = CityBoardPicking::GetCellCenterLocal(
		    {WindVaneDimensions.GetWidth() / 2, WindVaneDimensions.GetHeight() / 2});
		const auto Center =
		    CityBoardPicking::GetCellCenterLocal({GetDimensions().GetWidth() / 2, GetDimensions().GetHeight() / 2});
		for (UStaticMeshComponent* Piece :
		     {WindVanePost.Get(), WindCardinalEast.Get(), WindCardinalSouth.Get(), WindCardinalWest.Get(),
		      WindVaneArrow.Get(), WindVaneHead.Get(), WindCardinalNorthHead.Get(), WindCardinalEastHead.Get(),
		      WindCardinalSouthHead.Get(), WindCardinalWestHead.Get()})
			if (Piece)
				Piece->SetRelativeLocation(Piece->GetRelativeLocation() + Center - PreviousCenter);
		WindVaneDimensions = GetDimensions();
	}
	const float Yaw = static_cast<float>(Atmosphere->GetMotion().WindYaw);
	// The authored cone primitive faces opposite its mesh-forward axis after the
	// horizontal conversion. Reverse the vane marker so its visible head states
	// the downwind travel direction used by pollution transport.
	const float TravelYaw = Yaw + 180.0f;
	WindVaneArrow->SetRelativeRotation(FRotator(0.0f, TravelYaw, 0.0f));
	if (WindVaneHead)
	{
		WindVaneHead->SetRelativeLocation(WindVaneArrow->GetRelativeLocation() +
		                                  FRotator(0.0f, TravelYaw, 0.0f).Vector() * 1450.0f);
		WindVaneHead->SetRelativeRotation(FRotator(-90.0f, TravelYaw, 0.0f));
	}
	const bool bShowOverlay = bWindOverlayRequested && DiagnosticOverlay == ECityDiagnosticOverlay::None;
	const uint32 WeatherSignature =
	    HashCombine(HashCombine(GetTypeHash(FMath::RoundToInt(Yaw)), GetTypeHash(GetDimensions().GetWidth())),
	                GetTypeHash(FMath::RoundToInt(Atmosphere->GetMotion().WindStrength * 10)));
	if (bShowOverlay == bWindOverlayVisible && (!bShowOverlay || WeatherSignature == DisplayedWindSignature))
	{
		return;
	}
	bWindOverlayVisible = bShowOverlay;
	DisplayedWindSignature = WeatherSignature;
	WindOverlayInstances->ClearInstances();
	if (!bShowOverlay)
	{
		return;
	}
	const FVector ArrowScale(16.0 + Atmosphere->GetMotion().WindStrength * 5.0, 1.5, 0.35);
	for (int32 Y = 6; Y < GetDimensions().GetHeight(); Y += 10)
	{
		for (int32 X = 6; X < GetDimensions().GetWidth(); X += 10)
		{
			WindOverlayInstances->AddInstance(
			    FTransform(FRotator(0.0f, Yaw, 0.0f), GetCellSurfaceCenter(FCityCellCoord(X, Y), 28.0), ArrowScale));
		}
	}
}

void ACityBoardActor::SetLightingPreset(const ECityLightingPreset Preset)
{
	for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
		It->SetLightingPreset(Preset);
}

ECityLightingPreset ACityBoardActor::GetLightingPreset() const
{
	for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
		return It->GetLightingPreset();
	return ECityLightingPreset::Day;
}

void ACityBoardActor::UpdateProgressionShowcase(const float DeltaSeconds)
{
	if (!bShowProgressionShowcase || !ProgressionCraneJib || !ProgressionCraneHook)
	{
		return;
	}
	ProgressionAnimationSeconds = FMath::Fmod(ProgressionAnimationSeconds + DeltaSeconds, 120.0f);
	const float CraneYaw = 180.0f + FMath::Sin(ProgressionAnimationSeconds * 0.55f) * 55.0f;
	const FRotator CraneRotation(0.0, CraneYaw, 0.0);
	const FVector MastTop(7000.0, -6000.0, 2040.0);
	ProgressionCraneJib->SetRelativeLocationAndRotation(MastTop, CraneRotation);
	const float CableDrop = 650.0f + FMath::Sin(ProgressionAnimationSeconds * 1.35f) * 180.0f;
	const FVector HookLocation = MastTop + CraneRotation.RotateVector(FVector(650.0, 0.0, -CableDrop));
	ProgressionCraneHook->SetRelativeLocationAndRotation(HookLocation, CraneRotation);
}

bool ACityBoardActor::IsLiveConstructionAnimated(const uint64 EntityId) const
{
	const FCityDevelopmentState* State = SimulationSubsystem->GetDevelopmentSnapshot().Find(EntityId);
	const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(EntityId);
	const FCityBuildingDefinition* Definition =
	    Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
	return State && (State->Stage == ECityDevelopmentStage::Construction || State->HasCivicUpgrade()) && Definition &&
	       !Definition->ConstructionMesh.IsNull() &&
	       !SimulationSubsystem->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
	           [EntityId](const FCityTransitTripRecord& Trip)
	           {
		           return !Trip.bCompleted && Trip.Purpose == ECityTripPurpose::MoveInArrival &&
		                  Trip.DestinationEntityId == EntityId;
	           });
}

void ACityBoardActor::RebuildLiveConstructionAnimations()
{
	AnimatedConstructionEntityIds.Reset();
	LiveConstructionCraneJibs->ClearInstances();
	LiveConstructionCraneHooks->ClearInstances();
	LiveConstructionCraneCables->ClearInstances();
	if (!SimulationSubsystem || !PlacementModel || !BuildingCatalog)
	{
		return;
	}
	TArray<uint64> EntityIds;
	SimulationSubsystem->GetDevelopmentSnapshot().States.GetKeys(EntityIds);
	EntityIds.Sort();
	for (const uint64 EntityId : EntityIds)
	{
		if (!IsLiveConstructionAnimated(EntityId))
		{
			continue;
		}
		AnimatedConstructionEntityIds.Add(EntityId);
		LiveConstructionCraneJibs->AddInstance(FTransform::Identity);
		LiveConstructionCraneHooks->AddInstance(FTransform::Identity);
		LiveConstructionCraneCables->AddInstance(FTransform::Identity);
	}
	UpdateLiveConstructionTransforms(0.0f);
}

void ACityBoardActor::UpdateLiveConstructionAnimations(const float DeltaSeconds)
{
	if (!SimulationSubsystem || !PlacementModel || !BuildingCatalog)
	{
		return;
	}
	int32 AuthoritativeConstructionCount = 0;
	for (const TPair<uint64, FCityDevelopmentState>& Pair : SimulationSubsystem->GetDevelopmentSnapshot().States)
	{
		AuthoritativeConstructionCount += IsLiveConstructionAnimated(Pair.Key) ? 1 : 0;
	}
	if (AuthoritativeConstructionCount != AnimatedConstructionEntityIds.Num())
	{
		RebuildLiveConstructionAnimations();
		return;
	}
	UpdateLiveConstructionTransforms(DeltaSeconds);
}

void ACityBoardActor::UpdateLiveConstructionTransforms(const float DeltaSeconds)
{
	if (AnimatedConstructionEntityIds.IsEmpty())
	{
		return;
	}
	ProgressionAnimationSeconds = FMath::Fmod(ProgressionAnimationSeconds + DeltaSeconds, 120.0f);
	for (int32 Index = 0; Index < AnimatedConstructionEntityIds.Num(); ++Index)
	{
		const uint64 EntityId = AnimatedConstructionEntityIds[Index];
		const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(EntityId);
		const FCityBuildingDefinition* Definition =
		    Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
		if (!Entity || !Definition)
		{
			continue;
		}
		const FTransform SiteTransform =
		    MakeBuildingTransform(Entity->Anchor, *Definition, Entity->QuarterTurnsClockwise);
		const float Phase = static_cast<float>((EntityId * 37u) % 100u) * 0.031f;
		const float BaseYaw = SiteTransform.Rotator().Yaw;
		const float CraneYaw = BaseYaw + FMath::Sin(ProgressionAnimationSeconds * 0.55f + Phase) * 55.0f;
		const FRotator CraneRotation(0.0f, CraneYaw, 0.0f);
		const FVector MastTop = SiteTransform.GetLocation() + FVector(0.0, 0.0, 2000.0);
		const float CableDrop = 650.0f + FMath::Sin(ProgressionAnimationSeconds * 1.35f + Phase) * 180.0f;
		const FVector HookLocation = MastTop + CraneRotation.RotateVector(FVector(650.0, 0.0, -CableDrop));
		LiveConstructionCraneJibs->UpdateInstanceTransform(Index, FTransform(CraneRotation, MastTop), false, false,
		                                                   true);
		LiveConstructionCraneHooks->UpdateInstanceTransform(Index, FTransform(CraneRotation, HookLocation), false,
		                                                    Index == AnimatedConstructionEntityIds.Num() - 1, true);
		LiveConstructionCraneCables->UpdateInstanceTransform(
		    Index,
		    FTransform(FRotator::ZeroRotator, HookLocation + FVector(0, 0, CableDrop * .5),
		               FVector(.12, .12, CableDrop / 100.0)),
		    false, Index == AnimatedConstructionEntityIds.Num() - 1, true);
	}
}

void ACityBoardActor::SetManipulationActive(const bool bActive)
{
	if (bManipulationActive == bActive)
	{
		return;
	}
	bManipulationActive = bActive;
	if (bActive && PlacementModel)
	{
		PlacementModel->CancelPreview(PlacementPreview);
		PlacementModel->CancelRoadPreview(RoadPreview);
		ZoneStartCell.Reset();
		ClearGesturePreviews();
		PlacementGhost->SetVisibility(false);
		SetHoveredCell(FCityCellCoord(), false);
		CanceledAtCell.Reset();
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_BOARD_MANIPULATION=%d"), bManipulationActive);
}

void ACityBoardActor::SetControllerRay(const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
                                       const bool bActive, const bool bSuppressMouseFallback)
{
	bSuppressDesktopRay = bSuppressMouseFallback;
	ControllerRayOrigin = WorldRayOrigin;
	ControllerRayDirection = WorldRayDirection.GetSafeNormal();
	bControllerRayActive = bActive && !ControllerRayDirection.IsNearlyZero();
}

void ACityBoardActor::SetToolMode(const ECityToolMode NewToolMode)
{
	if (!PlacementModel || bManipulationActive || ToolMode == NewToolMode)
	{
		return;
	}

	PlacementModel->CancelPreview(PlacementPreview);
	PlacementModel->CancelRoadPreview(RoadPreview);
	ZoneStartCell.Reset();
	ClearGesturePreviews();
	ClearServiceCoverage();
	PlacementGhost->SetVisibility(false);
	CanceledAtCell.Reset();
	ToolMode = NewToolMode;
	UpdateZoneVisibility();
	bConstructionPriceBlocked = false;
	bRoadToolSelected = NewToolMode == ECityToolMode::Road;

	switch (NewToolMode)
	{
	case ECityToolMode::Home:
		SelectDefinition(0);
		break;
	case ECityToolMode::Shop:
		SelectDefinition(1);
		break;
	case ECityToolMode::Park:
		SelectDefinition(2);
		break;
	case ECityToolMode::Power:
		SelectDefinition(3);
		break;
	case ECityToolMode::Service:
		SelectDefinition(4);
		break;
	case ECityToolMode::NeighborhoodPark:
	case ECityToolMode::CityPark:
	case ECityToolMode::BasketballPark:
	case ECityToolMode::SkatePark:
	case ECityToolMode::BaseballPark:
	case ECityToolMode::SoccerPark:
	{
		const FName Id = NewToolMode == ECityToolMode::NeighborhoodPark ? TEXT("NeighborhoodPark")
		                 : NewToolMode == ECityToolMode::CityPark       ? TEXT("CityPark")
		                 : NewToolMode == ECityToolMode::BasketballPark ? TEXT("BasketballPark")
		                 : NewToolMode == ECityToolMode::SkatePark      ? TEXT("SkatePark")
		                 : NewToolMode == ECityToolMode::BaseballPark   ? TEXT("BaseballPark")
		                                                                : TEXT("SoccerPark");
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId == Id)
				{
					SelectDefinition(Index);
					break;
				}
		break;
	}
	case ECityToolMode::School:
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId == TEXT("School"))
				{
					SelectDefinition(Index);
					break;
				}
		break;
	case ECityToolMode::WasteDepot:
	case ECityToolMode::Recycling:
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId ==
				    (NewToolMode == ECityToolMode::WasteDepot ? TEXT("WasteDepot") : TEXT("Recycling")))
				{
					SelectDefinition(Index);
					break;
				}
		break;
	case ECityToolMode::Hospital:
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId == TEXT("Hospital"))
				{
					SelectDefinition(Index);
					break;
				}
		break;
	case ECityToolMode::Fire:
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId == TEXT("Fire"))
				{
					SelectDefinition(Index);
					break;
				}
		break;
	case ECityToolMode::Police:
		if (BuildingCatalog)
			for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
				if (BuildingCatalog->Definitions[Index].DefinitionId == TEXT("Police"))
				{
					SelectDefinition(Index);
					break;
				}
		break;
	default:
		break;
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_TOOL_SELECTED %s"), *GetToolDisplayName());
}

void ACityBoardActor::SetDiagnosticOverlay(const ECityDiagnosticOverlay NewOverlay)
{
	DiagnosticOverlay = DiagnosticOverlay == NewOverlay ? ECityDiagnosticOverlay::None : NewOverlay;
	bSuppressLinkedOverlay = DiagnosticOverlay == ECityDiagnosticOverlay::None;
	ClearServiceCoverage();
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_DIAGNOSTIC_OVERLAY %s"),
	       *FCityDiagnosticOverlayEvaluator::DisplayName(DiagnosticOverlay));
}

FString ACityBoardActor::GetToolDisplayName() const
{
	switch (ToolMode)
	{
	case ECityToolMode::Inspect:
		return TEXT("Inspect");
	case ECityToolMode::Road:
		return FString::Printf(TEXT("%s / %s"), RoadDisplayName(SelectedRoadDefinitionId),
		                       SelectedRoadProfile == ECityRoadProfile::Bridge     ? TEXT("Bridge")
		                       : SelectedRoadProfile == ECityRoadProfile::Elevated ? TEXT("Elevated")
		                                                                           : TEXT("Ground"));
	case ECityToolMode::Home:
		return TEXT("Home");
	case ECityToolMode::Shop:
		return TEXT("Shop");
	case ECityToolMode::Park:
		return TEXT("Pocket Park");
	case ECityToolMode::BasketballPark:
		return TEXT("Basketball Park");
	case ECityToolMode::SkatePark:
		return TEXT("Skate Park");
	case ECityToolMode::BaseballPark:
		return TEXT("Baseball Park");
	case ECityToolMode::SoccerPark:
		return TEXT("Soccer Park");
	case ECityToolMode::NeighborhoodPark:
		return TEXT("Neighborhood Park");
	case ECityToolMode::CityPark:
		return TEXT("City Park");
	case ECityToolMode::Power:
		return GetSelectedDefinition() ? GetSelectedDefinition()->DisplayName : TEXT("Power");
	case ECityToolMode::Service:
		return GetSelectedDefinition() ? GetSelectedDefinition()->DisplayName : TEXT("Water");
	case ECityToolMode::Police:
		return TEXT("Police Station");
	case ECityToolMode::Fire:
		return TEXT("Fire Station");
	case ECityToolMode::WasteDepot:
		return TEXT("Waste Depot");
	case ECityToolMode::Recycling:
		return TEXT("Recycling Center");
	case ECityToolMode::TestWaste:
		return TEXT("Test waste: point at home/shop/factory, trigger");
	case ECityToolMode::Hospital:
		return TEXT("Hospital");
	case ECityToolMode::Technology:
	case ECityToolMode::School:
		return GetSelectedDefinition() ? GetSelectedDefinition()->DisplayName : TEXT("Education");
	case ECityToolMode::Repair:
		return TEXT("Repair structural damage");
	case ECityToolMode::TestSickness:
		return TEXT("Test mild sickness: point at a populated building, trigger");
	case ECityToolMode::TestSevereSickness:
		return TEXT("Test severe sickness: point at a populated building, trigger");
	case ECityToolMode::TestFire:
		return TEXT("Test FIRE: point at building, trigger (real damage)");
	case ECityToolMode::TestCrime:
		return TEXT("Test CRIME: point at home/shop/industry, trigger");
	case ECityToolMode::TestArson:
		return TEXT("Test ARSON: point at home/shop/industry, trigger (real damage)");
	case ECityToolMode::Zone:
		return SelectedZone == ECityZoneType::Residential  ? TEXT("Zone Residential")
		       : SelectedZone == ECityZoneType::Commercial ? TEXT("Zone Commercial")
		                                                   : TEXT("Zone Industrial");
	case ECityToolMode::Demolish:
		return TEXT("Demolish");
	default:
		return TEXT("Unknown");
	}
}

FString ACityBoardActor::DescribeCell(const FCityCellCoord& Cell) const
{
	if (!PlacementModel || !PlacementModel->GetGrid().IsValidCell(Cell))
	{
		return TEXT("Point at the city board");
	}

	const FCityTerrainHeightfield* Terrain = SimulationSubsystem ? &SimulationSubsystem->GetTerrain() : nullptr;
	const ECityWaterClass WaterClass = Terrain ? Terrain->GetWaterClass(Cell) : ECityWaterClass::Land;
	const TCHAR* TerrainName = WaterClass == ECityWaterClass::DeepWater      ? TEXT("Deep water")
	                           : WaterClass == ECityWaterClass::ShallowWater ? TEXT("Shallow water")
	                           : WaterClass == ECityWaterClass::Shoreline    ? TEXT("Shoreline")
	                                                                         : TEXT("Land");

	if (PlacementModel->IsRoad(Cell))
	{
		const FName RoadDefinitionId = PlacementModel->GetRoadDefinitionId(Cell);
		const FCityRoadDefinition RoadDefinition = ResolveRoadDefinition(RoadDefinitionId);
		const FCityRoadTerrainEvaluation RoadTerrain = PlacementModel->EvaluateRoadTerrain(Cell);
		const auto* Span = PlacementModel->FindRoadStructure(Cell);
		const TCHAR* Treatment =
		    Span ? (Span->Profile == ECityRoadProfile::Bridge ? TEXT("Bridge span; demolish removes span + ramps")
		                                                      : TEXT("Elevated span; demolish removes span + ramps"))
		    : RoadTerrain.TerrainClass == ECityRoadTerrainClass::Bridge    ? TEXT("Bridge")
		    : RoadTerrain.TerrainClass == ECityRoadTerrainClass::Supported ? TEXT("Supported grade")
		                                                                   : TEXT("Ordinary grade");
		return FString::Printf(
		    TEXT(
		        "%s\nCell %d, %d\n%d lanes | Speed %d | Capacity %d\n%s | %s\nBuild $%lld | Upkeep $%lld\nConnected: %s"),
		    RoadDisplayName(RoadDefinitionId), Cell.X, Cell.Y,
		    RoadDefinition.ForwardLanes + RoadDefinition.ReverseLanes, RoadDefinition.SpeedUnitsPerStep,
		    RoadDefinition.CapacityPerLane * (RoadDefinition.ForwardLanes + RoadDefinition.ReverseLanes), TerrainName,
		    Treatment, RoadDefinition.PlacementCostPerCell, RoadDefinition.MonthlyUpkeepPerCell,
		    PlacementModel->IsRoadConnected(Cell) ? TEXT("Yes") : TEXT("No"));
	}

	const uint64 EntityId = PlacementModel->GetGrid().GetOccupant(Cell);
	if (EntityId == FCityGrid::EmptyEntityId)
	{
		const ECityZoneType Zone = PlacementModel->GetZone(Cell);
		const int32 Pollution =
		    SimulationSubsystem ? SimulationSubsystem->GetEnvironmentSnapshot().GetPollution(Cell) : 0;
		FString ResidentialDetail;
		if (SimulationSubsystem && Zone == ECityZoneType::Residential)
		{
			const FCityDesirabilityResult Desirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
			    *PlacementModel, SimulationSubsystem->GetServiceSnapshot(),
			    SimulationSubsystem->GetEnvironmentSnapshot(), Cell, FIntPoint(1, 1),
			    SimulationSubsystem->GetEconomyTuning());
			ResidentialDetail =
			    FString::Printf(TEXT("\nDesirability: %d/100 (road +%d, utilities +%d, park +%d, pollution -%d)"),
			                    Desirability.Score, Desirability.RoadBonus, Desirability.UtilityBonus,
			                    Desirability.ParkBonus, Desirability.PollutionPenalty);
		}
		return FString::Printf(TEXT("Empty cell\n%d, %d\nTerrain: %s\nZone: %s\nPollution: %d%s"), Cell.X, Cell.Y,
		                       TerrainName,
		                       Zone == ECityZoneType::Residential  ? TEXT("Residential")
		                       : Zone == ECityZoneType::Commercial ? TEXT("Commercial")
		                       : Zone == ECityZoneType::Industrial ? TEXT("Industrial")
		                                                           : TEXT("None"),
		                       Pollution, *ResidentialDetail);
	}

	const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(EntityId);
	if (!Entity)
	{
		return TEXT("Unknown entity");
	}
	const FCityBuildingDefinition* Definition =
	    BuildingCatalog ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
	const FString TypeName = Definition ? Definition->DisplayName : Entity->DefinitionId.ToString();
	const ECityOperationFailure OperationFailure = SimulationSubsystem
	                                                   ? SimulationSubsystem->GetServiceSnapshot().GetFailure(EntityId)
	                                                   : ECityOperationFailure::None;
	FString CoverageDetail;
	if (SimulationSubsystem && FCityBuildingDefinition::IsWaterFacility(Entity->DefinitionId))
	{
		const auto& Water = SimulationSubsystem->GetServiceSnapshot().Water;
		CoverageDetail = Water.RulesVersion
		                     ? TEXT("\nWater follows road pipes; pressure falls with distance")
		                     : FString::Printf(TEXT("\nLegacy water range %d road cells"),
		                                       SimulationSubsystem->GetEconomyTuning().WaterRangeRoadHops);
		CoverageDetail += TEXT("\nBlue pipes | green supplied | yellow partial | red none");
	}
	else if (OperationFailure != ECityOperationFailure::None)
	{
		const ECityDiagnosticOverlay LinkedOverlay =
		    FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(OperationFailure);
		if (LinkedOverlay != ECityDiagnosticOverlay::None)
		{
			CoverageDetail = FString::Printf(TEXT("\nDiagnostic: %s overlay"),
			                                 *FCityDiagnosticOverlayEvaluator::DisplayName(LinkedOverlay));
		}
	}
	if (SimulationSubsystem)
	{
		const FCityPopulationSnapshot& Population = SimulationSubsystem->GetPopulationSnapshot();
		if (const FCityHouseholdRecord* Household = Population.Households.FindByPredicate(
		        [&](const FCityHouseholdRecord& H) { return H.HomeEntityId == EntityId; }))
		{
			CoverageDetail +=
			    FString::Printf(TEXT("\nShared household budget $%lld\nPurchases today %d / %d"),
			                    Household->Balance, Household->CompletedPurchases, Household->ConsumptionOrders);
		}
		const FCityBusinessLedgerRecord* Business = Population.Businesses.FindByPredicate(
		    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == EntityId; });
		if (Business)
		{
			int32 Incoming = 0, Outgoing = 0;
			for (const FCityTransitTripRecord& Trip : Population.TransitTrips)
			{
				if (Trip.bCompleted || (Trip.Purpose != ECityTripPurpose::FreightDelivery &&
				                        Trip.Purpose != ECityTripPurpose::FreightExport))
					continue;
				if (Trip.DestinationEntityId == EntityId)
					Incoming += Trip.Quantity;
				if (Trip.OriginEntityId == EntityId)
					Outgoing += Trip.Quantity;
			}
			CoverageDetail += FString::Printf(
			    TEXT(
			        "\nStock %d | inbound %d | outbound %d\nSales last day %d | exported %lld\nBusiness balance $%lld\nPayroll paid $%lld / $%lld | unpaid $%lld"),
			    Business->Inventory, Incoming, Outgoing, Business->CompletedSales, Business->ExportedGoods,
			    Business->Balance, Business->PayrollPaid, Business->PayrollDue,
			    Business->PayrollDue - Business->PayrollPaid);
		}
	}
	FString EnvironmentDetail;
	if (SimulationSubsystem)
	{
		const auto Park =
		    FCityParkEvaluator::Policy(Entity->DefinitionId, SimulationSubsystem->GetEconomyTuning(), BuildingCatalog);
		if (Park.bIsPark)
			EnvironmentDetail = FString::Printf(
			    TEXT("\nPark: +%d appeal within %d cells\nDaily upkeep: $%lld; strongest benefit wins"), Park.Bonus,
			    Park.Radius, Park.Upkeep);
	}
	if (SimulationSubsystem && Definition && Definition->DevelopmentDemandType != ECityDevelopmentDemandType::None)
	{
		const FCityDevelopmentState* Development = SimulationSubsystem->GetDevelopmentSnapshot().Find(EntityId);
		if (Development && Development->Stage == ECityDevelopmentStage::Construction)
		{
			const int64 Remaining = FMath::Max<int64>(0, Development->ConstructionCompleteStep -
			                                                 SimulationSubsystem->GetEconomySnapshot().FixedStepId);
			const bool bAwaitingDelivery =
			    SimulationSubsystem->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
			        [EntityId](const FCityTransitTripRecord& Trip)
			        {
				        return Trip.DestinationEntityId == EntityId &&
				               Trip.Purpose == ECityTripPurpose::MoveInArrival && !Trip.bCompleted;
			        });
			EnvironmentDetail =
			    bAwaitingDelivery
			        ? FString::Printf(TEXT("\nAwaiting construction delivery\nThen %lld game minutes of construction"),
			                          Definition->ConstructionSteps * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth))
			        : FString::Printf(TEXT("\nUnder construction: %lld game minutes remaining"), Remaining * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth));
		}
		else if (Development)
		{
			const TCHAR* DensityName = Development->DensityLevel == ECityDensityLevel::High     ? TEXT("High density")
			                           : Development->DensityLevel == ECityDensityLevel::Medium ? TEXT("Medium density")
			                                                                                    : TEXT("Low density");
			EnvironmentDetail =
			    Definition->bPlayerPurchasedLevels
			        ? FString::Printf(TEXT("\nFacility stage: %d/3"), static_cast<int32>(Development->DensityLevel) + 1)
			        : FString::Printf(TEXT("\nDevelopment: %s"), DensityName);
		}
		if (Development && Development->Stage == ECityDevelopmentStage::Operational)
		{
			if (const FCityBuildingLevelDefinition* Level = Definition->FindDensityLevel(Development->DensityLevel))
			{
				EnvironmentDetail += FString::Printf(
				    TEXT(
				        "\nCapacity: residents %d | jobs %d | output %d\nUtilities: P %d W %d | upkeep $%lld | pollution %d/step"),
				    Level->ResidentsProvided, Level->JobsProvided, Level->EconomicOutput, Level->PowerDemand,
				    Level->WaterDemand, Level->MonthlyUpkeep, Level->PollutionEmission);
			}
		}
		if (Entity->DefinitionId == SimulationSubsystem->GetEconomyTuning().HomeDefinitionId)
		{
			const FCityDesirabilityResult Desirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
			    *PlacementModel, SimulationSubsystem->GetServiceSnapshot(),
			    SimulationSubsystem->GetEnvironmentSnapshot(), Entity->Anchor, Entity->FootprintSize,
			    SimulationSubsystem->GetEconomyTuning());
			EnvironmentDetail += FString::Printf(
			    TEXT("\nDesirability: %d/100 (park +%d)\nPollution exposure: %d (penalty -%d)"), Desirability.Score,
			    Desirability.ParkBonus, Desirability.PollutionExposure, Desirability.PollutionPenalty);
		}
		if (const FCityDevelopmentPressure* Pressure =
		        SimulationSubsystem->GetDevelopmentSnapshot().LatestPressures.Find(EntityId))
		{
			EnvironmentDetail +=
			    FString::Printf(TEXT("\nPressure %d = demand %d + road %d + land %d + compatibility %d + utilities %d"),
			                    Pressure->Total, Pressure->CitywideDemand, Pressure->RoadAccess, Pressure->LandValue,
			                    Pressure->PollutionCompatibility, Pressure->InfrastructureHeadroom);
		}
		if (Development && Development->Stage == ECityDevelopmentStage::Operational &&
		    Development->DensityLevel != ECityDensityLevel::High && Definition)
		{
			const ECityDensityLevel TargetLevel = Development->DensityLevel == ECityDensityLevel::Low
			                                          ? ECityDensityLevel::Medium
			                                          : ECityDensityLevel::High;
			const FCityBuildingLevelDefinition* Current = Definition->FindDensityLevel(Development->DensityLevel);
			const FCityBuildingLevelDefinition* Target = Definition->FindDensityLevel(TargetLevel);
			if (Current && Target)
			{
				const FCityServiceSnapshot& Services = SimulationSubsystem->GetServiceSnapshot();
				const FCityNeedsSnapshot& Needs = SimulationSubsystem->GetNeedsSnapshot();
				const int64 Age =
				    SimulationSubsystem->GetEconomySnapshot().FixedStepId - Development->OperationalSinceStep;
				const int32 AddedPower = FMath::Max(0, Target->PowerDemand - Current->PowerDemand);
				const int32 AddedWater = FMath::Max(0, Target->WaterDemand - Current->WaterDemand);
				const auto& Density = Needs.DensityFor(Definition->DevelopmentDemandType);
				const bool UsesDensityDemand = Needs.bHasDensityDemand && !Definition->bPlayerPurchasedLevels;
				const int32 RawDemand = UsesDensityDemand ? Density.UpgradeDemand(TargetLevel)
				                        : Definition->DevelopmentDemandType == ECityDevelopmentDemandType::Commercial
				                            ? Needs.CommercialDemandBalance
				                        : Definition->DevelopmentDemandType == ECityDevelopmentDemandType::Industrial
				                            ? Needs.IndustrialDemandBalance
				                            : Needs.ResidentialDemandBalance;
				const int32 VacantFootprints =
				    Definition->DevelopmentDemandType == ECityDevelopmentDemandType::Commercial
				        ? Needs.CommercialEligibleFootprints
				    : Definition->DevelopmentDemandType == ECityDevelopmentDemandType::Industrial
				        ? Needs.IndustrialEligibleFootprints
				        : Needs.ResidentialEligibleFootprints;
				const int32 CurrentCapacity =
				    Definition->DevelopmentDemandType == ECityDevelopmentDemandType::Residential
				        ? Current->ResidentsProvided
				        : FMath::Max(Current->JobsProvided, Current->EconomicOutput);
				const int32 ReservedInfill = UsesDensityDemand ? 0 : FMath::Min(VacantFootprints, 2) * CurrentCapacity;
				const int32 UpgradeDemand = RawDemand - ReservedInfill;
				const FString FundingDetail = Definition->bPlayerPurchasedLevels
				                                  ? FString::Printf(TEXT("city funds $%lld/$%lld"),
				                                                    PlacementModel->GetFunds(), Target->UpgradeCost)
				                                  : FString(TEXT("privately funded"));
				EnvironmentDetail += FString::Printf(
				    TEXT(
				        "\nNext %s: age %lld/%lld game minutes | upgrade demand %+d/%+d\nRaw demand %+d, low-density reserve %d\nUtilities P %d/%d W %d/%d | %s"),
				    TargetLevel == ECityDensityLevel::High ? TEXT("High") : TEXT("Medium"),
				    FMath::Min(Age, Target->MinimumOperationalSteps) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth), Target->MinimumOperationalSteps * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth), UpgradeDemand,
				    Target->MinimumDemand, RawDemand, ReservedInfill, Services.PowerCapacity - Services.PowerDemand,
				    AddedPower, Services.ServiceCapacity - Services.ServiceDemand, AddedWater, *FundingDetail);
				if (UsesDensityDemand)
				{
					const int32 RetainedCapacity = Density.Reserved[static_cast<int32>(Development->DensityLevel)];
					const int32 RetentionTarget = Density.Desired[static_cast<int32>(Development->DensityLevel)];
					const int32 SourceCapacity =
					    FCityDensityDemand::Capacity(*Current, Definition->DevelopmentDemandType);
					const int32 TargetCapacity =
					    FCityDensityDemand::Capacity(*Target, Definition->DevelopmentDemandType);
					const TCHAR* DensityReason = RetainedCapacity - SourceCapacity < RetentionTarget
					                                 ? TEXT("Retaining capacity at the current density")
					                             : Density.AdditionalCapacity() < TargetCapacity - SourceCapacity
					                                 ? TEXT("Existing capacity covers city needs")
					                             : Density.UpgradeDemand(TargetLevel) < TargetCapacity
					                                 ? TEXT("Waiting for demand for a full target-tier building")
					                                 : TEXT("Density demand ready; age and services still required");
					EnvironmentDetail += FString::Printf(
					    TEXT("\nRetain %s: %d/%d places/jobs\nTotal extra capacity needed: %d\n%s"),
					    Development->DensityLevel == ECityDensityLevel::Low ? TEXT("Low") : TEXT("Medium"),
					    RetainedCapacity, RetentionTarget, Density.AdditionalCapacity(), DensityReason);
				}
			}
		}
	}
	return FString::Printf(TEXT("ID %llu\n%s\nZone: %s\nCity purchase $%lld\n%s%s%s"), EntityId, *TypeName,
	                       PlacementModel->GetZone(Cell) == ECityZoneType::Residential  ? TEXT("Residential")
	                       : PlacementModel->GetZone(Cell) == ECityZoneType::Commercial ? TEXT("Commercial")
	                       : PlacementModel->GetZone(Cell) == ECityZoneType::Industrial ? TEXT("Industrial")
	                                                                                    : TEXT("None"),
	                       Entity->RecordedPlacementCost, *FCityServiceEvaluator::FailureToString(OperationFailure),
	                       *CoverageDetail, *EnvironmentDetail);
}

FString ACityBoardActor::DescribeCitySummary() const
{
	if (!SimulationSubsystem || !PlacementModel)
	{
		return FString::Printf(TEXT("Tool: %s\nSimulation unavailable"), *GetToolDisplayName());
	}

	const FCityEconomySnapshot& Snapshot = SimulationSubsystem->GetEconomySnapshot();
	int32 Homeless = 0, Unemployed = 0;
	for (const auto& Citizen : SimulationSubsystem->GetPopulationSnapshot().Citizens)
	{
		Homeless += Citizen.IsHomeless() ? 1 : 0;
		Unemployed += Citizen.WorkplaceEntityId == 0 ? 1 : 0;
	}
	const auto& Health = SimulationSubsystem->GetHealthSnapshot();
	int32 Severe = 0, InCare = 0;
	for (const auto& P : Health.Patients)
	{
		Severe += P.Illness == ECityIllness::Severe;
		InCare += P.ReceivingCare();
	}
	const auto& Waste = SimulationSubsystem->GetWasteSnapshot();
	const int64 Net = Snapshot.LastIncome - Snapshot.LastUpkeep;
	return FString::Printf(
	    TEXT(
	        "CITY DETAILS\nJobs %d  Net %s$%lld\nHomeless %d  Unemployed %d\nIll %d (severe %d) | in care %d\nWaste %lld piled | %lld in trucks | %lld stored"),
	    Snapshot.Jobs, Net >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Net), Homeless, Unemployed, Health.Patients.Num(),
	    Severe, InCare, Waste.Piled(), Waste.InTrucks(), Waste.InDepots());
}

FString ACityBoardActor::DescribeCityTotals() const
{
	if (!SimulationSubsystem || !PlacementModel)
		return TEXT("TREASURY --  |  POPULATION --");
	return FString::Printf(TEXT("TREASURY $%lld  |  POPULATION %d"), PlacementModel->GetFunds(),
	                       SimulationSubsystem->GetEconomySnapshot().Residents);
}

void ACityBoardActor::GetCityHudTrends(int64& OutMonthlyMoney, int64& OutPopulationChange,
                                       bool& bOutPopulationHistory) const
{
	OutMonthlyMoney = OutPopulationChange = 0;
	bOutPopulationHistory = false;
	if (!SimulationSubsystem)
		return;
	OutMonthlyMoney = SimulationSubsystem->GetMonthSettlementPreview().Net();
	const auto& Months = SimulationSubsystem->GetReportHistory().Months;
	if (Months.Num() < 2)
		return;
	const auto& Last = Months.Last();
	const auto& Previous = Months[Months.Num() - 2];
	if (Last.MonthId != Previous.MonthId + 1)
		return;
	OutPopulationChange = Last.Get(ECityReportMetric::Population) - Previous.Get(ECityReportMetric::Population);
	bOutPopulationHistory = true;
}

FString ACityBoardActor::DescribeConstructionCost(FName Id) const
{
	if (!SimulationSubsystem || !BuildingCatalog)
		return FString();
	if (Id == TEXT("DirtRoad") || Id == TEXT("LocalRoad") || Id == TEXT("Avenue") || Id == TEXT("Highway"))
	{
		const auto Road = Id == TEXT("DirtRoad")  ? FCityRoadDefinition::Dirt()
		                  : Id == TEXT("Avenue")  ? FCityRoadDefinition::Avenue()
		                  : Id == TEXT("Highway") ? FCityRoadDefinition::Highway()
		                                          : FCityRoadDefinition::Local();
		return FString::Printf(
		    TEXT("Build $%lld/tile | Daily $%lld/tile\nRaised supports/bridges cost extra; preview shows total"),
		    Road.PlacementCostPerCell, Road.MonthlyUpkeepPerCell);
	}
	const auto* D = BuildingCatalog->FindDefinition(Id);
	if (!D)
		return FString();
	const auto& T = SimulationSubsystem->GetEconomyTuning();
	const auto* L = D->FindDensityLevel(ECityDensityLevel::Low);
	int64 Upkeep = L ? L->MonthlyUpkeep : D->MonthlyUpkeep;
	const auto Park = FCityParkEvaluator::Policy(Id, T, BuildingCatalog);
	if (Park.bIsPark)
		Upkeep = Park.Upkeep;
	else if (Id == T.PowerDefinitionId)
		Upkeep = T.PowerUpkeep;
	else if (Id == T.WaterTreatmentDefinitionId)
		Upkeep = T.WaterTreatmentUpkeep;
	else if (Id == T.HomeDefinitionId && !L)
		Upkeep = T.HomeUpkeep;
	else if (Id == T.ShopDefinitionId && !L)
		Upkeep = T.ShopUpkeep;
	const bool Specialized = FCityTechnology::IsFacility(Id) ||
	                         FCityBuildingDefinition::IsAlternativeUtility(Id) || Id == TEXT("College") ||
	                         Id == TEXT("Hospital") || FCityWasteSimulation::IsFacility(Id);
	const int32 Staff = L && D->bPlayerPurchasedLevels ? L->JobsProvided
	                    : Specialized                  ? D->JobsProvided
	                                                   : FCityPopulationSimulation::PublicJobs(Id, T);
	const int64 Payroll = static_cast<int64>(Staff) * T.HouseholdIncomePerEmployedResident;
	return Payroll > 0 ? FString::Printf(TEXT("Build $%lld | Daily $%lld\nUpkeep $%lld + full staff $%lld"), D->Cost,
	                                     Upkeep + Payroll, Upkeep, Payroll)
	                   : FString::Printf(TEXT("Build $%lld | Daily $%lld upkeep"), D->Cost, Upkeep);
}

FString ACityBoardActor::GetConstructionStatus() const
{
	if (ToolMode == ECityToolMode::Road)
		return RoadConstructionStatus;
	if (!PlacementGhost || !PlacementGhost->IsVisible())
		return FString();
	return BuildingConstructionStatus;
}

bool ACityBoardActor::IsConstructionPriceBlocked() const
{
	return !GetConstructionStatus().IsEmpty() && bConstructionPriceBlocked &&
	       (ToolMode != ECityToolMode::Road || RoadPreview.bActive);
}

FString ACityBoardActor::DescribeNeedsPanel() const
{
	if (!SimulationSubsystem)
	{
		return TEXT("NEEDS\nSimulation unavailable");
	}
	return FString::Printf(TEXT("%s\nOverlay: %s"),
	                       *FCityNeedsPresentation::BuildGuidance(SimulationSubsystem->GetNeedsSnapshot(),
	                                                              SimulationSubsystem->GetEconomySnapshot()),
	                       *FCityDiagnosticOverlayEvaluator::DisplayName(DiagnosticOverlay));
}

TArray<FCityInfoCard> ACityBoardActor::GetCityInfoCards() const
{
	if (!SimulationSubsystem)
		return {};
	const auto& Economy = SimulationSubsystem->GetEconomySnapshot();
	const auto& Population = SimulationSubsystem->GetPopulationSnapshot();
	const auto& Health = SimulationSubsystem->GetHealthSnapshot();
	const auto& Waste = SimulationSubsystem->GetWasteSnapshot();
	const auto& Staff = SimulationSubsystem->GetNeedsSnapshot().WorkforceEducation;
	const auto Budget = SimulationSubsystem->GetMonthSettlementPreview();
	const auto Happiness = FCityHappinessEvaluator::Summarize(Population);
	int32 Homeless = 0, Unemployed = 0, Severe = 0, InCare = 0;
	for (const auto& Citizen : Population.Citizens)
	{
		Homeless += Citizen.IsHomeless() ? 1 : 0;
		Unemployed += Citizen.WorkplaceEntityId == 0 ? 1 : 0;
	}
	for (const auto& Patient : Health.Patients)
	{
		Severe += Patient.Illness == ECityIllness::Severe;
		InCare += Patient.ReceivingCare();
	}
	const FString LastNet =
	    Population.LastSettledMonthId >= 0 && Economy.LastBilledMonthId > 0
	        ? FString::Printf(TEXT("%s$%lld"), Economy.LastIncome >= Economy.LastUpkeep ? TEXT("+") : TEXT("-"),
	                          FMath::Abs(Economy.LastIncome - Economy.LastUpkeep))
	        : TEXT("--");
	return {{TEXT("PEOPLE"), TEXT("Happiness\nDiscontented\nHomeless\nUnemployed"),
	         FString::Printf(TEXT("%s\n%d\n%d\n%d"),
	                         Happiness.Residents ? *FString::Printf(TEXT("%d/100"), Happiness.Average) : TEXT("--"),
	                         Happiness.Discontented, Homeless, Unemployed)},
	        {TEXT("DAILY BUDGET"), TEXT("Income now\nFull bills\nNet now\nLast net"),
	         FString::Printf(TEXT("$%lld\n$%lld\n%s$%lld\n%s"), Budget.Income, Budget.Expenses,
	                         Budget.Net() >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Budget.Net()), *LastNet)},
	        {TEXT("HEALTH"), TEXT("Ill\nSevere\nIn care"),
	         FString::Printf(TEXT("%d\n%d\n%d"), Health.Patients.Num(), Severe, InCare)},
	        {TEXT("WASTE"), TEXT("Piled\nIn trucks\nStored"),
	         FString::Printf(TEXT("%lld\n%lld\n%lld"), Waste.Piled(), Waste.InTrucks(), Waste.InDepots())},
	        {TEXT("EDUCATION"), TEXT("Uneducated\nBasic\nSecondary\nHigher"),
	         FString::Printf(TEXT("%d\n%d\n%d\n%d"), Staff.Uneducated, Staff.Basic, Staff.Secondary, Staff.Higher)},
	        {TEXT("TAX INCOME NOW"), TEXT("Residential\nCommercial\nIndustrial"),
	         FString::Printf(TEXT("$%lld\n$%lld\n$%lld"), Budget.ResidentialTax, Budget.CommercialTax,
	                         Budget.IndustrialTax)}};
}

bool ACityBoardActor::GetConstructionMarkerAnchor(FVector& OutAnchor) const
{
	if (!PlacementModel || GetConstructionStatus().IsEmpty())
		return false;
	if (ToolMode == ECityToolMode::Road)
	{
		if (!RoadPreview.bActive)
			return false;
		FVector Local = GetCellSurfaceCenter(RoadPreview.End, 56.0);
		FCityRoadStructure Candidate;
		FString Error;
		if (PlacementModel->MakeRoadStructure(RoadPreview, Candidate, Error))
			Local.Z =
			    PlacementModel->SampleStructureHeight(Candidate, RoadPreview.End.X + .5, RoadPreview.End.Y + .5) + 56.0;
		OutAnchor = GetActorTransform().TransformPosition(Local);
		return true;
	}
	if (!PlacementGhost || !PlacementGhost->IsVisible())
		return false;
	OutAnchor = PlacementGhost->Bounds.Origin + FVector::UpVector * PlacementGhost->Bounds.BoxExtent.Z;
	return true;
}

FString ACityBoardActor::DescribeCalendar(const bool bIncludeTime) const
{
    if (!SimulationSubsystem) return TEXT("YEAR 1 | MONTH 1 | DAY 1/25 | 00:00");
    const auto& Snapshot = SimulationSubsystem->GetEconomySnapshot();
    const auto& Tuning = SimulationSubsystem->GetEconomyTuning();
    const int32 StepsPerDay = FMath::Max(1, Tuning.FixedStepsPerMonth);
    const int64 DayIndex = FCityCalendar::DayIndex(Snapshot.FixedStepId, StepsPerDay);
    const int64 MonthIndex = DayIndex / FCityCalendar::DaysPerMonth;
    const double Progress = SimulationSubsystem->GetSimulationStepProgress();
    const int32 Minute = FMath::Min(1439, FMath::FloorToInt(FCityCalendar::DayFraction(Snapshot.FixedStepId, Progress, StepsPerDay) * 1440));
    return FString::Printf(TEXT("YEAR %lld | MONTH %lld | DAY %lld/%d | %02d:%02d%s\n%s"),
        MonthIndex / 12 + 1, MonthIndex % 12 + 1, DayIndex % FCityCalendar::DaysPerMonth + 1,
        FCityCalendar::DaysPerMonth, Minute / 60, Minute % 60,
        bIncludeTime ? *(TEXT("\n") + SimulationSubsystem->GetTimeStatus()) : TEXT(""),
        *SimulationSubsystem->GetEnvironmentSnapshot().DescribeWeather());
}

FString ACityBoardActor::DescribeEconomy() const
{
	if (!SimulationSubsystem || !PlacementModel)
	{
		return TEXT("Simulation unavailable");
	}

	const FCityEconomySnapshot& Snapshot = SimulationSubsystem->GetEconomySnapshot();
	const auto& Health = SimulationSubsystem->GetHealthSnapshot();
	int32 Severe = 0, InCare = 0;
	for (const auto& P : Health.Patients)
	{
		Severe += P.Illness == ECityIllness::Severe;
		InCare += P.ReceivingCare();
	}
	const int64 Net = Snapshot.LastIncome - Snapshot.LastUpkeep;
	const FString Shortfall =
	    Snapshot.UpkeepShortfall > 0 ? FString::Printf(TEXT("  Shortfall $%lld"), Snapshot.UpkeepShortfall) : FString();
	const FString ControlStatus = SimulationSubsystem->GetControlStatus().IsEmpty()
	                                  ? FString()
	                                  : FString::Printf(TEXT("\n%s"), *SimulationSubsystem->GetControlStatus());
	return FString::Printf(TEXT("%s\nPopulation %d  Jobs %d\nTreasury $%lld  Last day %s$%lld%s\nGrowth: %s\n%s%s"),
	                       *SimulationSubsystem->GetTimeStatus(), Snapshot.Residents, Snapshot.Jobs,
	                       PlacementModel->GetFunds(), Net >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(Net), *Shortfall,
	                       *FCityGrowthSimulation::DescribeResult(SimulationSubsystem->GetLastGrowthResult()),
	                       *SimulationSubsystem->GetPersistenceStatus(), *ControlStatus);
}

FCityGridDimensions ACityBoardActor::GetDimensions() const
{
	return SimulationSubsystem ? SimulationSubsystem->GetTerrain().GetDimensions() : FCityGridDimensions();
}

bool ACityBoardActor::HasCurrentCityModel() const
{
	return PlacementModel && (!SimulationSubsystem || PlacementModel == &SimulationSubsystem->GetPlacementModel());
}

bool ACityBoardActor::GetInspectionMesh(const uint64 EntityId, FBox& OutBounds, FTransform& OutTransform) const
{
	const FCityInstanceHandle* Handle = InstanceIndex.Find(EntityId);
	if (!Handle)
		return false;
	const auto* Pool = BuildingInstances.Find(Handle->DefinitionId);
	if (!Pool || !*Pool || !(*Pool)->GetStaticMesh() ||
	    !(*Pool)->GetInstanceTransform(Handle->InstanceIndex, OutTransform, true))
		return false;
	OutBounds = (*Pool)->GetStaticMesh()->GetBoundingBox();
	return true;
}

bool ACityBoardActor::GetDesktopModelSource(const uint64 EntityId, FCityDesktopModelSource& Out) const
{
	Out = {};
	if (!HasCurrentCityModel() || !PlacementModel->GetEntities().Contains(EntityId))
		return false;
	const auto* Handle = InstanceIndex.Find(EntityId);
	const auto* Pool = Handle ? BuildingInstances.Find(Handle->DefinitionId) : nullptr;
	FTransform Transform;
	if (!Pool || !*Pool || !(*Pool)->GetStaticMesh() ||
	    !(*Pool)->GetInstanceTransform(Handle->InstanceIndex, Transform, true))
		return false;
	Out.EntityId = EntityId;
	Out.Mesh = (*Pool)->GetStaticMesh();
	Out.Rotation = GetActorQuat().Inverse() * Transform.GetRotation();
	for (int32 Index = 0; Index < (*Pool)->GetNumMaterials(); ++Index)
		Out.Materials.Add((*Pool)->GetMaterial(Index));
	if(Out.Mesh==WindBodyMesh.Get())
	{
		// The one-mesh inspector uses the approved assembled reference. The city
		// keeps its mast, weather-yaw nacelle and axle-spin rotor in separate pools.
		if(auto* Whole=BuildingGroundMeshes.FindRef(TEXT("SM_Building_WindPower_A")).Get())
		{
			Out.Mesh=Whole;Out.Materials.Reset();
			for(int32 Index=0;Index<Whole->GetStaticMaterials().Num();++Index) Out.Materials.Add(Whole->GetMaterial(Index));
		}
	}
	return true;
}

bool ACityBoardActor::PickInspectionBuilding(const FVector& Origin, const FVector& Direction, uint64& OutEntityId,
                                             FVector& OutHit) const
{
	OutEntityId = FCityGrid::EmptyEntityId;
	if (!HasCurrentCityModel() || Direction.IsNearlyZero())
		return false;
	FCityCellCoord Cell;
	// Match hover and demolition exactly. Buildings are selected by their tile;
	// the shared picker still resolves ground/deck road addresses physically.
	if (!PickCellFromWorldRay(Origin, Direction, Cell))
		return false;
	const uint64 Occupant = PlacementModel->GetGrid().GetOccupant(Cell);
	if (!PlacementModel->GetEntities().Contains(Occupant))
		return false;
	OutEntityId = Occupant;
	OutHit = GetActorTransform().TransformPosition(GetCellSurfaceCenter(Cell));
	// Mesh bounds only position the tooltip leader; they never choose its entity.
	FBox Bounds;
	FTransform Transform;
	if (GetInspectionMesh(Occupant, Bounds, Transform))
	{
		const FVector Center = Bounds.GetCenter();
		OutHit = Transform.TransformPosition(FVector(Center.X, Center.Y, Bounds.Max.Z));
	}
	return true;
}

bool ACityBoardActor::GetBuildingMarkerAnchor(const uint64 EntityId, FVector& OutRoof) const
{
	if (!HasCurrentCityModel())
		return false;
	const FCityPlacedEntity* Entity = PlacementModel ? PlacementModel->GetEntities().Find(EntityId) : nullptr;
	if (!Entity)
		return false;
	FBox Bounds;
	FTransform Transform;
	if (GetInspectionMesh(EntityId, Bounds, Transform))
	{
		const FVector Center = Bounds.GetCenter();
		OutRoof = Transform.TransformPosition(FVector(Center.X, Center.Y, Bounds.Max.Z));
	}
	else
		OutRoof = GetActorTransform().TransformPosition(GetCellSurfaceCenter(Entity->Anchor, 200.0));
	return true;
}

bool ACityBoardActor::GetInspectionBuilding(const uint64 EntityId, FVector& OutRoof, FString& OutDetail,
                                            TArray<FCityInspectionMetric>* OutMetrics,
                                            TArray<FCityUiInspectionSection>* OutSections) const
{
	if (!GetBuildingMarkerAnchor(EntityId, OutRoof))
		return false;
	const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(EntityId);
	OutDetail = DescribeCell(Entity->Anchor);
	if (OutMetrics && SimulationSubsystem)
	{
		OutMetrics->Reset();
		const FCityBuildingDefinition* Definition =
		    BuildingCatalog ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
		const auto& Services = SimulationSubsystem->GetServiceSnapshot();
		const auto& Population = SimulationSubsystem->GetPopulationSnapshot();
		const auto* Development = SimulationSubsystem->GetDevelopmentSnapshot().Find(EntityId);
		const FString Density = Development ? (Development->DensityLevel == ECityDensityLevel::High     ? TEXT("High")
		                                       : Development->DensityLevel == ECityDensityLevel::Medium ? TEXT("Medium")
		                                                                                                : TEXT("Low"))
		                                    : TEXT("Low");
		OutDetail = FString::Printf(
		    TEXT("%s | %s %s\n%s"), Definition ? *Definition->DisplayName : *Entity->DefinitionId.ToString(), *Density,
		    Definition && Definition->bPlayerPurchasedLevels ? TEXT("facility stage") : TEXT("density"),
		    Development && Development->Stage == ECityDevelopmentStage::Construction
		        ? TEXT("Under construction")
		        : *FCityServiceEvaluator::FailureToString(Services.GetFailure(EntityId)));
        if(FCityPlacementModel::RequiresFullRoadFrontage(Entity->DefinitionId) &&
            !PlacementModel->HasFullRoadFrontage(Entity->Anchor,Entity->FootprintSize,Entity->QuarterTurnsClockwise))
            OutDetail+=FString::Printf(TEXT("\nEntrance access: extend ground road across the entire %d-tile front"),
                Entity->QuarterTurnsClockwise%2?Entity->FootprintSize.Y:Entity->FootprintSize.X);
		if (Development && Development->Stage == ECityDevelopmentStage::Construction)
			OutDetail +=
			    Development->StationCompleteHalfDay >= 0
			        ? FString::Printf(TEXT("\nConstruction: %.2f game days left"),
			                          FMath::Max(0.0, Development->StationCompleteHalfDay * 0.5 -
			                                              (SimulationSubsystem->GetEconomySnapshot().FixedStepId +
			                                               SimulationSubsystem->GetSimulationStepProgress()) / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth)))
			        : FString::Printf(TEXT("\nConstruction: %lld game minutes left"),
			                          FMath::Max<int64>(0, Development->ConstructionCompleteStep -
			                                                   SimulationSubsystem->GetEconomySnapshot().FixedStepId) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth));
		if (OutSections)
		{
			OutSections->SetNum(6);
			const TCHAR* Titles[] = {TEXT("Utilities & waste"), TEXT("Work, shopping & finances"), TEXT("Education"),
			                         TEXT("Health"), TEXT("Wellbeing & growth"), TEXT("Safety & repair")};
			for (int32 I = 0; I < 6; ++I)
				(*OutSections)[I] = {FText::FromString(Titles[I]), FText::GetEmpty(), {}};
		}
		int32 DetailStart = OutDetail.Len(), MetricStart = OutMetrics->Num();
		auto Capture = [&](int32 Index)
		{
			if (OutSections)
			{
				auto& Section = (*OutSections)[Index];
				const FString Added = OutDetail.Mid(DetailStart).TrimStartAndEnd();
				if (!Added.IsEmpty())
					Section.Detail = FText::FromString(Section.Detail.ToString() +
					    (Section.Detail.IsEmpty() ? TEXT("") : TEXT("\n")) + Added);
				for (int32 I = MetricStart; I < OutMetrics->Num(); ++I)
				{
					const auto& M = (*OutMetrics)[I];
					Section.Gauges.Add({FText::FromString(M.Label),
					    FText::FromString(FString::Printf(TEXT("%lld/%lld"), M.Value, M.Maximum)),
					    M.Maximum > 0 ? FMath::Clamp(double(M.Value) / M.Maximum, 0., 1.) : 0., false,
					    FLinearColor(.3f, .75f, .95f)});
				}
			}
			DetailStart = OutDetail.Len();
			MetricStart = OutMetrics->Num();
		};
		const auto* DemandLevel = Definition && Development ? Definition->FindDensityLevel(Development->DensityLevel) : nullptr;
		const int32 PowerNeeded = DemandLevel ? DemandLevel->PowerDemand : Definition ? Definition->PowerDemand : 0;
		if (PowerNeeded > 0)
			OutDetail += FString::Printf(TEXT("\nPower requirement %d | city spare %d\nPower is shared citywide; add/upgrade supply when capacity is short."),
			                             PowerNeeded, Services.GetAvailablePower());
		if (const int32* Demand = Services.Water.EntityDemand.Find(EntityId); Demand && *Demand > 0)
		{
			const int32 Delivered = Services.Water.EntityDelivered.FindRef(EntityId);
			OutDetail += TEXT("\n") + FCityServiceGuidance::Water(*PlacementModel, Services, EntityId);
			OutMetrics->Add({TEXT("Water delivered"), Delivered, *Demand});
		}
		if (const auto* Source =
		        Services.Water.Sources.FindByPredicate([EntityId](const auto& S) { return S.EntityId == EntityId; }))
		{
			OutDetail += FString::Printf(
			    TEXT("\nGroundwater yield %d%% | output %d\nWater supplied %d/%d | head %d%%\n%s"),
			    Source->YieldPercent, Source->Capacity, Source->Used, Source->Capacity, Source->SourcePressurePercent,
			    Services.Water.RulesVersion ? TEXT("Road pipes: -2%/cell; usable at20%")
			                                : TEXT("Legacy city: original water rules"));
			OutMetrics->Add({TEXT("Water source used"), Source->Used, Source->Capacity});
			if (Services.Water.RulesVersion > 0)
				OutDetail += FString::Printf(
				    TEXT(
				        "\nFull flow up to %d road steps; usable partial up to %d\nSource output shared; improve staff/yield or add/upgrade supply"),
				    FMath::Max(0, (Source->SourcePressurePercent - FCityWaterEvaluator::FullDeliveryPressurePercent) /
				                      FCityWaterEvaluator::PressureLossPerRoadHop),
				    FMath::Max(0, (Source->SourcePressurePercent - FCityWaterEvaluator::MinimumUsablePressurePercent) /
				                      FCityWaterEvaluator::PressureLossPerRoadHop));
		}
		Capture(0);
		if (Definition && FCityTechnology::IsFacility(Entity->DefinitionId))
		{
			OutDetail += TEXT("\n") + Definition->DescribePrerequisites(Services, BuildingCatalog);
			OutDetail += FString::Printf(
			    TEXT("\nTechnology capability: %s | qualified paid staff %d%% / 50%% needed\nUpkeep "
			         "$%lld/day + public payroll"),
			    Services.GetFailure(EntityId) == ECityOperationFailure::None &&
			            (!Development || Development->Stage != ECityDevelopmentStage::Construction)
			        ? TEXT("operating")
			        : TEXT("offline"),
			    Services.PublicStaffPercent.FindRef(EntityId), Definition->MonthlyUpkeep);
			Capture(1);
		}
		if (Entity->DefinitionId == TEXT("Home"))
			OutDetail +=
			    TEXT("\n") + FCityServiceGuidance::Education(*PlacementModel, Services, Population,
			                                                 SimulationSubsystem->GetEconomyTuning(),
			                                                 SimulationSubsystem->GetDevelopmentSnapshot(), EntityId);
		Capture(2);
		const FString Work =
		    FCityServiceGuidance::Employment(Population, Services, EntityId, Entity->DefinitionId == TEXT("Home"));
		if (!Work.IsEmpty())
			OutDetail += TEXT("\n") + Work;
		if (const auto* Business =
		        Population.Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == EntityId; }))
		{
			int32 Workers = 0, Incoming = 0, Outgoing = 0;
			for (const auto& Citizen : Population.Citizens)
				if (Citizen.WorkplaceEntityId == EntityId)
					++Workers;
			for (const auto& Trip : Population.TransitTrips)
			{
				if (Trip.bCompleted || (Trip.Purpose != ECityTripPurpose::FreightDelivery &&
				                        Trip.Purpose != ECityTripPurpose::FreightExport))
					continue;
				if (Trip.DestinationEntityId == EntityId)
					Incoming += Trip.Quantity;
				if (Trip.OriginEntityId == EntityId)
					Outgoing += Trip.Quantity;
			}
			int32 Reserved = 0;
			for (const auto& Order : Population.PurchaseReservations)
				if (Order.ShopId == EntityId)
					++Reserved;
            if (!Business->bIndustrial)
                OutDetail += TEXT("\nAI: ") + FCityCommercialAi::Status(Population, Services, EntityId);
			OutDetail += FString::Printf(TEXT("\nCash $%lld | Stock %d (held %d)\nIn transit: in %d / out %d"),
			                             Business->Balance, Business->Inventory, Reserved, Incoming, Outgoing);
			OutMetrics->Add({TEXT("Staff assigned"), Workers, Business->JobCapacity});
			if (Business->bIndustrial)
			{
				OutDetail += FString::Printf(TEXT("\nMade last day %d | Exports total %lld"), Business->ProducedGoods,
				                             Business->ExportedGoods);
				OutMetrics->Add(
				    {TEXT("Output next day"),
				     FCityPopulationSimulation::StaffedOutput(Business->PendingOrders, Business->JobCapacity, Workers),
				     Business->PendingOrders});
			}
			else
				OutMetrics->Add(
				    {Population.bUsesPurchaseReservations ? TEXT("Sales today") : TEXT("Sales last day"),
				     Population.bUsesPurchaseReservations ? Business->SalesThisMonth : Business->CompletedSales,
				     Business->PendingOrders});
			OutMetrics->Add({TEXT("Payroll last day $"), Business->PayrollPaid, Business->PayrollDue});
		}
		else if (const auto* Household =
		             Population.Households.FindByPredicate([&](const auto& H) { return H.HomeEntityId == EntityId; }))
		{
			int32 Residents = 0, AtHome = 0, Travelling = 0;
			for (const auto& Citizen : Population.Citizens)
			{
				if (Citizen.HomeEntityId != EntityId)
					continue;
				++Residents;
				if (Citizen.TripPurpose != ECityTripPurpose::None)
					++Travelling;
				else if (Citizen.Schedule == ECityCitizenSchedule::Home)
					++AtHome;
			}
			int64 HeldMoney = 0;
			for (const auto& Order : Population.PurchaseReservations)
				if (Order.HouseholdId == Household->Id)
					HeldMoney += Order.Price;
			OutDetail += FString::Printf(TEXT("\nShared savings $%lld (held $%lld)\nResidents %d | Travelling %d"),
			                             Household->Balance, HeldMoney, Residents, Travelling);
			OutMetrics->Add({TEXT("At home now"), AtHome, Residents});
			OutMetrics->Add(
			    {Population.bUsesPurchaseReservations ? TEXT("Purchases today") : TEXT("Purchases last day"),
			     Population.bUsesPurchaseReservations ? Household->PurchasesThisMonth : Household->CompletedPurchases,
			     Household->ConsumptionOrders});
			Capture(1);
			const auto Desirability = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
			    *PlacementModel, Services, SimulationSubsystem->GetEnvironmentSnapshot(), Entity->Anchor,
			    Entity->FootprintSize, SimulationSubsystem->GetEconomyTuning());
			OutMetrics->Add({TEXT("Desirability"), Desirability.Score, 100});
			const auto Wellbeing = FCityHappinessEvaluator::Summarize(Population, EntityId);
			OutMetrics->Add({TEXT("Happiness"), Wellbeing.Average, 100});
			OutMetrics->Add({TEXT("Happiness outlook"), Wellbeing.Target, 100});
			OutMetrics->Add({TEXT("Discontented residents"), Wellbeing.Discontented, Wellbeing.Residents});
			OutDetail += FString::Printf(
			    TEXT(
			        "\nHappiness %d/100 | outlook %d/100\nDiscontented %d/%d | sustained %d\nSustained discontent: 3+ days below 50\nRecorded concerns: %s"),
			    Wellbeing.Average, Wellbeing.Target, Wellbeing.Discontented, Wellbeing.Residents, Wellbeing.Sustained,
			    *FCityHappinessEvaluator::DescribeConcerns(Wellbeing.Concerns));
			TSet<ECityHousingStatus> HousingStates;
			int32 Seeking = 0, LongestLow = 0;
			for (const auto& C : Population.Citizens)
				if (C.HomeEntityId == EntityId)
				{
					HousingStates.Add(C.HousingStatus);
					if (FCityHousingHistory::IsSearching(static_cast<uint8>(C.HousingStatus)))
					{
						++Seeking;
						LongestLow = FMath::Max(LongestLow, C.DiscontentMonths);
					}
				}
			if (Seeking)
			{
				const FString Warning = FString::Printf(
				        TEXT(
				            "HOUSEHOLD SEARCH: %d residents | low up to %d days\nDeparture possible at6 unresolved low days; improve needs or housing\n"),
				        Seeking, LongestLow);
				// Structured Desktop details keep the warning in wellbeing; the
				// full-text/VR presenter retains its existing leading alert.
				OutDetail = OutSections ? OutDetail + TEXT("\n") + Warning : Warning + OutDetail;
			}
			TArray<FString> HousingText;
			for (int32 I = 0; I <= static_cast<int32>(ECityHousingStatus::Recovering); ++I)
				if (HousingStates.Contains(static_cast<ECityHousingStatus>(I)))
					HousingText.Add(FCityHousingMigration::Describe(static_cast<ECityHousingStatus>(I)));
			OutDetail += TEXT("\nHousing decision: ") + FString::Join(HousingText, TEXT("; "));
		}
		else
		{
			Capture(1);
			if (!FCityWasteSimulation::IsFacility(Entity->DefinitionId))
				OutDetail += FString::Printf(TEXT("\nLocation %d, %d"), Entity->Anchor.X, Entity->Anchor.Y);
			if (!Services.Water.RulesVersion &&
			    Entity->DefinitionId == SimulationSubsystem->GetEconomyTuning().WaterTreatmentDefinitionId)
				OutDetail += FString::Printf(TEXT("\nWater reach: %d road hops"),
				                             SimulationSubsystem->GetEconomyTuning().WaterRangeRoadHops);
			const auto Park = FCityParkEvaluator::Policy(Entity->DefinitionId, SimulationSubsystem->GetEconomyTuning(),
			                                             BuildingCatalog);
			if (Park.bIsPark)
			{
				const bool bOperating = Services.ParkProviders.ContainsByPredicate(
				    [&](const auto& Provider) { return Provider.EntityId == EntityId; });
				OutDetail += FString::Printf(
				    TEXT(
				        "\nPark: +%d appeal within %d cells\nUpkeep $%lld/day | %s\nBuild > PARKS / PARK VIEW: show benefit area\nOverlaps use strongest benefit; cap30"),
				    Park.Bonus, Park.Radius, Park.Upkeep,
				    bOperating ? TEXT("operating")
				    : SimulationSubsystem->GetEconomySnapshot().HasOperatingUpkeepShortfall()
				        ? TEXT("paused: unpaid bills")
				        : TEXT("awaiting operation"));
				OutMetrics->Add({TEXT("Park benefit active"), bOperating ? Park.Bonus : 0, Park.Bonus});
			}
		}
		Capture(Entity->DefinitionId == TEXT("Home") ? 4 : 1);
		if (Definition && (FCityBuildingDefinition::IsAlternativeUtility(Entity->DefinitionId) ||
		                   Entity->DefinitionId == TEXT("College")))
		{
			const TCHAR* EducationNames[] = {TEXT("None"), TEXT("Basic"), TEXT("Secondary"), TEXT("Higher")};
			OutDetail +=
			    FString::Printf(TEXT("\nStaff education: %s | pollution %d"),
			                    EducationNames[Definition->MinimumStaffEducation], Definition->PollutionEmission);
			if (Entity->DefinitionId == TEXT("WindPower") || Entity->DefinitionId == TEXT("SolarPower"))
				OutDetail += FString::Printf(
				    TEXT("\nWeather output: %d%% | %s"),
				    CityUtilityWeatherPercent(Entity->DefinitionId, &SimulationSubsystem->GetEnvironmentSnapshot()),
				    *SimulationSubsystem->GetEnvironmentSnapshot().DescribeWeather());
			if (Entity->DefinitionId == TEXT("College"))
			{
				OutDetail += FString::Printf(
				    TEXT(
				        "\nCollege: Secondary to Higher in %d served days\nRoad reach %d hops | upkeep $%lld/day + payroll"),
				    SimulationSubsystem->GetEconomyTuning().SchoolMonthsPerLevel,
				    SimulationSubsystem->GetEconomyTuning().SchoolRoadRangeHops, Definition->MonthlyUpkeep);
				OutMetrics->Add({TEXT("College places used"), Services.SchoolUsedCapacity.FindRef(EntityId),
				                 Services.SchoolCapacities.FindRef(EntityId)});
			}
		}
		Capture(Entity->DefinitionId == TEXT("College") ? 2 : 0);
		if (ToolMode == ECityToolMode::Repair ||
		    (ToolMode >= ECityToolMode::TestFire && ToolMode <= ECityToolMode::TestArson) ||
		    ToolMode == ECityToolMode::TestSickness || ToolMode == ECityToolMode::TestSevereSickness ||
		    ToolMode == ECityToolMode::TestWaste)
			OutDetail += TEXT("\n") + SimulationSubsystem->GetControlStatus();
		Capture(5);
		if (const auto* Employer =
		        Population.PublicEmployers.FindByPredicate([&](const auto& E) { return E.EntityId == EntityId; }))
		{
			const auto& Tuning = SimulationSubsystem->GetEconomyTuning();
			const bool Alternative = FCityBuildingDefinition::IsAlternativeUtility(Entity->DefinitionId);
			const bool bUtility = Alternative || Entity->DefinitionId == Tuning.PowerDefinitionId ||
			                      Entity->DefinitionId == Tuning.WaterTreatmentDefinitionId;
			const auto* StaffingLevel = (bUtility || Entity->DefinitionId == TEXT("Hospital") ||
			                             FCityWasteSimulation::IsFacility(Entity->DefinitionId)) &&
			                                    Definition && Development
			                                ? Definition->FindDensityLevel(Development->DensityLevel)
			                                : nullptr;
			OutMetrics->Add({bUtility ? TEXT("Assigned staff") : TEXT("Public staff"), Population.Workers(EntityId),
			                 bUtility        ? (StaffingLevel               ? StaffingLevel->JobsProvided
			                                    : Alternative && Definition ? Definition->JobsProvided
			                                                                : Tuning.UtilityStaff)
			                 : StaffingLevel ? StaffingLevel->JobsProvided
			                                 : Services.PublicJobs.FindRef(EntityId)});
			const int32 OutputPercent =
			    Services.GetFailure(EntityId) != ECityOperationFailure::None ? 0
			    : Alternative && Definition
			        ? CityUtilityOutputPercent(*Definition, Services.PublicStaffPercent.FindRef(EntityId)) *
			              CityUtilityWeatherPercent(Entity->DefinitionId,
			                                        &SimulationSubsystem->GetEnvironmentSnapshot()) /
			              100
			    : bUtility ? Tuning.GetUtilityOutputPercent(Services.PublicStaffPercent.FindRef(EntityId))
			               : Population.StaffPercent(EntityId);
			OutDetail += FString::Printf(TEXT("\nPublic wages $%lld/$%lld last day | output %d%%"),
			                             Employer->PayrollPaid, Employer->PayrollDue, OutputPercent);
			if (bUtility)
			{
				Capture(1);
				const auto* UtilityLevel =
				    Definition && Development ? Definition->FindDensityLevel(Development->DensityLevel) : nullptr;
				const int32 Capacity =
				    Alternative && Definition
				        ? Definition->UtilityCapacity
				        : (Entity->DefinitionId == Tuning.PowerDefinitionId ? Tuning.PowerCapacityPerBuilding
				                                                            : Tuning.WaterCapacityPerTreatment) *
				              (UtilityLevel ? UtilityLevel->UtilitySupplyPercent : 100) / 100;
				const int32 Available = Services.GetFailure(EntityId) == ECityOperationFailure::None
				                            ? static_cast<int32>(static_cast<int64>(Capacity) * OutputPercent / 100)
				                            : 0;
				OutMetrics->Add({TEXT("Supply capacity"), Available, Capacity});
				const int64 BaseUpkeep = Alternative && Definition ? Definition->MonthlyUpkeep
				                         : Entity->DefinitionId == Tuning.PowerDefinitionId
				                             ? Tuning.PowerUpkeep
				                             : Tuning.WaterTreatmentUpkeep;
				OutDetail +=
				    FString::Printf(TEXT("\nUpkeep $%lld/day + payroll"),
				                    BaseUpkeep + (UtilityLevel && Development->DensityLevel != ECityDensityLevel::Low
				                                      ? FMath::Max<int64>(0, UtilityLevel->MonthlyUpkeep - BaseUpkeep)
				                                      : 0));
				OutDetail += FString::Printf(
				    TEXT("\nMinimum supply %d while connected and undamaged"),
				    static_cast<int32>(static_cast<int64>(Capacity) *
				                       (Alternative && Definition ? Definition->UtilityMinimumOutputPercent
				                                                  : Tuning.GetUtilityOutputPercent(0)) /
				                       100));
				Capture(0);
			}
		}
		Capture(1);
		if (FCityBuildingDefinition::IsSchoolFacility(Entity->DefinitionId) && Entity->DefinitionId != TEXT("College"))
		{
			OutMetrics->Add({TEXT("Resident places used"), Services.SchoolUsedCapacity.FindRef(EntityId),
			                 Services.SchoolCapacities.FindRef(EntityId)});
			OutDetail += FString::Printf(
			    TEXT(
			        "\nSchool: %d road hops | upkeep $%lld/day\n%s: %d served days\nCity access %d/%d | Secondary+ %d"),
			    SimulationSubsystem->GetEconomyTuning().SchoolRoadRangeHops, Definition ? Definition->MonthlyUpkeep : 0,
			    Entity->DefinitionId == TEXT("PrimarySchool")     ? TEXT("None to Basic")
			    : Entity->DefinitionId == TEXT("SecondarySchool") ? TEXT("Basic to Secondary")
			                                                      : TEXT("None to Secondary"),
			    SimulationSubsystem->GetEconomyTuning().SchoolMonthsPerLevel, Services.EducationServed,
			    Services.EducationDemand, Services.SecondaryResidents);
		}
		if (Entity->DefinitionId == TEXT("Home"))
		{
			int32 Local = 0, Qualified = 0, Learning = 0, Months = 0;
			for (const auto& C : Population.Citizens)
				if (C.HomeEntityId == EntityId)
				{
					++Local;
					Qualified += C.Education >= ECityEducationLevel::Secondary ? 1 : 0;
					if (C.Education < ECityEducationLevel::Secondary)
					{
						++Learning;
						Months += C.EducationMonths;
					}
				}
			OutMetrics->Add({TEXT("Education access"), Services.HomeEducationPlaces.FindRef(EntityId),
			                 Services.HomeEducationDemand.FindRef(EntityId)});
			OutMetrics->Add({TEXT("Secondary or higher"), Qualified, Local});
			OutDetail += FString::Printf(
			    TEXT(
			        "\nLearning progress avg %d/%d days\nSecondary qualifies for skilled roles at larger businesses"),
			    Learning ? Months / Learning : 0, SimulationSubsystem->GetEconomyTuning().SchoolMonthsPerLevel);
		}
		Capture(2);
		if (const auto* B =
		        Population.Businesses.FindByPredicate([&](const auto& V) { return V.EntityId == EntityId; }))
		{
			int32 Skilled = 0;
			for (const auto& C : Population.Citizens)
				Skilled += C.WorkplaceEntityId == EntityId && C.bSkilledJob ? 1 : 0;
			OutMetrics->Add({TEXT("Skilled roles filled"), Skilled, B->SkilledJobCapacity});
			if (B->SkilledJobCapacity > 0)
				OutDetail += FString::Printf(
				    TEXT("\nSecondary required for skilled roles: $%d/day target\nActual wages depend on employer cash"),
				    2 * SimulationSubsystem->GetEconomyTuning().HouseholdIncomePerEmployedResident);
		}
		Capture(1);
		if (const auto* Waste = SimulationSubsystem->GetWasteSnapshot().Find(EntityId))
		{
			OutDetail +=
			    FString::Printf(TEXT("\nWaste %d/%d | collected day %lld | %d/%s/day"), Waste->Units, Waste->Capacity,
			                    Waste->LastCollectionStep,
			                    Entity->DefinitionId == TEXT("Home")      ? 2
			                    : Entity->DefinitionId == TEXT("Factory") ? 8
			                                                              : 4,
			                    Entity->DefinitionId == TEXT("Home") ? TEXT("resident") : TEXT("operating worker"));
			if (OutMetrics)
			{
				FCityInspectionMetric Metric{TEXT("Waste pile"), Waste->Units, Waste->Capacity};
				if (!OutSections && DiagnosticOverlay == ECityDiagnosticOverlay::Waste)
					OutMetrics->Insert(Metric, 0);
				else
					OutMetrics->Add(Metric);
			}
		}
		if (FCityWasteSimulation::IsFacility(Entity->DefinitionId))
		{
			const auto Policy = SimulationSubsystem->GetWastePolicy(EntityId);
			const auto& Waste = SimulationSubsystem->GetWasteSnapshot();
			const auto* Depot = Waste.Depots.FindByPredicate([&](const auto& D) { return D.EntityId == EntityId; });
			int32 Busy = 0, Cargo = 0, Stops = 0;
			for (const auto& Truck : Waste.Trucks)
				if (Truck.DepotId == EntityId)
				{
					Busy += Truck.TripId || Truck.Cargo || Truck.Stops.Num();
					Cargo += Truck.Cargo;
					Stops += Truck.Stops.Num();
				}
			OutDetail += FString::Printf(
			    TEXT(
			        "\nWaste storage %d/%d | processing %d/day\nFleet %d active | %d busy | cargo %d | stops %d\nRange %d road cells | largest piles first\nTruck200 | pickup50/day | unload100/day\n%s staff | %s"),
			    Depot ? Depot->Stored : 0, Policy.Storage, Policy.ProcessingPerStep, Policy.Fleet, Busy, Cargo, Stops,
			    Policy.Range, Policy.bRecycling ? TEXT("Secondary") : TEXT("Basic"),
			    Policy.bRecycling ? TEXT("60% recycling /40% residue") : TEXT("Disposal"));
			if (OutMetrics)
				OutMetrics->Add({TEXT("Waste storage"), Depot ? Depot->Stored : 0, Policy.Storage});
		}
		Capture(0);
		if (Entity->DefinitionId == TEXT("Hospital"))
		{
			const auto* Level = Definition ? Definition->FindDensityLevel(Development ? Development->DensityLevel
			                                                                          : ECityDensityLevel::Low)
			                               : nullptr;
			int32 Patients=0,ReservedBeds=0,Visits=0;
			for (const auto& Patient:SimulationSubsystem->GetHealthSnapshot().Patients) if (Patient.HospitalId==EntityId)
			{
				Patients+=Patient.Care==ECityHealthCare::Hospital;
				ReservedBeds+=Patient.Care==ECityHealthCare::AwaitingAmbulance;
				Visits+=Patient.UsesOutpatientPlace();
			}
			const int32 Capacity=Services.HospitalCapacities.FindRef(EntityId);
			OutMetrics->Add({TEXT("Inpatient beds"),Patients,Capacity});
			OutMetrics->Add({TEXT("Beds reserved"),ReservedBeds,Capacity});
			OutMetrics->Add({TEXT("Outpatient visits"),Visits,Capacity});
			OutDetail += FString::Printf(
			    TEXT("\nWard %d/%d places | ambulance capacity %d/%d\nSecondary staff | upkeep $%lld/day + payroll"),
			    Services.HospitalCapacities.FindRef(EntityId), Level ? Level->CivicProtectionCapacity : 0,
			    Services.HospitalFleetCapacities.FindRef(EntityId), Level ? Level->CivicResponseVehicles : 0,
			    Level ? Level->MonthlyUpkeep : 0);
		}
		Capture(3);
		const auto& Fire = SimulationSubsystem->GetFireSnapshot();
		if (Entity->DefinitionId == TEXT("Fire"))
		{
			const auto* Level =
			    Definition && Development ? Definition->FindDensityLevel(Development->DensityLevel) : nullptr;
			OutDetail += FString::Printf(TEXT("\nFire: %d road hops | upkeep $%lld/day\n%s"),
			                             Level ? Level->CivicRoadRange
			                                   : SimulationSubsystem->GetEconomyTuning().FireRoadRangeHops,
			                             Level        ? Level->MonthlyUpkeep
			                             : Definition ? Definition->MonthlyUpkeep
			                                          : 0,
			                             *Fire.Describe());
			OutMetrics->Add({TEXT("Fire capacity used"), Fire.StationUsedCapacity.FindRef(EntityId),
			                 (Level ? Level->CivicProtectionCapacity
			                        : SimulationSubsystem->GetEconomyTuning().FireProtectionCapacity) *
			                     Services.PublicStaffPercent.FindRef(EntityId) / 100});
			if (SimulationSubsystem->GetEconomySnapshot().HasOperatingUpkeepShortfall())
				OutDetail += TEXT("\nResponse paused: unpaid operating bills");
			OutDetail += TEXT("\nFireproof | priority power/water");
		}
		if (Entity->DefinitionId == TEXT("Fire") || Entity->DefinitionId == TEXT("Police"))
		{
			const auto& Trips = SimulationSubsystem->GetPopulationSnapshot().TransitTrips;
			const auto* Crew =
			    Trips.FindByPredicate([&](const auto& T) { return T.IsEmergency() && T.StationEntityId == EntityId; });
			OutDetail += Crew ? Crew->Purpose == ECityTripPurpose::EmergencyReturn
			                        ? TEXT("\nCrew returning along roads; next call after arrival")
			                    : Crew->bCompleted ? TEXT("\nCrew on scene")
			                                       : TEXT("\nCrew travelling; check road connection/traffic")
			                  : TEXT("\nCrew available when staffed, supplied and paid");
		}
		if (Fire.EntityRisk.Contains(EntityId) &&
		    ((Entity->DefinitionId != TEXT("Hospital") && !FCityWasteSimulation::IsFacility(Entity->DefinitionId)) ||
		     Fire.IsIntroduced()))
			OutDetail += FString::Printf(TEXT("\nFire risk %d/100 | protected %d%%"), Fire.EntityRisk.FindRef(EntityId),
			                             Fire.EntityProtection.FindRef(EntityId));
		if (Fire.EntityRisk.Contains(EntityId) && Fire.IsIntroduced())
			OutDetail += FString::Printf(TEXT("\nFire risk factors: crime +%d | nearby industry +%d"),
			                             Fire.CrimeRiskContribution.FindRef(EntityId),
			                             Fire.IndustryRiskContribution.FindRef(EntityId));
		if (const auto* Damage = Fire.DamagedBuildings.Find(EntityId))
		{
			OutMetrics->Add({Damage->Cause == 1   ? TEXT("Earthquake damage")
			                 : Damage->Cause == 2 ? TEXT("Tornado damage")
			                                      : TEXT("Fire damage"),
			                 Damage->Damage, 100});
			if (Entity->DefinitionId != TEXT("Hospital") && !FCityWasteSimulation::IsFacility(Entity->DefinitionId))
				OutDetail += FString::Printf(TEXT("\nDamage cause: %s"), Damage->Cause == 1   ? TEXT("Earthquake")
				                                                         : Damage->Cause == 2 ? TEXT("Tornado")
				                                                                              : TEXT("Fire"));
			if (Damage->Damage == 100)
				OutDetail += TEXT("\nDestroyed: rebuild with REPAIR");
			if (Fire.IsBurning(EntityId))
				OutDetail += FString::Printf(
				    TEXT("\nPrompt response auto repairs damage up to %d%%"),
				    FMath::Clamp(SimulationSubsystem->GetEconomyTuning().FireMinorDamageAutoRepairLimit, 0, 99));
			OutDetail += Fire.IsBurning(EntityId)
			                 ? (Fire.ResponseStations.Contains(EntityId)
			                        ? TEXT("\nEngine on scene: extinguishing")
			                        : (Fire.EnRouteStations.Contains(EntityId)
			                               ? TEXT("\nEngine travelling along roads")
			                               : TEXT("\nBurning: waiting for staffed road/water response")))
			             : Damage->RepairCompleteStep >= 0
			                 ? FString::Printf(TEXT("\nRepair underway: %lld game minutes left"),
			                                   (Damage->RepairCompleteStep - Fire.LastFixedStepId) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth))
			                 : FString::Printf(TEXT("\nREPAIR beside Inspect: $%lld, %d game minutes"),
			                                   SimulationSubsystem->GetRepairCost(EntityId),
			                                   SimulationSubsystem->GetEconomyTuning().FireRepairSteps * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth));
		}
		const auto& Crime = SimulationSubsystem->GetCrimeSnapshot();
		for (int32 I = Fire.RecentIncidents.Num() - 1; I >= 0; --I)
		{
			const auto& Incident = Fire.RecentIncidents[I];
			if (Incident.EntityId == EntityId)
			{
				OutDetail += FString::Printf(TEXT("\nLast ignition: %s"), Incident.SpreadFromEntityId
				                                                              ? TEXT("Spread from nearby building")
				                                                          : Incident.bArson ? TEXT("Arson")
				                                                          : Incident.bTest  ? TEXT("Safety test")
				                                                                            : TEXT("Accidental fire"));
				break;
			}
		}
		for (int32 I = Crime.RecentIncidents.Num() - 1; I >= 0; --I)
		{
			const auto& Incident = Crime.RecentIncidents[I];
			if (Incident.EntityId == EntityId && Incident.bArson)
			{
				OutDetail += FString::Printf(TEXT("\nArsonist #%llu: %s"), Incident.OffenderCitizenId,
				                             FCityCrimeSimulation::OutcomeName(Incident.ArsonOutcome));
				break;
			}
		}
		for (const auto& Incident : Crime.RecentIncidents)
		{
			if (Incident.EntityId != EntityId || Incident.StartStep > Crime.LastFixedStepId ||
			    Incident.EndStep <= Crime.LastFixedStepId)
				continue;
			OutDetail +=
			    FString::Printf(TEXT("\nActive crime: %lld game minutes left"), (Incident.EndStep - Crime.LastFixedStepId) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth));
			const auto* Crew = Population.TransitTrips.FindByPredicate(
			    [&](const auto& Trip)
			    { return Trip.Purpose == ECityTripPurpose::PoliceResponse && Trip.DestinationEntityId == EntityId; });
			OutDetail += Crew ? (Crew->bCompleted ? TEXT("\nPolice on scene") : TEXT("\nPolice travelling along roads"))
			                  : TEXT("\nCity > POLICE: connect roads and fund response");
			break;
		}
		const auto Indicators = FCityConditionIndicators::Build(
		    *PlacementModel, Fire, Crime, SimulationSubsystem->GetEconomyTuning().FireRepairSteps);
		if (const auto* Row =
		        Indicators.FindByPredicate([&](const auto& R) { return R.EntityId == EntityId && R.bRepairing; }))
			OutMetrics->Add({TEXT("Repair progress"), Row->RepairPercent, 100});
		if (Entity->DefinitionId == TEXT("Police"))
		{
			const auto* Level =
			    Definition && Development ? Definition->FindDensityLevel(Development->DensityLevel) : nullptr;
			const int32 JailCapacity =
			    FCityCrimeSimulation::JailCapacity(SimulationSubsystem->GetDevelopmentSnapshot(), EntityId);
			int32 Jailed = 0;
			int64 NextRelease = MAX_int64;
			for (const auto& C : Population.Citizens)
				if (C.JailStationId == EntityId)
				{
					++Jailed;
					NextRelease = FMath::Min(NextRelease, C.JailReleaseStep);
				}
			OutMetrics->Add({TEXT("Jail spaces occupied"), Jailed, JailCapacity});
			OutDetail += FString::Printf(TEXT("\nJail: %d/%d | short reform stay"), Jailed, JailCapacity);
			if (Jailed)
				OutDetail += FString::Printf(TEXT("\nNext release in %lld game minutes"),
				                             FMath::Max(int64(0), NextRelease - Crime.LastFixedStepId) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth));
			if (Development && Development->HasCivicUpgrade())
				OutDetail += TEXT("\nJail expansion available when upgrade completes");
			OutDetail += FString::Printf(TEXT("\nPolice: %d road hops | upkeep $%lld/day\n%s"),
			                             Level ? Level->CivicRoadRange
			                                   : SimulationSubsystem->GetEconomyTuning().PoliceRoadRangeHops,
			                             Level        ? Level->MonthlyUpkeep
			                             : Definition ? Definition->MonthlyUpkeep
			                                          : 0,
			                             *Crime.Describe());
			OutMetrics->Add({TEXT("Protection capacity used"), Crime.StationUsedCapacity.FindRef(EntityId),
			                 (Level ? Level->CivicProtectionCapacity
			                        : SimulationSubsystem->GetEconomyTuning().PoliceProtectionCapacity) *
			                     Services.PublicStaffPercent.FindRef(EntityId) / 100});
			OutMetrics->Add({TEXT("City average crime risk"), Crime.AverageRisk, 100});
			if (SimulationSubsystem->GetEconomySnapshot().HasOperatingUpkeepShortfall())
				OutDetail += TEXT("\nProtection paused: unpaid operating bills");
		}
		else if (Crime.IsIntroduced() && Crime.EntityRisk.Contains(EntityId))
		{
			OutDetail += FString::Printf(TEXT("\nCrime risk %d/100 | protected %d%%"),
			                             Crime.EntityRisk.FindRef(EntityId), Crime.EntityProtection.FindRef(EntityId));
		}
		Capture(5);
		int32 Ill = 0, SevereIll = 0, Unassigned = 0, Waiting = 0, Treating = 0;
		for (const auto& P : SimulationSubsystem->GetHealthSnapshot().Patients)
			if (const auto* Person = SimulationSubsystem->GetPopulationSnapshot().Citizens.FindByPredicate(
			        [&](const auto& C) { return C.Id == P.CitizenId && C.HomeEntityId == EntityId; }))
			{
				++Ill;
				SevereIll += P.Illness == ECityIllness::Severe;
				Unassigned += P.Care == ECityHealthCare::None;
				Waiting += P.AwaitingCare();
				Treating += P.ReceivingCare();
			}
		if (Ill)
		{
			OutDetail += FString::Printf(TEXT("\nIll residents %d | severe %d\nReceiving care %d | awaiting arrival %d | unassigned %d"),
			                             Ill, SevereIll, Treating, Waiting, Unassigned);
			if (Unassigned)
				OutDetail += TEXT("\nCheck hospital road access, treatment places, qualified staff, utilities and funded operating bills.");
			if (Waiting)
				OutDetail += TEXT("\nCare begins on arrival; check road connections and ambulance availability.");
		}
		Capture(3);
	}
	return true;
}

bool ACityBoardActor::PickCellFromWorldRay(const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
                                           FCityCellCoord& OutCell) const
{
	const bool GroundHit =
	    SimulationSubsystem
	        ? CityBoardPicking::PickCellFromWorldRay(GetActorTransform(), SimulationSubsystem->GetTerrain(),
	                                                 WorldRayOrigin, WorldRayDirection, OutCell)
	        : CityBoardPicking::PickCellFromWorldRay(GetActorTransform(), WorldRayOrigin, WorldRayDirection, OutCell);
	if (PlacementModel &&
	    (ToolMode == ECityToolMode::Inspect || ToolMode == ECityToolMode::Demolish || ToolMode == ECityToolMode::Road))
	{
		const FVector Origin = GetActorTransform().InverseTransformPosition(WorldRayOrigin);
		const FVector Direction = GetActorTransform().InverseTransformVector(WorldRayDirection).GetSafeNormal();
		double Best = GroundHit && FMath::Abs(Direction.Z) > .00001
		                  ? (GetCellSurfaceCenter(OutCell).Z - Origin.Z) / Direction.Z
		                  : MAX_dbl;
		bool RoadHit = false;
		for (const auto& Cell : PlacementModel->GetRoadCells())
		{
			if (FMath::Abs(Direction.Z) < .00001)
				break;
			const double Z = GetRoadProfileHeight(Cell.X + .5, Cell.Y + .5, Cell.RoadLevel) + 5;
			const double T = (Z - Origin.Z) / Direction.Z;
			if (T < 0 || T >= Best)
				continue;
			const FVector Hit = Origin + Direction * T;
			if (Hit.X >= Cell.X * 800 && Hit.X < (Cell.X + 1) * 800 && Hit.Y >= Cell.Y * 800 &&
			    Hit.Y < (Cell.Y + 1) * 800)
			{
				Best = T;
				OutCell = Cell;
				RoadHit = true;
			}
		}
		if (RoadHit)
			return true;
	}
	return GroundHit;
}

bool ACityBoardActor::PickCellFromPlayerController(APlayerController* PlayerController, FCityCellCoord& OutCell) const
{
	if (!PlayerController)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	return PlayerController->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection) &&
	       PickCellFromWorldRay(WorldOrigin, WorldDirection, OutCell);
}

void ACityBoardActor::ClearSelectionHighlight()
{
	if (SelectionHighlightCell.IsSet())
	{
		SelectionOutlineInstances->ClearInstances();
		SelectionRetainedInstances->ClearInstances();
	}
	SelectionHighlightCell.Reset();
	SelectionHighlightModel = nullptr;
}

void ACityBoardActor::UpdateSelectionHighlight(const FCityCellCoord& Cell, const bool bVisible)
{
	if (!bVisible || !PlacementModel)
	{
		ClearSelectionHighlight();
		return;
	}
	const uint64 Generation = SimulationSubsystem ? SimulationSubsystem->GetPresentationGeneration() : 0;
	if (SelectionHighlightCell.IsSet() && SelectionHighlightCell.GetValue() == Cell &&
	    SelectionHighlightModel == PlacementModel && SelectionHighlightRevision == PlacementModel->GetRevision() &&
	    SelectionHighlightTool == ToolMode && SelectionHighlightGeneration == Generation &&
	    SelectionHighlightOverlay == DiagnosticOverlay)
		return;
	SelectionHighlightCell = Cell;
	SelectionHighlightModel = PlacementModel;
	SelectionHighlightRevision = PlacementModel->GetRevision();
	SelectionHighlightTool = ToolMode;
	SelectionHighlightGeneration = Generation;
	SelectionHighlightOverlay = DiagnosticOverlay;
	SelectionOutlineInstances->ClearInstances();
	SelectionRetainedInstances->ClearInstances();
	if (!SelectionOutlineMaterial)
	{
		SelectionOutlineMaterial = UMaterialInstanceDynamic::Create(SelectionOutlineInstances->GetMaterial(0), this);
		SelectionRetainedMaterial = UMaterialInstanceDynamic::Create(SelectionRetainedInstances->GetMaterial(0), this);
		SelectionOutlineInstances->SetMaterial(0, SelectionOutlineMaterial);
		SelectionRetainedInstances->SetMaterial(0, SelectionRetainedMaterial);
		SelectionRetainedMaterial->SetVectorParameterValue(TEXT("BarColor"), FLinearColor(.1, .85, 1.));
	}
	const bool Demolish = ToolMode == ECityToolMode::Demolish;
	// Saturated emissive colors keep selection distinct from terrain and neutral roofs.
	SelectionOutlineMaterial->SetVectorParameterValue(TEXT("BarColor"), Demolish ? FLinearColor(3., .22, .01)
	                                                                             : FLinearColor(.025, .22, 3.));
	const auto Plan = CitySelectionHighlight::Build(*PlacementModel, Cell, Demolish);
	const double Lift = DiagnosticOverlay == ECityDiagnosticOverlay::None ? 110. : 310.;
	const auto Line = [](UInstancedStaticMeshComponent* Layer, const FVector& A, const FVector& B)
	{
		const FVector Delta = B - A;
		if (Delta.IsNearlyZero())
			return;
		Layer->AddInstance(FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector, Delta.GetSafeNormal()),
		                              (A + B) * .5, FVector(Delta.Size() / 100., .45, .22)));
	};
	double BuildingFloor = -MAX_dbl;
	if (Plan.EntityId != FCityGrid::EmptyEntityId)
		for (const auto& C : Plan.Cells)
			BuildingFloor = FMath::Max(
			    BuildingFloor, SimulationSubsystem
			                       ? static_cast<double>(SimulationSubsystem->GetTerrain().GetCellMaxHeightCm(C)) + Lift
			                       : GetCellSurfaceCenter(C, Lift).Z);
	const auto Point = [&](const FCityCellCoord& C, const double X, const double Y)
	{
		return FVector(X * 800, Y * 800,
		               BuildingFloor > -MAX_dbl ? BuildingFloor : GetRoadProfileHeight(X, Y, C.RoadLevel) + Lift);
	};
	const TSet<FCityCellCoord> Selected(Plan.Cells);
	for (const auto& C : Plan.Cells)
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const FIntPoint D = Side == 0   ? FIntPoint(0, -1)
			                    : Side == 1 ? FIntPoint(1, 0)
			                    : Side == 2 ? FIntPoint(0, 1)
			                                : FIntPoint(-1, 0);
			if (Selected.Contains({C.X + D.X, C.Y + D.Y, C.RoadLevel}))
				continue;
			for (int32 Part = 0; Part < 4; ++Part)
			{
				const double A = Part * .25, B = (Part + 1) * .25;
				const FVector P = Side == 0   ? Point(C, C.X + A, C.Y)
				                  : Side == 1 ? Point(C, C.X + 1, C.Y + A)
				                  : Side == 2 ? Point(C, C.X + A, C.Y + 1)
				                              : Point(C, C.X, C.Y + A);
				const FVector Q = Side == 0   ? Point(C, C.X + B, C.Y)
				                  : Side == 1 ? Point(C, C.X + 1, C.Y + B)
				                  : Side == 2 ? Point(C, C.X + B, C.Y + 1)
				                              : Point(C, C.X, C.Y + B);
				Line(SelectionOutlineInstances, P, Q);
			}
		}
	for (const auto& C : Plan.RetainedCells)
		for (int32 X = 0; X <= 1; ++X)
			for (int32 Y = 0; Y <= 1; ++Y)
			{
				const FVector P = Point(C, C.X + X, C.Y + Y);
				Line(SelectionRetainedInstances, P, Point(C, C.X + X + (X ? -.18 : .18), C.Y + Y));
				Line(SelectionRetainedInstances, P, Point(C, C.X + X, C.Y + Y + (Y ? -.18 : .18)));
			}
	if (Plan.EntityId != FCityGrid::EmptyEntityId)
	{
		FBox Bounds;
		FTransform MeshTransform;
		if (GetInspectionMesh(Plan.EntityId, Bounds, MeshTransform))
		{
			TArray<FVector> Roof;
			for (const auto& P :
			     {FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z), FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Z),
			      FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z), FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Max.Z)})
				Roof.Add(GetActorTransform().InverseTransformPosition(MeshTransform.TransformPosition(P)) +
				         FVector(0, 0, 40));
			for (int32 I = 0; I < 4; ++I)
				Line(SelectionOutlineInstances, Roof[I], Roof[(I + 1) % 4]);
			if (Demolish)
			{
				Line(SelectionOutlineInstances, Roof[0], Roof[2]);
				Line(SelectionOutlineInstances, Roof[1], Roof[3]);
			}
		}
	}
}

void ACityBoardActor::SetHoveredCell(const FCityCellCoord& Cell, const bool bVisible)
{
	HeatMapPoint = bVisible ? TOptional<FCityCellCoord>(Cell) : TOptional<FCityCellCoord>();
	const bool bValid = bVisible && Cell.X >= 0 && Cell.X < GetDimensions().GetWidth() && Cell.Y >= 0 &&
	                    Cell.Y < GetDimensions().GetHeight();
	if (!TerrainMaterial && BoardSurface->GetMaterial(0))
	{
		TerrainMaterial = UMaterialInstanceDynamic::Create(BoardSurface->GetMaterial(0), this);
		for (int32 Index = 0; Index < FMath::Max(1, BoardSurface->GetNumSections()); ++Index)
			BoardSurface->SetMaterial(Index, TerrainMaterial);
	}
	if (TerrainMaterial)
	{
		const bool bPlacing = ToolMode != ECityToolMode::Inspect && ToolMode != ECityToolMode::Demolish &&
		                      (ToolMode < ECityToolMode::Repair ||
		                       (ToolMode >= ECityToolMode::NeighborhoodPark && ToolMode <= ECityToolMode::SoccerPark));
		const bool bReveal = bValid && bPlacing && !bManipulationActive && !IsPointingAtWindVane();
		TerrainMaterial->SetScalarParameterValue(TEXT("GridReveal"), bReveal ? 1.0f : 0.0f);
		TerrainMaterial->SetScalarParameterValue(
		    TEXT("GridRadius"), ToolMode == ECityToolMode::Zone   ? 6.0f
		                        : ToolMode == ECityToolMode::Road ? 4.0f
		                        : (ToolMode == ECityToolMode::NeighborhoodPark ||
		                           ToolMode == ECityToolMode::BasketballPark || ToolMode == ECityToolMode::SkatePark)
		                            ? 6.0f
		                        : (ToolMode == ECityToolMode::CityPark || ToolMode == ECityToolMode::BaseballPark ||
		                           ToolMode == ECityToolMode::SoccerPark)
		                            ? 8.0f
		                            : 3.0f);
		TerrainMaterial->SetVectorParameterValue(TEXT("GridCenter"), FLinearColor(Cell.X + 0.5f, Cell.Y + 0.5f, 0, 0));
	}
	const bool bContextSelection = ToolMode == ECityToolMode::Inspect || ToolMode == ECityToolMode::Demolish;
	UpdateSelectionHighlight(Cell, bValid && bContextSelection);
	HoverHighlight->SetVisibility(bValid && !bContextSelection);
	if (bValid)
	{
		HoverHighlight->SetRelativeLocation(
		    GetCellSurfaceCenter(Cell, HeatMapResult.Overlay == ECityDiagnosticOverlay::None ? 40.0 : 270.0));
		if (!bLoggedFirstHover)
		{
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_FIRST_HOVER Cell=(%d,%d)"), Cell.X, Cell.Y);
			bLoggedFirstHover = true;
		}
	}
}

const FCityBuildingDefinition* ACityBoardActor::GetSelectedDefinition() const
{
	return BuildingCatalog && BuildingCatalog->Definitions.IsValidIndex(SelectedDefinitionIndex)
	           ? &BuildingCatalog->Definitions[SelectedDefinitionIndex]
	           : nullptr;
}

void ACityBoardActor::SetSelectedFacility(const FName DefinitionId)
{
	if (!BuildingCatalog || !PlacementModel || bManipulationActive)
		return;
	const auto* Definition = BuildingCatalog->FindDefinition(DefinitionId);
	if (!Definition)
		return;
	SetToolMode(FCityBuildingDefinition::IsPowerFacility(DefinitionId)   ? ECityToolMode::Power
	            : FCityBuildingDefinition::IsWaterFacility(DefinitionId) ? ECityToolMode::Service
	            : FCityTechnology::IsFacility(DefinitionId)              ? ECityToolMode::Technology
	                                                                     : ECityToolMode::School);
	for (int32 Index = 0; Index < BuildingCatalog->Definitions.Num(); ++Index)
		if (BuildingCatalog->Definitions[Index].DefinitionId == DefinitionId)
		{
			SelectDefinition(Index);
			break;
		}
}

void ACityBoardActor::SelectDefinition(const int32 DefinitionIndex)
{
	if (!BuildingCatalog || !BuildingCatalog->Definitions.IsValidIndex(DefinitionIndex))
	{
		return;
	}

	if (bRoadToolSelected || DefinitionIndex != SelectedDefinitionIndex)
	{
		SelectedDefinitionIndex = DefinitionIndex;
		bRoadToolSelected = false;
		PlacementModel->CancelRoadPreview(RoadPreview);
		PlacementModel->CancelPreview(PlacementPreview);
		CanceledAtCell.Reset();
	}
}

void ACityBoardActor::UpdatePlacementPreview(APlayerController* PlayerController, const FCityCellCoord& HoveredCell,
                                             const bool bHasHoveredCell, const bool bConfirm, const bool bCancel,
                                             const bool bRotate)
{
	if (!PlayerController || !PlacementModel || !BuildingCatalog || BuildingCatalog->Definitions.IsEmpty())
	{
		PlacementGhost->SetVisibility(false);
		return;
	}

	if (!bCommandInputBound)
	{
		static const FKey SelectionKeys[] = {EKeys::One, EKeys::Two, EKeys::Three};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SelectionKeys) && Index < BuildingCatalog->Definitions.Num();
		     ++Index)
		{
			if (PlayerController->WasInputKeyJustPressed(SelectionKeys[Index]))
			{
				SetToolMode(static_cast<ECityToolMode>(static_cast<uint8>(ECityToolMode::Home) + Index));
				break;
			}
		}
		if (PlayerController->WasInputKeyJustPressed(EKeys::Four))
		{
			SetToolMode(ECityToolMode::Road);
		}
	}

	if (ToolMode == ECityToolMode::Inspect)
	{
		ClearWaterPlacementOverlay();
		PlacementGhost->SetVisibility(false);
		UpdateServiceCoverage(HoveredCell, bHasHoveredCell);
		if (GEngine && bHasHoveredCell && !bCommandInputBound)
		{
			GEngine->AddOnScreenDebugMessage(7001, 0.0f, FColor::Cyan, DescribeCell(HoveredCell));
		}
		return;
	}
	if (ToolMode != ECityToolMode::School)
		ClearServiceCoverage();
	if (bCancel || CanceledAtCell.IsSet())
		ClearWaterPlacementOverlay();
	else
		UpdateWaterPlacementOverlay();
	if (ToolMode == ECityToolMode::Repair ||
	    (ToolMode >= ECityToolMode::TestFire && ToolMode <= ECityToolMode::TestArson) ||
	    ToolMode == ECityToolMode::TestSickness || ToolMode == ECityToolMode::TestSevereSickness ||
	    ToolMode == ECityToolMode::TestWaste)
	{
		PlacementGhost->SetVisibility(false);
		if (bConfirm && bHasHoveredCell && SimulationSubsystem)
		{
			uint64 Target = PlacementModel->GetGrid().GetOccupant(HoveredCell);
			FVector Hit;
			if (bControllerRayActive)
				PickInspectionBuilding(ControllerRayOrigin, ControllerRayDirection, Target, Hit);
			if (ToolMode == ECityToolMode::Repair)
				SimulationSubsystem->RepairBuilding(Target);
			else if (ToolMode == ECityToolMode::TestWaste)
				SimulationSubsystem->StartWasteTest(Target);
			else if (ToolMode == ECityToolMode::TestSickness || ToolMode == ECityToolMode::TestSevereSickness ||
			         ToolMode == ECityToolMode::TestWaste)
				SimulationSubsystem->StartHealthTest(Target, ToolMode == ECityToolMode::TestSevereSickness ||
				                                                 ToolMode == ECityToolMode::TestWaste);
			else
				SimulationSubsystem->StartSafetyTest(Target, ToolMode != ECityToolMode::TestCrime,
				                                     ToolMode != ECityToolMode::TestFire);
		}

		return;
	}
	if (ToolMode == ECityToolMode::Demolish)
	{
		PlacementGhost->SetVisibility(false);
		if (bConfirm && bHasHoveredCell)
		{
			HandleDemolition(HoveredCell);
		}
		return;
	}
	if (ToolMode == ECityToolMode::Zone)
	{
		PlacementGhost->SetVisibility(false);
		UpdateZoneTool(HoveredCell, bHasHoveredCell, bConfirm, bCancel, bRotate);
		return;
	}
	if (bRoadToolSelected)
	{
		UpdateRoadTool(PlayerController, HoveredCell, bHasHoveredCell, bConfirm, bCancel);
		return;
	}

	if (bCancel)
	{
		ClearServiceCoverage();
		PlacementModel->CancelPreview(PlacementPreview);
		CanceledAtCell = bHasHoveredCell ? TOptional<FCityCellCoord>(HoveredCell) : TOptional<FCityCellCoord>();
		PlacementGhost->SetVisibility(false);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(7001, 0.0f, FColor::Silver,
			                                 TEXT("Placement canceled - move to another cell to resume"));
		}
		return;
	}

	if (!bHasHoveredCell)
	{
		ClearServiceCoverage();
		PlacementGhost->SetVisibility(false);
		return;
	}
	if (!bCommandInputBound && PlayerController->WasInputKeyJustPressed(EKeys::Delete))
	{
		HandleDemolition(HoveredCell);
		PlacementModel->CancelPreview(PlacementPreview);
	}

	if (CanceledAtCell.IsSet() && !(CanceledAtCell.GetValue() == HoveredCell))
	{
		CanceledAtCell.Reset();
	}

	if (CanceledAtCell.IsSet())
	{
		PlacementGhost->SetVisibility(false);
		return;
	}

	const FCityBuildingDefinition* Definition = GetSelectedDefinition();
	if (!Definition)
	{
		PlacementGhost->SetVisibility(false);
		return;
	}
	UpdateWaterPlacementOverlay();

	if (!PlacementPreview.bActive || PlacementPreview.DefinitionId != Definition->DefinitionId)
	{
		PlacementPreview = PlacementModel->BeginRoadFacingPreview(*Definition, HoveredCell);
	}
	else
	{
		PlacementModel->UpdateRoadFacingPreviewAnchor(PlacementPreview, *Definition, HoveredCell);
	}

	if (bRotate)
	{
		PlacementModel->RotatePreviewClockwise(PlacementPreview);
	}

	const FCityPlacementResult Validation = PlacementModel->ValidatePlacement(
	    PlacementPreview, *Definition, ECityConstructionFunding::CityTreasury,
	    SimulationSubsystem ? &SimulationSubsystem->GetServiceSnapshot() : nullptr);
	UpdateGhost(*Definition, Validation);
	UpdatePlacementStatus(*Definition, Validation);
	if (ToolMode == ECityToolMode::School)
		UpdateServiceCoverage(HoveredCell, bHasHoveredCell);

	if (!bConfirm)
	{
		return;
	}

	const FCityPlacementResult CommitResult = PlacementModel->CommitPlacement(
	    PlacementPreview, *Definition, ECityConstructionFunding::CityTreasury,
	    SimulationSubsystem ? &SimulationSubsystem->GetServiceSnapshot() : nullptr);
	if (!CommitResult.IsSuccess())
	{
		return;
	}

	const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(CommitResult.EntityId);
	if (Entity)
	{
		RenderPlacement(CommitResult.EntityId, *Entity, *Definition);
		RebuildBuildingEntranceProfiles();
	}
	PlacementGhost->SetVisibility(false);
	UE_LOG(LogTemp, Display,
	       TEXT("CITYSIM_PLACEMENT_COMMITTED Entity=%llu Definition=%s Cell=(%d,%d) Cost=%lld Funds=%lld"),
	       CommitResult.EntityId, *Definition->DefinitionId.ToString(), HoveredCell.X, HoveredCell.Y, Definition->Cost,
	       PlacementModel->GetFunds());
}

void ACityBoardActor::UpdateZoneTool(const FCityCellCoord& HoveredCell, const bool bHasHoveredCell, const bool bConfirm,
                                     const bool bCancel, const bool bRotate)
{
	RoadPreviewInstances->ClearInstances();
	if (bRotate)
	{
		SelectedZone = SelectedZone == ECityZoneType::Residential  ? ECityZoneType::Commercial
		               : SelectedZone == ECityZoneType::Commercial ? ECityZoneType::Industrial
		                                                           : ECityZoneType::Residential;
	}
	if (bCancel)
	{
		ZoneStartCell.Reset();
		ZonePreviewInstances->ClearInstances();
		ZoneSkippedPreviewInstances->ClearInstances();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(7001, 0.0f, FColor::Silver, TEXT("Zone rectangle canceled"));
		}
		return;
	}
	if (!bHasHoveredCell)
	{
		return;
	}

	const FCityCellCoord FirstCorner = ZoneStartCell.IsSet() ? ZoneStartCell.GetValue() : HoveredCell;
	const FCityAreaSelection Selection = CityAreaSelection::MakeInclusiveRectangle(FirstCorner, HoveredCell);
	TArray<FCityCellCoord> PreviewCells;
	CityAreaSelection::BuildCells(Selection, PreviewCells);
	TArray<FCityCellCoord> EligibleCells;
	TArray<FCityCellCoord> SkippedCells;
	EligibleCells.Reserve(PreviewCells.Num());
	SkippedCells.Reserve(PreviewCells.Num());
	for (const FCityCellCoord& Cell : PreviewCells)
	{
		(PlacementModel->CanSetZone(Cell) ? EligibleCells : SkippedCells).Add(Cell);
	}
	const bool bValid = !EligibleCells.IsEmpty();
	UpdateAreaPreview(ZonePreviewInstances, EligibleCells, true, SelectedZone);
	UpdateAreaPreview(ZoneSkippedPreviewInstances, SkippedCells, false, SelectedZone);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		    7001, 0.0f, bValid ? FColor::Cyan : FColor::Red,
		    FString::Printf(
		        TEXT(
		            "Zone: %s  Area %dx%d (%d cells)  Cost $0\nEligible %d | Skipped %d - %s\nTrigger: %s | A/X: switch | B/Y: cancel"),
		        SelectedZone == ECityZoneType::Residential  ? TEXT("Residential")
		        : SelectedZone == ECityZoneType::Commercial ? TEXT("Commercial")
		                                                    : TEXT("Industrial"),
		        Selection.Size.X, Selection.Size.Y, Selection.GetCellCount(), EligibleCells.Num(), SkippedCells.Num(),
		        bValid ? TEXT("blocked cells will be ignored") : TEXT("no zoneable land"),
		        ZoneStartCell.IsSet() ? TEXT("apply rectangle") : TEXT("set first corner")));
	}
	if (!bConfirm)
	{
		return;
	}
	if (!ZoneStartCell.IsSet())
	{
		ZoneStartCell = HoveredCell;
		return;
	}
	if (bValid && PlacementModel->SetZoneArea(Selection.Anchor, Selection.Size, SelectedZone))
	{
		RebuildZoneInstances();
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_ZONE_PAINTED Anchor=(%d,%d) Size=(%d,%d) Eligible=%d Skipped=%d Zone=%d"),
		       Selection.Anchor.X, Selection.Anchor.Y, Selection.Size.X, Selection.Size.Y, EligibleCells.Num(),
		       SkippedCells.Num(), static_cast<int32>(SelectedZone));
		ZoneStartCell.Reset();
		ZonePreviewInstances->ClearInstances();
		ZoneSkippedPreviewInstances->ClearInstances();
	}
}

void ACityBoardActor::SetSelectedZone(const ECityZoneType NewZone)
{
	if (NewZone != ECityZoneType::Residential && NewZone != ECityZoneType::Commercial &&
	    NewZone != ECityZoneType::Industrial)
	{
		return;
	}
	SelectedZone = NewZone;
	ZoneStartCell.Reset();
	ZonePreviewInstances->ClearInstances();
	ZoneSkippedPreviewInstances->ClearInstances();
	SetToolMode(ECityToolMode::Zone);
}

void ACityBoardActor::AdjustRoadElevation(const int32 Delta)
{
	if (ToolMode != ECityToolMode::Road || (Delta != 1 && Delta != -1))
		return;
	if (SelectedRoadProfile == ECityRoadProfile::Ground)
	{
		if (Delta < 0)
			return;
		SelectedRoadElevationStep =
		    RoadPreview.bActive && (RoadPreview.Profile == ECityRoadProfile::Bridge ||
		                            RoadPreview.Start.RoadLevel == 1 || RoadPreview.End.RoadLevel == 1)
		        ? 1
		        : 0;
		SelectedRoadProfile = ECityRoadProfile::Elevated;
	}
	else if (Delta < 0 && SelectedRoadElevationStep == 0)
		SelectedRoadProfile = ECityRoadProfile::Ground;
	else
		SelectedRoadElevationStep =
		    FMath::Clamp(SelectedRoadElevationStep + Delta, 0, FCityPlacementModel::MaximumRoadElevationStep);
	RefreshRoadDrawingProfile();
	RoadConstructionStatus.Empty();
}

void ACityBoardActor::RefreshRoadDrawingProfile()
{
	if (!PlacementModel || !RoadPreview.bActive)
		return;
	RoadPreview.Profile = PlacementModel->ResolveRoadDrawingProfile(RoadPreview.Start, RoadPreview.End,
	                                                                SelectedRoadProfile != ECityRoadProfile::Ground);
	RoadPreview.ElevationStep = SelectedRoadElevationStep;
}

void ACityBoardActor::SetSelectedRoadProfile(const ECityRoadProfile Profile)
{
	if (Profile != ECityRoadProfile::Ground && Profile != ECityRoadProfile::Bridge &&
	    Profile != ECityRoadProfile::Elevated)
		return;
	SelectedRoadProfile = Profile;
	SelectedRoadElevationStep = 0;
	SetToolMode(ECityToolMode::Road);
	if (PlacementModel)
		PlacementModel->CancelRoadPreview(RoadPreview);
	RoadPreviewInstances->ClearInstances();
	RoadConstructionStatus.Empty();
}

void ACityBoardActor::SetSelectedRoadDefinition(const FName DefinitionId)
{
	const FCityRoadDefinition Definition = ResolveRoadDefinition(DefinitionId);
	if (Definition.Id != DefinitionId || !Definition.IsValid())
	{
		return;
	}
	SelectedRoadDefinitionId = DefinitionId;
	SelectedRoadProfile = ECityRoadProfile::Ground;
	SelectedRoadElevationStep = 0;
	RoadConstructionStatus.Empty();
	if (PlacementModel)
	{
		PlacementModel->CancelRoadPreview(RoadPreview);
	}
	RoadPreviewInstances->ClearInstances();
	SetToolMode(ECityToolMode::Road);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_ROAD_CLASS_SELECTED %s"), *DefinitionId.ToString());
}

void ACityBoardActor::UpdateGhost(const FCityBuildingDefinition& Definition, const FCityPlacementResult& Validation)
{
	FCityBuildingDefinition PreviewDefinition = Definition;
	if (CityUtilityEntrances::IsDefinition(Definition.DefinitionId) || CityCivicEntrances::IsDefinition(Definition.DefinitionId))
		ApplyPrivateGroundPresentation(PreviewDefinition);
	UStaticMesh* PresentationMesh = PreviewDefinition.PresentationMesh.LoadSynchronous();
	if (Definition.DefinitionId == TEXT("Home") && Definition.Footprint.Width == 2 &&
	    Definition.Footprint.Height == 2 && GroundPilotResidentialMesh)
		PresentationMesh = GroundPilotResidentialMesh.Get();
	if (SimulationSubsystem)
	{
		const auto& Economy = SimulationSubsystem->GetEconomySnapshot();
		const int32 Month =
		    static_cast<int32>(
		        (FCityCalendar::MonthIndex(Economy.FixedStepId, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth)) %
		        12) +
		    1;
		if (UStaticMesh* ParkMesh = GetSeasonalParkMesh(Definition.DefinitionId, Month))
			PresentationMesh = ParkMesh;
	}
	UStaticMesh* PreviewMesh = PresentationMesh ? PresentationMesh : PlaceholderBuildingMesh.Get();
	if (PlacementGhost->GetStaticMesh() != PreviewMesh)
	{
		PlacementGhost->SetStaticMesh(PreviewMesh);
	}
	PlacementGhost->SetRelativeTransform(
	    MakeBuildingTransform(PlacementPreview.Anchor, PreviewDefinition, PlacementPreview.QuarterTurnsClockwise));
	UMaterialInterface* PreviewMaterial =
	    Validation.IsSuccess() ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get();
	for (int32 MaterialIndex = 0; MaterialIndex < PlacementGhost->GetNumMaterials(); ++MaterialIndex)
	{
		PlacementGhost->SetMaterial(MaterialIndex, PreviewMaterial);
	}
	PlacementGhost->SetVisibility(true);
}

void ACityBoardActor::UpdatePlacementStatus(const FCityBuildingDefinition& Definition,
                                            const FCityPlacementResult& Validation)
{
	bConstructionPriceBlocked = PlacementModel && PlacementModel->GetFunds() < Definition.Cost;
	BuildingConstructionStatus =
	    Definition.DisplayName + TEXT(" | ") + DescribeConstructionCost(Definition.DefinitionId) + TEXT("\n") +
	    (bConstructionPriceBlocked ? TEXT("INSUFFICIENT FUNDS - cannot build")
	                               : *FCityPlacementModel::FailureToString(Validation.Failure));
	if (bConstructionPriceBlocked && Validation.Failure != ECityPlacementFailure::InsufficientFunds &&
	    !Validation.IsSuccess())
		BuildingConstructionStatus += TEXT("\n") + FCityPlacementModel::FailureToString(Validation.Failure);
	if (SimulationSubsystem)
	{
		const FString Range =
		    FCityBuildingDefinition::IsWaterFacility(Definition.DefinitionId) &&
		            !SimulationSubsystem->GetServiceSnapshot().Water.RulesVersion
		        ? FString::Printf(TEXT("Water reach %d road steps under this city's legacy rules; output shared"),
		                          SimulationSubsystem->GetEconomyTuning().WaterRangeRoadHops)
		        : FCityServiceGuidance::Range(Definition, SimulationSubsystem->GetEconomyTuning());
		if (!Range.IsEmpty())
			BuildingConstructionStatus += TEXT("\n") + Range;
	}
	if (SimulationSubsystem &&
	    (!Definition.RequiredOperatingFacilities.IsEmpty() || Definition.bRequiresAlternativePower))
		BuildingConstructionStatus +=
		    TEXT("\n") +
		    Definition.DescribePrerequisites(SimulationSubsystem->GetServiceSnapshot(), BuildingCatalog);
	if (SimulationSubsystem && FCityBuildingDefinition::IsWaterFacility(Definition.DefinitionId))
	{
		const auto& Terrain = SimulationSubsystem->GetTerrain();
		const int32 Yield = Terrain.GetParameters().WaterRulesVersion
		                        ? Terrain.GetGroundwaterYieldPercent(PlacementPreview.Anchor)
		                        : 100;
		BuildingConstructionStatus +=
		    FString::Printf(TEXT("\nGroundwater yield %d%% | %s"), Yield,
		                    Terrain.GetParameters().WaterRulesVersion ? TEXT("pressure follows connected roads")
		                                                              : TEXT("legacy water rules"));
	}
	if (!GEngine)
		return;
	const FIntPoint Size = Definition.Footprint.GetRotatedSize(PlacementPreview.QuarterTurnsClockwise);
	const ECityOperationFailure OperationFailure =
	    SimulationSubsystem
	        ? FCityServiceEvaluator::EvaluateCandidate(*PlacementModel, SimulationSubsystem->GetServiceSnapshot(),
	                                                   SimulationSubsystem->GetEconomyTuning(), Definition.DefinitionId,
	                                                   PlacementPreview.Anchor, Size, BuildingCatalog)
	        : ECityOperationFailure::None;
	const FString Status = FString::Printf(
	    TEXT("%s  %dx%d  Cost $%lld  Treasury $%lld\n%s\n%s\nTrigger: build | Dominant face button: rotate"),
	    *Definition.DisplayName, Size.X, Size.Y, Definition.Cost, PlacementModel->GetFunds(),
	    *FCityPlacementModel::FailureToString(Validation.Failure),
	    *FCityServiceEvaluator::FailureToString(OperationFailure));
	GEngine->AddOnScreenDebugMessage(7001, 0.0f, Validation.IsSuccess() ? FColor::Green : FColor::Red, Status);
}

void ACityBoardActor::UpdateRoadTool(APlayerController* PlayerController, const FCityCellCoord& HoveredCell,
                                     const bool bHasHoveredCell, const bool bConfirm, const bool bCancel)
{
	PlacementGhost->SetVisibility(false);
	ZonePreviewInstances->ClearInstances();
	ZoneSkippedPreviewInstances->ClearInstances();
	// Cancel must clear the gesture and its callout even when pointing away from the board.
	if (bCancel)
	{
		PlacementModel->CancelRoadPreview(RoadPreview);
		SelectedRoadProfile = ECityRoadProfile::Ground;
		SelectedRoadElevationStep = 0;
		RoadPreviewInstances->ClearInstances();
		RoadConstructionStatus.Empty();
		bConstructionPriceBlocked = false;
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(7001, 0.0f, FColor::Silver, TEXT("Road gesture canceled"));
		return;
	}
	if (!bHasHoveredCell)
	{
		return;
	}

	if (!bCommandInputBound && PlayerController->WasInputKeyJustPressed(EKeys::Delete))
	{
		HandleDemolition(HoveredCell);
		PlacementModel->CancelRoadPreview(RoadPreview);
	}

	if (!RoadPreview.bActive)
	{
		const FCityRoadDefinition RoadDefinition = ResolveRoadDefinition(SelectedRoadDefinitionId);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
			    7001, 0.0f, FColor::Silver,
			    FString::Printf(
			        TEXT(
			            "%s  $%lld/cell | +$%lld support | +$%lld bridge  Treasury $%lld\nDraw over roads to upgrade | Trigger: start | B/Y: cancel"),
			        RoadDisplayName(SelectedRoadDefinitionId), RoadDefinition.PlacementCostPerCell,
			        FCityPlacementModel::SupportedGradeSurcharge, FCityPlacementModel::BridgeSurcharge,
			        PlacementModel->GetFunds()));
		}
		if (bConfirm)
		{
			RoadPreview =
			    PlacementModel->BeginRoadPreview(HoveredCell, RoadDefinition.Id, RoadDefinition.PlacementCostPerCell);
			RefreshRoadDrawingProfile();
			TArray<FCityCellCoord> StartCell = {HoveredCell};
			UpdateAreaPreview(RoadPreviewInstances, StartCell, true, ECityZoneType::None);
		}
		return;
	}

	PlacementModel->UpdateRoadPreviewEnd(RoadPreview, HoveredCell);
	RefreshRoadDrawingProfile();

	const FCityRoadResult Validation = PlacementModel->ValidateRoad(RoadPreview);
	bConstructionPriceBlocked = PlacementModel->GetFunds() < Validation.Cost;
	RoadConstructionStatus =
	    FString::Printf(TEXT("%s / %s | Auto +%d m | Deck %.1f m | $%lld\nStick up/down: height | %s"),
	                    RoadDisplayName(SelectedRoadDefinitionId),
	                    RoadPreview.Profile == ECityRoadProfile::Bridge ? TEXT("Bridge") : TEXT("Elevated"),
	                    SelectedRoadElevationStep * 2, Validation.Structure.DeckHeightCm / 100.0, Validation.Cost,
	                    Validation.IsSuccess()        ? TEXT("Trigger to build")
	                    : Validation.Detail.IsEmpty() ? *FCityPlacementModel::FailureToString(Validation.Failure)
	                                                  : *Validation.Detail);

	if (RoadPreview.Profile == ECityRoadProfile::Ground)
		RoadConstructionStatus =
		    FString::Printf(TEXT("%s | $%lld\nStick up: raise road | %s"), *GetToolDisplayName(), Validation.Cost,
		                    Validation.IsSuccess()        ? TEXT("Trigger to build")
		                    : Validation.Detail.IsEmpty() ? *FCityPlacementModel::FailureToString(Validation.Failure)
		                                                  : *Validation.Detail);
	UpdateAreaPreview(RoadPreviewInstances, Validation.PathCells, Validation.IsSuccess(), ECityZoneType::None);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
		    7001, 0.0f, Validation.IsSuccess() ? FColor::Green : FColor::Red,
		    FString::Printf(
		        TEXT(
		            "%s  %d cells | New %d | Change %d | Support %d | Bridge %d\nCost $%lld  Treasury $%lld\n%s  Route: X then Y\nTrigger: commit | B/Y: cancel"),
		        RoadDisplayName(RoadPreview.DefinitionId), Validation.PathCells.Num(), Validation.ChangedCells.Num(),
		        Validation.UpgradedCells.Num(), Validation.SupportedCells.Num(), Validation.BridgeCells.Num(),
		        Validation.Cost, PlacementModel->GetFunds(),
		        *FCityPlacementModel::FailureToString(Validation.Failure)));
	}
	if (!bConfirm)
	{
		return;
	}

	const FCityRoadResult Result = PlacementModel->CommitRoad(RoadPreview);
	if (Result.IsSuccess())
		RoadConstructionStatus = FString::Printf(TEXT("Built %d cells for $%lld | Trigger: start another span"),
		                                         Result.PathCells.Num(), Result.Cost);
	if (Result.IsSuccess())
	{
		SelectedRoadProfile = ECityRoadProfile::Ground;
		SelectedRoadElevationStep = 0;
		RebuildRoadInstances();
		RoadPreviewInstances->ClearInstances();
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_ROAD_COMMITTED Class=%s Cells=%d New=%d Changed=%d Cost=%lld Funds=%lld"),
		       *SelectedRoadDefinitionId.ToString(), Result.PathCells.Num(), Result.ChangedCells.Num(),
		       Result.UpgradedCells.Num(), Result.Cost, PlacementModel->GetFunds());
	}
}

void ACityBoardActor::UpdateAreaPreview(UInstancedStaticMeshComponent* Instances, const TArray<FCityCellCoord>& Cells,
                                        const bool bValid, const ECityZoneType ZoneType)
{
	if (!Instances)
	{
		return;
	}
	Instances->ClearInstances();
	UMaterialInterface* Material = InvalidPreviewMaterial.Get();
	if (bValid)
	{
		Material = ZoneType == ECityZoneType::Residential && ResidentialZoneMaterial ? ResidentialZoneMaterial.Get()
		           : ZoneType == ECityZoneType::Commercial && CommercialZoneMaterial ? CommercialZoneMaterial.Get()
		           : ZoneType == ECityZoneType::Industrial && IndustrialZoneMaterial ? IndustrialZoneMaterial.Get()
		                                                                             : ValidPreviewMaterial.Get();
	}
	Instances->SetMaterial(0, Material);
	const double CellScale = CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm;
	const FVector Scale(CellScale * 0.9, CellScale * 0.9, 0.12);
	const double Height = ZoneType != ECityZoneType::None ? 24.0 : 56.0;
	FCityRoadStructure Candidate;
	FString StructureError;
	const bool bStructurePreview =
	    Instances == RoadPreviewInstances.Get() && PlacementModel &&
	    (RoadPreview.Profile == ECityRoadProfile::Bridge || RoadPreview.Profile == ECityRoadProfile::Elevated) &&
	    PlacementModel->MakeRoadStructure(RoadPreview, Candidate, StructureError);
	for (const FCityCellCoord& Cell : Cells)
	{
		FVector Location = GetCellSurfaceCenter(Cell, Height);
		if (Instances == RoadPreviewInstances.Get() && SimulationSubsystem && PlacementModel)
		{
			const FCityRoadTerrainEvaluation Evaluation = PlacementModel->EvaluateRoadTerrain(Cell);
			const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();

			if (bStructurePreview)
				Location.Z = PlacementModel->SampleStructureHeight(Candidate, Cell.X + .5, Cell.Y + .5) + Height;
			else
				Location.Z = (Evaluation.TerrainClass == ECityRoadTerrainClass::Bridge
				                  ? Terrain.GetWaterSurfaceHeightCm(Cell) + 120.0
				                  : Terrain.GetCellCenterHeightCm(Cell)) +
				             Height;
		}
		Instances->AddInstance(FTransform(FRotator::ZeroRotator, Location, Scale));
	}
}

void ACityBoardActor::ClearGesturePreviews()
{
	ClearWaterPlacementOverlay();
	ClearSelectionHighlight();
	if (ZonePreviewInstances)
	{
		ZonePreviewInstances->ClearInstances();
	}
	if (ZoneSkippedPreviewInstances)
	{
		ZoneSkippedPreviewInstances->ClearInstances();
	}
	if (RoadPreviewInstances)
	{
		RoadPreviewInstances->ClearInstances();
	}
}

void ACityBoardActor::UpdateServiceCoverage(const FCityCellCoord& HoveredCell, const bool bHasHoveredCell)
{
	uint64 EntityId = FCityGrid::EmptyEntityId;
	const FCityPlacedEntity* Entity = nullptr;
	if (PlacementModel && SimulationSubsystem && (bCommandInputBound || bHasHoveredCell))
	{
		EntityId = bCommandInputBound ? InspectionEntityId : PlacementModel->GetGrid().GetOccupant(HoveredCell);
		Entity = PlacementModel->GetEntities().Find(EntityId);
		if (!Entity)
		{
			EntityId = FCityGrid::EmptyEntityId;
		}
	}

	ECityDiagnosticOverlay EffectiveOverlay = DiagnosticOverlay;
	uint64 FocusEntityId = FCityGrid::EmptyEntityId;
	const auto* PreviewDefinition = GetSelectedDefinition();
	const bool bSchoolPreview = ToolMode == ECityToolMode::School && bHasHoveredCell && PlacementPreview.bActive &&
	                            PreviewDefinition &&
	                            FCityBuildingDefinition::IsSchoolFacility(PreviewDefinition->DefinitionId);
	if (bSchoolPreview)
		EffectiveOverlay = ECityDiagnosticOverlay::Education;
	if (Entity && SimulationSubsystem && !bSuppressLinkedOverlay)
	{
		const FCityEconomyTuning& Tuning = SimulationSubsystem->GetEconomyTuning();
		if ((EffectiveOverlay == ECityDiagnosticOverlay::None ||
		     EffectiveOverlay == ECityDiagnosticOverlay::Education) &&
		    FCityBuildingDefinition::IsSchoolFacility(Entity->DefinitionId))
		{
			EffectiveOverlay = ECityDiagnosticOverlay::Education;
			FocusEntityId = EntityId;
		}
		else if ((EffectiveOverlay == ECityDiagnosticOverlay::None ||
		          EffectiveOverlay == ECityDiagnosticOverlay::Service) &&
		         (FCityBuildingDefinition::IsWaterFacility(Entity->DefinitionId) ||
		          Entity->DefinitionId == Tuning.WaterTreatmentDefinitionId))
		{
			EffectiveOverlay = ECityDiagnosticOverlay::Service;
			FocusEntityId = EntityId;
		}
		else if (EffectiveOverlay == ECityDiagnosticOverlay::None)
		{
			EffectiveOverlay = FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(
			    SimulationSubsystem->GetServiceSnapshot().GetFailure(EntityId));
			FocusEntityId = EffectiveOverlay != ECityDiagnosticOverlay::None ? EntityId : FCityGrid::EmptyEntityId;
		}
	}

	const FCityNeedsSnapshot& Needs = SimulationSubsystem->GetNeedsSnapshot();
	uint32 Signature =
	    HashCombine(GetTypeHash(PlacementModel ? PlacementModel->GetRevision() : 0), GetTypeHash(EffectiveOverlay));
	Signature = HashCombine(Signature, GetTypeHash(FocusEntityId));
	if (bSchoolPreview)
	{
		Signature = HashCombine(Signature, GetTypeHash(PlacementPreview.Anchor));
		Signature = HashCombine(Signature, GetTypeHash(PlacementPreview.QuarterTurnsClockwise));
		Signature = HashCombine(Signature, GetTypeHash(PreviewDefinition->DefinitionId));
	}
	Signature = HashCombine(Signature, GetTypeHash(SimulationSubsystem->GetEconomySnapshot().FixedStepId));
	Signature = HashCombine(Signature, GetTypeHash(SimulationSubsystem->GetPresentationGeneration()));
	Signature = HashCombine(Signature, GetTypeHash(Needs.ResidentialDemandBalance));
	Signature = HashCombine(Signature, GetTypeHash(Needs.CommercialDemandBalance));
	Signature = HashCombine(Signature, GetTypeHash(Needs.IndustrialDemandBalance));
	for (const auto Type : {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
	                        ECityDevelopmentDemandType::Industrial})
		for (const auto Density : {ECityDensityLevel::Low, ECityDensityLevel::Medium, ECityDensityLevel::High})
			Signature = HashCombine(Signature, GetTypeHash(Needs.DensityFor(Type).Balance(Density)));
	Signature = HashCombine(Signature, GetTypeHash(Needs.PowerUsage));
	Signature = HashCombine(Signature, GetTypeHash(Needs.ServiceUsage));
	Signature = HashCombine(Signature, GetTypeHash(Needs.ResidentialConstraint));
	Signature = HashCombine(Signature, GetTypeHash(Needs.CommercialConstraint));
	Signature = HashCombine(Signature, GetTypeHash(Needs.IndustrialConstraint));
	Signature = HashCombine(Signature, GetTypeHash(SimulationSubsystem->GetEnvironmentSnapshot().TotalPollution));
	Signature = HashCombine(Signature, GetTypeHash(SimulationSubsystem->GetEnvironmentSnapshot().PeakPollution));
	if (DisplayedDiagnosticOverlay == EffectiveOverlay && DisplayedServiceCoverageEntityId == FocusEntityId &&
	    DisplayedDiagnosticSignature == Signature)
	{
		return;
	}
	if (EffectiveOverlay == ECityDiagnosticOverlay::None || !ServiceCoverageInstances ||
	    !ServiceServedParcelInstances || !ServiceUnservedParcelInstances)
	{
		ClearServiceCoverage();
		return;
	}

	const FCityEconomyTuning& Tuning = SimulationSubsystem->GetEconomyTuning();
	FCityDiagnosticOverlayResult Result;
	FCityDiagnosticOverlayEvaluator::Build(
	    *PlacementModel, SimulationSubsystem->GetServiceSnapshot(), Needs, Tuning,
	    BuildingCatalog ? BuildingCatalog->FindDefinition(Tuning.HomeDefinitionId) : nullptr,
	    BuildingCatalog ? BuildingCatalog->FindDefinition(Tuning.ShopDefinitionId) : nullptr,
	    BuildingCatalog ? BuildingCatalog->FindDefinition(Tuning.IndustrialDefinitionId) : nullptr,
	    SimulationSubsystem->GetEnvironmentSnapshot(), EffectiveOverlay, FocusEntityId, Result,
	    &SimulationSubsystem->GetTrafficSnapshot(), &SimulationSubsystem->GetRoadNetworkSnapshot(),
	    &SimulationSubsystem->GetDevelopmentSnapshot(), &SimulationSubsystem->GetHealthSnapshot(),
	    &SimulationSubsystem->GetPopulationSnapshot(), &SimulationSubsystem->GetWasteSnapshot());
	if (bSchoolPreview)
	{
		TArray<FCityCellCoord> Starts;
		FCityServiceReach Reach;
		PlacementModel->GetAdjacentRoadCells(
		    PlacementPreview.Anchor,
		    PreviewDefinition->Footprint.GetRotatedSize(PlacementPreview.QuarterTurnsClockwise), true, Starts);
		const int32 Range = FMath::Clamp(Tuning.SchoolRoadRangeHops, 1, 64);
		PlacementModel->GetRoadReach(Starts, Range, true, Reach.RoadHopDistances);
		FCityDiagnosticOverlayEvaluator::BuildEducationReach(
		    *PlacementModel, SimulationSubsystem->GetServiceSnapshot(), Reach,
		    PreviewDefinition->DisplayName + TEXT(" proposed reach"), Range, Result);
	}
	uint32 ContentSignature = HashCombine(GetTypeHash(EffectiveOverlay), GetTypeHash(Result.Legend));
	ContentSignature = HashCombine(ContentSignature, GetTypeHash(SimulationSubsystem->GetPresentationGeneration()));
	for (int32 Index = 0; Index < Result.Values.Num(); ++Index)
	{
		ContentSignature = HashCombine(ContentSignature, GetTypeHash(Result.Values[Index]));
		ContentSignature = HashCombine(ContentSignature, GetTypeHash(Result.Status[Index]));
	}
	for (const auto& Cell : Result.RoadCells)
	{
		ContentSignature = HashCombine(ContentSignature, GetTypeHash(Cell));
		if (const auto* Detail = Result.CellDetails.Find(Cell))
			ContentSignature = HashCombine(ContentSignature, GetTypeHash(*Detail));
	}
	HeatMapResult = MoveTemp(Result);
	if (ContentSignature != HeatMapContentSignature)
	{
		RebuildHeatMap();
		HeatMapContentSignature = ContentSignature;
	}
	SetHeatMapLayers(true);
	SetBuildingsFaded(true);
	DisplayedServiceCoverageEntityId = FocusEntityId;
	DisplayedServiceCoverageRevision = PlacementModel->GetRevision();
	DisplayedDiagnosticOverlay = EffectiveOverlay;
	DisplayedDiagnosticSignature = Signature;
}

void ACityBoardActor::ClearServiceCoverage()
{
	SetHeatMapLayers(false);
	HeatMapSurface->ClearAllMeshSections();
	HeatMapResult = FCityDiagnosticOverlayResult();
	HeatMapContentSignature = MAX_uint32;
	SetBuildingsFaded(false);
	if (ServiceCoverageInstances)
	{
		ServiceCoverageInstances->ClearInstances();
	}
	if (ServiceServedParcelInstances)
	{
		ServiceServedParcelInstances->ClearInstances();
	}
	if (ServiceUnservedParcelInstances)
	{
		ServiceUnservedParcelInstances->ClearInstances();
	}
	DisplayedServiceCoverageEntityId = FCityGrid::EmptyEntityId;
	DisplayedServiceCoverageRevision = MAX_uint64;
	DisplayedDiagnosticOverlay = ECityDiagnosticOverlay::None;
	DisplayedDiagnosticSignature = MAX_uint32;
}

void ACityBoardActor::SetBuildingsFaded(const bool bFaded)
{
	if (bBuildingsFaded == bFaded)
	{
		return;
	}
	bBuildingsFaded = bFaded;
	for (const TPair<FName, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : BuildingInstances)
	{
		if (Pair.Value)
		{
			ApplyBuildingFade(*Pair.Value);
		}
	}
}

void ACityBoardActor::ApplyBuildingFade(UInstancedStaticMeshComponent& Instances) const
{
	UStaticMesh* Mesh = Instances.GetStaticMesh();
	const int32 MaterialCount = Mesh ? Mesh->GetStaticMaterials().Num() : 0;
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		UMaterialInterface* Original = Mesh->GetMaterial(MaterialIndex);
		if (!Original)
		{
			Original = PlacedBuildingMaterial.Get();
		}
		UMaterialInterface* DisplayMaterial = Original;
		if (bBuildingsFaded && Original)
		{
			const TObjectPtr<UMaterialInterface>* FadeMaterial = OverlayFadeMaterials.Find(Original->GetFName());
			if (!FadeMaterial || !FadeMaterial->Get())
			{
				FadeMaterial = OverlayFadeMaterials.Find(NAME_None);
			}
			if (FadeMaterial && FadeMaterial->Get())
			{
				DisplayMaterial = FadeMaterial->Get();
			}
		}
		Instances.SetMaterial(MaterialIndex, DisplayMaterial);
	}
}

void ACityBoardActor::HandleDemolition(const FCityCellCoord& HoveredCell)
{
	if (PlacementModel->IsRoad(HoveredCell))
	{
		if (PlacementModel->RemoveRoad(HoveredCell))
		{
			RebuildRoadInstances();
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_ROAD_REMOVED Cell=(%d,%d)"), HoveredCell.X, HoveredCell.Y);
		}
		return;
	}

	const uint64 EntityId = PlacementModel->GetGrid().GetOccupant(HoveredCell);
	if (EntityId == FCityGrid::EmptyEntityId || EntityId == FCityPlacementModel::RoadEntityId)
	{
		return;
	}
	const FCityDemolitionResult Result = PlacementModel->DemolishEntity(EntityId);
	if (Result.IsSuccess())
	{
		RemoveRenderedEntity(EntityId);
		UE_LOG(LogTemp, Display, TEXT("CITYSIM_ENTITY_DEMOLISHED Entity=%llu Refund=%lld Funds=%lld"), EntityId,
		       Result.Refund, PlacementModel->GetFunds());
	}
}

void ACityBoardActor::RenderPlacement(const uint64 EntityId, const FCityPlacedEntity& Entity,
                                      const FCityBuildingDefinition& Definition)
{
	const FCityBuildingDefinition PresentationDefinition = ResolvePresentationDefinition(EntityId, Definition);
	UInstancedStaticMeshComponent* Instances = GetOrCreateInstances(PresentationDefinition);
	if (!Instances)
	{
		return;
	}

	const int32 RenderIndex = Instances->AddInstance(
	    MakeBuildingTransform(Entity.Anchor, PresentationDefinition, Entity.QuarterTurnsClockwise));
	UInstancedStaticMeshComponent* FoundationPool =
	    Definition.DefinitionId == TEXT("Home") ? ResidentialFoundationInstances.Get() : FoundationInstances.Get();
	if (FoundationPool && SimulationSubsystem)
	{
		const bool bGradedSite = Definition.DefinitionId.ToString().Contains(TEXT("Park"));
		const auto Contours=CityUtilityFoundations::Find(PresentationDefinition.PresentationMesh.GetAssetName());
		if(!Contours.IsEmpty())
		{
			const auto Pose=MakeBuildingTransform(Entity.Anchor,PresentationDefinition,Entity.QuarterTurnsClockwise);
			for(const auto& Group:Contours)
				if(auto* ContourPool=UtilityFoundationInstances.FindRef(FName(Group.Mesh)).Get())
					ContourPool->AddInstance(CityUtilityFoundations::MakeTransform(Group,Pose,SimulationSubsystem->GetTerrain()));
		}
		else if (!bGradedSite && !PresentationDefinition.PresentationMesh.IsNull())
		{
			auto Supports = CityBuildingSupports::Find(PresentationDefinition.PresentationMesh.ToSoftObjectPath().GetLongPackageName());
			if (Supports.IsEmpty())
				Supports = CityBuildingSupports::Find(Definition.PresentationMesh.ToSoftObjectPath().GetLongPackageName());
			if (ensureMsgf(!Supports.IsEmpty(), TEXT("Missing structural support metadata for %s"),
			               *PresentationDefinition.PresentationMesh.ToString()))
			{
				TArray<FTransform> Transforms;
				CityBuildingSupports::MakeTransforms(Supports,
				    MakeBuildingTransform(Entity.Anchor, PresentationDefinition, Entity.QuarterTurnsClockwise),
				    SimulationSubsystem->GetTerrain(), Transforms);
				for (const auto& Transform : Transforms) FoundationPool->AddInstance(Transform);
			}
		}
		else
		{
			// Parks/sports retain their authored graded site. An un-authored cube
			// placeholder itself occupies the logical rectangle; it needs that support.
			const FIntPoint Size = Definition.Footprint.GetRotatedSize(Entity.QuarterTurnsClockwise);
			int32 Minimum = MAX_int32;
			const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();
			for (int32 Y = Entity.Anchor.Y * Terrain.SamplesPerCell;
			     Y <= (Entity.Anchor.Y + Size.Y) * Terrain.SamplesPerCell; ++Y)
			{
				for (int32 X = Entity.Anchor.X * Terrain.SamplesPerCell;
				     X <= (Entity.Anchor.X + Size.X) * Terrain.SamplesPerCell; ++X)
				{
					const int32 Height = Terrain.GetSampleHeightCm(X, Y);
					Minimum = FMath::Min(Minimum, Height);
				}
			}
			const double BuildingBaseHeight =
			    GetBuildingBaseHeight(Entity.Anchor, Definition, Entity.QuarterTurnsClockwise);
			const double FoundationTop = BuildingBaseHeight + 20.0;
			const double FoundationBottom = FMath::Min(BuildingBaseHeight - 20.0, static_cast<double>(Minimum) - 20.0);
			const double FoundationHeight = FMath::Max(40.0, FoundationTop - FoundationBottom);
			FoundationPool->AddInstance(
			    FTransform(FRotator::ZeroRotator,
			               FVector((Entity.Anchor.X + Size.X * 0.5) * CityBoardPicking::LogicalCellSizeCm,
			                       (Entity.Anchor.Y + Size.Y * 0.5) * CityBoardPicking::LogicalCellSizeCm,
			                       FoundationBottom + FoundationHeight * 0.5),
			               FVector(Size.X * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm,
			                       Size.Y * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm,
			                       FoundationHeight / PlaneMeshSizeCm)));
		}
	}
	const int32 LookupIndex = InstanceIndex.Add(PresentationDefinition.DefinitionId, EntityId);
	ensureMsgf(RenderIndex == LookupIndex, TEXT("City instance render and entity indices diverged"));
}

void ACityBoardActor::RebuildRoadInstances()
{
	RoadInstances->ClearInstances();
	RoadCornerInstances->ClearInstances();
	RoadTInstances->ClearInstances();
	RoadCrossInstances->ClearInstances();
	RoadSupportInstances->ClearInstances();
	DirtRoadSurfaceInstances->ClearInstances();
	LocalRoadSurfaceInstances->ClearInstances();
	AvenueRoadSurfaceInstances->ClearInstances();
	HighwayRoadSurfaceInstances->ClearInstances();
	RoadMarkingInstances->ClearInstances();
	SidewalkSurface->ClearAllMeshSections();
	RoadDetailSurface->ClearAllMeshSections();
	TArray<FCityCellCoord> Cells;
	Cells.Reserve(PlacementModel->GetRoadCells().Num());
	for (const FCityCellCoord& Cell : PlacementModel->GetRoadCells())
	{
		Cells.Add(Cell);
	}
	Cells.Sort(
	    [](const FCityCellCoord& Left, const FCityCellCoord& Right)
	    {
		    return Left.Y == Right.Y ? (Left.X == Right.X ? Left.RoadLevel < Right.RoadLevel : Left.X < Right.X)
		                             : Left.Y < Right.Y;
	    });
	if (RoadSurfaceProfile && SimulationSubsystem)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		Vertices.Reserve(Cells.Num() * 4);
		Triangles.Reserve(Cells.Num() * 6);
		for (const FCityCellCoord& Cell : Cells)
		{
			constexpr int32 Side = FCityTerrainHeightfield::SamplesPerCell + 1;
			const int32 Base = Vertices.Num();
			for (int32 LocalY = 0; LocalY < Side; ++LocalY)
			{
				for (int32 LocalX = 0; LocalX < Side; ++LocalX)
				{
					const double CellX = Cell.X + static_cast<double>(LocalX) / FCityTerrainHeightfield::SamplesPerCell;
					const double CellY = Cell.Y + static_cast<double>(LocalY) / FCityTerrainHeightfield::SamplesPerCell;
					Vertices.Add(FVector(CellX * CityBoardPicking::LogicalCellSizeCm,
					                     CellY * CityBoardPicking::LogicalCellSizeCm,
					                     GetRoadProfileHeight(CellX, CellY, Cell.RoadLevel) + 22.0 - FCityRoadRibbonMesh::AssemblyDropCm));
					Normals.Add(FVector::UpVector);
					UVs.Add(FVector2D(static_cast<double>(LocalX) / FCityTerrainHeightfield::SamplesPerCell,
					                  static_cast<double>(LocalY) / FCityTerrainHeightfield::SamplesPerCell));
				}
			}
			for (int32 Y = 0; Y < FCityTerrainHeightfield::SamplesPerCell; ++Y)
			{
				for (int32 X = 0; X < FCityTerrainHeightfield::SamplesPerCell; ++X)
				{
					const int32 A = Base + Y * Side + X;
					Triangles.Append({A, A + Side, A + 1, A + 1, A + Side, A + Side + 1});
				}
			}
		}
		RoadSurfaceProfile->ClearAllMeshSections();
		if (!Cells.IsEmpty())
		{
			RoadSurfaceProfile->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
			RoadSurfaceProfile->SetMaterial(0, RoadInstances->GetMaterial(0));
		}
	}

	FCityRoadRibbonMesh GroundRibbons[4];
	for (auto& Ribbon : GroundRibbons)
		Ribbon.Terrain = &SimulationSubsystem->GetTerrain();
	for (const FCityCellCoord& Cell : Cells)
	{
		const uint8 Mask = PlacementModel->GetRoadNeighborMask(Cell);
		const int32 NeighborCount = FMath::CountBits(static_cast<uint64>(Mask));
		UInstancedStaticMeshComponent* Target = RoadInstances;
		double Yaw = (Mask == 2 || Mask == 8 || Mask == 10) ? 90.0 : 0.0;
		if (NeighborCount >= 4)
		{
			Target = RoadCrossInstances;
		}
		else if (NeighborCount == 3)
		{
			Target = RoadTInstances;
			// FBX conversion flips Blender Y, so the imported base T opens south (mask 14).
			Yaw = Mask == 7 ? 90.0 : Mask == 11 ? 180.0 : Mask == 13 ? 270.0 : 0.0;
		}
		else if (NeighborCount == 2 && Mask != 5 && Mask != 10)
		{
			Target = RoadCornerInstances;
			// The imported base corner opens east and south (mask 6) for the same reason.
			Yaw = Mask == 3 ? 90.0 : Mask == 9 ? 180.0 : Mask == 12 ? 270.0 : 0.0;
		}
		const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();
		const FCityRoadTerrainEvaluation RoadTerrain = PlacementModel->EvaluateRoadTerrain(Cell);
		const FVector SurfaceNormal = RoadTerrain.TerrainClass == ECityRoadTerrainClass::Bridge
		                                  ? FVector::UpVector
		                                  : Terrain.GetSurfaceNormal(Cell.X + 0.5, Cell.Y + 0.5);
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
		const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
		const bool bSampledGround =
		    RoadTerrain.TerrainClass == ECityRoadTerrainClass::Ordinary || PlacementModel->FindRoadStructure(Cell);
		// The old rigid road body could protrude through the deformed paving on a crest.
		if (!bSampledGround)
			Target->AddInstance(
			    FTransform(Tilt * YawRotation,
			               FVector((Cell.X + 0.5) * CityBoardPicking::LogicalCellSizeCm,
			                       (Cell.Y + 0.5) * CityBoardPicking::LogicalCellSizeCm,
			                       GetRoadProfileHeight(Cell.X + 0.5, Cell.Y + 0.5, Cell.RoadLevel) + 40.0 - FCityRoadRibbonMesh::AssemblyDropCm),
			               FVector::OneVector));

		const FName DefinitionId = PlacementModel->GetRoadDefinitionId(Cell);
		UInstancedStaticMeshComponent* SurfaceInstances = LocalRoadSurfaceInstances;
		const double WidthCells = CityRoadPresentation::SurfaceWidth(DefinitionId);
		if (DefinitionId == TEXT("DirtRoad"))
		{
			SurfaceInstances = DirtRoadSurfaceInstances;
		}
		else if (DefinitionId == TEXT("Avenue"))
		{
			SurfaceInstances = AvenueRoadSurfaceInstances;
		}
		else if (DefinitionId == TEXT("Highway"))
		{
			SurfaceInstances = HighwayRoadSurfaceInstances;
		}
		const bool bJunctionOrCorner = NeighborCount >= 2 && Mask != 5 && Mask != 10;
		const bool bHorizontal = Mask == 2 || Mask == 8 || Mask == 10;
		const double SurfaceYaw = bHorizontal ? 0.0 : 90.0;
		// Adjacent straight surfaces meet at the cell boundary; inset tiles exposed dark transverse gaps.
		const double LengthCells = bJunctionOrCorner ? WidthCells : 1.0;
		if (bSampledGround)
		{
			const int32 ClassIndex = DefinitionId == TEXT("DirtRoad")  ? 0
			                         : DefinitionId == TEXT("Avenue")  ? 2
			                         : DefinitionId == TEXT("Highway") ? 3
			                                                           : 1;
			const auto Height = [this, &Cell](double X, double Y)
			{ return GetRoadProfileHeight(X, Y, Cell.RoadLevel); };
			if (bJunctionOrCorner)
			{
				TArray<double> Portals = {GetRoadWidthAt(Cell, false, 1), GetRoadWidthAt(Cell, true, 1),
				                          GetRoadWidthAt(Cell, false, 0), GetRoadWidthAt(Cell, true, 0)};
				GroundRibbons[ClassIndex].AppendJunction(Cell, Mask, WidthCells, Portals, Height);
			}
			else
				GroundRibbons[ClassIndex].AppendStraight(Cell, bHorizontal, WidthCells, Height,
				                                         GetRoadWidthAt(Cell, bHorizontal, 0),
				                                         GetRoadWidthAt(Cell, bHorizontal, 1));
		}
		else
			SurfaceInstances->AddInstance(FTransform(
			    Tilt * FQuat(FVector::UpVector, FMath::DegreesToRadians(SurfaceYaw)), GetCellSurfaceCenter(Cell, 0.0),
			    FVector(LengthCells * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm,
			            WidthCells * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm, 8.0 / PlaneMeshSizeCm)));
		if (PlacementModel->HasRoadSupport(Cell))
		{
			const double Top = GetRoadProfileHeight(Cell.X + 0.5, Cell.Y + 0.5, Cell.RoadLevel) + 12.0 - FCityRoadRibbonMesh::AssemblyDropCm;
			const double Bottom = Terrain.GetCellMinHeightCm(Cell) - 20.0;
			const double Height = FMath::Max(40.0, Top - Bottom);
			const double WidthScale = (PlacementModel->FindRoadStructure(Cell, false) ||
			                           RoadTerrain.TerrainClass == ECityRoadTerrainClass::Bridge)
			                              ? 0.18
			                              : 0.72;
			RoadSupportInstances->AddInstance(FTransform(
			    FRotator::ZeroRotator,
			    FVector((Cell.X + 0.5) * CityBoardPicking::LogicalCellSizeCm,
			            (Cell.Y + 0.5) * CityBoardPicking::LogicalCellSizeCm, Bottom + Height * 0.5),
			    FVector(CityBoardPicking::LogicalCellSizeCm * WidthScale / PlaneMeshSizeCm,
			            CityBoardPicking::LogicalCellSizeCm * WidthScale / PlaneMeshSizeCm, Height / PlaneMeshSizeCm)));
		}
	}

	UInstancedStaticMeshComponent* ClassSurfaces[] = {DirtRoadSurfaceInstances, LocalRoadSurfaceInstances,
	                                                  AvenueRoadSurfaceInstances, HighwayRoadSurfaceInstances};
	for (int32 ClassIndex = 0; ClassIndex < 4; ++ClassIndex)
	{
		const auto& Mesh = GroundRibbons[ClassIndex];
		if (Mesh.Vertices.IsEmpty())
			continue;
		RoadSurfaceProfile->CreateMeshSection(ClassIndex + 1, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs,
		                                      TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		RoadSurfaceProfile->SetMaterial(ClassIndex + 1, ClassSurfaces[ClassIndex]->GetMaterial(0));
	}

	// Subdivide edge strips against the same height profile as streets. Junction
	// tiles retain a perimeter path; strips across open approaches are crossings.
	TArray<FVector> WalkVertices, WalkNormals;
	TArray<int32> WalkTriangles;
	TArray<FVector2D> WalkUVs;
	for (const FCityCellCoord& Cell : Cells)
	{
		const FName RoadId = PlacementModel->GetRoadDefinitionId(Cell);
		const double Width = CityRoadPresentation::SidewalkWidth(RoadId);
		if (Width <= 0.0)
			continue;
		const uint8 Mask = PlacementModel->GetRoadNeighborMask(Cell);
		const bool bHorizontal = Mask == 2 || Mask == 8 || Mask == 10;
		const bool bVertical = Mask == 1 || Mask == 4 || Mask == 5;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const bool bAlongX = Side < 2;
            // Close only the unconnected end of a one-neighbor street.
            // Its side strips otherwise leave pedestrian/vehicle approaches
            // sampling fallback heights over an absent public sidewalk.
            const bool bEndCap=(Mask==1 && Side==0) || (Mask==4 && Side==1) ||
                (Mask==2 && Side==2) || (Mask==8 && Side==3);
			if (((bHorizontal && !bAlongX) || (bVertical && bAlongX)) && !bEndCap)
				continue;
			const double Sign = Side % 2 == 0 ? -1.0 : 1.0;
			constexpr int32 Segments = 4;
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const int32 Base = WalkVertices.Num();
				for (int32 Corner = 0; Corner < 4; ++Corner)
				{
					const double Along = (Segment + (Corner == 1 || Corner == 2 ? 1.0 : 0.0)) / Segments;
					const double WalkWidth =
					    bHorizontal || bVertical ? GetRoadWidthAt(Cell, bAlongX, Along, true) : Width;
					const double PavedWidth = bHorizontal || bVertical ? GetRoadWidthAt(Cell, bAlongX, Along)
					                                                   : CityRoadPresentation::SurfaceWidth(RoadId);
					const double WalkOffset = (PavedWidth + WalkWidth) * .5;
					const double Across = 0.5 + Sign * WalkOffset + (Corner >= 2 ? WalkWidth * 0.5 : -WalkWidth * 0.5);
					const double X = Cell.X + (bAlongX ? Along : Across);
					const double Y = Cell.Y + (bAlongX ? Across : Along);
					WalkVertices.Add(FVector(X * CityBoardPicking::LogicalCellSizeCm,
					                         Y * CityBoardPicking::LogicalCellSizeCm,
					                         GetRoadProfileHeight(X, Y, Cell.RoadLevel) + FCityRoadRibbonMesh::SidewalkLiftCm));
					WalkNormals.Add(FVector::UpVector);
					WalkUVs.Add(
					    FVector2D(X, Y)); // Shared board coordinates prevent differing samples at overlapping corners.
				}
                FCityRoadRibbonMesh Patch;
				Patch.Terrain = &SimulationSubsystem->GetTerrain();
				TArray<FVector2D> Polygon;
                for(int32 I=0; I<4; ++I) Polygon.Add(FVector2D(WalkVertices[Base+I].X,WalkVertices[Base+I].Y)/800.);
                Patch.AppendPolygon(Polygon,[&](double X,double Y){return GetRoadProfileHeight(X,Y,Cell.RoadLevel);},FCityRoadRibbonMesh::SidewalkLiftCm);
                WalkVertices.SetNum(Base); WalkNormals.SetNum(Base); WalkUVs.SetNum(Base);
                WalkVertices.Append(Patch.Vertices); WalkNormals.Append(Patch.Normals); WalkUVs.Append(Patch.UVs);
                for(const int32 I:Patch.Triangles) WalkTriangles.Add(Base+I);
			}
		}
	}
	if (!WalkVertices.IsEmpty())
	{
		SidewalkSurface->CreateMeshSection(0, WalkVertices, WalkTriangles, WalkNormals, WalkUVs, TArray<FColor>(),
		                                   TArray<FProcMeshTangent>(), false);
	}

	// Curbs stop at open road approaches, so junctions and crossings remain unobstructed.
	// Highway edge paint stays inside the paved width; no new routing or pedestrian surfaces.
	TArray<FVector> DetailVertices, DetailNormals;
	TArray<int32> DetailTriangles;
	TArray<FVector2D> DetailUVs;
	for (const FCityCellCoord& Cell : Cells)
	{
		const FName RoadId = PlacementModel->GetRoadDefinitionId(Cell);
		if (RoadId == TEXT("DirtRoad"))
			continue;
		const uint8 Mask = PlacementModel->GetRoadNeighborMask(Cell);
		const bool bHighway = RoadId == TEXT("Highway");
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const uint8 Approach = Side == 0 ? 4 : Side == 1 ? 1 : Side == 2 ? 8 : 2;
			if ((Mask & Approach) != 0 ||
			    (Cell == PlacementModel->GetEntranceCell() && Side == (Cell.X == 0                                ? 2
			                                                           : Cell.X == GetDimensions().GetWidth() - 1 ? 3
			                                                           : Cell.Y == 0                              ? 0
			                                                                                                      : 1)))
				continue;
			const bool bAlongX = Side < 2;
			const double Sign = Side % 2 == 0 ? -1.0 : 1.0;
			const double Inner = CityRoadPresentation::SurfaceWidth(RoadId) * 0.5 - (bHighway ? 0.035 : 0.0);
			const double Width = bHighway ? 0.012 : 0.015;
			for (int32 Segment = 0; Segment < 4; ++Segment)
			{
				for (int32 Face = 0; Face < (bHighway ? 1 : 2); ++Face)
				{
					const int32 Base = DetailVertices.Num();
					for (int32 Corner = 0; Corner < 4; ++Corner)
					{
						const bool bJunction =
						    FMath::CountBits(static_cast<uint64>(Mask)) >= 2 && Mask != 5 && Mask != 10;
						const double Half = CityRoadPresentation::SurfaceWidth(RoadId) * .5;
						const double Start = bJunction && !(Mask & (bAlongX ? 8 : 4)) ? .5 - Half : 0.;
						const double End = bJunction && !(Mask & (bAlongX ? 2 : 1)) ? .5 + Half : 1.;
						const double Along =
						    FMath::Lerp(Start, End, (Segment + (Corner == 1 || Corner == 2 ? 1.0 : 0.0)) / 4.0);
						const bool bSampledEdge =
						    PlacementModel->EvaluateRoadTerrain(Cell).TerrainClass == ECityRoadTerrainClass::Ordinary;
						const double Edge =
						    bSampledEdge ? GetRoadWidthAt(Cell, bAlongX, Along) * .5 - (bHighway ? .035 : 0.) : Inner;
						const double Across = 0.5 + Sign * (Edge + (Face == 0 && Corner >= 2 ? Width : 0.0));
						const double X = Cell.X + (bAlongX ? Along : Across), Y = Cell.Y + (bAlongX ? Across : Along);
						const double Height = bHighway ? 11.0 : (Face == 1 && Corner >= 2 ? 9.0 : 24.0);
						DetailVertices.Add(FVector(X * CityBoardPicking::LogicalCellSizeCm,
						                           Y * CityBoardPicking::LogicalCellSizeCm,
						                           GetRoadProfileHeight(X, Y, Cell.RoadLevel) + Height));
						DetailNormals.Add(Face == 0 ? FVector::UpVector
						                            : (bAlongX ? FVector(0, -Sign, 0) : FVector(-Sign, 0, 0)));
						DetailUVs.Add(Face == 0 ? FVector2D(X, Y)
						                        : FVector2D(bAlongX ? X : Y, Corner >= 2 ? 0.25 : 0.0));
					}
					const FVector Cross = FVector::CrossProduct(DetailVertices[Base + 1] - DetailVertices[Base],
					                                            DetailVertices[Base + 2] - DetailVertices[Base]);
					// Unreal procedural front faces use clockwise winding against outward normals.
					if (FVector::DotProduct(Cross, DetailNormals[Base]) < 0)
						DetailTriangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
					else
						DetailTriangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
				}
			}
		}
	}
	if (!DetailVertices.IsEmpty())
		RoadDetailSurface->CreateMeshSection(0, DetailVertices, DetailTriangles, DetailNormals, DetailUVs,
		                                     TArray<FColor>(), TArray<FProcMeshTangent>(), false);

	FCityRoadRibbonMesh CenterPaint;
	CenterPaint.Terrain = &SimulationSubsystem->GetTerrain();
	// Generate center markings from maximal graph edges, not individual cells, so
	// dash phase remains continuous and naturally stops short of each junction.
	FCityRoadNetworkSnapshot RoadNetwork;
	FCityRoadNetworkBuilder::Build(*PlacementModel, FCityRoadDefinition::Local(), RoadNetwork);
	const double CellSize = CityBoardPicking::LogicalCellSizeCm;
	const double DashLength = CellSize * 0.18;
	const double DashPeriod = CellSize * 0.48;
	const double JunctionClearance = CellSize * 0.30;
	for (const FCityRoadEdge& Edge : RoadNetwork.Edges)
	{
		if (Edge.RoadClass == ECityRoadClass::Dirt || Edge.Cells.Num() < 2)
		{
			continue;
		}
		const FVector Start = CityBoardPicking::GetCellCenterLocal(Edge.Cells[0]);
		const FVector End = CityBoardPicking::GetCellCenterLocal(Edge.Cells.Last());
		const FVector2D Direction2D = FVector2D(End.X - Start.X, End.Y - Start.Y).GetSafeNormal();
		const double TotalLength = FVector2D::Distance(FVector2D(Start.X, Start.Y), FVector2D(End.X, End.Y));
		const FVector2D Across(-Direction2D.Y,Direction2D.X);
		for (double Distance = JunctionClearance; Distance + DashLength <= TotalLength - JunctionClearance;
		     Distance += DashPeriod)
		{
			const double CenterDistance = Distance + DashLength * 0.5;
			const FVector Center(
			    Start.X + Direction2D.X * CenterDistance, Start.Y + Direction2D.Y * CenterDistance,
			    GetRoadProfileHeight(
			        (Start.X + Direction2D.X * CenterDistance) / CellSize,
			        (Start.Y + Direction2D.Y * CenterDistance) / CellSize,
			        Edge.Cells.ContainsByPredicate([](const auto& C) { return C.RoadLevel == 1; }) ? 1 : 0) +
			        6.0);
            const FVector2D C(Center.X,Center.Y);
            TArray<FVector2D> Polygon;
            for(const auto& Corner:{FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
                Polygon.Add((C+Direction2D*(Corner.X*DashLength*.5)+Across*(Corner.Y*CellSize*.0275))/CellSize);
            const int32 Level=Edge.Cells.ContainsByPredicate([](const auto& Cell){return Cell.RoadLevel==1;})?1:0;
            CenterPaint.AppendPolygon(Polygon,[&](double X,double Y){return GetRoadProfileHeight(X,Y,Level);},FCityRoadRibbonMesh::SurfaceLiftCm+1.);

		}
	}
    if(!CenterPaint.Vertices.IsEmpty()) {
        RoadDetailSurface->CreateMeshSection(1,CenterPaint.Vertices,CenterPaint.Triangles,CenterPaint.Normals,CenterPaint.UVs,TArray<FColor>(),TArray<FProcMeshTangent>(),false);
        RoadDetailSurface->SetMaterial(1,RoadMarkingInstances->GetMaterial(0));
    }
	RebuildBuildingEntranceProfiles();
}

void ACityBoardActor::UpdateZoneVisibility()
{
	const bool bShowZones = ToolMode == ECityToolMode::Zone;
	for (USceneComponent* Layer :
	     {ResidentialZoneInstances.Get(), CommercialZoneInstances.Get(), IndustrialZoneInstances.Get()})
	{
		// A heat map owns temporary suppression. Update its restoration state
		// without exposing zones when a paint/load rebuild happens underneath it.
		if (bool* RestoreVisibility = HeatMapVisibility.Find(Layer))
		{
			*RestoreVisibility = bShowZones;
			Layer->SetVisibility(false);
		}
		else
			Layer->SetVisibility(bShowZones);
	}
}

void ACityBoardActor::RebuildZoneInstances()
{
	UpdateZoneVisibility();
	ResidentialZoneInstances->ClearInstances();
	CommercialZoneInstances->ClearInstances();
	IndustrialZoneInstances->ClearInstances();
	const double CellSize = CityBoardPicking::LogicalCellSizeCm;
	const FVector Scale(CellSize * 0.46 / PlaneMeshSizeCm, CellSize * 0.46 / PlaneMeshSizeCm, 10.0 / PlaneMeshSizeCm);
	for (int32 Y = 0; Y < GetDimensions().GetHeight(); ++Y)
	{
		for (int32 X = 0; X < GetDimensions().GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			UInstancedStaticMeshComponent* Instances =
			    PlacementModel->GetZone(Cell) == ECityZoneType::Residential  ? ResidentialZoneInstances.Get()
			    : PlacementModel->GetZone(Cell) == ECityZoneType::Commercial ? CommercialZoneInstances.Get()
			    : PlacementModel->GetZone(Cell) == ECityZoneType::Industrial ? IndustrialZoneInstances.Get()
			                                                                 : nullptr;
			if (Instances)
			{
				Instances->AddInstance(FTransform(FRotator::ZeroRotator, GetCellSurfaceCenter(Cell, 5.0), Scale));
			}
		}
	}
}

void ACityBoardActor::RebuildBenchmarkVehicles()
{
	BenchmarkVehicleInstances->ClearInstances();
	if (!SimulationSubsystem || !SimulationSubsystem->IsBenchmarkActive() || !PlacementModel)
	{
		return;
	}

	TArray<FCityCellCoord> RoadCells = PlacementModel->GetRoadCells().Array();
	RoadCells.Sort(
	    [](const FCityCellCoord& Left, const FCityCellCoord& Right)
	    {
		    return Left.Y == Right.Y ? (Left.X == Right.X ? Left.RoadLevel < Right.RoadLevel : Left.X < Right.X)
		                             : Left.Y < Right.Y;
	    });
	const int32 VehicleCount = FMath::Min(SimulationSubsystem->GetBenchmarkResult().PlacedVehicles, RoadCells.Num());
	for (int32 Index = 0; Index < VehicleCount; ++Index)
	{
		const FCityCellCoord& Cell = RoadCells[Index];
		const uint8 Mask = PlacementModel->GetRoadNeighborMask(Cell);
		const bool bVertical = (Mask & 5) != 0 && (Mask & 10) == 0;
		BenchmarkVehicleInstances->AddInstance(FTransform(FRotator(0.0, bVertical ? 90.0 : 0.0, 0.0),
		                                                  GetCellSurfaceCenter(Cell, 110.0), FVector(3.0, 1.4, 0.8)));
	}
}

void ACityBoardActor::UpdatePopulationTraffic()
{
	TArray<FTransform> Pedestrians, Cars, Trucks, PoliceCars, FireEngines, Ambulances, Garbage, Recycling;
	TArray<FTransform> OfficePedestrians, WorkerPedestrians;
	TArray<FCityStreetWalkerSample> StreetWalkers;
	if (SimulationSubsystem && PlacementModel)
	{
		const FCityTrafficSnapshot& Traffic = SimulationSubsystem->GetTrafficSnapshot();
		// Cargo trucks retain their owning facility through pickups and return trips.
		// Trip endpoints alone cannot distinguish a recycling truck between stops.
		TSet<uint64> RecyclingTrips;
		for (const auto& Truck : SimulationSubsystem->GetWasteSnapshot().Trucks)
		{
			const auto* Depot = PlacementModel->GetEntities().Find(Truck.DepotId);
			if (Truck.TripId && Depot && Depot->DefinitionId == TEXT("Recycling"))
				RecyclingTrips.Add(Truck.TripId);
		}
		const FCityTrafficSnapshot& Previous = SimulationSubsystem->GetPreviousTrafficSnapshot();
		const FCityRoadNetworkSnapshot& Network = SimulationSubsystem->GetRoadNetworkSnapshot();
		const bool bValidNetwork = Traffic.Vehicles.IsEmpty() ||
		                           Traffic.NetworkFingerprint == FCityTrafficSimulation::GetNetworkFingerprint(Network);
		const double Alpha = SimulationSubsystem->GetSimulationStepProgress();
		TMap<uint64, const FCityTrafficVehicleState*> PreviousVehicles;
		if (Previous.NetworkFingerprint == Traffic.NetworkFingerprint)
			for (const FCityTrafficVehicleState& Vehicle : Previous.Vehicles)
				PreviousVehicles.Add(Vehicle.Id, &Vehicle);
		int32 VisibleCount = 0;
		TArray<const FCityTrafficVehicleState*> VisibleVehicles;
		for (const auto& V : Traffic.Vehicles)
			VisibleVehicles.Add(&V);
		VisibleVehicles.StableSort(
		    [](const auto& A, const auto& B)
		    {
			    return (A.VehicleType >= ECityTripVehicleType::PoliceCar) >
			           (B.VehicleType >= ECityTripVehicleType::PoliceCar);
		    });
		for (const auto* Record : VisibleVehicles)
		{
			const auto& Vehicle = *Record;
			const int32 VisibleLimit = SimulationSubsystem->IsBenchmarkActive()
			                               ? SimulationSubsystem->GetBenchmarkResult().Config.VisibleTraffic
			                               : 64;
			if (!bValidNetwork || VisibleCount >= VisibleLimit)
				break;
			if (Vehicle.SourceId != 0 &&
			    ((Vehicle.DestinationEntityId != 0 &&
			      !PlacementModel->GetEntities().Contains(Vehicle.DestinationEntityId)) ||
			     (Vehicle.OriginEntityId != 0 && !PlacementModel->GetEntities().Contains(Vehicle.OriginEntityId))))
				continue;
			const FCityTrafficVehicleState* Before = PreviousVehicles.FindRef(Vehicle.Id);
			if (Vehicle.bCompleted && Vehicle.VehicleType < ECityTripVehicleType::PoliceCar &&
			    (!Before || Before->bCompleted))
				continue;
			FVector GridPosition, Direction;
			if (!FCityTrafficSimulation::SamplePosition(Vehicle, Before, Network, Alpha, GridPosition, Direction))
				continue;
			const bool bWalk = Vehicle.VehicleType == ECityTripVehicleType::Walk;
			const bool bTruck = Vehicle.VehicleType == ECityTripVehicleType::FreightTruck ||
			                    Vehicle.VehicleType == ECityTripVehicleType::GarbageTruck;
			const FCityCellCoord NearCell(FMath::RoundToInt(GridPosition.X), FMath::RoundToInt(GridPosition.Y),
			                              FMath::RoundToInt(GridPosition.Z));
			const uint8 NearMask = PlacementModel->GetRoadNeighborMask(NearCell);
			const bool bStraightWalk = NearMask == 0 || NearMask == 1 || NearMask == 2 || NearMask == 4 ||
			                           NearMask == 8 || NearMask == 5 || NearMask == 10;
			const bool bAlongX = NearMask == 2 || NearMask == 8 || NearMask == 10;
			const double Along = bAlongX ? GridPosition.X + .5 - NearCell.X : GridPosition.Y + .5 - NearCell.Y;
			const double WalkOffset =
			    bWalk && bStraightWalk
			        ? (GetRoadWidthAt(NearCell, bAlongX, Along) + GetRoadWidthAt(NearCell, bAlongX, Along, true)) * .5
			        : CityRoadPresentation::SidewalkOffset(PlacementModel->GetRoadDefinitionId(NearCell));
			const double LaneOffset =
			    bWalk ? WalkOffset * CityBoardPicking::LogicalCellSizeCm +
			              FCityTrafficSimulation::SamplePedestrianOffsetCm(Vehicle, Before, Alpha,
			                  CityRoadPresentation::SidewalkWidth(PlacementModel->GetRoadDefinitionId(NearCell)) * CityBoardPicking::LogicalCellSizeCm)
			          : FCityTrafficSimulation::SampleMotorLaneOffsetCm(Vehicle, Before, Alpha,
			                                                            PlacementModel->GetRoadDefinitionId(NearCell));
			FVector Location(
			    (GridPosition.X + 0.5) * CityBoardPicking::LogicalCellSizeCm,
			    (GridPosition.Y + 0.5) * CityBoardPicking::LogicalCellSizeCm,
			    GetRoadProfileHeight(GridPosition.X + 0.5, GridPosition.Y + 0.5, GridPosition.Z > 0 ? 1 : 0) +
			        (bWalk ? 30.0 : 50.0));
			Location += FVector(-Direction.Y, Direction.X, 0.0) * LaneOffset;
            if (!bWalk) Location.Z=GetRoadProfileHeight(Location.X/800.,Location.Y/800.,GridPosition.Z>0?1:0)+FCityRoadRibbonMesh::SurfaceLiftCm+46.;
			if (bWalk)
				Location.Z =
				    GetRoadProfileHeight(Location.X / CityBoardPicking::LogicalCellSizeCm,
				                         Location.Y / CityBoardPicking::LogicalCellSizeCm, GridPosition.Z > 0 ? 1 : 0) +
				    CityCitizenPresentation::StreetOriginOffsetCm;
			if (!bWalk)
				Direction = FCityTrafficSimulation::SampleMotorHeading(Vehicle, Before, Alpha,
				    PlacementModel->GetRoadDefinitionId(NearCell), Direction,
				    Vehicle.SpeedUnitsPerStep * CityBoardPicking::LogicalCellSizeCm / FCityTrafficSimulation::TrafficUnitsPerCell);
			const int32 CitizenVariant = CityCitizenPresentation::VariantIndex(
			    Vehicle.SourceId ? Vehicle.SourceId : Vehicle.Id, bRefinedCitizens);
			if(bWalk)
			{
				FCityStreetWalkerSample Sample;
				Sample.Id=Vehicle.Id; Sample.Identity=Vehicle.SourceId ? Vehicle.SourceId : Vehicle.Id;
				Sample.Trip=HashCombineFast(GetTypeHash(Vehicle.OriginEntityId),GetTypeHash(Vehicle.DestinationEntityId)) ^ (Vehicle.bReturning ? 0x80000000u : 0);
				Sample.Variant=CitizenVariant;
				Sample.Transform=FTransform(FRotator(0,Direction.Rotation().Yaw,0),Location,FVector(CityCitizenPresentation::StreetUniformScale));
				StreetWalkers.Add(Sample); ++VisibleCount; continue;
			}
			TArray<FTransform>& Transforms =
			    Vehicle.VehicleType == ECityTripVehicleType::GarbageTruck
			        ? (Vehicle.bTransitTrip && RecyclingTrips.Contains(Vehicle.SourceId) ? Recycling : Garbage)
			    : Vehicle.VehicleType == ECityTripVehicleType::Ambulance  ? Ambulances
			    : Vehicle.VehicleType == ECityTripVehicleType::PoliceCar  ? PoliceCars
			    : Vehicle.VehicleType == ECityTripVehicleType::FireEngine ? FireEngines
			    : bWalk                                                   ? (CitizenVariant == 1   ? OfficePedestrians
			                                                                 : CitizenVariant == 2 ? WorkerPedestrians
			                                                                                       : Pedestrians)
			    : bTruck                                                  ? Trucks
			                                                              : Cars;
			Transforms.Add(FTransform(FRotator(0, Direction.Rotation().Yaw, 0), Location,
			                          FVector(bWalk    ? CityCitizenPresentation::StreetUniformScale
			                                  : bTruck ? .72
			                                           : .76)));
			++VisibleCount;
		}
	}
	RenderStreetWalkers(StreetWalkers,Pedestrians,OfficePedestrians,WorkerPedestrians);
	// Reuse the bounded instance pools. Rendering never changes traffic state.
	auto UpdatePool = [](UInstancedStaticMeshComponent* Pool, const TArray<FTransform>& Transforms)
	{
		while (Pool->GetInstanceCount() > Transforms.Num())
			Pool->RemoveInstance(Pool->GetInstanceCount() - 1);
		for (int32 Index = 0; Index < Transforms.Num(); ++Index)
		{
			if (Index < Pool->GetInstanceCount())
				Pool->UpdateInstanceTransform(Index, Transforms[Index], false, false, true);
			else
				Pool->AddInstance(Transforms[Index]);
		}
		if (!Transforms.IsEmpty())
			Pool->MarkRenderStateDirty();
	};
	UpdatePool(CitizenInstances, Pedestrians);
	UpdatePool(OfficeCitizenInstances, OfficePedestrians);
	UpdatePool(WorkerCitizenInstances, WorkerPedestrians);
	UpdatePool(TrafficVehicleInstances, Cars);
	UpdatePool(TrafficTruckInstances, Trucks);
	UpdatePool(GarbageTruckInstances, Garbage);
	UpdatePool(RecyclingTruckInstances, Recycling);
	UpdatePool(PoliceResponseInstances, PoliceCars);
	UpdatePool(FireResponseInstances, FireEngines);
	UpdatePool(AmbulanceResponseInstances, Ambulances);
}

void ACityBoardActor::UpdateParkActivity(const float DeltaSeconds)
{
	TArray<FCityParkActivitySample> Samples;
	if (SimulationSubsystem && PlacementModel && BuildingCatalog)
	{
		if (ParkActivityGeneration != SimulationSubsystem->GetPresentationGeneration())
		{
			ParkActivityGeneration = SimulationSubsystem->GetPresentationGeneration();
			ParkActivitySeconds = 0;
		}
		if ((!SimulationSubsystem->IsSimulationPaused() || bParkBenchmarkMotion) && FMath::IsFinite(DeltaSeconds) &&
		    DeltaSeconds > 0)
			ParkActivitySeconds +=
			    DeltaSeconds * (bParkBenchmarkMotion ? 1 : SimulationSubsystem->GetSimulationSpeed());
		float Daylight = 1.0f;
		for (TActorIterator<ACitySandboxEnvironment> It(GetWorld()); It; ++It)
		{
			Daylight = It->GetParkDaylightFactor();
			break;
		}
		if (DiagnosticOverlay == ECityDiagnosticOverlay::None)
			FCityParkActivity::Build(*PlacementModel, SimulationSubsystem->GetServiceSnapshot(), BuildingCatalog,
			                         ParkActivitySeconds, Daylight, Samples);
	}
	TArray<FTransform> Visitors, Basketballs, FieldBalls, Skateboards, Bats, Baseballs;
	TArray<FTransform> OfficeVisitors, WorkerVisitors;
	TArray<FTransform> SkateVisitors[3];
	const int32 AnimationCap =
	    PLATFORM_ANDROID ? FCityParkMotion::QuestAnimationCap : FCityParkMotion::DesktopAnimationCap;
	const FVector Eye = GetWorld()->GetFirstPlayerController()
	                        ? GetWorld()->GetFirstPlayerController()->GetFocalLocation()
	                        : GetActorLocation();
	TArray<TPair<double, int32>> Nearest;
	for (int32 I = 0; I < Samples.Num(); ++I)
		if (Samples[I].Prop == ECityParkActivityProp::Visitor && !Samples[I].Scale.IsNearlyZero())
		{
			const auto* Entity = PlacementModel->GetEntities().Find(Samples[I].EntityId);
			const auto* Definition = Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
			if (Definition)
			{
				const FTransform Local =
				    FCityParkActivity::ToImportedMeshSpace(Samples[I]) *
				    MakeBuildingTransform(Entity->Anchor, *Definition, Entity->QuarterTurnsClockwise);
				Nearest.Emplace(FVector::DistSquared(
				                    Eye, BoardRoot->GetComponentTransform().TransformPosition(Local.GetLocation())),
				                I);
			}
		}
	Nearest.StableSort([](const auto& A, const auto& B) { return A.Key < B.Key; });
	TSet<int32> AnimatedSamples;
	if (bParkSkeletalAnimation)
		for (int32 I = 0; I < FMath::Min(AnimationCap, Nearest.Num()); ++I)
			AnimatedSamples.Add(Nearest[I].Value);
	ParkPoseAccumulator += FMath::Max(0.f, DeltaSeconds);
	const bool UpdatePose = ParkPoseAccumulator >= 1.0 / (PLATFORM_ANDROID ? 15 : 30);
	if (UpdatePose)
		ParkPoseAccumulator = 0;
	int32 AnimatedCount = 0;
	TMap<uint64, uint64> VisitorOrdinals;
	for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
	{
		const auto& Sample = Samples[SampleIndex];
		const auto* Entity = PlacementModel->GetEntities().Find(Sample.EntityId);
		const auto* Definition = Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
		if (!Definition)
			continue;
		const FTransform Transform = FCityParkActivity::ToImportedMeshSpace(Sample) *
		                             MakeBuildingTransform(Entity->Anchor, *Definition, Entity->QuarterTurnsClockwise);
		auto& VisitorOrdinal = VisitorOrdinals.FindOrAdd(Sample.EntityId);
		const int32 VisitorVariant = CityCitizenPresentation::VariantIndex(Sample.EntityId + VisitorOrdinal, true);
		if (Sample.Prop == ECityParkActivityProp::Visitor)
		{
			++VisitorOrdinal;
			const bool Skate = FCityParkMotion::IsSkate(Sample.Motion);
			const int32 MeshIndex = (Skate ? 3 : 0) + VisitorVariant;
			const int32 ClipIndex = static_cast<int32>(Sample.Motion) - 1;
			if (AnimatedSamples.Contains(SampleIndex) && ClipIndex >= 0 && ClipIndex < FCityParkMotion::ClipCount &&
			    ParkCitizenSkeletonMeshes[MeshIndex] && ParkCitizenAnimations[ClipIndex])
			{
				if (ParkAnimatedVisitors.Num() <= AnimatedCount)
				{
					auto* Component = NewObject<USkeletalMeshComponent>(
					    this, *FString::Printf(TEXT("ParkAnimatedVisitor%d"), AnimatedCount));
					Component->SetupAttachment(BoardRoot);
					Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Component->SetCastShadow(false);
					Component->PrimaryComponentTick.bCanEverTick = false;
					Component->bEnableUpdateRateOptimizations = false;
					Component->VisibilityBasedAnimTickOption =
					    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					AddInstanceComponent(Component);
					Component->RegisterComponent();
					ParkAnimatedVisitors.Add(Component);
					ParkAssignedAnimations.Add(-1);
					ParkAssignedTimes.Add(-1);
				}
				auto* Component = ParkAnimatedVisitors[AnimatedCount].Get();
				const bool NewMesh = Component->GetSkeletalMeshAsset() != ParkCitizenSkeletonMeshes[MeshIndex];
				const bool NewClip = ParkAssignedAnimations[AnimatedCount] != ClipIndex;
				if (NewMesh)
					Component->SetSkeletalMesh(ParkCitizenSkeletonMeshes[MeshIndex]);
				if (NewMesh || NewClip)
				{
					Component->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					Component->SetAnimation(ParkCitizenAnimations[ClipIndex]);
					ParkAssignedAnimations[AnimatedCount] = ClipIndex;
				}
				Component->SetForcedLOD(PLATFORM_ANDROID ? 2 : 0);
				Component->SetRelativeTransform(Transform);
				Component->SetVisibility(true);
				// One board clock drives the bounded components. Paused poses do no work.
				if (NewMesh || NewClip ||
				    (UpdatePose &&
				     !FMath::IsNearlyEqual(ParkAssignedTimes[AnimatedCount], Sample.AnimationSeconds, 1.e-5f)))
				{
					Component->SetPosition(Sample.AnimationSeconds, false);
					Component->TickAnimation(0, false);
					Component->RefreshBoneTransforms();
					Component->UpdateBounds();
					ParkAssignedTimes[AnimatedCount] = Sample.AnimationSeconds;
				}
				++AnimatedCount;
				continue;
			}
			if (Skate)
			{
				SkateVisitors[VisitorVariant].Add(Transform);
				continue;
			}
		}
		auto& Pool = Sample.Prop == ECityParkActivityProp::Visitor      ? (VisitorVariant == 1   ? OfficeVisitors
		                                                                   : VisitorVariant == 2 ? WorkerVisitors
		                                                                                         : Visitors)
		             : Sample.Prop == ECityParkActivityProp::Basketball ? Basketballs
		             : Sample.Prop == ECityParkActivityProp::FieldBall  ? FieldBalls
		             : Sample.Prop == ECityParkActivityProp::Skateboard ? Skateboards
		             : Sample.Prop == ECityParkActivityProp::Baseball   ? Baseballs
		                                                                : Bats;
		Pool.Add(Transform);
	}
	auto UpdatePool = [](UInstancedStaticMeshComponent* Pool, const TArray<FTransform>& Transforms)
	{
		while (Pool->GetInstanceCount() > Transforms.Num())
			Pool->RemoveInstance(Pool->GetInstanceCount() - 1);
		for (int32 I = 0; I < Transforms.Num(); ++I)
		{
			if (I < Pool->GetInstanceCount())
				Pool->UpdateInstanceTransform(I, Transforms[I], false, false, true);
			else
				Pool->AddInstance(Transforms[I]);
		}
		if (!Transforms.IsEmpty())
			Pool->MarkRenderStateDirty();
	};
	UpdatePool(ParkVisitorInstances, Visitors);
	UpdatePool(ParkOfficeVisitorInstances, OfficeVisitors);
	UpdatePool(ParkWorkerVisitorInstances, WorkerVisitors);
	UpdatePool(ParkBasketballInstances, Basketballs);
	UpdatePool(ParkFieldBallInstances, FieldBalls);
	UpdatePool(ParkSkateboardInstances, Skateboards);
	UpdatePool(ParkBatInstances, Bats);
	UpdatePool(ParkBaseballInstances, Baseballs);
	for (int32 I = 0; I < ParkSkateVisitorPools.Num(); ++I)
		UpdatePool(ParkSkateVisitorPools[I], SkateVisitors[I]);
	for (int32 I = AnimatedCount; I < ParkAnimatedVisitors.Num(); ++I)
		ParkAnimatedVisitors[I]->SetVisibility(false);
}

void ACityBoardActor::RenderPendingGrowth()
{
	if (!SimulationSubsystem || !BuildingCatalog)
	{
		return;
	}
	TArray<uint64> SpawnedEntityIds;
	SimulationSubsystem->ConsumeSpawnedEntityIds(SpawnedEntityIds);
	for (const uint64 EntityId : SpawnedEntityIds)
	{
		const FCityPlacedEntity* Entity = PlacementModel->GetEntities().Find(EntityId);
		const FCityBuildingDefinition* Definition =
		    Entity ? BuildingCatalog->FindDefinition(Entity->DefinitionId) : nullptr;
		if (Entity && Definition)
		{
			RenderPlacement(EntityId, *Entity, *Definition);
		}
	}
	if (!SpawnedEntityIds.IsEmpty())
	{
		RebuildBuildingEntranceProfiles();
		RebuildLiveConstructionAnimations();
	}
}

void ACityBoardActor::RefreshTerrainPresentation()
{
	if (!SimulationSubsystem)
		return;
	const auto& Parameters = SimulationSubsystem->GetTerrain().GetParameters();
	const uint64 Session = SimulationSubsystem->GetCitySessionGeneration();
	if (RenderedTerrainParameters.IsSet() && RenderedTerrainParameters.GetValue() == Parameters &&
	    RenderedTerrainSession == Session)
		return;
	RebuildTerrainSurface();
	RebuildWaterSurface();
	RenderedTerrainParameters = Parameters;
	RenderedTerrainSession = Session;
}

void ACityBoardActor::RebuildAllPresentation()
{
	ClearSeasonalDecorations();
	RefreshTerrainPresentation();
	EntranceMarker->SetRelativeLocation(GetCellSurfaceCenter(PlacementModel->GetEntranceCell(), 15.0));
	PlacementPreview = FCityPlacementPreview();
	RoadPreview = FCityRoadPreview();
	RoadConstructionStatus.Empty();
	ZoneStartCell.Reset();
	ClearGesturePreviews();
	ClearServiceCoverage();
	CanceledAtCell.Reset();
	PlacementGhost->SetVisibility(false);
	for (TPair<FName, TObjectPtr<UInstancedStaticMeshComponent>>& Pair : BuildingInstances)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
	InstanceIndex.Reset();
	FoundationInstances->ClearInstances();
	ResidentialFoundationInstances->ClearInstances();
	for(const auto& Pair:UtilityFoundationInstances) if(Pair.Value) Pair.Value->ClearInstances();
	RebuildRoadInstances();
	RebuildBenchmarkVehicles();
	RebuildZoneInstances();
	if (!PlacementModel || !BuildingCatalog)
	{
		return;
	}
	TArray<uint64> EntityIds;
	PlacementModel->GetEntities().GetKeys(EntityIds);
	EntityIds.Sort();
	for (const uint64 EntityId : EntityIds)
	{
		const FCityPlacedEntity& Entity = PlacementModel->GetEntities().FindChecked(EntityId);
		if (const FCityBuildingDefinition* Definition = BuildingCatalog->FindDefinition(Entity.DefinitionId))
		{
			RenderPlacement(EntityId, Entity, *Definition);
		}
	}
	RebuildBuildingEntranceProfiles();
	RebuildLiveConstructionAnimations();
}

void ACityBoardActor::RemoveRenderedEntity(const uint64 EntityId)
{
	const FCityInstanceHandle* Handle = InstanceIndex.Find(EntityId);
	if (!Handle)
	{
		return;
	}
	const FCityInstanceHandle HandleCopy = *Handle;
	TObjectPtr<UInstancedStaticMeshComponent>* Instances = BuildingInstances.Find(HandleCopy.DefinitionId);
	if (Instances && Instances->Get()->RemoveInstance(HandleCopy.InstanceIndex))
	{
		ensureMsgf(InstanceIndex.Remove(EntityId), TEXT("Failed to repair entity-to-instance lookup after demolition"));
	}
	// Foundations are inexpensive and do not share the per-definition instance index.
	// Rebuild all presentation so demolition cannot leave a stale pad behind.
	RebuildAllPresentation();
}

UInstancedStaticMeshComponent* ACityBoardActor::GetOrCreateInstances(const FCityBuildingDefinition& Definition)
{
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = BuildingInstances.Find(Definition.DefinitionId))
	{
		return Existing->Get();
	}

	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
	Instances->SetupAttachment(BoardRoot);
	UStaticMesh* PresentationMesh = Definition.PresentationMesh.LoadSynchronous();
	if (SimulationSubsystem)
	{
		const auto& Economy = SimulationSubsystem->GetEconomySnapshot();
		const int32 Month =
		    static_cast<int32>(
		        (FCityCalendar::MonthIndex(Economy.FixedStepId, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth)) %
		        12) +
		    1;
		if (UStaticMesh* ParkMesh = GetSeasonalParkMesh(Definition.DefinitionId, Month))
			PresentationMesh = ParkMesh;
	}
	Instances->SetStaticMesh(PresentationMesh ? PresentationMesh : PlaceholderBuildingMesh.Get());
	if (Definition.DefinitionId == TEXT("Factory") && IndustrialZoneMaterial)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Instances->GetNumMaterials(); ++MaterialIndex)
		{
			Instances->SetMaterial(MaterialIndex, IndustrialZoneMaterial);
		}
	}
	else if (Definition.DefinitionId == TEXT("Police") && CommercialZoneMaterial)
	{
		Instances->SetMaterial(0, CommercialZoneMaterial);
	}
	else if (!PresentationMesh)
	{
		Instances->SetMaterial(0, PlacedBuildingMaterial);
	}
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->RegisterComponent();
	BuildingInstances.Add(Definition.DefinitionId, Instances);
	ApplyBuildingFade(*Instances);
	return Instances;
}

FCityBuildingDefinition ACityBoardActor::ResolvePresentationDefinition(const uint64 EntityId,
                                                                       const FCityBuildingDefinition& Definition) const
{
	FCityBuildingDefinition Result = Definition;
	if (!SimulationSubsystem)
	{
		return Result;
	}
	const FCityDevelopmentState* State = SimulationSubsystem->GetDevelopmentSnapshot().Find(EntityId);
	if (!State)
	{
		// Manual placement presentation can occur before the subsystem's next
		// reconciliation tick. Show the authoritative definition's initial stage
		// immediately rather than flashing the completed low-density mesh.
		if (Definition.ConstructionSteps > 0 && !Definition.ConstructionMesh.IsNull())
		{
			Result.DefinitionId = FName(*(Definition.DefinitionId.ToString() + TEXT("_Construction")));
			Result.PresentationMesh = Definition.ConstructionMesh;
		}
        if (Definition.DefinitionId == TEXT("Home") || Definition.DefinitionId == TEXT("Shop") || Definition.DefinitionId == TEXT("Factory") || CityCivicEntrances::IsDefinition(Definition.DefinitionId) || CityUtilityEntrances::IsDefinition(Definition.DefinitionId)) ApplyPrivateGroundPresentation(Result);
		return Result;
	}
	if (State->Stage == ECityDevelopmentStage::Construction && !Definition.ConstructionMesh.IsNull())
	{
		Result.DefinitionId = FName(*(Definition.DefinitionId.ToString() + TEXT("_Construction")));
		Result.PresentationMesh = Definition.ConstructionMesh;
		if (Definition.DefinitionId == TEXT("Home") || Definition.DefinitionId == TEXT("Shop") || Definition.DefinitionId == TEXT("Factory") || CityCivicEntrances::IsDefinition(Definition.DefinitionId) || CityUtilityEntrances::IsDefinition(Definition.DefinitionId)) ApplyPrivateGroundPresentation(Result);
		return Result;
	}
	if (const FCityBuildingLevelDefinition* Level = Definition.FindDensityLevel(State->DensityLevel))
	{
		const TCHAR* Suffix = State->DensityLevel == ECityDensityLevel::High     ? TEXT("_High")
		                      : State->DensityLevel == ECityDensityLevel::Medium ? TEXT("_Medium")
		                                                                         : TEXT("_Low");
		Result.DefinitionId = FName(*(Definition.DefinitionId.ToString() + Suffix));
		if (!Level->PresentationMesh.IsNull())
		{
			Result.PresentationMesh = Level->PresentationMesh;
		}
	}
	const bool bDamaged = SimulationSubsystem->GetFireSnapshot().DamagedBuildings.Contains(EntityId);
	bool bAuthoredDamagedVariant = false;
	if (Definition.DefinitionId == TEXT("Shop") && BuildingCatalog)
	{
		FName BusinessId;
		const auto Variant =
		    BuildingCatalog->FindCommercialPresentation(EntityId, State->DensityLevel, bDamaged, BusinessId);
		if (!Variant.IsNull())
		{
			Result.PresentationMesh = Variant;
			Result.DefinitionId = FName(*(Result.DefinitionId.ToString() + TEXT("_Store_") + BusinessId.ToString()));
			bAuthoredDamagedVariant = bDamaged;
		}
	}
	bool bLowDensityVariant = false;
	if (BuildingCatalog && !State->HasCivicUpgrade())
	{
		FName Appearance;
		const auto Mesh = BuildingCatalog->FindLowDensityPresentation(
		    EntityId, Definition.DefinitionId, State->DensityLevel, bDamaged, Appearance);
		if (!Mesh.IsNull())
		{
			Result.PresentationMesh = Mesh;
			Result.DefinitionId = FName(*(Result.DefinitionId.ToString() + TEXT("_Appearance_") + Appearance.ToString()));
			bLowDensityVariant = true;
			bAuthoredDamagedVariant = bDamaged;
		}
	}
	if (bDamaged)
	{
		if (!bAuthoredDamagedVariant)
		{
			const FString BasePackage = Result.PresentationMesh.ToSoftObjectPath().GetLongPackageName();
			const FString Package = (Definition.DefinitionId == TEXT("Hospital") ||
			                         FCityWasteSimulation::IsFacility(Definition.DefinitionId))
			                            ? BasePackage.LeftChop(2) + TEXT("_Damaged_A")
			                            : BasePackage + TEXT("_Damaged");
			const FString Asset = FPaths::GetBaseFilename(Package);
			Result.PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Package + TEXT(".") + Asset));
		}
		Result.DefinitionId = FName(*(Result.DefinitionId.ToString() + TEXT("_Damaged")));
	}
	if (Definition.DefinitionId == TEXT("Home") && Definition.Footprint.Width == 2 &&
	    Definition.Footprint.Height == 2 && State->DensityLevel == ECityDensityLevel::Low &&
	    !State->HasCivicUpgrade() && !bDamaged && !bLowDensityVariant && SeasonalResidentialMesh)
	{
		Result.PresentationMesh = SeasonalResidentialMesh.Get();
	}
	if (Definition.DefinitionId == TEXT("WindPower") && State->Stage != ECityDevelopmentStage::Construction &&
	    !SimulationSubsystem->GetFireSnapshot().DamagedBuildings.Contains(EntityId) && WindBodyMesh)
	{
		Result.PresentationMesh = WindBodyMesh.Get();
		Result.DefinitionId = TEXT("WindPower_Base");
	}
    if (Definition.DefinitionId == TEXT("Home") || Definition.DefinitionId == TEXT("Shop") || Definition.DefinitionId == TEXT("Factory") || CityCivicEntrances::IsDefinition(Definition.DefinitionId) || CityUtilityEntrances::IsDefinition(Definition.DefinitionId)) ApplyPrivateGroundPresentation(Result);
	return Result;
}

void ACityBoardActor::ApplyPrivateGroundPresentation(FCityBuildingDefinition& Result) const
{
    if (const auto* Mesh = BuildingGroundMeshes.Find(FName(*Result.PresentationMesh.GetAssetName())))
        Result.PresentationMesh = Mesh->Get();
}

FTransform ACityBoardActor::MakeBuildingTransform(const FCityCellCoord& Anchor,
                                                  const FCityBuildingDefinition& Definition,
                                                  const int32 QuarterTurnsClockwise) const
{
	const FIntPoint Size = Definition.Footprint.GetRotatedSize(QuarterTurnsClockwise);
	const bool bUsesAuthoredMesh = Definition.PresentationMesh.LoadSynchronous() != nullptr;
	const double SurfaceHeight = GetBuildingBaseHeight(Anchor, Definition, QuarterTurnsClockwise);
    double GroundLift=40.;
    if(const auto* Industrial=CityIndustrialEntrances::Find(Definition.PresentationMesh.GetAssetName()))
        if(Definition.PresentationMesh.ToSoftObjectPath().GetLongPackageName()==Industrial->TargetMesh)
            GroundLift=5.; // Ground-level loading floors need clearance, not a residential plinth.
    if(const auto* Civic=CityCivicEntrances::Find(Definition.PresentationMesh.GetAssetName()))
        if(Definition.PresentationMesh.ToSoftObjectPath().GetLongPackageName()==Civic->TargetMesh) GroundLift=5.;
    if(const auto* Utility=CityUtilityEntrances::Find(Definition.PresentationMesh.GetAssetName()))
        if(Definition.PresentationMesh.ToSoftObjectPath().GetLongPackageName()==Utility->TargetMesh) GroundLift=5.;
	const FVector Location((Anchor.X + Size.X * 0.5) * CityBoardPicking::LogicalCellSizeCm,
	                       (Anchor.Y + Size.Y * 0.5) * CityBoardPicking::LogicalCellSizeCm,
	                       SurfaceHeight + (bUsesAuthoredMesh ? GroundLift : Definition.LogicalHeightCm * 0.5 + 40.0));
	const FVector Scale = bUsesAuthoredMesh ? FVector::OneVector
	                                        : FVector(Size.X * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm,
	                                                  Size.Y * CityBoardPicking::LogicalCellSizeCm / PlaneMeshSizeCm,
	                                                  Definition.LogicalHeightCm / PlaneMeshSizeCm);
	// The mast, its doorway and its foundation remain aligned to the placed lot.
	// Only the separate nacelle and rotor follow the weather direction.
	const double PresentationYaw=((QuarterTurnsClockwise+Definition.PresentationYawOffsetQuarterTurns)%4)*90.0;
	return FTransform(FRotator(0.0, PresentationYaw, 0.0), Location, Scale);
}

FVector ACityBoardActor::GetCellSurfaceCenter(const FCityCellCoord& Cell, const double OffsetCm) const
{
	FVector Center = CityBoardPicking::GetCellCenterLocal(Cell);
	Center.Z =
	    PlacementModel && PlacementModel->IsRoad(Cell)
	        ? GetRoadProfileHeight(Cell.X + 0.5, Cell.Y + 0.5, Cell.RoadLevel) + OffsetCm
	        : (SimulationSubsystem ? SimulationSubsystem->GetTerrain().GetCellCenterHeightCm(Cell) : 0.0) + OffsetCm;
	return Center;
}

double ACityBoardActor::GetRoadWidthAt(const FCityCellCoord& Cell, const bool bAlongX, const double Along,
                                       const bool bSidewalk) const
{
	const auto Width = [&](const FCityCellCoord& C)
	{
		const FName Id = PlacementModel->GetRoadDefinitionId(C);
		return bSidewalk ? CityRoadPresentation::SidewalkWidth(Id) : CityRoadPresentation::SurfaceWidth(Id);
	};
	const double Center = Width(Cell);
	if (!PlacementModel->FindRoadStructure(Cell) &&
	    PlacementModel->EvaluateRoadTerrain(Cell).TerrainClass != ECityRoadTerrainClass::Ordinary)
		return Center;
	const FCityCellCoord Previous = PlacementModel->GetRoadNeighbor(Cell, bAlongX ? -1 : 0, bAlongX ? 0 : -1);
	const FCityCellCoord Next = PlacementModel->GetRoadNeighbor(Cell, bAlongX ? 1 : 0, bAlongX ? 0 : 1);
	const auto Portal = [&](const FCityCellCoord& C)
	{
		return PlacementModel->CanRoadsConnect(Cell, C) &&
		               (PlacementModel->FindRoadStructure(C) ||
		                PlacementModel->EvaluateRoadTerrain(C).TerrainClass == ECityRoadTerrainClass::Ordinary)
		           ? (Center + Width(C)) * .5
		           : Center;
	};
	const uint8 Mask = PlacementModel->GetRoadNeighborMask(Cell);
	if (FMath::CountBits(static_cast<uint64>(Mask)) >= 2 && Mask != 5 && Mask != 10)
	{
		const double Half = CityRoadPresentation::SurfaceWidth(PlacementModel->GetRoadDefinitionId(Cell)) * .5;
		const double Distance = FMath::Abs(Along - .5);
		if (Distance <= Half)
			return Center;
		return FMath::Lerp(Center, Along < .5 ? Portal(Previous) : Portal(Next),
		                   FMath::Clamp((Distance - Half) / (.5 - Half), 0., 1.));
	}
	return FCityRoadRibbonMesh::WidthAt(Along, Portal(Previous), Center, Portal(Next));
}

double ACityBoardActor::GetRoadProfileHeight(const double CellX, const double CellY, const int32 RoadLevel) const
{
	if (!SimulationSubsystem || !PlacementModel)
	{
		return 0.0;
	}
	double StructuredHeight = 0;
	if (RoadLevel != 0 && PlacementModel->TrySampleStructureHeight(CellX, CellY, StructuredHeight))
		return StructuredHeight;
	const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();
	int32 MinimumX = FMath::Clamp(FMath::FloorToInt(CellX), 0, GetDimensions().GetWidth() - 1);
	int32 MaximumX = MinimumX;
	int32 MinimumY = FMath::Clamp(FMath::FloorToInt(CellY), 0, GetDimensions().GetHeight() - 1);
	int32 MaximumY = MinimumY;
	if (CellX > 0.0 && CellX < GetDimensions().GetWidth() &&
	    FMath::IsNearlyEqual(CellX, static_cast<double>(FMath::RoundToInt(CellX)), 0.001))
	{
		MinimumX = FMath::RoundToInt(CellX) - 1;
		MaximumX = FMath::RoundToInt(CellX);
	}
	if (CellY > 0.0 && CellY < GetDimensions().GetHeight() &&
	    FMath::IsNearlyEqual(CellY, static_cast<double>(FMath::RoundToInt(CellY)), 0.001))
	{
		MinimumY = FMath::RoundToInt(CellY) - 1;
		MaximumY = FMath::RoundToInt(CellY);
	}
	double TotalHeight = 0.0;
	int32 SampleCount = 0;
	for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
	{
		for (int32 X = MinimumX; X <= MaximumX; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (!PlacementModel->IsRoad(Cell))
			{
				continue;
			}
			const FCityRoadTerrainEvaluation Evaluation = PlacementModel->EvaluateRoadTerrain(Cell);
			TotalHeight += Evaluation.TerrainClass == ECityRoadTerrainClass::Bridge
			                   ? Terrain.GetWaterSurfaceHeightCm(Cell) + 120.0
			                   : FCityHomeWalk::GroundHeight(Terrain, FVector(CellX * 800., CellY * 800., 0));
			++SampleCount;
		}
	}
	return SampleCount > 0 ? TotalHeight / SampleCount : FCityHomeWalk::GroundHeight(Terrain, FVector(CellX * 800., CellY * 800., 0));
}

double ACityBoardActor::GetBuildingBaseHeight(const FCityCellCoord& Anchor, const FCityBuildingDefinition& Definition,
                                              const int32 QuarterTurnsClockwise) const
{
	if (!SimulationSubsystem)
	{
		return 0.0;
	}
	// Flat lots must clear the complete rendered heightfield, including peaks
	// between logical cell corners. Sampling only the road entrance lets uphill
	// terrain intersect the house/lawn. The shared base keeps foundations,
	// buildings, entrance ramps and seasonal accents aligned.
	const FIntPoint Size = Definition.Footprint.GetRotatedSize(QuarterTurnsClockwise);
	const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();
	int32 Maximum = MIN_int32;
	for (int32 Y = Anchor.Y * Terrain.SamplesPerCell; Y <= (Anchor.Y + Size.Y) * Terrain.SamplesPerCell; ++Y)
	{
		for (int32 X = Anchor.X * Terrain.SamplesPerCell; X <= (Anchor.X + Size.X) * Terrain.SamplesPerCell; ++X)
		{
			Maximum = FMath::Max(Maximum, Terrain.GetSampleHeightCm(X, Y));
		}
	}
	return Maximum;
}

bool ACityBoardActor::FindEntranceConnection(const FCityCellCoord& Anchor, const FCityBuildingDefinition& Definition,
                                             const int32 QuarterTurnsClockwise, FVector& OutDoorLocal,
                                             FVector& OutRoadLocal) const
{
	if (!PlacementModel)
	{
		return false;
	}
	const FIntPoint Size = Definition.Footprint.GetRotatedSize(QuarterTurnsClockwise);
	const double CellSize = CityBoardPicking::LogicalCellSizeCm;
	for (int32 LocalSide = 0; LocalSide < 4; ++LocalSide)
	{
		if ((Definition.EntranceSides & (1 << LocalSide)) == 0)
		{
			continue;
		}
		const int32 WorldSide = (LocalSide + QuarterTurnsClockwise) % 4;
		if (WorldSide == 0 || WorldSide == 2)
		{
			const int32 RoadY = WorldSide == 0 ? Anchor.Y - 1 : Anchor.Y + Size.Y;
			for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
			{
				const FCityCellCoord RoadCell(X, RoadY);
				if (PlacementModel->SupportsBuildingFrontage(RoadCell))
				{
					const double DoorX = Definition.DefinitionId == TEXT("Home") ? Anchor.X + Size.X * 0.5 : X + 0.5;
					OutDoorLocal =
					    FVector(DoorX * CellSize, (WorldSide == 0 ? Anchor.Y : Anchor.Y + Size.Y) * CellSize, 0.0);
					OutRoadLocal = CityBoardPicking::GetCellCenterLocal(RoadCell);
					return true;
				}
			}
		}
		else
		{
			const int32 RoadX = WorldSide == 1 ? Anchor.X + Size.X : Anchor.X - 1;
			for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
			{
				const FCityCellCoord RoadCell(RoadX, Y);
				if (PlacementModel->SupportsBuildingFrontage(RoadCell))
				{
					const double DoorY = Definition.DefinitionId == TEXT("Home") ? Anchor.Y + Size.Y * 0.5 : Y + 0.5;
					OutDoorLocal =
					    FVector((WorldSide == 1 ? Anchor.X + Size.X : Anchor.X) * CellSize, DoorY * CellSize, 0.0);
					OutRoadLocal = CityBoardPicking::GetCellCenterLocal(RoadCell);
					return true;
				}
			}
		}
	}
	return false;
}

void ACityBoardActor::UpdateConcreteMaterial()
{
	if (!ConcreteMaterial && FoundationInstances->GetMaterial(0))
	{
		ConcreteMaterial = UMaterialInstanceDynamic::Create(FoundationInstances->GetMaterial(0), this);
		FoundationInstances->SetMaterial(0, ConcreteMaterial);
		ResidentialFoundationInstances->SetMaterial(0, ConcreteMaterial);
	}
	if (ConcreteMaterial)
	{
		ConcreteMaterial->SetScalarParameterValue(TEXT("BoardMetreCm"), 100.f * BoardRoot->GetComponentScale().GetAbsMax());
		for(const auto& Pair:UtilityFoundationInstances) if(Pair.Value) Pair.Value->SetMaterial(0,ConcreteMaterial);
	}
}

double ACityBoardActor::SampleSidewalkSurface(const FVector& P) const
{
	const auto* Section = SidewalkSurface->GetProcMeshSection(0);
	if (Section)
		for (int32 I = 0; I < Section->ProcIndexBuffer.Num(); I += 3)
		{
			const FVector A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position;
			const FVector B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I + 1]].Position;
			const FVector C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I + 2]].Position;
			const double Det = (B.Y - C.Y) * (A.X - C.X) + (C.X - B.X) * (A.Y - C.Y);
			if (FMath::Abs(Det) < .001) continue;
			const double U = ((B.Y - C.Y) * (P.X - C.X) + (C.X - B.X) * (P.Y - C.Y)) / Det;
			const double V = ((C.Y - A.Y) * (P.X - C.X) + (A.X - C.X) * (P.Y - C.Y)) / Det;
			if (U >= -.00001 && V >= -.00001 && U + V <= 1.00001) return U * A.Z + V * B.Z + (1 - U - V) * C.Z;
		}
	const FCityCellCoord Cell(FMath::FloorToInt(P.X / 800.), FMath::FloorToInt(P.Y / 800.));
	return GetRoadProfileHeight(P.X / 800., P.Y / 800.) +
	       (CityRoadPresentation::SidewalkWidth(PlacementModel->GetRoadDefinitionId(Cell)) > 0 ? FCityRoadRibbonMesh::SidewalkLiftCm : FCityRoadRibbonMesh::SurfaceLiftCm);
}

bool ACityBoardActor::FindHomeWalkConnection(const FCityPlacedEntity& Entity,
                                             const FCityBuildingDefinition& Presentation,
                                             FVector& Door, FVector& Sidewalk) const
{
	const auto* Definition = BuildingCatalog->FindDefinition(Entity.DefinitionId);
	FVector Boundary, Road;
	if (!Definition || !FindEntranceConnection(Entity.Anchor, *Definition, Entity.QuarterTurnsClockwise, Boundary, Road)) return false;
	const auto* Entry = CityPrivateEntrances::Find(Presentation.PresentationMesh.GetAssetName());
    if (!Entry) return false;
    Door = MakeBuildingTransform(Entity.Anchor, Presentation, Entity.QuarterTurnsClockwise).TransformPosition(Entry->EntryCm);
	const FCityCellCoord RoadCell(FMath::FloorToInt(Road.X / 800.), FMath::FloorToInt(Road.Y / 800.));
	const FName RoadId = PlacementModel->GetRoadDefinitionId(RoadCell);
	const FVector Outward = FRotator(0, Entity.QuarterTurnsClockwise * 90., 0).RotateVector(FVector(0,-1,0));
	const FVector Along = FVector::CrossProduct(FVector::UpVector, Outward);
	const bool bAlongX = FMath::Abs(Along.X) > .5;
    if(Entry->bFrontageFlight)
    {
        // A turn at either end of this2x2 frontage leaves room for stairs.
        // Test both edges of its width against real ground-road support.
        for(const double Sign:{1.,-1.})
        {
            FVector Candidate=Road+Along*FVector::DotProduct(Door+Along*Sign*(Entry->FrontageHalfSpanCm-Entry->WidthCm*.5)-Road,Along);
            const FCityCellCoord C(FMath::FloorToInt(Candidate.X/800.),FMath::FloorToInt(Candidate.Y/800.));
            if(!PlacementModel->SupportsBuildingFrontage(C)) continue;
            const double T=bAlongX?Candidate.X/800.-C.X:Candidate.Y/800.-C.Y;
            const double E=GetRoadWidthAt(C,bAlongX,T)*.5+GetRoadWidthAt(C,bAlongX,T,true);
            Candidate-=Outward*E*800.;
            bool Supported=true;
            for(const double Side:{-1.,1.})
            {
                const FVector P=Candidate+Along*Side*(Entry->WidthCm*.5-.1)+Outward;
                const FCityCellCoord EdgeCell(FMath::FloorToInt(P.X/800.),FMath::FloorToInt(P.Y/800.));
                Supported &= PlacementModel->SupportsBuildingFrontage(EdgeCell);
            }
            if(!Supported || FVector::DotProduct(Candidate-Door,Outward)<Entry->WidthCm) continue;
            if(CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName())) Candidate+=Outward*45.;
            Sidewalk=Candidate;Sidewalk.Z=SampleSidewalkSurface(Sidewalk);return true;
        }
        return false;
    }
	const double At = bAlongX ? Door.X / 800. - RoadCell.X : Door.Y / 800. - RoadCell.Y;
	const double Edge = GetRoadWidthAt(RoadCell, bAlongX, At) * .5 + GetRoadWidthAt(RoadCell, bAlongX, At, true);
	Sidewalk = Road - Outward * Edge * 800.;
	Sidewalk += Along * FVector::DotProduct(Door - Sidewalk, Along);
	// Keep the centered walk when both frontage tiles exist. A partial frontage
	// instead targets the supported tile, avoiding a path into a missing sidewalk.
	for (const double Sign : {-1., 1.})
	{
		const FVector EdgePoint = Sidewalk + Along * Sign * 66. + Outward;
		const FCityCellCoord C(FMath::FloorToInt(EdgePoint.X / 800.), FMath::FloorToInt(EdgePoint.Y / 800.));
		if (!PlacementModel->SupportsBuildingFrontage(C))
		{
			Sidewalk = Road - Outward * Edge * 800.;
			break;
		}
	}
	if(CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName())) Sidewalk+=Outward*45.;
	(void)RoadId;
	Sidewalk.Z = SampleSidewalkSurface(Sidewalk);
	return true;
}

bool ACityBoardActor::FindVehicleBayConnection(const FCityPlacedEntity& Entity,
    const FCityBuildingDefinition& Presentation,const FCityIndustrialBay& Bay,FVector& Start,FVector& End) const
{
    const auto* Definition=BuildingCatalog->FindDefinition(Entity.DefinitionId);
    FVector Boundary,Road;
    if(!Definition || !FindEntranceConnection(Entity.Anchor,*Definition,Entity.QuarterTurnsClockwise,Boundary,Road)) return false;
    if(FCityPlacementModel::RequiresFullRoadFrontage(Entity.DefinitionId) &&
        !PlacementModel->HasFullRoadFrontage(Entity.Anchor,Entity.FootprintSize,Entity.QuarterTurnsClockwise)) return false;
    Start=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise).TransformPosition(Bay.EntryCm);
    if(Bay.bGroundStart) Start.Z=FCityHomeWalk::GroundHeight(SimulationSubsystem->GetTerrain(),Start)+6.;
    const FVector F=FRotator(0,Entity.QuarterTurnsClockwise*90.,0).RotateVector(FVector(0,-1,0));
    const FVector R=FVector::CrossProduct(FVector::UpVector,F);
    const bool AlongX=FMath::Abs(R.X)>.5;
    const FTransform Pose=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise);
    bool FullFrontage=true;
    for(int32 FrontCell=0;FrontCell<Definition->Footprint.Width;++FrontCell) {
        const double X=(FrontCell+.5-Definition->Footprint.Width*.5)*800.;
        const FVector P=Pose.TransformPosition(FVector(X,Definition->Footprint.Height*400.+400.,0));
        FullFrontage &= PlacementModel->SupportsBuildingFrontage(FCityCellCoord(FMath::FloorToInt(P.X/800.),FMath::FloorToInt(P.Y/800.)));
    }
    // Prefer the point directly opposite the actual bay. A partial frontage
    // can instead receive the full bay width on its supported8m road tile.
    for(const bool Direct:{true,false}) {
        if(!Direct && FCityPlacementModel::RequiresFullRoadFrontage(Entity.DefinitionId)) break;
        FVector Candidate=Road;
        if(Direct && FullFrontage) Candidate+=R*FVector::DotProduct(Start-Road,R);
        else if(CityPrivateEntrances::Bays(Presentation.PresentationMesh.GetAssetName()).Num()==2)
            Candidate+=R*FVector::DotProduct(Start-Pose.GetLocation(),R);
        const FCityCellCoord Cell(FMath::FloorToInt(Candidate.X/800.),FMath::FloorToInt(Candidate.Y/800.));
        if(!PlacementModel->SupportsBuildingFrontage(Cell)) continue;
        const double T=AlongX?Candidate.X/800.-Cell.X:Candidate.Y/800.-Cell.Y;
        const double Edge=GetRoadWidthAt(Cell,AlongX,T)*.5+GetRoadWidthAt(Cell,AlongX,T,true);
        Candidate-=F*Edge*800.;
        bool Supported=true;
        for(const double Side:{-1.,0.,1.}) {
            const FVector P=Candidate+R*Side*(Bay.WidthCm*.5-.1)+F;
            const FCityCellCoord C(FMath::FloorToInt(P.X/800.),FMath::FloorToInt(P.Y/800.));
            Supported &= PlacementModel->SupportsBuildingFrontage(C);
        }
        if(!Supported || FVector::DotProduct(Candidate-Start,F)<50.) continue;
        End=Candidate;End.Z=SampleSidewalkSurface(End);return true;
    }
    return false;
}

void ACityBoardActor::RebuildBuildingEntranceProfiles()
{
	if (!BuildingEntranceProfiles || !SimulationSubsystem || !PlacementModel || !BuildingCatalog)
	{
		return;
	}
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	FCityHomeWalk HomeWalks;
    auto AppendWalk=[&](const FCityHomeWalk& Walk) {
        const int32 Base=HomeWalks.Vertices.Num(),RailBase=HomeWalks.RailVertices.Num();
        HomeWalks.Vertices.Append(Walk.Vertices);HomeWalks.Normals.Append(Walk.Normals);HomeWalks.UVs.Append(Walk.UVs);
        for(const int32 I:Walk.Triangles) HomeWalks.Triangles.Add(Base+I);
        HomeWalks.RailVertices.Append(Walk.RailVertices);HomeWalks.RailNormals.Append(Walk.RailNormals);HomeWalks.RailUVs.Append(Walk.RailUVs);
        for(const int32 I:Walk.RailTriangles) HomeWalks.RailTriangles.Add(RailBase+I);
    };

	TArray<uint64> EntityIds;
	PlacementModel->GetEntities().GetKeys(EntityIds);
	EntityIds.Sort();
	for (const uint64 EntityId : EntityIds)
	{
		const FCityPlacedEntity& Entity = PlacementModel->GetEntities().FindChecked(EntityId);
		const FCityBuildingDefinition* Definition = BuildingCatalog->FindDefinition(Entity.DefinitionId);
		if (Definition)
		{
            // Restore missing station/utility frontage before drawing approaches;
            // never funnel entrances into a single surviving road tile.
            if(FCityPlacementModel::RequiresFullRoadFrontage(Entity.DefinitionId) &&
                !PlacementModel->HasFullRoadFrontage(Entity.Anchor,Entity.FootprintSize,Entity.QuarterTurnsClockwise)) continue;
			const auto Presentation = ResolvePresentationDefinition(EntityId, *Definition);
			if (const auto* Entry = CityPrivateEntrances::Find(Presentation.PresentationMesh.GetAssetName()))
			{
                bool VehiclePartial=false;
                if(CityIndustrialEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName())) {
                    const FTransform Pose=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise);
                    for(int32 FrontCell=0;FrontCell<Definition->Footprint.Width;++FrontCell) {
                        const double X=(FrontCell+.5-Definition->Footprint.Width*.5)*800.;
                        const FVector P=Pose.TransformPosition(FVector(X,Definition->Footprint.Height*400.+400.,0));
                        VehiclePartial |= !PlacementModel->SupportsBuildingFrontage(FCityCellCoord(FMath::FloorToInt(P.X/800.),FMath::FloorToInt(P.Y/800.)));
                    }
                }
				FVector HomeDoor, WalkEnd;
				if (FindHomeWalkConnection(Entity, Presentation, HomeDoor, WalkEnd))
				{
					FCityHomeWalk Walk;
					if(Entry->bFrontageFlight)
                    {
                        const FVector Outward=FRotator(0,Entity.QuarterTurnsClockwise*90.,0).RotateVector(FVector(0,-1,0));
                        Walk.BuildFrontage(HomeDoor,WalkEnd,Outward,SimulationSubsystem->GetTerrain(),[&](const FVector& P){return SampleSidewalkSurface(P);},Entry->WidthCm,Entry->DoorWidthCm,Entry->LandingCm,Entry->GroundReserveCm);
                    }
                    else if(CityIndustrialEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName()))
                    {
                        const auto& Terrain=SimulationSubsystem->GetTerrain();
                        const FVector F=FRotator(0,Entity.QuarterTurnsClockwise*90.,0).RotateVector(FVector(0,-1,0));
                        const FVector R=FVector::CrossProduct(FVector::UpVector,F);
                        if(!VehiclePartial)
                            Walk.Build(HomeDoor,WalkEnd,Terrain,[&](const FVector& P){return SampleSidewalkSurface(P);},Entry->LandingCm,Entry->GroundReserveCm,Entry->WidthCm);
                        else
                        {
                            // Keep the flight in the personnel corridor. The
                            // terrain strip crosses in front of the truck ramp,
                            // then turns onto the supported frontage tile.
                            const FVector Boundary=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise).TransformPosition(FVector(Entry->EntryCm.X,Definition->Footprint.Height*400.,0));
                            const double CrossingWidth=(CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName()))?150.:Entry->WidthCm;
                            FVector Foot=Boundary-F*CrossingWidth;
                            Foot.Z=FCityHomeWalk::GroundHeight(Terrain,Foot)+6.5;
                            Walk.Build(HomeDoor,Foot,Terrain,[&](const FVector& P){return FCityHomeWalk::GroundHeight(Terrain,P)+6.5;},Entry->LandingCm,Entry->GroundReserveCm,Entry->WidthCm);
                            const FVector Direction=R*(FVector::DotProduct(WalkEnd-HomeDoor,R)>0?1.:-1.);
                            const FVector Center=Foot+F*(CrossingWidth*.5);
                            FVector Turn=Center+R*FVector::DotProduct(WalkEnd-Center,R);
                            Turn.Z=FCityHomeWalk::GroundHeight(Terrain,Turn)+6.5;
                            const auto Ground=[&](const FVector& P){return FCityHomeWalk::GroundHeight(Terrain,P)+6.5;};
                            FCityIndustrialDriveway Cross;
                            Cross.Build(Center-Direction*(Entry->WidthCm*.5),Turn-Direction*(Entry->WidthCm*.5),Direction,Terrain,Ground,CrossingWidth,2000.,6.5,false);
                            AppendWalk(Cross);
                            FVector Link=Turn-F*(CrossingWidth*.5);Link.Z=Ground(Link);
                            Cross.Build(Link,WalkEnd,F,Terrain,[&](const FVector& P){return SampleSidewalkSurface(P);},Entry->WidthCm,2000.,6.5,false);
                            AppendWalk(Cross);
                        }
                    }
                    else Walk.Build(HomeDoor, WalkEnd, SimulationSubsystem->GetTerrain(), [&](const FVector& P) { return SampleSidewalkSurface(P); }, Entry->LandingCm, Entry->GroundReserveCm,Entry->WidthCm);

                    AppendWalk(Walk);
				}
                for(const auto& Bay:CityPrivateEntrances::Bays(Presentation.PresentationMesh.GetAssetName()))
                {
                    FVector BayStart,PublicEnd;
                    if(!FindVehicleBayConnection(Entity,Presentation,Bay,BayStart,PublicEnd)) continue;
                    const FVector F=FRotator(0,Entity.QuarterTurnsClockwise*90.,0).RotateVector(FVector(0,-1,0));
                    double GroundReserve=VehiclePartial?175.:60.;
                    if(VehiclePartial && (CityCivicEntrances::Find(Presentation.PresentationMesh.GetAssetName()) || CityUtilityEntrances::Find(Presentation.PresentationMesh.GetAssetName())))
                    {
                        // The wider civic pedestrian crossing needs a complete
                        // terrain strip before any vehicle lane turns across it.
                        const auto BayPose=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise);
                        const FVector CrossingStart=BayPose.TransformPosition(FVector(0,Definition->Footprint.Height*400.-Entry->WidthCm,0));
                        GroundReserve=FMath::Max(GroundReserve,FVector::DotProduct(PublicEnd-CrossingStart,F));
                    }
                    FCityIndustrialDriveway Drive;
                    Drive.Build(BayStart,PublicEnd,F,SimulationSubsystem->GetTerrain(),[&](const FVector& P){return SampleSidewalkSurface(P);},Bay.WidthCm,GroundReserve,6.,!Bay.bGroundStart);
                    const int32 Base=Vertices.Num();
                    Vertices.Append(Drive.Vertices);Normals.Append(Drive.Normals);UVs.Append(Drive.UVs);
                    for(const int32 Index:Drive.Triangles) Triangles.Add(Base+Index);
                }

				continue;
			}
		}
		FVector Door;
		FVector Road;
		if (!Definition ||
		    !FindEntranceConnection(Entity.Anchor, *Definition, Entity.QuarterTurnsClockwise, Door, Road))
		{
			continue;
		}
		const double BuildingHeight = GetBuildingBaseHeight(Entity.Anchor, *Definition, Entity.QuarterTurnsClockwise) +
		                              (Entity.DefinitionId == TEXT("Home") ? 36.0 : 42.0);
		Door.Z = BuildingHeight;
		Road.Z = SampleSidewalkSurface(Road);
		const FVector Direction = (Road - Door).GetSafeNormal2D();
		const FVector Right =
		    FVector::CrossProduct(FVector::UpVector, Direction) * (CityBoardPicking::LogicalCellSizeCm * 0.16);
		// Subdivide the short driveway so convex terrain beside the lot cannot
		// intersect a single flat entrance quad. The road endpoint follows the
		// same rendered road profile; the lot endpoint matches its raised pad.
		constexpr int32 RampSegments = 4;
		const int32 Base = Vertices.Num();
		for (int32 Segment = 0; Segment <= RampSegments; ++Segment)
		{
			const double Along = static_cast<double>(Segment) / RampSegments;
			const FVector Center = FMath::Lerp(Door, Road, Along);
			for (const double Side : {-1.0, 1.0})
			{
				FVector Vertex = Center + Right * Side;
				if (Segment > 0 && Segment < RampSegments)
				{
					Vertex.Z = FMath::Max(Vertex.Z, SimulationSubsystem->GetTerrain().SampleHeightCm(
					                                    Vertex.X / CityBoardPicking::LogicalCellSizeCm,
					                                    Vertex.Y / CityBoardPicking::LogicalCellSizeCm) +
					                                    42.0);
				}
				Vertices.Add(Vertex);
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(Side < 0 ? 0 : 1, Along));
			}
			if (Segment < RampSegments)
			{
				const int32 Row = Base + Segment * 2;
				Triangles.Append({Row, Row + 2, Row + 1, Row + 1, Row + 2, Row + 3});
			}
		}
	}
	BuildingEntranceProfiles->ClearAllMeshSections();
	if (!Vertices.IsEmpty())
	{
		BuildingEntranceProfiles->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
		BuildingEntranceProfiles->SetMaterial(0, RoadInstances->GetMaterial(0));
	}
	if (!HomeWalks.Vertices.IsEmpty())
	{
		BuildingEntranceProfiles->CreateMeshSection(1, HomeWalks.Vertices, HomeWalks.Triangles, HomeWalks.Normals, HomeWalks.UVs, Colors, Tangents, false);
		BuildingEntranceProfiles->SetMaterial(1, FoundationInstances->GetMaterial(0));
	}
    if(!HomeWalks.RailVertices.IsEmpty()) {
        BuildingEntranceProfiles->CreateMeshSection(2,HomeWalks.RailVertices,HomeWalks.RailTriangles,HomeWalks.RailNormals,HomeWalks.RailUVs,Colors,Tangents,false);
        BuildingEntranceProfiles->SetMaterial(2,RoadSupportInstances->GetMaterial(0));
    }
}

void ACityBoardActor::RebuildTerrainSurface(const bool bRefineBanks, const bool bSmoothBankShading)
{
	if (!BoardSurface || !SimulationSubsystem)
	{
		return;
	}
	BoardSurface->ClearAllMeshSections();
	const FCityTerrainHeightfield& Terrain = SimulationSubsystem->GetTerrain();
	constexpr int32 ChunkCells = 16;
	constexpr int32 ChunkSamples = ChunkCells * FCityTerrainHeightfield::SamplesPerCell;
	int32 SectionIndex = 0, TotalVertices = 0, TotalTriangles = 0;
	for (int32 ChunkY = 0; ChunkY < GetDimensions().GetHeight(); ChunkY += ChunkCells)
	{
		for (int32 ChunkX = 0; ChunkX < GetDimensions().GetWidth(); ChunkX += ChunkCells)
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FVector2D> WeatherUVs;
			TArray<FColor> Colors;
			TArray<FProcMeshTangent> Tangents;
			constexpr int32 Side = ChunkSamples + 1;
			Vertices.Reserve(Side * Side);
			Normals.Reserve(Side * Side);
			UVs.Reserve(Side * Side);
			WeatherUVs.Reserve(Side * Side);
			Colors.Reserve(Side * Side);
			for (int32 LocalY = 0; LocalY <= ChunkSamples; ++LocalY)
			{
				for (int32 LocalX = 0; LocalX <= ChunkSamples; ++LocalX)
				{
					const int32 SampleX = ChunkX * FCityTerrainHeightfield::SamplesPerCell + LocalX;
					const int32 SampleY = ChunkY * FCityTerrainHeightfield::SamplesPerCell + LocalY;
					const double CellX = static_cast<double>(SampleX) / FCityTerrainHeightfield::SamplesPerCell;
					const double CellY = static_cast<double>(SampleY) / FCityTerrainHeightfield::SamplesPerCell;
					Vertices.Add(FVector(CellX * CityBoardPicking::LogicalCellSizeCm,
					                     CellY * CityBoardPicking::LogicalCellSizeCm,
					                     Terrain.GetSampleHeightCm(SampleX, SampleY)));
					const FVector Normal = bRefineBanks && bSmoothBankShading
					                           ? FCityTerrainSurface::ShadingNormal(Terrain, SampleX, SampleY)
					                           : FCityTerrainSurface::SampleNormal(Terrain, SampleX, SampleY);
					Normals.Add(Normal);
					UVs.Add(FVector2D(CellX, CellY));
					WeatherUVs.Add(FCityWeatherCoverage::Sample(Terrain, FVector2D(CellX, CellY), Normal));
					const double ElevationAlpha =
					    FMath::Clamp((Terrain.GetSampleHeightCm(SampleX, SampleY) + 1200.0) / 4200.0, 0.0, 1.0);
					// Color follows continuous height/slope samples, including shared chunk borders.
					// Fine grass variation comes from the mipmapped material rather than noisy vertices.
					const double SlopeAlpha = FMath::SmoothStep(0.02, 0.20, 1.0 - Normal.Z);
					FLinearColor TerrainColor =
					    FMath::Lerp(FLinearColor(0.12, 0.27, 0.09), FLinearColor(0.30, 0.39, 0.18), ElevationAlpha);
					TerrainColor = FMath::Lerp(TerrainColor, FLinearColor(0.27, 0.255, 0.22), SlopeAlpha * 0.75);
					const double AboveWater = Terrain.GetSampleHeightCm(SampleX, SampleY) - Terrain.GetSeaLevelCm();
					const double SandAlpha = 1.0 - FMath::SmoothStep(40.0, 240.0, AboveWater);
					TerrainColor = FMath::Lerp(TerrainColor, FLinearColor(0.38, 0.30, 0.16), SandAlpha);
					const double WetAlpha = 1.0 - FMath::SmoothStep(-120.0, 25.0, AboveWater);
					TerrainColor = FMath::Lerp(TerrainColor, FLinearColor(0.13, 0.16, 0.10), WetAlpha);
					Colors.Add(TerrainColor.ToFColorSRGB());
				}
			}
			for (int32 Y = 0; Y < ChunkSamples; ++Y)
			{
				for (int32 X = 0; X < ChunkSamples; ++X)
				{
					const int32 A = Y * Side + X;
					const int32 B = A + 1;
					const int32 C = A + Side;
					const int32 D = C + 1;
					const int32 Sx = ChunkX * 2 + X, Sy = ChunkY * 2 + Y;
					const int32 N = bRefineBanks ? FCityTerrainSurface::Subdivisions(Terrain, Sx, Sy) : 1;
					if (N == 1)
					{
						Triangles.Append({A, C, B, B, C, D});
						continue;
					}
					const int32 Base = Vertices.Num();
					for (int32 J = 0; J <= N; ++J)
						for (int32 I = 0; I <= N; ++I)
						{
							const double U = double(I) / N, V = double(J) / N;
							auto LerpExact = [](const auto& Start, const auto& End, const double Alpha)
							{
								return Alpha == 0 ? Start : Alpha == 1 ? End : FMath::Lerp(Start, End, Alpha);
							};
							auto Blend = [&](const auto& Values)
							{
								return LerpExact(LerpExact(Values[A], Values[B], U), LerpExact(Values[C], Values[D], U),
								                 V);
							};
							Vertices.Add(FCityTerrainSurface::Point(Terrain, Sx, Sy, U, V, N));
							// Shared edges interpolate the original endpoint attributes, so
							// an adjacent coarse facet has the same shading and weather mask.
							Normals.Add(Blend(Normals));
							UVs.Add(Blend(UVs));
							WeatherUVs.Add(Blend(WeatherUVs));
							// Vertex colors are stored as byte attributes. Interpolate the
							// same encoded values as the adjacent coarse GPU edge, avoiding
							// a gamma roundtrip or endpoint floating-point drift at seams.
							const FColor CA = Colors[A], CB = Colors[B], CC = Colors[C], CD = Colors[D];
							auto Channel = [&](double P, double Q, double R, double S)
							{
								return uint8(
								    FMath::RoundToInt(FMath::Lerp(FMath::Lerp(P, Q, U), FMath::Lerp(R, S, U), V)));
							};
							Colors.Add(FColor(Channel(CA.R, CB.R, CC.R, CD.R), Channel(CA.G, CB.G, CC.G, CD.G),
							                  Channel(CA.B, CB.B, CC.B, CD.B), Channel(CA.A, CB.A, CC.A, CD.A)));
						}
					for (int32 J = 0; J < N; ++J)
						for (int32 I = 0; I < N; ++I)
						{
							const int32 P = Base + J * (N + 1) + I;
							Triangles.Append({P, P + N + 1, P + 1, P + 1, P + N + 1, P + N + 2});
						}
				}
			}
			BoardSurface->CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, WeatherUVs,
			                                TArray<FVector2D>(), TArray<FVector2D>(), Colors, Tangents, false);
			BoardSurface->SetMaterial(SectionIndex, BoardSurface->GetMaterial(0));
			TotalVertices += Vertices.Num();
			TotalTriangles += Triangles.Num() / 3;
			++SectionIndex;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_TERRAIN_REBUILT Seed=%d Sections=%d Vertices=%d Triangles=%d"),
	       Terrain.GetParameters().Seed, SectionIndex, TotalVertices, TotalTriangles);
	RebuildTerrainBase();
}

void ACityBoardActor::RebuildWaterSurface()
{
	if (!WaterSurface || !SimulationSubsystem)
		return;
	const FCityWaterMesh Mesh = FCityWaterMesh::Build(SimulationSubsystem->GetTerrain());
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Mesh.Vertices.Num());
	TArray<FProcMeshTangent> Tangents;
	WaterSurface->ClearAllMeshSections();
	if (!Mesh.Vertices.IsEmpty())
		WaterSurface->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Normals, Mesh.UVs, Mesh.Colors, Tangents, false);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_WATER_REBUILT Vertices=%d Triangles=%d River=%d"), Mesh.Vertices.Num(),
	       Mesh.Triangles.Num() / 3, SimulationSubsystem->GetTerrain().GetRiverCells().Num());
}

void ACityBoardActor::SetHeatMapLayers(const bool bActive)
{
	bSeasonalDecorationsAnalytical = bActive;
	if (!bActive)
	{
		for (const auto& Pair : HeatMapVisibility)
			if (Pair.Key.IsValid())
				Pair.Key->SetVisibility(Pair.Value);
		HeatMapVisibility.Reset();
		return;
	}
	TArray<USceneComponent*> SuppressedComponents = {BoardSurface,
	                                                 WaterSurface,
	                                                 FoundationInstances,
	                                                 ResidentialFoundationInstances,
	                                                 BuildingEntranceProfiles,
	                                                 RoadInstances,
	                                                 RoadSurfaceProfile,
	                                                 RoadSupportInstances,
	                                                 RoadCornerInstances,
	                                                 RoadTInstances,
	                                                 RoadCrossInstances,
	                                                 DirtRoadSurfaceInstances,
	                                                 LocalRoadSurfaceInstances,
	                                                 AvenueRoadSurfaceInstances,
	                                                 HighwayRoadSurfaceInstances,
	                                                 RoadMarkingInstances,
	                                                 SidewalkSurface,
	                                                 RoadDetailSurface,
	                                                 ResidentialZoneInstances,
	                                                 CommercialZoneInstances,
	                                                 IndustrialZoneInstances,
	                                                 BenchmarkVehicleInstances,
	                                                 CitizenInstances,
	                                                 OfficeCitizenInstances,
	                                                 WorkerCitizenInstances,
	                                                 TrafficVehicleInstances,
	                                                 TrafficTruckInstances,
	                                                 GarbageTruckInstances,
	                                                 RecyclingTruckInstances,
	                                                 ParkVisitorInstances,
	                                                 ParkOfficeVisitorInstances,
	                                                 ParkWorkerVisitorInstances,
	                                                 ParkBasketballInstances,
	                                                 ParkFieldBallInstances,
	                                                 ParkSkateboardInstances,
	                                                 ParkBatInstances,
	                                                 WindRotorInstances,
	                                                 WindNacelleInstances,
	                                                 WindOverlayInstances,
	                                                 LiveConstructionCraneJibs,
	                                                 LiveConstructionCraneHooks,
	                                                 LiveConstructionCraneCables};
	for (const auto& Pool : SeasonalDecorationPools)
		SuppressedComponents.Add(Pool);
	for(const auto& Pair:UtilityFoundationInstances) SuppressedComponents.Add(Pair.Value);
	for (const auto& Pool : ParkSkateVisitorPools)
		SuppressedComponents.Add(Pool);
	SuppressedComponents.Add(ParkBaseballInstances);
	for (const auto& Component : ParkAnimatedVisitors)
		SuppressedComponents.Add(Component);
	// Keep response vehicles visible on safety views so a jet never lacks its engine.
	for (USceneComponent* Layer : {PoliceResponseInstances.Get(), FireResponseInstances.Get()})
	{
		if (!HeatMapVisibility.Contains(Layer))
			HeatMapVisibility.Add(Layer, Layer->IsVisible());
		Layer->SetVisibility(
		    (DiagnosticOverlay == ECityDiagnosticOverlay::Fire || DiagnosticOverlay == ECityDiagnosticOverlay::Crime) &&
		    HeatMapVisibility.FindChecked(Layer));
	}
	for (USceneComponent* Layer : SuppressedComponents)
	{
		if (!Layer)
			continue;
		if (!HeatMapVisibility.Contains(Layer))
			HeatMapVisibility.Add(Layer, Layer->IsVisible());
		Layer->SetVisibility(false);
	}
}

void ACityBoardActor::RebuildHeatMap()
{
	HeatMapSurface->ClearAllMeshSections();
	if (HeatMapResult.Dimensions != GetDimensions() || HeatMapResult.Values.Num() != GetDimensions().GetCellCount() ||
	    HeatMapResult.Status.Num() != GetDimensions().GetCellCount() || !SimulationSubsystem)
		return;
	++HeatMapRebuildCount;
	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	auto CornerColor = [&](const int32 X, const int32 Y)
	{
		FLinearColor Color(0, 0, 0, 0);
		int32 Count = 0;
		for (int32 Dy = -1; Dy <= 0; ++Dy)
			for (int32 Dx = -1; Dx <= 0; ++Dx)
				if (X + Dx >= 0 && X + Dx < GetDimensions().GetWidth() && Y + Dy >= 0 &&
				    Y + Dy < GetDimensions().GetHeight())
				{
					Color += HeatMapResult.ColorAt((Y + Dy) * GetDimensions().GetWidth() + X + Dx);
					++Count;
				}
		return Color / FMath::Max(1, Count);
	};
	auto AddVertex = [&](const double X, const double Y, const FLinearColor Color)
	{
		const double Size = CityBoardPicking::LogicalCellSizeCm;
		const double Height = SimulationSubsystem->GetTerrain().SampleHeightCm(X, Y);
		Vertices.Add(FVector(X * Size, Y * Size, Height + 140.0 + Color.GetLuminance() * 100.0));
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(X, Y));
		Colors.Add(Color);
	};
	for (int32 Y = 0; Y < GetDimensions().GetHeight(); ++Y)
		for (int32 X = 0; X < GetDimensions().GetWidth(); ++X)
		{
			const int32 Base = Vertices.Num();
			AddVertex(X, Y, CornerColor(X, Y));
			AddVertex(X + 1, Y, CornerColor(X + 1, Y));
			AddVertex(X + 1, Y + 1, CornerColor(X + 1, Y + 1));
			AddVertex(X, Y + 1, CornerColor(X, Y + 1));
			AddVertex(X + 0.5, Y + 0.5, HeatMapResult.ColorAt(Y * GetDimensions().GetWidth() + X));
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				Triangles.Add(Base + Corner);
				Triangles.Add(Base + (Corner + 1) % 4);
				Triangles.Add(Base + 4);
			}
		}
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::Service ||
	    HeatMapResult.Overlay == ECityDiagnosticOverlay::Education)
		for (const auto& Cell : HeatMapResult.RoadCells)
		{
			if (!Cell.RoadLevel)
				continue;
			const int32 Base = Vertices.Num();
			for (const FVector2D Offset :
			     {FVector2D(.05, .05), FVector2D(.95, .05), FVector2D(.95, .95), FVector2D(.05, .95)})
			{
				const double X = Cell.X + Offset.X, Y = Cell.Y + Offset.Y;
				Vertices.Add(FVector(X * CityBoardPicking::LogicalCellSizeCm, Y * CityBoardPicking::LogicalCellSizeCm,
				                     GetRoadProfileHeight(X, Y, Cell.RoadLevel) + 85.0));
				Normals.Add(FVector::UpVector);
				UVs.Add(FVector2D(X, Y));
				Colors.Add(HeatMapResult.Overlay == ECityDiagnosticOverlay::Education ? FLinearColor(.04f, .65f, 1.f)
				                                                                      : FLinearColor(.04f, .35f, 1.f));
			}
			Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
		}
	HeatMapSurface->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
}

FString ACityBoardActor::DescribeHeatMap() const
{
	if (WaterPlacementSurface && WaterPlacementSurface->IsVisible())
	{
		FString Description = TEXT("Water facility siting\n") + WaterPlacementOverlay.Legend;
		if (HeatMapPoint.IsSet())
		{
			Description += TEXT("\n") + WaterPlacementOverlay.DescribeSite(HeatMapPoint.GetValue());
			if (const auto* Road = WaterPlacementOverlay.Roads.FindByPredicate(
			        [&](const auto& R) { return R.Cell == HeatMapPoint.GetValue(); }))
				Description += TEXT("\n") + FCityWaterPlacementOverlay::DescribeRoad(*Road);
		}
		return Description;
	}
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::None)
		return FString();
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::Fire && SimulationSubsystem)
	{
		const auto& Fire = SimulationSubsystem->GetFireSnapshot();
		const FString Phase = !Fire.IsIntroduced() ? TEXT("Starts at 150 residents")
		                      : Fire.IsInGrace()   ? FString::Printf(TEXT("Warning: %lld game minutes"),
		                                                             (Fire.GraceEndStep - Fire.LastFixedStepId) * 1440 / FMath::Max(1, SimulationSubsystem->GetEconomyTuning().FixedStepsPerMonth))
		                                           : Fire.Describe();
		return Phase + TEXT("\nRed burning | amber damaged | blue repair\n") +
		       (HeatMapPoint.IsSet() ? HeatMapResult.DescribeCell(HeatMapPoint.GetValue())
		                             : TEXT("Inspect lots for protection and recovery"));
	}
	if (HeatMapResult.Overlay == ECityDiagnosticOverlay::Crime && SimulationSubsystem)
	{
		const auto& Crime = SimulationSubsystem->GetCrimeSnapshot();
		const FString Phase =
		    !Crime.IsIntroduced() ? FString::Printf(TEXT("Starts at %d residents"),
		                                            SimulationSubsystem->GetEconomyTuning().CrimePopulationThreshold)
		    : Crime.IsInGrace() ? FString::Printf(TEXT("Grace: %lld days"), Crime.GraceEndStep - Crime.LastFixedStepId)
		                        : FString::Printf(TEXT("Active incidents: %d"), Crime.ActiveIncidents);
		return FString::Printf(
		    TEXT("Crime risk: 0 dark - 100 bright\n%s | police %d/%d\nCity unemployed %d | homeless %d\n%s"), *Phase,
		    Crime.ProtectedDemand, Crime.ProtectionDemand, Crime.JobShortage, Crime.HomelessResidents,
		    *(HeatMapPoint.IsSet() ? HeatMapResult.DescribeCell(HeatMapPoint.GetValue())
		                           : TEXT("Point at developed lots for exact risk")));
	}
	return HeatMapResult.Description + TEXT("\n") + HeatMapResult.Legend + TEXT("\n") +
	       (HeatMapPoint.IsSet() ? HeatMapResult.DescribeCell(HeatMapPoint.GetValue())
	                             : TEXT("Point at the map for exact values"));
}

void ACityBoardActor::UpdateFirePresentation()
{
	if (!SimulationSubsystem || !PlacementModel)
		return;
	const auto& Disasters = SimulationSubsystem->GetDisasterSnapshot();
	const auto* Event = Disasters.Current();
	const bool bActive = Event && Disasters.LastFixedStepId >= Event->StartStep;
	const int64 ActiveId = bActive ? Event->Id : -1;
	auto* ActiveEffect =
	    bActive && Event->Kind == ECityNaturalDisaster::Tornado ? TornadoParticles.Get() : EarthquakeDust.Get();
	if (ActiveId != DisplayedDisasterId)
	{
		for (auto* P : {EarthquakeDust.Get(), TornadoParticles.Get()})
		{
			P->DeactivateSystem();
			P->ResetParticles(true);
		}
		DisplayedDisasterId = ActiveId;
		if (bActive)
			ActiveEffect->ActivateSystem(true);
	}
	const bool bMoving =
	    bActive && !SimulationSubsystem->IsSimulationPaused() && !SimulationSubsystem->IsEntryMenuSuspended();
	EarthquakeDust->SetComponentTickEnabled(bMoving && ActiveEffect == EarthquakeDust);
	TornadoParticles->SetComponentTickEnabled(bMoving && ActiveEffect == TornadoParticles);
	if (bActive)
	{
		const auto Cell = FCityDisasterSimulation::Position(*Event, Disasters.LastFixedStepId);
		const auto NextCell = FCityDisasterSimulation::Position(*Event, Disasters.LastFixedStepId + 1);
		if (Event->Profile >= 3)
		{
			const auto P = FCityDisasterSimulation::ContinuousPosition(
			    *Event, Disasters.LastFixedStepId + SimulationSubsystem->GetSimulationStepProgress());
			const double Size = CityBoardPicking::LogicalCellSizeCm, X = (P.X + .5) * Size, Y = (P.Y + .5) * Size;
			ActiveEffect->SetRelativeLocation(
			    FVector(X, Y, SimulationSubsystem->GetTerrain().SampleHeightCm(X, Y) + 50));
		}
		else
			ActiveEffect->SetRelativeLocation(FMath::Lerp(GetCellSurfaceCenter(Cell, 50),
			                                              GetCellSurfaceCenter(NextCell, 50),
			                                              SimulationSubsystem->GetSimulationStepProgress()));
		if (Event->Kind == ECityNaturalDisaster::Tornado && bMoving)
			ActiveEffect->SetRelativeRotation(FRotator(0, ProgressionAnimationSeconds * 110, 0));
	}
	const auto& Fire = SimulationSubsystem->GetFireSnapshot();
	TSet<uint64> Damaged;
	for (const auto& Pair : Fire.DamagedBuildings)
		Damaged.Add(Pair.Key);
	bool bMeshChange = Damaged.Num() != DisplayedDamagedIds.Num();
	for (const uint64 Id : Damaged)
		bMeshChange |= !DisplayedDamagedIds.Contains(Id);
	if (bMeshChange)
	{
		DisplayedDamagedIds = Damaged;
		RebuildAllPresentation();
	}
	FireDamageMarkers->ClearInstances();
	TArray<uint64> Burning;
	for (const auto& I : Fire.RecentIncidents)
		if (I.ResolvedStep < 0)
			Burning.Add(I.EntityId);
	Burning.Sort();
	for (int32 Index = 0; Index < FireParticlePools.Num(); ++Index)
	{
		auto* Flames = FireParticlePools[Index].Get();
		auto* Water = WaterParticlePools[Index].Get();
		const uint64 Id = Burning.IsValidIndex(Index) ? Burning[Index] : 0;
		if (ParticleFireIds[Index] != Id)
		{
			Flames->DeactivateSystem();
			Water->DeactivateSystem();
			Flames->ResetParticles(true);
			Water->ResetParticles(true);
			ParticleFireIds[Index] = Id;
			if (Id)
				Flames->ActivateSystem(true);
		}
		const bool bPaused = SimulationSubsystem->IsSimulationPaused();
		Flames->SetComponentTickEnabled(Id && !bPaused);
		Water->SetComponentTickEnabled(Id && !bPaused);
		if (!Id)
			continue;
		const auto* Entity = PlacementModel->GetEntities().Find(Id);
		if (!Entity)
			continue;
		FVector Center = GetCellSurfaceCenter(Entity->Anchor);
		Center += FVector((Entity->FootprintSize.X - 1) * 400, (Entity->FootprintSize.Y - 1) * 400, 0);
		FBox Bounds;
		FTransform Transform;
		if (GetInspectionMesh(Id, Bounds, Transform))
			Center.Z = GetActorTransform()
			               .InverseTransformPosition(Transform.TransformPosition(FVector(0, 0, Bounds.Max.Z)))
			               .Z;
		Flames->SetRelativeLocation(Center);
		bool bSpraying = false;
		if (Fire.ResponseStations.Contains(Id) && FireResponseInstances->IsVisible())
		{
			const auto* Trip = SimulationSubsystem->GetPopulationSnapshot().TransitTrips.FindByPredicate(
			    [&](const auto& T)
			    { return T.Purpose == ECityTripPurpose::FireResponse && T.DestinationEntityId == Id && T.bCompleted; });
			if (Trip)
				for (const auto& V : SimulationSubsystem->GetTrafficSnapshot().Vehicles)
					if (V.bTransitTrip && V.SourceId == Trip->Id && V.bCompleted)
					{
						FVector Pos, Direction;
						const auto& Current = SimulationSubsystem->GetTrafficSnapshot();
						const auto& Previous = SimulationSubsystem->GetPreviousTrafficSnapshot();
						const auto* Before =
						    Previous.NetworkFingerprint == Current.NetworkFingerprint
						        ? Previous.Vehicles.FindByPredicate([&](const auto& P) { return P.Id == V.Id; })
						        : nullptr;
						if (!CityEmergencyPresentation::SampleArrivedVehicle(
						        V, Before, SimulationSubsystem->GetRoadNetworkSnapshot(),
						        SimulationSubsystem->GetSimulationStepProgress(), Pos, Direction))
							break;
						FVector Origin((Pos.X + .5) * 800, (Pos.Y + .5) * 800,
						               GetRoadProfileHeight(Pos.X + .5, Pos.Y + .5, Pos.Z > 0 ? 1 : 0) + 165);
						Origin +=
						    FVector(-Direction.Y, Direction.X, 0) *
						    FCityTrafficSimulation::SampleMotorLaneOffsetCm(
						        V, Before, SimulationSubsystem->GetSimulationStepProgress(),
						        PlacementModel->GetRoadDefinitionId(FCityCellCoord(
						            FMath::RoundToInt(Pos.X), FMath::RoundToInt(Pos.Y), FMath::RoundToInt(Pos.Z))));
						const FVector Jet = Center - Origin;
						Water->SetRelativeLocation(Origin);
						Water->SetRelativeRotation(Jet.Rotation());
						Water->SetRelativeScale3D(FVector(FMath::Max(.2, Jet.Size() / 650), 1, 1));
						bSpraying = true;
						break;
					}
		}
		if (bSpraying && !Water->IsActive())
			Water->ActivateSystem(true);
		if (!bSpraying && Water->IsActive())
		{
			Water->DeactivateSystem();
			Water->ResetParticles(true);
		}
	}
}

bool ACityBoardActor::StartNaturalDisasterTest(const bool bTornado)
{
	return SimulationSubsystem && SimulationSubsystem->StartDisasterTest(bTornado ? ECityNaturalDisaster::Tornado
	                                                                              : ECityNaturalDisaster::Earthquake);
}
void ACityBoardActor::SetCityPaused(const bool bPaused)
{
	if (SimulationSubsystem)
		SimulationSubsystem->SetSimulationPaused(bPaused);
}

void ACityBoardActor::UpdateWindRotors(const float DeltaSeconds)
{
	if (!SimulationSubsystem || !PlacementModel || !BuildingCatalog)
	{
		WindRotorInstances->ClearInstances();
		WindNacelleInstances->ClearInstances();
		WindRotorMotion.Reset();
		WindRotorIds.Reset();
		WindRotorGeneration = MAX_uint64;
		return;
	}
	if (WindRotorGeneration != SimulationSubsystem->GetPresentationGeneration())
	{
		WindRotorGeneration = SimulationSubsystem->GetPresentationGeneration();
		WindRotorIds.Reset();
		for (const auto& Pair : PlacementModel->GetEntities())
		{
			const auto* State = SimulationSubsystem->GetDevelopmentSnapshot().Find(Pair.Key);
			if (Pair.Value.DefinitionId == TEXT("WindPower") && State &&
			    State->Stage == ECityDevelopmentStage::Operational &&
			    !SimulationSubsystem->GetFireSnapshot().DamagedBuildings.Contains(Pair.Key))
				WindRotorIds.Add(Pair.Key);
		}
		WindRotorIds.Sort();
		for (auto It = WindRotorMotion.CreateIterator(); It; ++It)
			if (!WindRotorIds.Contains(It.Key()))
				It.RemoveCurrent();
	}
	const auto& Ids = WindRotorIds;
	while (WindRotorInstances->GetInstanceCount() > Ids.Num())
		WindRotorInstances->RemoveInstance(WindRotorInstances->GetInstanceCount() - 1);
	while(WindNacelleInstances->GetInstanceCount()>Ids.Num())
		WindNacelleInstances->RemoveInstance(WindNacelleInstances->GetInstanceCount()-1);
	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		const uint64 Id = Ids[Index];
		auto& Motion = WindRotorMotion.FindOrAdd(Id);
		if (!SimulationSubsystem->IsSimulationPaused())
			Motion.Advance(DeltaSeconds, SimulationSubsystem->GetEnvironmentSnapshot().WindStrength);
		const auto& Entity = PlacementModel->GetEntities().FindChecked(Id);
		const auto* Definition = BuildingCatalog->FindDefinition(Entity.DefinitionId);
		const auto Presentation=ResolvePresentationDefinition(Id,*Definition);
		const FTransform Base=MakeBuildingTransform(Entity.Anchor,Presentation,Entity.QuarterTurnsClockwise);
		// Measured yaw-bearing midpoint and axle offset from the pinned three-role
		// source. The reflected basis changes positive spin to native +Y.
		const FTransform Nacelle(FRotator(0,Atmosphere->GetMotion().WindYaw-90.,0),
		    Base.TransformPosition(FVector(0,0,2116.499996185303)));
		const FTransform Transform=Motion.Transform(FVector(0,290,83.500003814697))*Nacelle;
		if(Index<WindNacelleInstances->GetInstanceCount())
			WindNacelleInstances->UpdateInstanceTransform(Index,Nacelle,false,false,true);
		else WindNacelleInstances->AddInstance(Nacelle);
		if (Index < WindRotorInstances->GetInstanceCount())
			WindRotorInstances->UpdateInstanceTransform(Index, Transform, false, false, true);
		else
			WindRotorInstances->AddInstance(Transform);
	}
	if (!Ids.IsEmpty())
	{
		WindRotorInstances->MarkRenderStateDirty();
		WindNacelleInstances->MarkRenderStateDirty();
	}
}
