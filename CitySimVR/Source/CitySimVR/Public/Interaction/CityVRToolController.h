#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityPolicies.h"
#include "Simulation/CityReportHistory.h"
#include "GameFramework/Actor.h"
#include "Interaction/CityBoardManipulator.h"
#include "Interaction/CityPaletteModel.h"
#include "Interaction/CityToolInputRouter.h"
#include "Interaction/CityRoadHeightControl.h"
#include "Presentation/CityValueControl.h"
#include "Presentation/CityConditionIndicators.h"
#include "Presentation/CityNewCitySetup.h"
#include "Persistence/CitySaveLibrary.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityUiContent.h"
#include "Sandbox/CityDesktopVideoSettings.h"
#include "CityVRToolController.generated.h"

class UCityDesktopHud;
class UEnhancedInputComponent;
class ACityBoardActor;
class APawn;
class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMotionControllerComponent;
class UWidgetComponent;
class UFont;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UTextRenderComponent;
struct FCityGuidedObjective;
struct FCityDesktopModelSource;

UCLASS()
class CITYSIMVR_API ACityVRToolController : public AActor
{
	GENERATED_BODY()
	friend class FCityPolicyUiTest;
	friend class FCityCreationRevisionTest;
	friend class FCityDesktopSaveRevisionCommand;
	friend class FCityDesktopPoliciesCommand;
	friend class FCityDesktopTechnologyCommand;
	friend class FCityZoneVisibilityCaptureCommand;
	friend class FCityBeachCaptureCommand;
	friend class FCityShorelineCaptureCommand;
	friend class FCityDesktopLowDensityCommand;
	friend class FCityTechnologyLiveTest;
    friend class FCityAiLiveTest;
    friend class FCityDesktopAiCommand;
	friend class FCityLoanUiTest;
	friend class FCityDesktopLoansCommand;
	friend class FCityDesktopConditionsCommand;
	friend class FCityNewsUiTest;
	friend class FCityDesktopNewsCommand;

  public:
	ACityVRToolController();
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category = "City Presentation")
	void SetConditionIndicatorsVisible(bool bVisible);
	UFUNCTION(BlueprintCallable, Category = "City Inspection")
	void InspectBuilding(int64 EntityId);
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ShowMainMenu();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ShowGameSettings();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ShowDebugTools();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void SetDebugModeEnabled(bool bEnabled);
	UFUNCTION(BlueprintCallable, Category = "City Presentation")
	void SetCitySummaryVisible(bool bVisible);
	UFUNCTION(BlueprintCallable, Category = "City Presentation")
	void SetObjectivesVisible(bool bVisible);
	UFUNCTION(BlueprintCallable, Category = "City Reports")
	void ShowReports(bool bAnnual = false);
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ShowNewCitySetup();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ReturnToCity();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ReviewCityLoad();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ConfirmReviewedCityLoad();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ReviewCityCreation();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void ConfirmReviewedCityCreation();
	UFUNCTION(BlueprintCallable, Category = "City Menu")
	void RerollNewCityTerrain();
	UFUNCTION(BlueprintPure, Category = "City Menu")
	bool IsMainMenuEnvironmentActive() const
	{
		return bEntrySceneActive;
	}

	UFUNCTION(BlueprintPure, Category = "City UI")
	FCityUiSnapshot GetUiSnapshot() const;
	bool GetDesktopModelSource(FCityDesktopModelSource& Out) const;
	const FCityTerrainGenerationParameters* GetDesktopTerrainPreview() const;
	UFUNCTION(BlueprintCallable, Category = "City UI")
	void ExecuteDesktopUiAction(int32 Action);
	bool IsDesktopUi() const
	{
		return bDesktopUi;
	}
	bool IsDesktopCameraInputAllowed() const;
	bool IsDesktopPointerBlocked() const;

  private:
	friend class UCityDesktopHud;
	friend class FCityUiParityTest;
	friend class FCityDesktopFocusRecoveryTest;
	friend class FCityHappinessFeedbackUiTest;
	friend class FCityDesktopHousingFeedbackCommand;
	friend class FCityDesktopServiceGuidanceCommand;
	friend class FCityDesktopPackagedSmokeCommand;
	bool bDesktopUi = false;
	bool bDesktopFocused = true;
	UPROPERTY()
	TObjectPtr<UCityDesktopHud> DesktopHud;
	FText SharedReportText;
	FText SharedReportRange;
	FCityUiReportChart SharedReportChart;
	FText SharedUpgradeLabel;
	void InitializeDesktopInput(UEnhancedInputComponent* Input,
	                            UEnhancedInputLocalPlayerSubsystem* Subsystem);
	void UpdateDesktopInteraction();
	void UpdateDesktopFocus(bool bFocused);
	bool IsDesktopZoningPaletteActive() const;
	void InitializeDesktopVideo();
	FCityUiActionView GetPaletteEntryView(int32 Index) const;
	FCityUiActionView MakeActionView(const FCityPaletteEntry& Entry, int32 Index) const;
	FString GetPaletteHelp() const;
	FString GetPaletteEntryHelp(const FCityPaletteEntry& Entry) const;
	FCityGuidedObjective GetObjectiveCard() const;
	void ExecuteSharedPaletteAction(ECityPaletteAction Action);
	void ActivateInspectedUpgrade();
	friend class FCityReportsPanelTest;
	friend class FCityEntryMenuTest;
	friend class FCityLiveDimensionsTest;
	friend class FCityDisasterMenuTest;
	friend class FCityEntryEnvironmentTest;
	friend class FCityPresentationShadowsTest;
	friend class FCityHudOverlayTest;
	friend class FCityLibraryMenuTest;
	FCityNewCitySetup NewCityDraft;
	FCityIdentity NewCityIdentity;
	FCityTerrainGenerationParameters NewCityTerrain;
	TArray<FCitySaveSlot> ReviewedCitySlots;
	int32 ReviewedCityIndex = 0;
	bool bReviewArchivedCities = false;
	FCitySaveSlot ArchiveReviewSlot;
	void RefreshCityLibrary();
	void ReviewSelectedCityArchive();
	void ConfirmSelectedCityArchive();
	void RestoreSelectedCity();
	void UpdateTerrainPreview();
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> PaletteTerrainPreview;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> PaletteTerrainLegend;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PaletteTerrainEntryMarker;
	TOptional<FCityTerrainGenerationParameters> PreviewTerrainParameters;
	bool bSettingsFromMainMenu = false;
	bool bMainMenuPinned = false;
	bool bEntrySceneActive = false;
	TMap<TWeakObjectPtr<AActor>, bool> EntryHiddenActors;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> EntryHiddenUi;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EntrySky;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> EntrySkyMaterial;
	void SetEntrySceneActive(bool bActive);
	void UpdateEntryScenePresentation();
	bool UsesHandPalette() const
	{
		return !bDesktopUi && PaletteAnchorMode == 2 && !bEntrySceneActive &&
		       PalettePage != ECityPalettePage::Reports && PalettePage != ECityPalettePage::DisasterHistory &&
		       PalettePage != ECityPalettePage::HousingHistory;
	}
	FCityPaletteRect GetPaletteButtonRect(int32 Index, ECityPaletteAction Action) const;
	FString EntryMenuStatus;
	void NavigatePaletteBack();
	void ConfirmCityLoad(const FString& SavePath);
	FString GetEntryMenuHelp() const;
	friend class FCityStationUpgradeTest;
	friend class FCityHospitalIntegrationTest;
	bool HandleInspectorUpgradeRay(const FVector& Origin, const FVector& Direction);
	bool IsInspectorSurfaceHit(const FVector& Origin, const FVector& Direction) const;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> InspectorUpgradeButton;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> InspectorUpgradeLabel;
	bool bInspectorPanelFocused = false;
	bool bInspectorUpgradeAvailable = false;
	double InspectorHalfWidth = 0.0;
	double InspectorHalfHeight = 0.0;
	uint64 StationUpgradePreviewEntity = 0;
	ECityDensityLevel StationUpgradePreviewLevel = ECityDensityLevel::Low;
	friend class FCityDrawerInteractionTest;
	friend class FCityPanelsTest;
	friend class FCityConditionPresentationTest;
	friend class FCityConditionClusterPresentationTest;
	void UpdateConditionIndicators();
	uint64 PickConditionMember(int32 Index, const FVector& Origin, const FVector& Direction) const;
	int32 FindConditionIndicator(const FVector& Origin, const FVector& Direction, double& OutDistance) const;
	bool bConditionIndicatorsVisible = true;
	TArray<FCityConditionIndicator> ConditionIndicators;
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> ConditionRoots;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ConditionBackgrounds;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ConditionIcons;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ConditionCrimeIcons;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ConditionRails;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ConditionFills;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> ConditionLeaderInstances;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> ConditionTitles;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> ConditionDetails;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> ConditionIconMaterials;
	UPROPERTY()
	TMap<FName, TObjectPtr<UMaterialInterface>> ConditionSymbols;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ConditionSummaryText;
	uint64 LastFocusedCondition = 0;
	uint64 FocusedConditionSession = MAX_uint64;
	void FocusNextCondition();
	void TryInitializeInput();
	bool DisableTemplateMenuBindings(APawn& PlayerPawn);
	void FindMotionControllers();
	void UpdateHeadsetStatusAttachment();
	void UpdateControllerRay();
	void UpdateCityTotals();
	void UpdateCityInfoCards();
	void UpdateConstructionCallout();
	void UpdateBoardManipulation();
	void UpdatePaletteAttachment();
	void CyclePaletteAnchor();
	void LogRawInputState();
	void RecordInputAction(uint16 InputBit, const TCHAR* InputName, bool bPressed);
	void SetPaletteOpen(bool bOpen);
	void InitializeHudOverlay();
	float GetHudRayDistance(const FVector& Origin, const FVector& Direction, float MaxDistance) const;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> HudOverlayMasters;
	void RefreshPaletteText();
	void RefreshReports();
	int32 ReportMetricIndex = 0;
	int32 HousingHistoryPage = 0;
	ECityPolicy PendingPolicy = ECityPolicy::ReserveAssistance;
	bool bPendingPolicyEnable = false;
	FString PolicyMessage;
	FName TechnologyReviewDefinition = TEXT("DigitalExchange");
	int64 PendingLoanAmount = 0;
	bool bLoanPayoffReview = false;
	FString LoanMessage;
	int32 NewsPage = 0;
	FCityUiNewspaper SharedNewspaper;
	TSharedPtr<FCityUiNewspaper> WorldNewsContent;
	UPROPERTY() TObjectPtr<UWidgetComponent> NewspaperSheet;
	UPROPERTY() TObjectPtr<UFont> NewspaperTypeface;
	int32 ReportCategory = 0;
	int32 ReportPeriodIndex = INDEX_NONE;
	bool bAnnualReports = false;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ReportText;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ReportRangeText;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ReportBars;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ReportZeroLine;
	void UpdateNeedsStatusBars();
	void UpdateInspectionTooltip();
	void ClearInspection();
	void ToggleInspection(const FVector& Origin, const FVector& Direction);
	bool IsNeedsToggleHit(const FVector& Origin, const FVector& Direction) const;
	bool IsNeedsPanelHit(const FVector& Origin, const FVector& Direction) const;
	void RefreshNeedsVisibility();
	void UpdateObjectiveCard();
	int32 FindObjectiveFooter(const FVector& Origin, const FVector& Direction) const;
	void ExecuteObjectiveFooter(int32 Index);
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ObjectiveFooterButtons;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> ObjectiveFooterLabels;
	bool bObjectiveReview = false;
	int32 ObjectiveReviewIndex = 0;
	int32 HoveredObjectiveFooter = INDEX_NONE;
	void RefreshObjectiveText(const FCityGuidedObjective& Card);
	void SetDebugInfoVisible(bool bVisible);
	void RestoreDebugInfoDisplay();
	bool bDebugInfoVisible = false;
	bool bOwnsDebugInfoDisplay = false;
	bool bPreviousDebugInfoDisplay = true;
	bool IsObjectiveToggleHit(const FVector& Origin, const FVector& Direction) const;
	bool IsObjectivePanelHit(const FVector& Origin, const FVector& Direction) const;
	UPROPERTY()
	TObjectPtr<USceneComponent> ObjectiveRoot;
	UPROPERTY()
	TObjectPtr<USceneComponent> ObjectiveToggleRoot;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ObjectiveToggleIcon;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ObjectiveIconMaterial;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ObjectiveToggleHelp;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ObjectiveToggle;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ObjectiveBackground;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ObjectiveRail;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ObjectiveFill;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ObjectiveHeader;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> ObjectiveText;
	bool bObjectivesOpen = true;
	bool bObjectiveToggleFocused = false;
	bool bObjectivePanelFocused = false;
	int32 FindTimeControl(const FVector& RayOrigin, const FVector& RayDirection) const;
	void ExecuteTimeControl(int32 ControlIndex);
	void ExecutePaletteAction(int32 PaletteIndex);
	void SetPalettePage(ECityPalettePage NewPage);
	void BeginTaxEdit();
	void CancelTaxEdit();
	void UpdateTaxDragFromRay(const FVector& RayOrigin, const FVector& RayDirection);
	void EndTaxDrag();
	FString GetPaletteEntryLabel(const FCityPaletteEntry& Entry) const;
	bool IsPaletteSurfaceHit(const FVector& RayOrigin, const FVector& RayDirection) const;
	bool IsPaletteActionSelected(ECityPaletteAction Action) const;
	bool IsPaletteActionDisabled(ECityPaletteAction Action) const;
	UMaterialInterface* GetPaletteIconMaterial(ECityPaletteAction Action) const;
	int32 GetPaletteIconIndex(ECityPaletteAction Action) const;
	FVector GetPaletteButtonLocation(int32 PaletteIndex,
	                                 ECityPaletteAction Action = ECityPaletteAction::Close) const;
	int32 FindPaletteButton(const FVector& RayOrigin, const FVector& RayDirection) const;
	void RouteTrigger(ECityControllerHand Hand);
	void HandleGrip(ECityControllerHand Hand, bool bPressed);
	void SwapHandedness(ECityControllerHand PressedHand);
	void BeginManipulationSession();
	void EndManipulationSession();
	void ReinitializeManipulationGesture();
	void NudgeBoardHeight(double DeltaHeightCm);
	void RecenterBoard();
	void UpdateLifecycleInputGate();
	void HandleApplicationBackgrounded();
	void HandleApplicationForegrounded();

	void ConfirmDesktop();
	void Cancel();
	void Rotate();
	void SelectInspect();
	void SelectRoad();
	void SelectHome();
	void SelectShop();
	void SelectPark();
	void SelectPower();
	void SelectService();
	void SelectZone();
	void SelectDemolish();
	void StartScenario();
	void ResetScenario();
	void TogglePause();
	void SetSpeed1();
	void SetSpeed3();
	void SetSpeed5();
	void SaveCity();
	void LoadCity();
	void LeftTrigger();
	void LeftTriggerReleased();
	void RightTrigger();
	void RightTriggerReleased();
	void LeftPrimary();
	void LeftPrimaryReleased();
	void RightPrimary();
	void RightPrimaryReleased();
	void LeftSecondary();
	void LeftSecondaryReleased();
	void RightSecondary();
	void RightSecondaryReleased();
	void LeftGripStarted();
	void LeftGripCompleted();
	void RightGripStarted();
	void RightGripCompleted();
	void LeftStickClick();
	void LeftStickReleased();
	void RightStickClick();
	void RightStickReleased();

	FCityRoadHeightControl RoadHeightControl;
	UPROPERTY()
	TObjectPtr<UInputMappingContext> RoadHeightMapping;
	UPROPERTY()
	TObjectPtr<UInputAction> RoadHeightAction;
	bool bRoadHeightContextActive = false;
	ECityControllerHand RoadHeightHand = ECityControllerHand::Right;
	void UpdateRoadHeightInput();
	UInputAction* MakeAction(const TCHAR* Name);
	void MapAction(UInputAction* Action, const FKey& Key);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> PaletteRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> HeadsetStatusRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> NeedsStatusRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> InspectorStatusRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> InspectorBackground;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> InspectorLeader;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InspectorMetricRails;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> InspectorMetricFills;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> InspectorMetricLabels;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NeedsBackground;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> NeedsToggleRoot;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NeedsToggleBackground;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NeedsToggleIcon;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> NeedsToggleHelp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PaletteBackground;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PaletteButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PaletteButtonMaterials;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UTextRenderComponent>> PaletteLabels;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PaletteIcons;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PaletteSliderFills;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PaletteSliderThumbs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> PaletteIconMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PaletteIconDynamics;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResidentialSliderMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CommercialSliderMaterial;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PaletteTitle;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> PaletteHelp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> InspectorText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> SummaryText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> NeedsText;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> GrowthNeedsTitle;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GrowthNeedsBackground;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> CalendarText;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> CityTotalsText;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CityTotalsBackground;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> CityBudgetDetail;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> CityLastNetText;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CityInfoBackgrounds;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> CityInfoTitles;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> CityInfoLabels;
	UPROPERTY()
	TArray<TObjectPtr<UTextRenderComponent>> CityInfoValues;
	UPROPERTY()
	TObjectPtr<USceneComponent> ConstructionStatusRoot;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ConstructionLeader;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UTextRenderComponent>> CityTrendTexts;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UProceduralMeshComponent>> CityTrendArrows;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CityTrendMaterials;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> HeatMapLegend;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> HeatMapLegendBackground;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> TimeControlButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TimeButtonMaterials;
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UTextRenderComponent>> TimeControlLabels;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> RoadStatusText;
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> PriceBlockedIcon;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PriceBlockedSlash;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RoadStatusBackground;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> NeedsCapacityRails;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> NeedsCapacityFills;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UTextRenderComponent>> NeedsCapacityLabels;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> NeedsDemandRails;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> NeedsDemandFills;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> NeedsDemandZeroMarkers;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UTextRenderComponent>> NeedsDemandLabels;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DemandRailMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ResidentialDemandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CommercialDemandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> IndustrialDemandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CityFaceMappingContext;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> InputActions;

	UPROPERTY(Transient)
	TObjectPtr<ACityBoardActor> Board;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> LeftAim;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightAim;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> LeftGrip;

	UPROPERTY(Transient)
	TObjectPtr<UMotionControllerComponent> RightGrip;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> HeadsetCamera;

	ECityControllerHand DominantHand = ECityControllerHand::Right;
	bool bInputInitialized = false;
	bool bPaletteOpen = false;
	FCityDesktopVideoSettings DesktopVideo;
	bool bNeedsOpen = false;
	bool bNeedsToggleFocused = false;
	bool bNeedsPanelFocused = false;
	uint64 InspectedEntityId = 0;
	uint64 InspectionGeneration = 0;
	FVector InspectionLocalHit = FVector::ZeroVector;
	FString LastInspectionText;
	FString SharedInspectionMetrics;
	FString SharedInspectionDetail;
	FText SharedInspectionTitle;
	FText SharedInspectionSummary;
	TArray<FCityUiGauge> SharedInspectionGauges;
	TArray<FCityUiInspectionSection> SharedInspectionSections;
	// 0: view-following drawer, 1: world pose captured on open, 2: legacy hand palette.
	int32 PaletteAnchorMode = 0;
	bool bPalettePoseCaptured = false;
	uint16 SuppressedTriggerMask = 0;
	uint16 ActionButtonMask = 0;
	FString LastInputTransition = TEXT("Input monitor ready");
	bool bLeftGripHeld = false;
	bool bRightGripHeld = false;
	bool bManipulationSession = false;
	int32 HoveredPaletteIndex = INDEX_NONE;
	int32 HoveredTimeControl = INDEX_NONE;
	bool bPaletteSurfaceFocused = false;
	ECityPalettePage PalettePage = ECityPalettePage::Root;
	TArray<FCityPaletteEntry> VisiblePaletteEntries;
	FCityBoundedValueControl ResidentialTaxControl{FCityEconomySnapshot::MinimumTaxBasisPoints,
	                                               FCityEconomySnapshot::MaximumTaxBasisPoints,
	                                               FCityEconomySnapshot::TaxStepBasisPoints};
	FCityBoundedValueControl CommercialTaxControl{FCityEconomySnapshot::MinimumTaxBasisPoints,
	                                              FCityEconomySnapshot::MaximumTaxBasisPoints,
	                                              FCityEconomySnapshot::TaxStepBasisPoints};
	FCityBoundedValueControl IndustrialTaxControl{FCityEconomySnapshot::MinimumTaxBasisPoints,
	                                              FCityEconomySnapshot::MaximumTaxBasisPoints,
	                                              FCityEconomySnapshot::TaxStepBasisPoints};
	ECityPaletteAction ActiveTaxDrag = ECityPaletteAction::Close;
	FCityBoardManipulator BoardManipulator;
	FCityBoardPresentationPose BoardPresentationPose;
	FCityLifecycleInputGate LifecycleInputGate;
	FDelegateHandle BackgroundDelegateHandle;
	FDelegateHandle ForegroundDelegateHandle;
};
