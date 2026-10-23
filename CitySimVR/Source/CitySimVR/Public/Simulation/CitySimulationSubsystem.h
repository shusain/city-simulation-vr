#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityScenarioSimulation.h"
#include "Simulation/CityFirstPlayableGuide.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CityTerrain.h"
#include "Performance/CityBenchmark.h"
#include "Persistence/CityPersistence.h"
#include "Subsystems/WorldSubsystem.h"
#include "CitySimulationSubsystem.generated.h"

class FCityPlacementModel;
class UCityBuildingCatalog;

UCLASS()
class CITYSIMVR_API UCitySimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
    friend class FCityDesktopContinuityCommand;
	friend class FCityPlacementPreviewStudyCommand;
	friend class FCitySoundscapeRoutingTest;
	friend class FCityPolicyFixture;
	friend class FCityDesktopLoansCommand;
	friend class FCityDesktopConditionsCommand;
	friend class FCityConditionCauseTest;
	friend class FCityShoppingAllocationIntegrationTest;
	friend class FCitySavedBenchmarkTest;
	friend class FCityDimensionsPresentationTest;
	friend class FCityLiveDimensionsTest;
	friend class FCityDerivedInvalidationTest;
	friend class FCityMonthlyReconciliationTest;
	friend class FCityStepServiceReuseTest;
	friend class FCityCreationRevisionTest;
	friend class ACityBenchmarkProbe;
	friend class FCityTerrainPresentationCacheTest;
	friend class FCityReportsPanelTest;
	friend class FCitySeasonalDecorationPresentationTest;
	friend class FCityVisibleTrafficTest;
	friend class FCityCrosswalkGeometryTest;
	friend class FCityCrosswalkCaptureCommand;
	friend class FCityFlowVisualCommand;
	friend class FCityGroundPlotStudyCommand;
    friend class FCityApprovedCivicStudyCommand;
	friend class FCityCitizenWalkVisualCommand;
	friend class FCityRoadStructureTest;
	friend class FCityRoadUnderpassTest;
	friend class FCityRaisedBranchTest;
	friend class FCityMixedRaisedTest;
	friend class FCityEmergencySprayTest;
	friend class FCityFreightIntegrationTest;
	friend class FCityHeatMapTest;
	friend class FCityPanelsTest;
	friend class FCityGuidedIntegrationTest;
	friend class FCityCrimeIntegrationTest;
	friend class FCityFireIntegrationTest;
	friend class FCityNaturalIntegrationTest;
	friend class FCitySchoolIntegrationTest;
	friend class FCityHappinessIntegrationTest;
	friend class FCityHousingMigrationLiveTest;
	friend class FCityHousingFeedbackFixture;
	friend class FCityQuarterlyNewsFixture;
	friend class FCityTechnologyFixture;
	friend class FCityZoneVisibilityCaptureCommand;
    friend class FCityAiLiveTest;
	friend class FCityDesktopServiceGuidanceCommand;
	friend class FCityEducationRecoveryTest;
	friend class FCityAlternativeIntegrationTest;
	friend class FCityWindRotorTest;
	friend class FCityParkIntegrationTest;
	friend class FCityConditionPresentationTest;
	friend class FCityConditionClusterPresentationTest;
	friend class FCitySafetyControlsTest;
	friend class FCityCitizenHousingIntegrationTest;

  public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	FCityPlacementModel& GetPlacementModel() const;
	const FCityWasteSnapshot& GetWasteSnapshot() const
	{
		return Waste.GetSnapshot();
	}
	const FCityWasteFacilityPolicy GetWastePolicy(uint64 Id) const;
	bool StartWasteTest(uint64 Id);
	friend class FCityWasteIntegrationTest;
	const FCityHealthSnapshot& GetHealthSnapshot() const
	{
		return Health.GetSnapshot();
	}
	const FCityReportHistory& GetReportHistory() const
	{
		return Reports;
	}
	const FCityEconomySnapshot& GetEconomySnapshot() const
	{
		return Economy.GetSnapshot();
	}
	double GetSimulationStepProgress() const
	{
		return Economy.GetFixedStepProgress(EconomyTuning);
	}
	int64 GetStationHalfDayClock() const
	{
		return FCityCalendar::HalfDayIndex(Economy.GetSnapshot().FixedStepId,
		    Economy.GetFixedStepProgress(EconomyTuning), EconomyTuning.FixedStepsPerMonth);
	}
	int64 GetProjectedResidentialTaxRevenue() const
	{
		return Economy.GetProjectedResidentialTaxRevenue(EconomyTuning);
	}
	int64 GetProjectedCommercialTaxRevenue() const
	{
		return Economy.GetProjectedCommercialTaxRevenue(EconomyTuning);
	}
	FCityBudgetProjection GetProjectedBudget(int32 ResidentialBasisPoints = -1,
	                                         int32 CommercialBasisPoints = -1,
	                                         int32 IndustrialBasisPoints = -1) const;
	// Current residential taxes plus completed sales, less the full monthly bills.
	FCityBudgetProjection GetMonthSettlementPreview() const;
	const FCityEconomyTuning& GetEconomyTuning() const
	{
		return EconomyTuning;
	}
	const FCityServiceSnapshot& GetServiceSnapshot() const
	{
		return ServiceSnapshot;
	}
	const FCityNeedsSnapshot& GetNeedsSnapshot() const
	{
		return NeedsSnapshot;
	}
	const FCityGrowthResult& GetLastGrowthResult() const
	{
		return LastGrowthResult;
	}
	const FString& GetPersistenceStatus() const
	{
		return PersistenceStatus;
	}
	const FString& GetControlStatus() const
	{
		return ControlStatus;
	}
	const FCityScenarioSnapshot& GetScenarioSnapshot() const
	{
		return Scenario.GetSnapshot();
	}
	bool IsSimulationPaused() const
	{
		return Economy.IsPaused();
	}
	// Presentation suspension holds authority without changing the saved pause/speed choice.
	void SetEntryMenuSuspended(bool bSuspended)
	{
		bEntryMenuSuspended = bSuspended;
	}
	bool IsEntryMenuSuspended() const
	{
		return bEntryMenuSuspended;
	}
	int32 GetSimulationSpeed() const
	{
		return Economy.GetSpeedMultiplier();
	}
	bool IsResetConfirmationPending() const
	{
		return bResetConfirmationPending;
	}
	bool IsBenchmarkActive() const
	{
		return BenchmarkResult.Config.IsEnabled();
	}
	const FCityBenchmarkResult& GetBenchmarkResult() const
	{
		return BenchmarkResult;
	}
	void ResetBenchmarkTimings()
	{
		if (IsBenchmarkActive())
			BenchmarkTiming = MakeUnique<FCityBenchmarkSimulationTiming>();
		RoadNetworkSnapshot.RoutingTiming = BenchmarkTiming ? &BenchmarkTiming->Routing : nullptr;
		ServiceSnapshot.Water.EvaluationTiming =
		    BenchmarkTiming ? &BenchmarkTiming->ServiceEvaluation : nullptr;
	}
	const FCityBenchmarkSimulationTiming* GetBenchmarkTimings() const
	{
		return BenchmarkTiming.Get();
	}
	const FCityTerrainHeightfield& GetTerrain() const
	{
		return Terrain;
	}
	const FCityEnvironmentSnapshot& GetEnvironmentSnapshot() const
	{
		return Environment.GetSnapshot();
	}
	const FCityDevelopmentSnapshot& GetDevelopmentSnapshot() const
	{
		return Development.GetSnapshot();
	}
	const FCityPopulationSnapshot& GetPopulationSnapshot() const
	{
		return Population.GetSnapshot();
	}
	const FCityTrafficSnapshot& GetTrafficSnapshot() const
	{
		return Traffic.GetSnapshot();
	}
	const FCityTrafficSnapshot& GetPreviousTrafficSnapshot() const
	{
		return PreviousTraffic;
	}
	const FCityRoadNetworkSnapshot& GetRoadNetworkSnapshot() const
	{
		return RoadNetworkSnapshot;
	}
	FString GetBenchmarkStatus() const;
	FString GetScenarioStatus() const;
	FString GetTutorialStatus() const;
	FCityFirstPlayableGuideState GetGuideState() const;
	FCityGuidedObjective GetGuidedObjective() const;
	const FCityFireSnapshot& GetFireSnapshot() const
	{
		return Fire.GetSnapshot();
	}
	bool RepairBuilding(uint64 EntityId);
	bool PurchaseStationUpgrade(uint64 EntityId, ECityDensityLevel ExpectedLevel);
	FString DescribeStationUpgrade(uint64 EntityId) const;
	friend class FCityStationUpgradeTest;
	friend class FCityHospitalIntegrationTest;
	friend class FCityOutpatientIntegrationTest;
	bool StartHealthTest(uint64 EntityId, bool bSevere);
	bool StartSafetyTest(uint64 EntityId, bool bFire, bool bCrime);
	int64 GetRepairCost(uint64 EntityId) const;
	const FCityCrimeSnapshot& GetCrimeSnapshot() const
	{
		return Crime.GetSnapshot();
	}
	const FCityGuidedProgress& GetGuidedProgress() const
	{
		return Guidance;
	}
	FString GetTimeStatus() const;
	uint64 GetCitySessionGeneration() const
	{
		return CitySessionGeneration;
	}
	uint64 GetPresentationGeneration() const
	{
		return PresentationGeneration;
	}
	void RegisterBuildingCatalog(UCityBuildingCatalog* Catalog);
	const UCityBuildingCatalog* GetBuildingCatalog() const
	{
		return BuildingCatalog;
	}
	void ConsumeSpawnedEntityIds(TArray<uint64>& OutEntityIds);

	UFUNCTION(BlueprintCallable, Category = "City Simulation")
	void SetSimulationPaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "City Simulation")
	void SetSimulationSpeed(int32 SpeedMultiplier);

	UFUNCTION(BlueprintCallable, Category = "City Economy")
	bool SetCommercialAiPilot(bool Enabled);

	UFUNCTION(BlueprintCallable, Category = "City Economy")
	bool SetCityPolicy(ECityPolicy Policy, bool Enabled)
	{
		return Economy.SetPolicy(Policy, Enabled);
	}
	UFUNCTION(BlueprintCallable, Category = "City Economy")
	bool BorrowCityLoan(int64 Amount, FString& Error)
	{
		return PlacementModel && Economy.Borrow(*PlacementModel, Amount, Error);
	}
	bool RepayCityLoan(FString& Error)
	{
		return PlacementModel && Economy.RepayLoan(*PlacementModel, Error);
	}
	bool SetTaxRates(int32 ResidentialBasisPoints, int32 CommercialBasisPoints,
	                 int32 IndustrialBasisPoints = -1);

	UFUNCTION(BlueprintCallable, Category = "City Scenario")
	void StartScenario();

	UFUNCTION(BlueprintCallable, Category = "City Scenario")
	void RequestResetScenario();

	UFUNCTION(BlueprintCallable, Category = "City Simulation")
	void ToggleSimulationPaused();

	UFUNCTION(BlueprintCallable, Category = "City Persistence")
	bool SaveCity();

	UFUNCTION(BlueprintCallable, Category = "City Persistence")
	bool LoadCity();

	const FCityDisasterSnapshot& GetDisasterSnapshot() const
	{
		return Disasters.GetSnapshot();
	}
	bool StartDisasterTest(ECityNaturalDisaster Kind);
	UFUNCTION(BlueprintCallable, Category = "City Simulation")
	bool StartEarthquakeTest()
	{
		return StartDisasterTest(ECityNaturalDisaster::Earthquake);
	}
	UFUNCTION(BlueprintCallable, Category = "City Simulation")
	bool StartTornadoTest()
	{
		return StartDisasterTest(ECityNaturalDisaster::Tornado);
	}
	bool SetDisasterFrequency(ECityDisasterFrequency Frequency);
	const FCityIdentity& GetCityIdentity() const
	{
		return CityIdentity;
	}
	bool CreateCity(const FCityIdentity& Identity, const FCityTerrainGenerationParameters& Parameters,
	                const FString& Directory, FString& Error);
	bool SaveCityToPath(const FString& SavePath, FString& OutError,
	                    bool bSimulateInterruptionBeforeReplace = false);
	UFUNCTION(BlueprintCallable, Category = "City Persistence")
	bool LoadCityFromPath(const FString& SavePath, FString& OutError, bool bAllowBackup = true);

  private:
	friend class FCityWaterCaptureCommand;
	friend class FCityTerrainBaseTest;
	friend class FCityAtmospherePresentationTest;
	FCityReportHistory Reports;
	FCityIdentity CityIdentity;
	TUniquePtr<FCityPlacementModel> PlacementModel;
	FCityEconomySimulation Economy;
	FCityGrowthSimulation Growth;
	FCityEnvironmentSimulation Environment;
	FCityDevelopmentSimulation Development;
	FCityPopulationSimulation Population;
	FCityTrafficSimulation Traffic;
	FCityTrafficSnapshot PreviousTraffic;
	FCityServiceSnapshot ServiceSnapshot;
	FCityNeedsSnapshot NeedsSnapshot;
	FCityRoadNetworkSnapshot RoadNetworkSnapshot;
	const FCityPlacementModel* RoadNetworkOwner = nullptr;
	const FCityPlacementModel* DerivedOwner = nullptr;
	uint64 DerivedRevision = MAX_uint64;
	uint64 DerivedSession = MAX_uint64;
	uint64 DerivedPresentation = MAX_uint64;
	int64 DerivedFunds = -1;
	int64 DerivedStep = -1;
	FCityGrowthResult LastGrowthResult;
	FCityScenarioSimulation Scenario;
	FCityGuidedProgress Guidance;
	FCityCrimeSimulation Crime;
	FCityFireSimulation Fire;
	FCityHealthSimulation Health;
	FCityWasteSimulation Waste;
	FCityDisasterSimulation Disasters;
	FCityTerrainHeightfield Terrain;
	TArray<uint64> PendingSpawnedEntityIds;
	FString PersistenceStatus = TEXT("Save: not used");
	FString ControlStatus;
	uint64 PresentationGeneration = 0;
	uint64 CitySessionGeneration = 0;
	bool bStorageProbeRun = false;
	bool bResetConfirmationPending = false;
	bool bResumeAfterForeground = false;
	bool bEntryMenuSuspended = false;
	bool bBenchmarkInitializationAttempted = false;
	FCityBenchmarkResult BenchmarkResult;
	TUniquePtr<FCityBenchmarkSimulationTiming> BenchmarkTiming;
	FDelegateHandle BackgroundDelegateHandle;
	FDelegateHandle ForegroundDelegateHandle;

	UPROPERTY(Transient)
	TObjectPtr<UCityBuildingCatalog> BuildingCatalog;

	UPROPERTY(EditAnywhere, Category = "City Simulation")
	FCityEconomyTuning EconomyTuning;

	FCitySaveDocument BuildSaveDocument() const;
	bool ValidateSaveDocument(const FCitySaveDocument& Document, TUniquePtr<FCityPlacementModel>& OutModel,
	                          FCityEconomySimulation& OutEconomy, FCityGrowthSimulation& OutGrowth,
	                          FCityEnvironmentSimulation& OutEnvironment,
	                          FCityDevelopmentSimulation& OutDevelopment,
	                          FCityPopulationSimulation& OutPopulation, FCityTrafficSimulation& OutTraffic,
	                          FCityTerrainHeightfield& OutTerrain, FString& OutError) const;
	void RunStorageProbe();
	void RefreshDerivedSnapshots();
	void RefreshRoadNetwork();
	bool UsePreparedServiceQueries() const;
	bool UsePreparedPopulationQueries() const;
	bool UseInitialServiceReuse() const;
	bool UsePreparedCareQueries() const;
	bool NeedsDerivedRefresh() const;
	void AdvanceSimulationStep(FCityServiceSnapshot* StepServices = nullptr);
	void TryInitializeBenchmark();
	bool InitializeSavedBenchmark(const FCityBenchmarkConfig& Config, FString& OutError);
	void ResetCity(bool bStartRunning);
	void HandleApplicationBackgrounded();
	void HandleApplicationForegrounded();
};
