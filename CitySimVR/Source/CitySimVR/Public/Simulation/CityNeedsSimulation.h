#pragma once

#include "CoreMinimal.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityPopulationSimulation.h"

struct FCityBuildingDefinition;
struct FCityEconomyTuning;
struct FCityServiceSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityDevelopmentSnapshot;

// Capacity preferences, not a mandate to replace existing buildings. Reserved
// includes construction and unavailable lots, so delayed supply cannot spawn twice.
struct CITYSIMVR_API FCityDensityDemand
{
	int32 Desired[3] = {0, 0, 0};
	int32 Reserved[3] = {0, 0, 0};
	int32 BuildingCapacity[3] = {0, 0, 0};
	int32 AdditionalCapacity() const;
	int32 Balance(ECityDensityLevel Level) const;
	int32 UpgradeDemand(ECityDensityLevel Target) const;
	int32 NewLowDemand() const;
	bool CanUpgrade(const FCityBuildingLevelDefinition& Current, const FCityBuildingLevelDefinition& Target,
	                ECityDevelopmentDemandType Type) const;
	static int32 Capacity(const FCityBuildingLevelDefinition& Level, ECityDevelopmentDemandType Type);
};

enum class ECityDemandConstraint : uint8
{
	None,
	LowDemand,
	OperatingShortfall,
	NoZonedFootprint,
	NoRoad,
	NoService,
	ServiceCapacity,
	NoPower,
	PowerCapacity,
	LowDesirability,
	PollutionPressure,
	InvalidTerrain,
	InsufficientFunds,
	MissingDefinition
};

struct CITYSIMVR_API FCityNeedsSnapshot
{
	bool bUsesStoredSupplies = false;
	bool bHasDensityDemand = false;
	bool bStarterHousingDemand = false;
	int32 DensityEmploymentPercent = 0;
	int32 DensitySkilledPercent = 0;
	FCityDensityDemand ResidentialDensity;
	FCityDensityDemand CommercialDensity;
	FCityDensityDemand IndustrialDensity;
	const FCityDensityDemand& DensityFor(ECityDevelopmentDemandType Type) const;
	int32 BuiltHousingCapacity = 0;
	int32 HousingCapacity = 0;
	int32 OccupiedResidents = 0;
	int32 AvailableHousing = 0;
	int32 HousingShortage = 0;

	int32 BuiltJobCapacity = 0;
	int32 JobCapacity = 0;
	int32 EmployedJobs = 0;
	int32 AvailableJobs = 0;
	int32 JobShortage = 0;
	bool bHasWorkforceEducation = false;
	FCityWorkforceEducationSnapshot WorkforceEducation;

	int32 PowerCapacity = 0;
	int32 PowerDemand = 0;
	int32 PowerUsage = 0;
	int32 PowerShortage = 0;

	int32 ServiceCapacity = 0;
	int32 ServiceDemand = 0;
	int32 ServiceCoveredDemand = 0;
	int32 ServiceUsage = 0;
	int32 ServiceCoverageGap = 0;
	int32 ServiceCapacityShortage = 0;
	int32 UnservedServiceDemand = 0;

	int32 ResidentialDemand = 0;
	int32 CommercialDemand = 0;
	int32 IndustrialDemand = 0;
	int32 ResidentialDemandBalance = 0;
	int32 CommercialDemandBalance = 0;
	int32 IndustrialDemandBalance = 0;
	int32 ResidentialZonedFootprints = 0;
	int32 CommercialZonedFootprints = 0;
	int32 ResidentialEligibleFootprints = 0;
	int32 CommercialEligibleFootprints = 0;
	// Buildable lots under private funding (legacy field names retained for callers).
	int32 ResidentialAffordableFootprints = 0;
	int32 CommercialAffordableFootprints = 0;
	int32 IndustrialZonedFootprints = 0;
	int32 IndustrialEligibleFootprints = 0;
	int32 IndustrialAffordableFootprints = 0;
	// City treasury for public purchases; never an affordability gate for zoned growth.
	int64 AvailableConstructionFunds = 0;
	int64 ResidentialGrowthCost = 0;
	int64 CommercialGrowthCost = 0;
	int64 IndustrialGrowthCost = 0;
	int32 GrowthCheckIntervalDays = 3;
	int32 IndustrialJobCapacity = 0;
	int32 IndustrialOutput = 0;
	int32 HouseholdGoodsDemand = 0;
	int32 GoodsProduced = 0;
	int32 RetailCapacity = 0;
	int32 GoodsSold = 0;
	int32 UnmetGoodsDemand = 0;
	int32 UnfilledIndustrialOrders = 0;
	int32 AverageResidentialDesirability = 100;
	int32 ResidentialPollutionExposure = 0;
	ECityDemandConstraint ResidentialConstraint = ECityDemandConstraint::LowDemand;
	ECityDemandConstraint CommercialConstraint = ECityDemandConstraint::LowDemand;
	ECityDemandConstraint IndustrialConstraint = ECityDemandConstraint::LowDemand;
	ECityZoneType RecommendedGrowthZone = ECityZoneType::None;
	bool bInstantOccupancy = true;
};

struct FCityPopulationSnapshot;

class CITYSIMVR_API FCityNeedsEvaluator
{
public:
	static FCityNeedsSnapshot BuildSnapshot(
	    const FCityPlacementModel& City, const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	    const FCityBuildingDefinition* HomeDefinition, const FCityBuildingDefinition* ShopDefinition,
	    bool bOperatingShortfall, const FCityBuildingDefinition* IndustryDefinition = nullptr,
	    const FCityEnvironmentSnapshot* Environment = nullptr, int32 ResidentialTaxBasisPoints = 1000,
	    int32 CommercialTaxBasisPoints = 1000, const FCityPopulationSnapshot* Population = nullptr,
	    int32 IndustrialTaxBasisPoints = 1000, const FCityDevelopmentSnapshot* Development = nullptr,
	    struct FCityNeedsEvaluationTiming* Timing = nullptr, bool bPreparePopulationQueries = true);
	static void BuildDensityDemand(FCityNeedsSnapshot& Needs, const FCityPlacementModel& City,
	                               const FCityServiceSnapshot& Services, const FCityDevelopmentSnapshot& Development,
	                               const FCityEconomyTuning& Tuning, const FCityBuildingDefinition& Home,
	                               const FCityBuildingDefinition* Shop, const FCityBuildingDefinition* Industry,
	                               const FCityPopulationSnapshot* Population);
	static FString ConstraintToString(ECityDemandConstraint Constraint);
};
