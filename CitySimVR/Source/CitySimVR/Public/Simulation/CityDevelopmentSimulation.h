#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityBuildingDefinition.h"

class FCityPlacementModel;
class UCityBuildingCatalog;
struct FCityEconomyTuning;
struct FCityEnvironmentSnapshot;
struct FCityNeedsSnapshot;
struct FCityServiceSnapshot;

enum class ECityDevelopmentStage : uint8
{
	Construction,
	Operational
};

struct CITYSIMVR_API FCityDevelopmentState
{
	uint64 EntityId = 0;
	ECityDevelopmentStage Stage = ECityDevelopmentStage::Operational;
	ECityDensityLevel DensityLevel = ECityDensityLevel::Low;
	int64 ConstructionCompleteStep = 0;
	int64 OperationalSinceStep = 0;
	int32 PendingCivicLevel = -1;
	int64 CivicUpgradeCompleteStep = 0;
	// Absolute half-day boundary for rapid station construction/extension.
	int64 StationCompleteHalfDay = -1;
	bool HasCivicUpgrade() const
	{
		return PendingCivicLevel >= 0;
	}
};

struct CITYSIMVR_API FCityDevelopmentPressure
{
	int32 CitywideDemand = 0;
	int32 RoadAccess = 0;
	int32 LandValue = 0;
	int32 PollutionCompatibility = 0;
	int32 InfrastructureHeadroom = 0;
	int32 Total = 0;
};

struct CITYSIMVR_API FCityDevelopmentSnapshot
{
	int64 LastFixedStepId = 0;
	TMap<uint64, FCityDevelopmentState> States;
	TMap<uint64, FCityDevelopmentPressure> LatestPressures;

	const FCityDevelopmentState* Find(uint64 EntityId) const
	{
		return States.Find(EntityId);
	}
	bool IsOperational(uint64 EntityId) const;
};

struct CITYSIMVR_API FCityDevelopmentResult
{
	TArray<uint64> ChangedEntityIds;
	int32 CompletedConstruction = 0;
	int32 CompletedUpgrades = 0;
};

class CITYSIMVR_API FCityDevelopmentSimulation
{
  public:
	FCityDevelopmentResult AdvanceToStep(int64 FixedStepId, FCityPlacementModel& City,
	                                     const UCityBuildingCatalog* Catalog,
	                                     const FCityServiceSnapshot& Services,
	                                     const FCityNeedsSnapshot& Needs,
	                                     const FCityEnvironmentSnapshot& Environment,
	                                     const FCityEconomyTuning& Tuning);
	void Reconcile(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	               int64 CurrentHalfDay = -1, int32 StepsPerDay = 150);
	FCityDevelopmentResult AdvanceStationsToHalfDay(int64 HalfDay, const FCityServiceSnapshot& Services);
	void DelayConstructionUntil(uint64 EntityId, int64 StartStep);
	void HoldForDelivery(uint64 EntityId, int64 Step, int64 ConstructionSteps);
	bool RestoreState(const FCityDevelopmentSnapshot& InSnapshot, const FCityPlacementModel& City,
	                  const UCityBuildingCatalog* Catalog, int64 MaximumFixedStepId, FString& OutError,
	                  int64 CurrentHalfDay = -1, int32 StepsPerDay = 150);
	void InitializeExistingAsOperational(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                                     int64 FixedStepId);
	void Reset(int64 FixedStepId = 0);
	bool PurchaseCivicUpgrade(uint64 EntityId, ECityDensityLevel ExpectedLevel, FCityPlacementModel& City,
	                          const UCityBuildingCatalog* Catalog, const FCityServiceSnapshot& Services,
	                          FString& OutStatus, int64 CurrentHalfDay = -1, int32 StepsPerDay = 150);
	const FCityDevelopmentSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

  private:
	void Step(int64 StepId, FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	          const FCityServiceSnapshot& Services, const FCityNeedsSnapshot& Needs,
	          const FCityEnvironmentSnapshot& Environment, const FCityEconomyTuning& Tuning,
	          FCityDevelopmentResult& Result);

	FCityDevelopmentSnapshot Snapshot;
};
