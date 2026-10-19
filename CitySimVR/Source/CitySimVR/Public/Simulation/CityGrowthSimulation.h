#pragma once

#include "CoreMinimal.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityWaterSimulation.h"

struct FCityBuildingDefinition;
struct FCityEnvironmentSnapshot;
struct FCityPopulationSnapshot;
struct FCityNeedsSnapshot;
struct FCityFireSnapshot;
struct FCityDevelopmentSnapshot;
struct FCityServiceEvaluationTiming;
class FCityTerrainHeightfield;
class UCityBuildingCatalog;

enum class ECityOperationFailure : uint8
{
	None,
	NoRoad,
	NoPower,
	PowerCapacity,
	NoService,
	ServiceCapacity,
	AwaitingFreight,
	OnFire,
	FireDamage,
	Repairing,
	NoStaff,
	LowWaterPressure,
	TechnologyPrerequisite
};

enum class ECityGrowthFailure : uint8
{
	None,
	NotDue,
	UpkeepShortfall,
	DisabledBuilding,
	MissingDefinition,
	NoEligibleZone,
	InsufficientFunds,
	LowDemand
};

struct CITYSIMVR_API FCityServiceReach
{
	TMap<FCityCellCoord, int32> RoadHopDistances;

	bool IsRoadReached(const FCityCellCoord& Cell) const
	{
		return RoadHopDistances.Contains(Cell);
	}
	int32 GetRoadHopDistance(const FCityCellCoord& Cell) const;
};

struct CITYSIMVR_API FCityServiceSnapshot
{
	FCityGridDimensions Dimensions;
	// Current operating prerequisites; derived each evaluation and never serialized.
	TMap<FName, int32> BuiltTechnology;
	TMap<FName, int32> OperatingFacilities;
	uint64 PlacementRevision = 0;
	int32 OperatingAlternativePower = 0;
	int32 BuiltHomes = 0;
	int32 BuiltShops = 0;
	int32 BuiltIndustries = 0;
	int32 BuiltParks = 0;
	int32 OperationalParks = 0;
	int32 OperationalLargeParks = 0;
	TArray<FCityParkProvider> ParkProviders;
	TArray<int32> ParkCoverage;
	int32 BuiltSchools = 0;
	int32 OperationalSchools = 0;
	int32 EducationDemand = 0;
	int32 EducationCapacity = 0;
	int32 EducationServed = 0;
	int32 SecondaryResidents = 0;
	TSet<uint64> EducationCitizenIds;
	TMap<uint64, int32> SchoolUsedCapacity;
	TMap<uint64, int32> SchoolCapacities;
	TMap<uint64, int32> HomeEducationPlaces;
	TMap<uint64, int32> HomeEducationDemand;
	TMap<uint64, int32> EntitySkilledJobs;
	TMap<uint64, int32> EntitySkilledMinimumEducation;
	TSet<uint64> CommercialAiShops;
    TSet<uint64> CommercialAiStaffedShops;
	TArray<uint64> OperatingAiCenters;
	TMap<uint64, uint64> CommercialAiProviders;
	TArray<int32> EducationCoverage;
	int32 OperationalHomes = 0;
	int32 OperationalShops = 0;
	int32 OperationalIndustries = 0;
	int32 BuiltPowerBuildings = 0;
	int32 BuiltServiceBuildings = 0;
	int32 OperationalPowerBuildings = 0;
	int32 OperationalServiceBuildings = 0;
	int32 Residents = 0;
	int32 HomelessResidents = 0;
	int32 UnemployedResidents = -1;
	int32 DetainedResidents = 0;
	int32 BuiltHousingCapacity = 0;
	int32 HousingCapacity = 0;
	int64 ResidentialUpkeep = 0;
	int64 CommercialUpkeep = 0;
	int64 IndustrialUpkeep = 0;
	int64 ParkUpkeep = 0;
	int64 CivicUpkeep = 0;
	int64 UtilityUpgradeUpkeep = 0;
	bool bUsesPublicStaffing = false;
	TMap<uint64, int32> PublicJobs;
	TMap<uint64, int32> PublicStaffPercent;
	TMap<uint64, int32> PublicMinimumEducation;
	TMap<uint64, int32> HospitalCapacities;
	TMap<uint64, int32> HospitalFleetCapacities;
	int32 LegacyPowerBuildings = 0;
	int32 LegacyServiceBuildings = 0;
	TSet<uint64> HigherEducationCitizenIds;
	TMap<uint64, int32> StationRoadRanges;
	TMap<uint64, int32> StationProtectionCapacities;
	TMap<uint64, int32> StationFleetCapacities;
	TArray<int32> CrimeRisk;
	TArray<int32> FireRisk;
	TArray<uint8> FireStatus;
	int32 BuiltJobs = 0;
	int32 Jobs = 0;
	int32 CommercialJobs = 0;
	int32 CommercialOutput = 0;
	int32 IndustrialJobs = 0;
	int32 IndustrialOutput = 0;
	int32 PowerCapacity = 0;
	int32 PowerDemand = 0;
	int32 PowerUsed = 0;
	int32 ServiceCapacity = 0;
	int32 ServiceDemand = 0;
	int32 ServiceCoveredDemand = 0;
	int32 ServiceUsed = 0;
	bool bHasDisabledEntities = false;
	TMap<uint64, ECityOperationFailure> EntityFailures;
	// Operational capacities shared by economy totals and persistent population records.
	TMap<uint64, int32> EntityResidents;
	TMap<uint64, int32> EntityJobs;
	TMap<uint64, int32> EntityOutput;
	TMap<uint64, int32> EntityPollutionEmissions;
	TArray<uint64> OperationalServiceEntityIds;
	FCityServiceReach ServiceReach;
	FCityWaterSnapshot Water;

	int32 GetAvailablePower() const
	{
		return FMath::Max(0, PowerCapacity - PowerUsed);
	}
	int32 GetPowerShortage() const
	{
		return FMath::Max(0, PowerDemand - PowerCapacity);
	}
	int32 GetAvailableService() const
	{
		return FMath::Max(0, ServiceCapacity - ServiceUsed);
	}
	int32 GetServiceCoverageGap() const
	{
		return FMath::Max(0, ServiceDemand - ServiceCoveredDemand);
	}
	int32 GetServiceCapacityShortage() const
	{
		return FMath::Max(0, ServiceCoveredDemand - ServiceCapacity);
	}
	int32 GetUnservedServiceDemand() const
	{
		return FMath::Max(0, ServiceDemand - ServiceUsed);
	}
	ECityOperationFailure GetFailure(uint64 EntityId) const;
};

struct CITYSIMVR_API FCityGrowthResult
{
	ECityGrowthFailure Failure = ECityGrowthFailure::NotDue;
	ECityOperationFailure BlockingOperationFailure = ECityOperationFailure::None;
	uint64 BlockingEntityId = FCityGrid::EmptyEntityId;
	FName BlockingDefinitionId = NAME_None;
	uint64 EntityId = FCityGrid::EmptyEntityId;
	FCityCellCoord Anchor;
	ECityZoneType Zone = ECityZoneType::None;

	bool IsSuccess() const
	{
		return Failure == ECityGrowthFailure::None;
	}
};

class CITYSIMVR_API FCityServiceEvaluator
{
public:
	static void GetAdjacentRoadCells(const FCityPlacementModel& City, const FCityCellCoord& Anchor,
	                                 const FIntPoint& FootprintSize, bool bConnectedRoadsOnly,
	                                 TArray<FCityCellCoord>& OutRoadCells);
	static void BuildServiceReach(const FCityPlacementModel& City, const TArray<uint64>& ServiceEntityIds,
	                              int32 HopBudget, bool bConnectedRoadsOnly, FCityServiceReach& OutReach);
	static void BuildProviderReach(const FCityPlacementModel& City, uint64 ServiceEntityId, int32 HopBudget,
	                               bool bConnectedRoadsOnly, FCityServiceReach& OutReach);
	static bool IsFootprintReached(const FCityPlacementModel& City, const FCityServiceReach& Reach,
	                               const FCityCellCoord& Anchor, const FIntPoint& FootprintSize);
	static FCityServiceSnapshot BuildSnapshot(
	    const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	    const UCityBuildingCatalog* Catalog = nullptr, const FCityDevelopmentSnapshot* Development = nullptr,
	    const FCityEnvironmentSnapshot* Environment = nullptr, const FCityPopulationSnapshot* Population = nullptr,
	    const FCityFireSnapshot* Fire = nullptr, const FCityTerrainHeightfield* Terrain = nullptr,
	    int32 WaterRulesVersion = -1, FCityServiceEvaluationTiming* Timing = nullptr, bool bPrepareQueries = true);
	static ECityOperationFailure EvaluateCandidate(const FCityPlacementModel& City,
	                                               const FCityServiceSnapshot& Services,
	                                               const FCityEconomyTuning& Tuning, FName DefinitionId,
	                                               const FCityCellCoord& Anchor, const FIntPoint& FootprintSize,
	                                               const UCityBuildingCatalog* Catalog = nullptr);
	static FString FailureToString(ECityOperationFailure Failure);
};

class CITYSIMVR_API FCityGrowthSimulation
{
public:
	FCityGrowthSimulation() = default;
	explicit FCityGrowthSimulation(uint32 Seed) : RandomState(Seed == 0 ? 1u : Seed) {}

	FCityGrowthResult AdvanceToStep(int64 FixedStepId, FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                                const FCityNeedsSnapshot& Needs, const FCityEconomyTuning& Tuning,
	                                const FCityBuildingDefinition* HomeDefinition,
	                                const FCityBuildingDefinition* ShopDefinition, bool bGrowthBlockedByShortfall,
	                                const FCityBuildingDefinition* IndustryDefinition = nullptr,
	                                const FCityEnvironmentSnapshot* Environment = nullptr);

	uint32 GetRandomState() const
	{
		return RandomState;
	}
	int64 GetNextGrowthStep() const
	{
		return NextGrowthStep;
	}
	bool RestoreState(uint32 InRandomState, int64 InNextGrowthStep, FString& OutError);
	static FString FailureToString(ECityGrowthFailure Failure);
	static FString DescribeResult(const FCityGrowthResult& Result);

private:
	uint32 NextRandom();

	uint32 RandomState = 1u;
	int64 NextGrowthStep = 0;
};
