#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityCalendar.h"
#include "Simulation/CityLoans.h"
#include "Simulation/CityPolicies.h"
#include "CityEconomySimulation.generated.h"

class FCityPlacementModel;
class UCityBuildingCatalog;
struct FCityDevelopmentSnapshot;
struct FCityPopulationSnapshot;
struct FCityFireSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityServiceSnapshot;

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityEconomyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock", meta = (ClampMin = "0.01"))
	// Fine traffic updates stay at 2.4 seconds. 150 updates form one six-minute day.
	double FixedStepSeconds = 2.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock", meta = (ClampMin = "1"))
	// Legacy C++ field name: this is the DAILY settlement interval, not a calendar month.
	int32 FixedStepsPerMonth = FCityCalendar::DefaultStepsPerDay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Population", meta = (ClampMin = "1"))
	int32 UnemploymentMonthsBeforeHomelessness = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock", meta = (ClampMin = "1"))
	int32 MaxCatchUpSteps = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	FName HomeDefinitionId = TEXT("Home");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	FName ShopDefinitionId = TEXT("Shop");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	FName IndustrialDefinitionId = TEXT("Factory");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	FName ParkDefinitionId = TEXT("Park");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	FName PowerDefinitionId = TEXT("Power");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buildings")
	// "Service" remains the persisted schema-v1 content key so existing saves load.
	// Player-facing systems and all new simulation work treat it as Water Treatment.
	FName WaterTreatmentDefinitionId = TEXT("Service");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int32 ResidentsPerOccupiedHome = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int32 JobsPerOperationalShop = 10;

	// Leave room for new employers to attract households from outside the city.
	// Pending buildings still reserve these slots; this is not demand per tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 PrivateJobTargetOccupancyPercent = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int32 JobsPerOperationalIndustry = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int32 OutputPerOperationalIndustry = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int64 IncomePerOperationalIndustry = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int64 IndustrialUpkeep = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int32 IndustrialPowerDemand = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Industry", meta = (ClampMin = "0"))
	int32 IndustrialWaterDemand = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0"))
	int32 PollutionEmissionPerIndustrialBuilding = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0"))
	int32 PollutionDecayPerStep = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "8"))
	int32 PollutionSpreadDivisor = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "1"))
	int32 MaximumPollutionPerCell = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BaseResidentialDesirability = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DesirabilityRoadBonus = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DesirabilityUtilityBonus = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "1"))
	int32 PollutionPerDesirabilityPenalty = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MinimumResidentialDesirability = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ParkDesirabilityBonus = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desirability", meta = (ClampMin = "1"))
	int32 ParkDesirabilityRadiusCells = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 ParkUpkeep = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "1"))
	int32 CrimePopulationThreshold = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0", ClampMax = "12"))
	int32 CrimeGraceMonths = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "1"))
	int32 CrimeCheckIntervalSteps = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CrimeBaseRisk = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CrimeJobShortageWeight = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CrimeHomelessnessWeight = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0"))
	int32 PoliceRoadRangeHops = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "1"))
	int32 PoliceProtectionCapacity = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety", meta = (ClampMin = "0", ClampMax = "100"))
	int32 PoliceRiskReduction = 35;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FirePopulationThreshold = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireGraceMonths = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireCheckIntervalSteps = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireBaseRisk = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireRoadRangeHops = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireProtectionCapacity = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireRiskReduction = 75;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireSpreadChance = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireWindSpreadModifier = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireSpreadIntervalSteps = 5;
	// Accumulated damage per unattended game day. Suppression adds only one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "1", ClampMax = "100"))
	int32 FireDamagePerStep = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireRepairSteps = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0"))
	int32 FireRepairCostPercent = 25;
	// Damage at or below this limit recovers when a real response extinguishes
	// the fire. Zero disables automatic recovery; burnout always needs repair.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0", ClampMax = "99"))
	int32 FireMinorDamageAutoRepairLimit = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Staff", meta = (ClampMin = "1", ClampMax = "20"))
	int32 DepartmentStaff = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "1", ClampMax = "20"))
	int32 SchoolStaff = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "1", ClampMax = "500"))
	int32 SchoolCapacity = 48;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "1", ClampMax = "64"))
	int32 SchoolRoadRangeHops = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "1", ClampMax = "120"))
	int32 SchoolMonthsPerLevel = 6;
	// Stable-ID arrival distribution; existing citizens retain their qualification.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "0", ClampMax = "100"))
	int32 NewArrivalUneducatedPercent = 25;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Education", meta = (ClampMin = "0", ClampMax = "20"))
	int32 SchoolDesirabilityBonus = 8;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Staff", meta = (ClampMin = "1", ClampMax = "20"))
	int32 UtilityStaff = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0", ClampMax = "100"))
	int32 FireCrimeRiskWeight = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "0", ClampMax = "100"))
	int32 FireIndustryRiskWeight = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Safety|Fire", meta = (ClampMin = "1", ClampMax = "16"))
	int32 FireIndustryRadiusCells = 6;

	// Aggregate household/production tuning. These intentionally model flows,
	// not individual wallets or inventories.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Goods", meta = (ClampMin = "1"))
	int32 ResidentsPerGoodsUnit = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Goods", meta = (ClampMin = "1"))
	int32 HouseholdIncomePerEmployedResident = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Goods", meta = (ClampMin = "1"))
	int32 HouseholdIncomePerGoodsPurchase = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 IncomePerResident = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 IncomePerOperationalShop = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 HomeUpkeep = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 ShopUpkeep = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 PowerUpkeep = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy", meta = (ClampMin = "0"))
	int64 WaterTreatmentUpkeep = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth", meta = (ClampMin = "1"))
	// One fixed step is one calendar game day; billing still uses 25 days per month.
	int32 GrowthIntervalSteps = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth", meta = (ClampMin = "1"))
	int32 UtilityDemandPerBuilding = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth", meta = (ClampMin = "1"))
	int32 PowerCapacityPerBuilding = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth", meta = (ClampMin = "1"))
	int32 WaterCapacityPerTreatment = 200;

	// Connected, undamaged utilities retain baseline supply during hiring/payroll shortages.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MinimumUtilityOutputPercent = 50;

	int32 GetUtilityOutputPercent(int32 PaidStaffPercent) const
	{
		return FMath::Max(FMath::Clamp(MinimumUtilityOutputPercent, 0, 100), FMath::Clamp(PaidStaffPercent, 0, 100));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "0"))
	int32 WaterDemandPerResident = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "0"))
	int32 WaterDemandPerCommercialJob = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water", meta = (ClampMin = "0"))
	int32 WaterRangeRoadHops = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth")
	int32 GrowthSeed = 123456789;
};

struct CITYSIMVR_API FCityEconomySnapshot
{
	static constexpr int32 MinimumTaxBasisPoints = 0;
	static constexpr int32 MaximumTaxBasisPoints = 2500;
	static constexpr int32 DefaultTaxBasisPoints = 1000;
	static constexpr int32 TaxStepBasisPoints = 100;

	int64 FixedStepId = 0;
	int64 MonthId = 0;
	int64 LastBilledMonthId = 0;
	int64 PublicPayrollDue = 0;
	int64 LastPublicPayrollPaid = 0;
	int64 LastPublicPayrollDue = 0;
	bool HasOperatingUpkeepShortfall() const
	{
		return UpkeepShortfall > LastPublicPayrollDue;
	}
	int32 OccupiedHomes = 0;
	int32 BuiltShops = 0;
	int32 OperationalShops = 0;
	int32 CommercialOutput = 0;
	int32 BuiltIndustries = 0;
	int32 OperationalIndustries = 0;
	int32 IndustrialOutput = 0;
	int32 BuiltParks = 0;
	int32 BuiltPower = 0;
	int32 BuiltService = 0;
	int32 Residents = 0;
	int32 Jobs = 0;
	int32 EmployedResidents = 0;
	int32 HouseholdGoodsDemand = 0;
	int32 GoodsProduced = 0;
	int32 RetailCapacity = 0;
	int32 GoodsSold = 0;
	int32 UnmetGoodsDemand = 0;
	int64 HouseholdIncome = 0;
	int64 HouseholdSpending = 0;
	FCityPolicyState Policies;
	FCityLoanState Loan;
	int64 LastIncome = 0;
	int64 LastResidentialTaxRevenue = 0;
	int64 LastCommercialTaxRevenue = 0;
	int64 LastIndustrialTaxRevenue = 0;
	int64 LastUpkeep = 0;
	int64 ResidentialUpkeep = 0;
	int64 CommercialUpkeep = 0;
	int64 IndustrialUpkeep = 0;
	int64 ParkUpkeep = 0;
	int64 CivicUpkeep = 0;
	int64 UtilityUpgradeUpkeep = 0;
	int64 UpkeepShortfall = 0;
	bool bGrowthBlockedByShortfall = false;
	int32 ResidentialTaxBasisPoints = DefaultTaxBasisPoints;
	int32 CommercialTaxBasisPoints = DefaultTaxBasisPoints;
	int32 IndustrialTaxBasisPoints = DefaultTaxBasisPoints;
};

// Current-city forecast. Billing uses the same cost calculation with completed sales.
struct CITYSIMVR_API FCityBudgetProjection
{
	int64 ResidentialTax = 0;
	int64 CommercialTax = 0;
	int64 IndustrialTax = 0;
	int64 Income = 0;
	int64 RoadUpkeep = 0;
	int64 UtilityUpkeep = 0;
	int64 BuildingUpkeep = 0;
	int64 PublicPayroll = 0;
	int64 PolicyCost = 0;
	int64 DebtService = 0;
	int64 Expenses = 0;
	int64 Net() const
	{
		return Income - Expenses;
	}
};

class CITYSIMVR_API FCityEconomySimulation
{
public:
	// Optional result is overwritten before each synchronous AfterStep callback.
	// Reuse only while service inputs remain unchanged; later phases must reevaluate.
	void Advance(double RealDeltaSeconds, FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	             const UCityBuildingCatalog* Catalog = nullptr, const FCityDevelopmentSnapshot* Development = nullptr,
	             const FCityPopulationSnapshot* Population = nullptr, TFunction<void()> AfterStep = {},
	             const FCityFireSnapshot* Fire = nullptr, TFunction<void(int64)> BeforeStep = {},
	             const FCityEnvironmentSnapshot* Environment = nullptr,
	             struct FCityEconomyEvaluationTiming* Timing = nullptr, bool bPreparePopulationQueries = true,
	             FCityServiceSnapshot* OutStepServices = nullptr);
	void SetPaused(bool bInPaused)
	{
		bPaused = bInPaused;
	}
	void SetSpeedMultiplier(int32 InSpeedMultiplier)
	{
		SpeedMultiplier = FMath::Clamp(InSpeedMultiplier, 1, 16);
	}
	bool IsPaused() const
	{
		return bPaused;
	}
	int32 GetSpeedMultiplier() const
	{
		return SpeedMultiplier;
	}
	const FCityEconomySnapshot& GetSnapshot() const
	{
		return Snapshot;
	}
	// Closing census changes after arrivals/migration; settled money/goods stay intact.
	void UpdateResidentCensus(const FCityPopulationSnapshot& Population);
	int64 GetProjectedResidentialTaxRevenue(const FCityEconomyTuning& Tuning) const;
	int64 GetProjectedCommercialTaxRevenue(const FCityEconomyTuning& Tuning) const;
	FCityBudgetProjection GetProjectedBudget(const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	                                         int32 ResidentialBasisPoints = -1, int32 CommercialBasisPoints = -1,
	                                         int32 IndustrialBasisPoints = -1, int32 CompletedGoodsSales = -1) const;
	void SettlePolicies(FCityPlacementModel& City, class FCityPopulationSimulation& Population,
	                    const FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning);
	bool SetPolicy(ECityPolicy Policy, bool Enabled) { return Snapshot.Policies.SetEnabled(Policy,Enabled); }
	bool Borrow(FCityPlacementModel& City, int64 Amount, FString& Error)
	{ return Snapshot.Loan.Borrow(City, Amount, Snapshot.LastBilledMonthId, Error); }
	bool RepayLoan(FCityPlacementModel& City, FString& Error)
	{ return Snapshot.Loan.Repay(City, Error); }
	bool SetTaxRates(int32 ResidentialBasisPoints, int32 CommercialBasisPoints, int32 IndustrialBasisPoints = -1);
	static int64 CalculateTaxRevenue(int64 Units, int64 IncomeAtDefaultRate, int32 TaxBasisPoints);
	double GetAccumulatorSeconds() const
	{
		return AccumulatorSeconds;
	}
	double GetFixedStepProgress(const FCityEconomyTuning& Tuning) const
	{
		return Tuning.FixedStepSeconds > 0.0 ? AccumulatorSeconds / Tuning.FixedStepSeconds : 0.0;
	}
	bool RestoreState(const FCityEconomySnapshot& InSnapshot, double InAccumulatorSeconds, bool bInPaused,
	                  int32 InSpeedMultiplier, const FCityEconomyTuning& Tuning, FString& OutError);

private:
	void Step(FCityPlacementModel& City, const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog,
	          const FCityDevelopmentSnapshot* Development, const FCityPopulationSnapshot* Population,
	          const FCityFireSnapshot* Fire, const FCityEnvironmentSnapshot* Environment,
	          struct FCityEconomyEvaluationTiming* Timing, bool bPreparePopulationQueries,
	          FCityServiceSnapshot* OutStepServices);
	void RefreshAggregates(const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	                       const UCityBuildingCatalog* Catalog, const FCityDevelopmentSnapshot* Development,
	                       const FCityPopulationSnapshot* Population, const FCityFireSnapshot* Fire,
	                       const FCityEnvironmentSnapshot* Environment, struct FCityEconomyEvaluationTiming* Timing,
	                       bool bPreparePopulationQueries, FCityServiceSnapshot* OutStepServices);
	void BillMonth(FCityPlacementModel& City, const FCityEconomyTuning& Tuning, int64 MonthId);
	static int64 SaturatingMultiply(int64 Left, int64 Right);
	static int64 SaturatingAdd(int64 Left, int64 Right);

	double AccumulatorSeconds = 0.0;
	bool bPaused = false;
	int32 SpeedMultiplier = 1;
	FCityEconomySnapshot Snapshot;
};
