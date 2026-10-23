#pragma once
#include "Presentation/CityCitizenWalkMotion.h"

#include "Presentation/CityWindRotorMotion.h"
#include "CoreMinimal.h"
#include "Commands/CityPlacementModel.h"
#include "GameFramework/Actor.h"
#include "Presentation/CityInstanceIndex.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityWaterPlacementOverlay.h"
#include "Presentation/CitySeasonalDecorations.h"
#include "Simulation/CityGrid.h"
#include "Simulation/CityTerrain.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Presentation/CityDisasterAlarm.h"
#include "CityBoardActor.generated.h"

class APlayerController;
class UCityBuildingCatalog;
class UCitySimulationSubsystem;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimSequence;
class UAudioComponent;
class UParticleSystem;
class UParticleSystemComponent;
class UCityAtmosphereComponent;
class UCitySoundscapeComponent;
class UCityMusicComponent;
struct FCityDesktopModelSource;

UENUM(BlueprintType)
enum class ECityToolMode : uint8
{
	Inspect,
	Road,
	Home,
	Shop,
	Park,
	Power,
	Service,
	Zone,
	Demolish,
	Police,
	Fire,
	School,
	Repair,
	TestFire,
	TestCrime,
	TestArson,
	NeighborhoodPark,
	CityPark,
	BasketballPark,
	SkatePark,
	BaseballPark,
	SoccerPark,
	Hospital,
	TestSickness,
	TestSevereSickness,
	WasteDepot,
	Recycling,
	TestWaste,
	Technology
};

struct FCityInspectionMetric
{
	FString Label;
	int64 Value = 0;
	int64 Maximum = 0;
};

struct FCityInfoCard
{
	FString Title;
	FString Labels;
	FString Values;
};

UCLASS()
class CITYSIMVR_API ACityBoardActor : public AActor
{
	GENERATED_BODY()
    friend class FCityDesktopContinuityCommand;
	friend class FCityPlacementPreviewStudyCommand;
	friend class FCityPlacementFeedbackTest;
	friend class FCityTutorialLifecycleTest;
	friend class FCityTutorialWalkthroughCommand;
	friend class FCityQuarterlyNewsFixture;
	friend class FCityLowDensityLifecycleTest;
	friend class FCityDesktopLowDensityCommand;

public:
	UFUNCTION(BlueprintPure, Category = "City Board")
	UCitySimulationSubsystem* GetCitySimulation() const
	{
		return SimulationSubsystem;
	}
	UFUNCTION(BlueprintCallable, Category = "City Board|Lighting")
	void SetLightingPreset(ECityLightingPreset Preset);
	ECityLightingPreset GetLightingPreset() const;
	ACityBoardActor();

	UFUNCTION(BlueprintCallable, Category = "City Board|Safety Tests")
	bool StartNaturalDisasterTest(bool bTornado);
	UFUNCTION(BlueprintCallable, Category = "City Board")
	void SetCityPaused(bool bPaused);
	UFUNCTION(BlueprintCallable, Category = "City Board|Audio")
	bool PreviewDisasterAlarm();
	UFUNCTION(BlueprintPure, Category = "City Board|Audio")
	int32 GetDisasterAlarmPlayCount() const
	{
		return DisasterAlarmPlayCount;
	}
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void PrepareCityView();

	UFUNCTION(BlueprintCallable, Category = "City Board")
	bool PickCellFromWorldRay(const FVector& WorldRayOrigin, const FVector& WorldRayDirection,
	                          FCityCellCoord& OutCell) const;

	UFUNCTION(BlueprintCallable, Category = "City Board")
	bool PickCellFromPlayerController(APlayerController* PlayerController, FCityCellCoord& OutCell) const;

	UFUNCTION(BlueprintCallable, Category = "City Board")
	void SetHoveredCell(const FCityCellCoord& Cell, bool bVisible);

	void SetCommandInputBound(bool bBound)
	{
		bCommandInputBound = bBound;
	}
	void SetManipulationActive(bool bActive);
	bool IsManipulationActive() const
	{
		return bManipulationActive;
	}
	void SetControllerRay(const FVector& WorldRayOrigin, const FVector& WorldRayDirection, bool bActive,
	                      bool bSuppressMouseFallback = false);
	UFUNCTION(BlueprintCallable, Category = "City Board")
	void SetToolMode(ECityToolMode NewToolMode);
	bool TryToggleWindOverlay();
	UFUNCTION(BlueprintCallable, Category = "City Board|Commands")
	void RequestConfirm()
	{
		if (!bManipulationActive)
			bConfirmRequested = true;
	}
	UFUNCTION(BlueprintCallable, Category = "City Board|Commands")
	void RequestCancel()
	{
		if (!bManipulationActive)
			bCancelRequested = true;
	}
	bool HasActiveToolAction() const
	{
		return ZoneStartCell.IsSet() || RoadPreview.bActive || PlacementPreview.bActive;
	}
	UFUNCTION(BlueprintCallable, Category = "City Board|Commands")
	void RequestRotate()
	{
		if (!bManipulationActive)
			bRotateRequested = true;
	}
	FString DescribeCell(const FCityCellCoord& Cell) const;
	FString DescribeCitySummary() const;
	FString DescribeCityTotals() const;
	FString DescribeConstructionCost(FName DefinitionId) const;
	FString GetConstructionStatus() const;
	bool GetConstructionMarkerAnchor(FVector& OutAnchor) const;
	TArray<FCityInfoCard> GetCityInfoCards() const;
	bool IsConstructionPriceBlocked() const;
	void GetCityHudTrends(int64& OutMonthlyMoney, int64& OutPopulationChange, bool& bOutPopulationHistory) const;
	FString DescribeNeedsPanel() const;
	FString DescribeCalendar(bool bIncludeTime = true) const;
	FString DescribeHeatMap() const;
	FString DescribeDesktopToolContext() const;
	FString DescribeEconomy() const;
	FString GetToolDisplayName() const;
	ECityToolMode GetToolMode() const
	{
		return ToolMode;
	}
	void SetSelectedZone(ECityZoneType NewZone);
	ECityZoneType GetSelectedZone() const
	{
		return SelectedZone;
	}
	void SetSelectedRoadDefinition(FName DefinitionId);
	UFUNCTION(BlueprintCallable, Category = "CitySim|Tools")
	void SetSelectedFacility(FName DefinitionId);
	FName GetSelectedFacilityId() const
	{
		const auto* D = GetSelectedDefinition();
		return D ? D->DefinitionId : NAME_None;
	}

	FName GetSelectedRoadDefinition() const
	{
		return SelectedRoadDefinitionId;
	}
	void SetSelectedRoadProfile(ECityRoadProfile Profile);
	void AdjustRoadElevation(int32 Delta);
	void RefreshRoadDrawingProfile();
	int32 GetSelectedRoadElevationStep() const
	{
		return SelectedRoadElevationStep;
	}
	ECityRoadProfile GetSelectedRoadProfile() const
	{
		return SelectedRoadProfile;
	}
	FString GetRoadConstructionStatus() const
	{
		return RoadConstructionStatus;
	}

	UFUNCTION(BlueprintCallable, Category = "CitySim|Diagnostics")
	void SetDiagnosticOverlay(ECityDiagnosticOverlay NewOverlay);
	ECityDiagnosticOverlay GetDiagnosticOverlay() const
	{
		return DiagnosticOverlay;
	}
	// Read-only presentation queries; rendered bounds never author simulation state.
	bool PickInspectionBuilding(const FVector& Origin, const FVector& Direction, uint64& OutEntityId,
	                            FVector& OutHit) const;
	bool GetBuildingMarkerAnchor(uint64 EntityId, FVector& OutRoof) const;
	bool GetInspectionBuilding(uint64 EntityId, FVector& OutRoof, FString& OutDetail,
	                           TArray<FCityInspectionMetric>* OutMetrics = nullptr,
	                           TArray<struct FCityUiInspectionSection>* OutSections = nullptr) const;
	bool GetDesktopModelSource(uint64 EntityId, FCityDesktopModelSource& Out) const;
	bool HasCurrentCityModel() const;
	FCityGridDimensions GetDimensions() const;
	void SetInspectionEntity(uint64 EntityId)
	{
		InspectionEntityId = EntityId;
		bSuppressLinkedOverlay = false;
	}

private:
	friend class FCityWaterCaptureCommand;
	friend class FCityDesktopServiceGuidanceCommand;
	friend class FCitySchoolReachPresentationTest;
	friend class FCityTechnologyLiveTest;
    friend class FCityAiLiveTest;
    friend class FCityDesktopAiCommand;
	friend class FCitySoundscapeRoutingTest;
	friend class FCityRuntimeAssetOrganizationTest;
	friend class FCityDimensionsPresentationTest;
	friend class FCityShorelineCaptureCommand;
	friend class FCityCitizenPresentationTest;
	friend class FCityHousingFeedbackFixture;
	void ConfigureCitizenPools();
	virtual void PostInitializeComponents() override;
	UPROPERTY()
	TObjectPtr<UStaticMesh> CitizenPresentationMeshes[3];
	bool bRefinedCitizens = false;
	friend class FCityStorefrontPresentationTest;
	friend class FCityAtmospherePresentationTest;
	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UCityAtmosphereComponent> Atmosphere;
	friend class FCityStationUpgradeTest;
	friend class FCityHospitalIntegrationTest;
	friend class FCityWasteIntegrationTest;
	friend class FCityConditionClusterPresentationTest;
	friend class FCityRaisedBranchTest;
	friend class FCityMixedRaisedTest;
	friend class FCitySelectionHighlightTest;
	friend class FCityAlternativeIntegrationTest;
	friend class FCityPanelsTest;
	friend class FCityHappinessIntegrationTest;
	friend class FCityVisibleTrafficTest;
	friend class FCityFlowVisualCommand;
	friend class FCityGroundPlotStudyCommand;
    friend class FCityApprovedCivicStudyCommand;
    friend class FCityApprovedCivicGroundTest;
	friend class FCityRoadStructureTest;
	friend class FCityRoadHeightTest;
	friend class FCityRoadUnderpassTest;
	friend class FCityEmergencySprayTest;
	friend class FCityTerrainPresentationTest;
	friend class FCityTerrainBaseTest;
	friend class FCityHeatMapTest;
	friend class FCityZoneVisibilityTest;
	friend class FCityZoneVisibilityCaptureCommand;
	friend class FCityWaterPlacementPresentationTest;
	friend class FCitySeasonalDecorationPresentationTest;
	friend class FCityParkIntegrationTest;
	friend class FCityParkActivityTest;
	friend class FCityParkTerrainTest;
	friend class FCityBuildingFoundationTest;
	friend class FCityResidentialGroundTest;
	friend class FCityCommercialGroundTest;
    friend class FCityIndustrialGroundTest;
    friend class FCityCivicGroundTest;
    friend class FCityUtilityGroundTest;
    friend class FCityApprovedUtilityGroundTest;
	friend class FCityConditionPresentationTest;
	friend class FCityWindRotorTest;
	void UpdateWindRotors(float DeltaSeconds);
	TMap<uint64, FCityWindRotorMotion> WindRotorMotion;
	TArray<uint64> WindRotorIds;
	uint64 WindRotorGeneration = MAX_uint64;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> WindRotorInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> WindNacelleInstances;
	UPROPERTY()
	TObjectPtr<UStaticMesh> WindBodyMesh;
	void UpdateParkActivity(float DeltaSeconds);
	void CreateSeasonalDecorationPools();
	void UpdateSeasonalDecorations();
	void ClearSeasonalDecorations();
	void UpdateSeasonalBuildingMeshes(int32 Month);
	UStaticMesh* GetSeasonalParkMesh(FName DefinitionId, int32 Month) const;
	UPROPERTY()
	TObjectPtr<UStaticMesh> SeasonalResidentialMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> PocketParkMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> PocketParkDecemberMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> NeighborhoodParkMesh;
	UPROPERTY()
	TObjectPtr<UStaticMesh> NeighborhoodParkDecemberMesh;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SeasonalDecorationPools;
	TArray<FCitySeasonalDecorationSample> SeasonalDecorationSamples;
	uint32 SeasonalDecorationCheckSignature = MAX_uint32;
	uint32 SeasonalDecorationSelectionSignature = MAX_uint32;
	int32 SeasonalDecorationRebuildCount = 0;
	bool bSeasonalDecorationsAnalytical = false;
	double ParkActivitySeconds = 0.0;
	uint64 ParkActivityGeneration = MAX_uint64;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkVisitorInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkOfficeVisitorInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkWorkerVisitorInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkBasketballInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkFieldBallInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkSkateboardInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkBatInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ParkBaseballInstances;
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ParkSkateVisitorPools;
	UPROPERTY()
	TObjectPtr<UStaticMesh> ParkCitizenMeshes[6];
	UPROPERTY()
	TObjectPtr<USkeletalMesh> ParkCitizenSkeletonMeshes[6];
	UPROPERTY()
	TObjectPtr<UAnimSequence> ParkCitizenAnimations[14];
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> ParkAnimatedVisitors;
	TArray<int32> ParkAssignedAnimations;
	TArray<float> ParkAssignedTimes;
	UPROPERTY()
	TObjectPtr<class UBlendSpace1D> StreetWalkBlend;
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> StreetAnimatedWalkers;
	TMap<uint64, FCityCitizenWalkMotion> StreetWalkStates;
	TArray<uint64> StreetAssignedIds;
	TArray<float> StreetAssignedPhases, StreetAssignedWeights;
	uint64 StreetWalkGeneration = MAX_uint64, StreetWalkNetwork = 0;
	double StreetPoseClock = -1;
	bool bStreetSkeletalAnimation = true;
	void RenderStreetWalkers(const TArray<FCityStreetWalkerSample>& Samples, TArray<FTransform>& Casual,
	                         TArray<FTransform>& Office, TArray<FTransform>& Worker);
	friend class FCityCitizenWalkRuntimeTest;
	friend class FCityCitizenWalkVisualCommand;
	double ParkPoseAccumulator = 0;
	bool bParkSkeletalAnimation = true;
	bool bParkBenchmarkMotion = false;
	friend class FCityParkRigIntegrationTest;
	friend class UCityParkAssetLibrary;
	friend class UCitySoundscapeComponent;
	void UpdateDisasterAlarm();
	void StopDisasterAlarm();
	UPROPERTY()
	TObjectPtr<UAudioComponent> DisasterAlarmAudio;
	UPROPERTY()
	TObjectPtr<UCitySoundscapeComponent> Soundscape;
	UPROPERTY()
	TObjectPtr<UCityMusicComponent> Music;
	FCityDisasterAlarmGate DisasterAlarmGate;
	uint64 ObservedAlarmSession = MAX_uint64;
	int32 DisasterAlarmPlayCount = 0;
	bool bAlarmBackgrounded = false, bAlarmPreview = false;
	double AlarmPreviewEndSeconds = 0;
	FDelegateHandle AlarmBackgroundHandle, AlarmForegroundHandle;
	void UpdateFirePresentation();
	int64 DisplayedFireStep = -1;
	uint64 DisplayedFireRevision = MAX_uint64;
	uint64 DisplayedFireGeneration = MAX_uint64;
	UPROPERTY()
	TObjectPtr<UParticleSystem> FireParticleTemplate;
	UPROPERTY()
	TObjectPtr<UParticleSystem> WaterParticleTemplate;
	UPROPERTY()
	TArray<TObjectPtr<UParticleSystemComponent>> FireParticlePools;
	UPROPERTY()
	TArray<TObjectPtr<UParticleSystemComponent>> WaterParticlePools;
	UPROPERTY()
	TObjectPtr<UParticleSystemComponent> EarthquakeDust;
	UPROPERTY()
	TObjectPtr<UParticleSystemComponent> TornadoParticles;
	int64 DisplayedDisasterId = -1;
	TArray<uint64> ParticleFireIds;
	TSet<uint64> DisplayedDamagedIds;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> FireDamageMarkers;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> HeatMapSurface;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> WaterPlacementSurface;
	FCityWaterPlacementOverlaySnapshot WaterPlacementOverlay;
	uint32 WaterPlacementSignature = MAX_uint32;
	uint32 WaterPlacementContentSignature = MAX_uint32;
	int32 WaterPlacementRebuildCount = 0;
	void UpdateWaterPlacementOverlay();
	void ClearWaterPlacementOverlay();
	void RebuildWaterPlacementOverlay();
	FCityDiagnosticOverlayResult HeatMapResult;
	TOptional<FCityCellCoord> HeatMapPoint;
	TMap<TWeakObjectPtr<USceneComponent>, bool> HeatMapVisibility;
	uint32 HeatMapContentSignature = MAX_uint32;
	int32 HeatMapRebuildCount = 0;
	bool bSuppressLinkedOverlay = false;
	void SetHeatMapLayers(bool bActive);
	void RebuildHeatMap();
	bool GetInspectionMesh(uint64 EntityId, FBox& OutBounds, FTransform& OutTransform) const;
	uint64 InspectionEntityId = FCityGrid::EmptyEntityId;
	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<USceneComponent> BoardRoot;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> BoardSurface;
	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> TerrainBase;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TerrainMaterial;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> WaterSurface;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ShoreFoamSurface;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ShoreSplashSurface;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WaterEffectsMaterials;
	uint64 WaterEffectsRevision = MAX_uint64;
	void UpdateWaterEffects();

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> FoundationInstances;
	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ResidentialFoundationInstances;
	UPROPERTY()
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> UtilityFoundationInstances;
	UPROPERTY()
	TObjectPtr<UStaticMesh> GroundPilotResidentialMesh;

	UPROPERTY()
    TMap<FName, TObjectPtr<UStaticMesh>> BuildingGroundMeshes;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ConcreteMaterial;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> BuildingEntranceProfiles;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UStaticMeshComponent> HoverHighlight;
	UPROPERTY(VisibleAnywhere, Category = "City Board|Selection")
	TObjectPtr<UInstancedStaticMeshComponent> SelectionOutlineInstances;
	UPROPERTY(VisibleAnywhere, Category = "City Board|Selection")
	TObjectPtr<UInstancedStaticMeshComponent> SelectionRetainedInstances;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SelectionOutlineMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SelectionRetainedMaterial;
	void UpdateSelectionHighlight(const FCityCellCoord& Cell, bool bVisible);
	void ClearSelectionHighlight();
	TOptional<FCityCellCoord> SelectionHighlightCell;
	const FCityPlacementModel* SelectionHighlightModel = nullptr;
	uint64 SelectionHighlightRevision = MAX_uint64, SelectionHighlightGeneration = 0;
	ECityDiagnosticOverlay SelectionHighlightOverlay = ECityDiagnosticOverlay::None;
	ECityToolMode SelectionHighlightTool = ECityToolMode::Inspect;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UStaticMeshComponent> EntranceMarker;
	void UpdateEntranceGuidance();

	// Read-only weather instrument. Pointing the active board ray at it reveals
	// the current downwind arrows without making weather presentation authoritative.
	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindVanePost;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindVaneArrow;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalEast;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalSouth;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalWest;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindVaneHead;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalNorthHead;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalEastHead;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalSouthHead;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UStaticMeshComponent> WindCardinalWestHead;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Environment")
	TObjectPtr<UInstancedStaticMeshComponent> WindOverlayInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UStaticMeshComponent> PlacementGhost;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInstancedStaticMeshComponent> PlacementFootprintInstances;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PlacementFootprintMaterial;

	// Temporary BuildingPresentation proof of the authored construction/low/medium/high visual
	// pipeline. It is presentation-only and does not imply density simulation.
	UPROPERTY(EditAnywhere, Category = "City Board|Development")
	bool bShowProgressionShowcase = false;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionConstructionSite;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionCraneJib;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionCraneHook;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionResidentialLow;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionResidentialMedium;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UStaticMeshComponent> ProgressionResidentialHigh;

	// Two shared instance pools animate attachments for all authoritative live
	// construction lots without introducing per-building actors or components.
	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UInstancedStaticMeshComponent> LiveConstructionCraneJibs;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Development")
	TObjectPtr<UInstancedStaticMeshComponent> LiveConstructionCraneHooks;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> LiveConstructionCraneCables;

	UPROPERTY(EditInstanceOnly, Category = "City Board")
	TObjectPtr<UCityBuildingCatalog> BuildingCatalog;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> BuildingInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> RoadSurfaceProfile;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadSupportInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadCornerInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadTInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadCrossInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Roads")
	TObjectPtr<UInstancedStaticMeshComponent> DirtRoadSurfaceInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Roads")
	TObjectPtr<UInstancedStaticMeshComponent> LocalRoadSurfaceInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Roads")
	TObjectPtr<UInstancedStaticMeshComponent> AvenueRoadSurfaceInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Roads")
	TObjectPtr<UInstancedStaticMeshComponent> HighwayRoadSurfaceInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Roads")
	TObjectPtr<UInstancedStaticMeshComponent> RoadMarkingInstances;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> SidewalkSurface;
	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UProceduralMeshComponent> RoadDetailSurface;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> BenchmarkVehicleInstances;

	// Pooled presentation only: these instances bind to persistent trip records
	// and are rebuilt from deterministic route progress. They never simulate AI.
	UPROPERTY(VisibleAnywhere, Category = "City Board|Traffic")
	TObjectPtr<UInstancedStaticMeshComponent> CitizenInstances;
	UPROPERTY(VisibleAnywhere, Category = "City Board|Traffic")
	TObjectPtr<UInstancedStaticMeshComponent> OfficeCitizenInstances;
	UPROPERTY(VisibleAnywhere, Category = "City Board|Traffic")
	TObjectPtr<UInstancedStaticMeshComponent> WorkerCitizenInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Traffic")
	TObjectPtr<UInstancedStaticMeshComponent> TrafficVehicleInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board|Traffic")
	TObjectPtr<UInstancedStaticMeshComponent> TrafficTruckInstances;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> GarbageTruckInstances;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> RecyclingTruckInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> PoliceResponseInstances;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> FireResponseInstances;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> AmbulanceResponseInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ResidentialZoneInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> CommercialZoneInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> IndustrialZoneInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ZonePreviewInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ZoneSkippedPreviewInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> RoadPreviewInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ServiceCoverageInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ServiceServedParcelInstances;

	UPROPERTY(VisibleAnywhere, Category = "City Board")
	TObjectPtr<UInstancedStaticMeshComponent> ServiceUnservedParcelInstances;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaceholderBuildingMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ValidPreviewMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InvalidPreviewMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlacedBuildingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ZoneBaseMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> PavementBaseMaterial;

	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInterface>> OverlayFadeMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ResidentialZoneMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CommercialZoneMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> IndustrialZoneMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ServiceCoverageMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WindNorthMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WindCardinalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DirtRoadMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LocalRoadMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AvenueRoadMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighwayRoadMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SidewalkMaterial;

	const FCityBuildingDefinition* GetSelectedDefinition() const;
	void SelectDefinition(int32 DefinitionIndex);
	void UpdatePlacementPreview(APlayerController* PlayerController, const FCityCellCoord& HoveredCell,
	                            bool bHasHoveredCell, bool bConfirm, bool bCancel, bool bRotate);
	void UpdateRoadTool(APlayerController* PlayerController, const FCityCellCoord& HoveredCell, bool bHasHoveredCell,
	                    bool bConfirm, bool bCancel);
	void UpdateZoneTool(const FCityCellCoord& HoveredCell, bool bHasHoveredCell, bool bConfirm, bool bCancel,
	                    bool bRotate);
	void HandleDemolition(const FCityCellCoord& HoveredCell);
	void UpdatePlacementFootprint(const FCityBuildingDefinition& Definition, bool bValid);
	void UpdateGhost(const FCityBuildingDefinition& Definition, const FCityPlacementResult& Validation);
	void UpdatePlacementStatus(const FCityBuildingDefinition& Definition, const FCityPlacementResult& Validation);
	void RenderPlacement(uint64 EntityId, const FCityPlacedEntity& Entity, const FCityBuildingDefinition& Definition);
	void RebuildRoadInstances();
	void RebuildBenchmarkVehicles();
	void UpdatePopulationTraffic();
	void RebuildCrosswalkPaint();
	FVector PedestrianPointToLocal(const FIntVector& P) const;
	double PedestrianSurfaceHeight(const FVector& P, int32 Level) const;
	bool SampleEntranceAwareWalker(const struct FCityTrafficVehicleState& Vehicle,
	    const struct FCityTrafficVehicleState* Previous, double Alpha, FVector& Location, FVector& Direction) const;
	TMap<uint64, TArray<FVector>> CitizenEntrancePaths;
	void RebuildZoneInstances();
	void UpdateZoneVisibility();
	void UpdateAreaPreview(UInstancedStaticMeshComponent* Instances, const TArray<FCityCellCoord>& Cells, bool bValid,
	                       ECityZoneType ZoneType);
	void UpdateServiceCoverage(const FCityCellCoord& HoveredCell, bool bHasHoveredCell);
	void UpdateWindPresentation();

	bool IsPointingAtWindVane() const;
	void ClearServiceCoverage();
	void SetBuildingsFaded(bool bFaded);
	void ApplyBuildingFade(UInstancedStaticMeshComponent& Instances) const;
	void ClearGesturePreviews();
	void UpdateProgressionShowcase(float DeltaSeconds);
	void RebuildLiveConstructionAnimations();
	void UpdateLiveConstructionAnimations(float DeltaSeconds);
	bool IsLiveConstructionAnimated(uint64 EntityId) const;
	void UpdateLiveConstructionTransforms(float DeltaSeconds);
	void RenderPendingGrowth();
	void RebuildAllPresentation();
	void RebuildTerrainSurface(bool bRefineBanks = true, bool bSmoothBankShading = true);
	void RebuildTerrainBase();
	void RebuildWaterSurface();
	void RemoveRenderedEntity(uint64 EntityId);
	UInstancedStaticMeshComponent* GetOrCreateInstances(const FCityBuildingDefinition& Definition);
	void ApplyPrivateGroundPresentation(FCityBuildingDefinition& Result) const;
	FCityBuildingDefinition ResolvePresentationDefinition(uint64 EntityId,
	                                                      const FCityBuildingDefinition& Definition) const;
	FTransform MakeBuildingTransform(const FCityCellCoord& Anchor, const FCityBuildingDefinition& Definition,
	                                 int32 QuarterTurnsClockwise) const;
	FVector GetCellSurfaceCenter(const FCityCellCoord& Cell, double OffsetCm = 0.0) const;
	double GetRoadWidthAt(const FCityCellCoord& Cell, bool bAlongX, double Along, bool bSidewalk = false) const;
	double GetRoadProfileHeight(double CellX, double CellY, int32 RoadLevel = 0) const;
	double GetBuildingBaseHeight(const FCityCellCoord& Anchor, const FCityBuildingDefinition& Definition,
	                             int32 QuarterTurnsClockwise) const;
	bool FindEntranceConnection(const FCityCellCoord& Anchor, const FCityBuildingDefinition& Definition,
	                            int32 QuarterTurnsClockwise, FVector& OutDoorLocal, FVector& OutRoadLocal) const;
	void RebuildBuildingEntranceProfiles();
    bool FindVehicleBayConnection(const FCityPlacedEntity& Entity,const FCityBuildingDefinition& Presentation,
        const struct FCityIndustrialBay& Bay,FVector& Start,FVector& End) const;
	bool FindHomeWalkConnection(const FCityPlacedEntity& Entity, const FCityBuildingDefinition& Presentation,
	                            FVector& Door, FVector& Sidewalk) const;
	double SampleSidewalkSurface(const FVector& Position) const;
	void UpdateConcreteMaterial();

	bool bLoggedFirstHover = false;
	bool bRoadToolSelected = false;
	bool bCommandInputBound = false;
	bool bManipulationActive = false;
	bool bControllerRayActive = false;
	bool bSuppressDesktopRay = false;
	bool bConfirmRequested = false;
	bool bCancelRequested = false;
	bool bRotateRequested = false;
	FVector ControllerRayOrigin = FVector::ZeroVector;
	FVector ControllerRayDirection = FVector::ForwardVector;
	ECityToolMode ToolMode = ECityToolMode::Inspect;
	int32 SelectedDefinitionIndex = 0;
	ECityZoneType SelectedZone = ECityZoneType::Residential;
	FName SelectedRoadDefinitionId = TEXT("LocalRoad");
	ECityRoadProfile SelectedRoadProfile = ECityRoadProfile::Ground;
	int32 SelectedRoadElevationStep = 0;
	FString RoadConstructionStatus;
	bool bConstructionPriceBlocked = false;
	FString BuildingConstructionStatus;
	ECityDiagnosticOverlay DiagnosticOverlay = ECityDiagnosticOverlay::None;
	UPROPERTY(Transient)
	TObjectPtr<UCitySimulationSubsystem> SimulationSubsystem;
	FCityPlacementModel* PlacementModel = nullptr;
	FCityGridDimensions WindVaneDimensions;
	FCityPlacementPreview PlacementPreview;
	FCityRoadPreview RoadPreview;
	TOptional<FCityCellCoord> ZoneStartCell;
	uint64 DisplayedServiceCoverageEntityId = FCityGrid::EmptyEntityId;
	uint64 DisplayedServiceCoverageRevision = MAX_uint64;
	ECityDiagnosticOverlay DisplayedDiagnosticOverlay = ECityDiagnosticOverlay::None;
	uint32 DisplayedDiagnosticSignature = MAX_uint32;
	bool bBuildingsFaded = false;
	bool bWindOverlayVisible = false;
	bool bWindOverlayRequested = false;
	uint32 DisplayedWindSignature = MAX_uint32;

	float ProgressionAnimationSeconds = 0.0f;
	TArray<uint64> AnimatedConstructionEntityIds;
	FCityInstanceIndex InstanceIndex;
	uint64 ObservedPresentationGeneration = 0;
	uint64 InteractionSessionGeneration = 0;
	friend class FCityTerrainPresentationCacheTest;
	friend class FCityCrosswalkGeometryTest;
	friend class FCityCrosswalkCaptureCommand;
	TOptional<FCityTerrainGenerationParameters> RenderedTerrainParameters;
	uint64 RenderedTerrainSession = MAX_uint64;
	void RefreshTerrainPresentation();
	TOptional<FCityCellCoord> CanceledAtCell;
};
