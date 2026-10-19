#include "Simulation/CityEconomySimulation.h"
#include "Performance/CityEvaluationTiming.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityPopulationSimulation.h"

void FCityEconomySimulation::SettlePolicies(FCityPlacementModel& City, FCityPopulationSimulation& Population,
    const FCityServiceSnapshot& Services,const FCityEconomyTuning& Tuning)
{
    Population.SettlePolicies(Snapshot.Policies,City,Snapshot.LastBilledMonthId,Services,Tuning,!Snapshot.HasOperatingUpkeepShortfall());
}

void FCityEconomySimulation::UpdateResidentCensus(const FCityPopulationSnapshot& Population)
{
	Snapshot.Residents = Population.Citizens.Num();
	Snapshot.EmployedResidents = 0;
	Snapshot.HouseholdGoodsDemand = 0;
	for (const auto& C : Population.Citizens)
	{
		Snapshot.EmployedResidents += C.WorkplaceEntityId != 0;
		Snapshot.HouseholdGoodsDemand += C.ConsumptionNeed;
	}
	Snapshot.UnmetGoodsDemand = FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.GoodsSold);
	const int32 StoredShortage = FCityPopulationSimulation::StoredEssentialShortage(Population);
	if (StoredShortage >= 0) Snapshot.UnmetGoodsDemand = StoredShortage;
}

namespace
{
	int64 GetRoadMonthlyUpkeep(const FName DefinitionId)
	{
		if (DefinitionId == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt().MonthlyUpkeepPerCell;
		if (DefinitionId == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue().MonthlyUpkeepPerCell;
		if (DefinitionId == TEXT("Highway"))
			return FCityRoadDefinition::Highway().MonthlyUpkeepPerCell;
		return FCityRoadDefinition::Local().MonthlyUpkeepPerCell;
	}
} // namespace

void FCityEconomySimulation::Advance(const double RealDeltaSeconds, FCityPlacementModel& City,
                                     const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog,
                                     const FCityDevelopmentSnapshot* Development,
                                     const FCityPopulationSnapshot* Population, TFunction<void()> AfterStep,
                                     const FCityFireSnapshot* Fire, TFunction<void(int64)> BeforeStep,
                                     const FCityEnvironmentSnapshot* Environment, FCityEconomyEvaluationTiming* Timing,
                                     const bool bPreparePopulationQueries, FCityServiceSnapshot* OutStepServices)
{
	if (bPaused || RealDeltaSeconds <= 0.0 || Tuning.FixedStepSeconds <= 0.0 || Tuning.FixedStepsPerMonth <= 0 ||
	    Tuning.MaxCatchUpSteps <= 0)
	{
		return;
	}

	const double MaxAccumulator = Tuning.FixedStepSeconds * Tuning.MaxCatchUpSteps;
	const double ScaledDelta = RealDeltaSeconds * SpeedMultiplier;
	AccumulatorSeconds = FMath::Min(MaxAccumulator, AccumulatorSeconds + ScaledDelta);

	int32 Steps = 0;
	while (AccumulatorSeconds + UE_DOUBLE_SMALL_NUMBER >= Tuning.FixedStepSeconds && Steps < Tuning.MaxCatchUpSteps)
	{
		AccumulatorSeconds = FMath::Max(0.0, AccumulatorSeconds - Tuning.FixedStepSeconds);
		if (BeforeStep)
			BeforeStep(Snapshot.FixedStepId + 1);
		Step(City, Tuning, Catalog, Development, Population, Fire, Environment, Timing, bPreparePopulationQueries,
		     OutStepServices);
		const int32 PreviousSpeed = SpeedMultiplier;
		if (AfterStep)
			AfterStep();
		// Remaining scaled time must adopt an onset speed change in this same frame.
		if (SpeedMultiplier != PreviousSpeed)
			AccumulatorSeconds *= static_cast<double>(SpeedMultiplier) / PreviousSpeed;
		++Steps;
	}
}

bool FCityEconomySimulation::RestoreState(const FCityEconomySnapshot& InSnapshot, const double InAccumulatorSeconds,
                                          const bool bInPaused, const int32 InSpeedMultiplier,
                                          const FCityEconomyTuning& Tuning, FString& OutError)
{
	const bool bInvalidCounts =
	    InSnapshot.FixedStepId < 0 || InSnapshot.MonthId < 0 || InSnapshot.LastBilledMonthId < 0 ||
	    InSnapshot.LastBilledMonthId > InSnapshot.MonthId || InSnapshot.OccupiedHomes < 0 ||
	    InSnapshot.BuiltShops < 0 || InSnapshot.OperationalShops < 0 || InSnapshot.CommercialOutput < 0 ||
	    InSnapshot.BuiltIndustries < 0 || InSnapshot.OperationalIndustries < 0 || InSnapshot.IndustrialOutput < 0 ||
	    InSnapshot.BuiltParks < 0 || InSnapshot.BuiltPower < 0 || InSnapshot.BuiltService < 0 ||
	    InSnapshot.Residents < 0 || InSnapshot.Jobs < 0 || InSnapshot.EmployedResidents < 0 ||
	    InSnapshot.HouseholdGoodsDemand < 0 || InSnapshot.GoodsProduced < 0 || InSnapshot.RetailCapacity < 0 ||
	    InSnapshot.GoodsSold < 0 || InSnapshot.UnmetGoodsDemand < 0 || InSnapshot.HouseholdIncome < 0 ||
	    InSnapshot.HouseholdSpending < 0 || InSnapshot.LastPublicPayrollDue < 0 ||
	    InSnapshot.LastPublicPayrollPaid > InSnapshot.LastPublicPayrollDue || InSnapshot.PublicPayrollDue < 0 ||
	    InSnapshot.LastPublicPayrollPaid < 0 || InSnapshot.LastIncome < 0 || InSnapshot.LastResidentialTaxRevenue < 0 ||
	    InSnapshot.LastCommercialTaxRevenue < 0 || InSnapshot.LastIndustrialTaxRevenue < 0 ||
	    InSnapshot.LastUpkeep < 0 || InSnapshot.ResidentialUpkeep < 0 || InSnapshot.CommercialUpkeep < 0 ||
	    InSnapshot.IndustrialUpkeep < 0 || InSnapshot.ParkUpkeep < 0 || InSnapshot.UtilityUpgradeUpkeep < 0 ||
	    InSnapshot.CivicUpkeep < 0 || InSnapshot.UpkeepShortfall < 0 ||
	    InSnapshot.ResidentialTaxBasisPoints < FCityEconomySnapshot::MinimumTaxBasisPoints ||
	    InSnapshot.ResidentialTaxBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints ||
	    InSnapshot.CommercialTaxBasisPoints < FCityEconomySnapshot::MinimumTaxBasisPoints ||
	    InSnapshot.CommercialTaxBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints ||
	    InSnapshot.IndustrialTaxBasisPoints < 0 ||
	    InSnapshot.IndustrialTaxBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints ||
	    InSnapshot.bGrowthBlockedByShortfall != (InSnapshot.UpkeepShortfall > 0);
	const int64 ExpectedMonth = Tuning.FixedStepsPerMonth > 0 ? InSnapshot.FixedStepId / Tuning.FixedStepsPerMonth : 0;
	if (!InSnapshot.Policies.Validate(InSnapshot.LastBilledMonthId, OutError)) return false;
	if (!InSnapshot.Loan.Validate(InSnapshot.LastBilledMonthId, OutError))
		return false;
	if (bInvalidCounts || InSnapshot.MonthId != ExpectedMonth || !FMath::IsFinite(InAccumulatorSeconds) ||
	    InAccumulatorSeconds < 0.0 || InAccumulatorSeconds >= Tuning.FixedStepSeconds || InSpeedMultiplier < 1 ||
	    InSpeedMultiplier > 16)
	{
		OutError = TEXT("Invalid saved economy clock or aggregate state");
		return false;
	}
	Snapshot = InSnapshot;
	AccumulatorSeconds = InAccumulatorSeconds;
	bPaused = bInPaused;
	SpeedMultiplier = InSpeedMultiplier;
	return true;
}

int64 FCityEconomySimulation::GetProjectedResidentialTaxRevenue(const FCityEconomyTuning& Tuning) const
{
	return CalculateTaxRevenue(Snapshot.EmployedResidents, Tuning.HouseholdIncomePerEmployedResident,
	                           Snapshot.ResidentialTaxBasisPoints);
}

int64 FCityEconomySimulation::GetProjectedCommercialTaxRevenue(const FCityEconomyTuning& Tuning) const
{
	return CalculateTaxRevenue(Snapshot.GoodsSold, Tuning.IncomePerOperationalShop, Snapshot.CommercialTaxBasisPoints) /
	       FMath::Max(1, Tuning.JobsPerOperationalShop);
}

bool FCityEconomySimulation::SetTaxRates(const int32 ResidentialBasisPoints, const int32 CommercialBasisPoints,
                                         const int32 IndustrialBasisPoints)
{
	if (ResidentialBasisPoints < FCityEconomySnapshot::MinimumTaxBasisPoints ||
	    ResidentialBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints ||
	    CommercialBasisPoints < FCityEconomySnapshot::MinimumTaxBasisPoints ||
	    CommercialBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints || IndustrialBasisPoints < -1 ||
	    IndustrialBasisPoints > FCityEconomySnapshot::MaximumTaxBasisPoints)
	{
		return false;
	}
	Snapshot.ResidentialTaxBasisPoints = ResidentialBasisPoints;
	Snapshot.CommercialTaxBasisPoints = CommercialBasisPoints;
	if (IndustrialBasisPoints >= 0)
		Snapshot.IndustrialTaxBasisPoints = IndustrialBasisPoints;
	return true;
}

FCityBudgetProjection FCityEconomySimulation::GetProjectedBudget(
    const FCityPlacementModel& City, const FCityEconomyTuning& Tuning, const int32 ResidentialBasisPoints,
    const int32 CommercialBasisPoints, const int32 IndustrialBasisPoints, const int32 CompletedGoodsSales) const
{
	FCityBudgetProjection Budget;
	// UI settlement preview can use completed deliveries without changing forecasting or billing.
	const int32 GoodsSales = CompletedGoodsSales < 0 ? Snapshot.GoodsSold : CompletedGoodsSales;
	Budget.ResidentialTax =
	    CalculateTaxRevenue(Snapshot.EmployedResidents, Tuning.HouseholdIncomePerEmployedResident,
	                        ResidentialBasisPoints < 0
	                            ? Snapshot.ResidentialTaxBasisPoints
	                            : FMath::Clamp(ResidentialBasisPoints, 0, FCityEconomySnapshot::MaximumTaxBasisPoints));
	Budget.CommercialTax =
	    CalculateTaxRevenue(GoodsSales, Tuning.IncomePerOperationalShop,
	                        CommercialBasisPoints < 0
	                            ? Snapshot.CommercialTaxBasisPoints
	                            : FMath::Clamp(CommercialBasisPoints, 0, FCityEconomySnapshot::MaximumTaxBasisPoints)) /
	    FMath::Max(1, Tuning.JobsPerOperationalShop);
	Budget.IndustrialTax =
	    CalculateTaxRevenue(GoodsSales, Tuning.IncomePerOperationalIndustry,
	                        IndustrialBasisPoints < 0
	                            ? Snapshot.IndustrialTaxBasisPoints
	                            : FMath::Clamp(IndustrialBasisPoints, 0, FCityEconomySnapshot::MaximumTaxBasisPoints)) /
	    FMath::Max(1, Tuning.OutputPerOperationalIndustry);
	Budget.Income = SaturatingAdd(SaturatingAdd(Budget.ResidentialTax, Budget.CommercialTax), Budget.IndustrialTax);
	for (const FCityCellCoord& Cell : City.GetRoadCells())
		Budget.RoadUpkeep = SaturatingAdd(Budget.RoadUpkeep, GetRoadMonthlyUpkeep(City.GetRoadDefinitionId(Cell)));
	Budget.UtilityUpkeep =
	    SaturatingAdd(SaturatingMultiply(Snapshot.BuiltPower, Tuning.PowerUpkeep),
	                  SaturatingAdd(SaturatingMultiply(Snapshot.BuiltService, Tuning.WaterTreatmentUpkeep),
	                                Snapshot.UtilityUpgradeUpkeep));
	Budget.BuildingUpkeep = SaturatingAdd(
	    SaturatingAdd(Snapshot.ResidentialUpkeep, Snapshot.CommercialUpkeep),
	    SaturatingAdd(Snapshot.IndustrialUpkeep, SaturatingAdd(Snapshot.ParkUpkeep, Snapshot.CivicUpkeep)));
	Budget.PublicPayroll = Snapshot.PublicPayrollDue;
	Budget.Expenses = SaturatingAdd(SaturatingAdd(Budget.RoadUpkeep, Budget.UtilityUpkeep),
	                                SaturatingAdd(Budget.BuildingUpkeep, Budget.PublicPayroll));
	Budget.DebtService = Snapshot.Loan.NextPayment();
	Budget.Expenses = SaturatingAdd(Budget.Expenses, Budget.DebtService);
	return Budget;
}

void FCityEconomySimulation::Step(FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
                                  const UCityBuildingCatalog* Catalog, const FCityDevelopmentSnapshot* Development,
                                  const FCityPopulationSnapshot* Population, const FCityFireSnapshot* Fire,
                                  const FCityEnvironmentSnapshot* Environment, FCityEconomyEvaluationTiming* Timing,
                                  const bool bPreparePopulationQueries, FCityServiceSnapshot* OutStepServices)
{
	++Snapshot.FixedStepId;
	RefreshAggregates(City, Tuning, Catalog, Development, Population, Fire, Environment, Timing,
	                  bPreparePopulationQueries, OutStepServices);
	Snapshot.MonthId = Snapshot.FixedStepId / Tuning.FixedStepsPerMonth;
	if (Snapshot.MonthId > Snapshot.LastBilledMonthId)
	{
		if (Population)
		{
			// Bill completed arrivals from the period that just ended, never reservations.
			Snapshot.GoodsSold = Population->SalesThisMonth;
			Snapshot.HouseholdSpending = Population->SpendingThisMonth;
			Snapshot.UnmetGoodsDemand = FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.GoodsSold);
			const int32 StoredShortage = FCityPopulationSimulation::StoredEssentialShortage(*Population, Snapshot.MonthId);
			if (StoredShortage >= 0) Snapshot.UnmetGoodsDemand = StoredShortage;
		}
		const double BillingStart = Timing ? FPlatformTime::Seconds() : 0;
		BillMonth(City, Tuning, Snapshot.MonthId);
		if (Timing)
			Timing->Billing.Add((FPlatformTime::Seconds() - BillingStart) * 1000);
	}
}

void FCityEconomySimulation::RefreshAggregates(
    const FCityPlacementModel& City, const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog,
    const FCityDevelopmentSnapshot* Development, const FCityPopulationSnapshot* Population,
    const FCityFireSnapshot* Fire, const FCityEnvironmentSnapshot* Environment, FCityEconomyEvaluationTiming* Timing,
    const bool bPreparePopulationQueries, FCityServiceSnapshot* OutStepServices)
{
	const double ServiceStart = Timing ? FPlatformTime::Seconds() : 0;
	FCityServiceSnapshot Services =
	    FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, Development, Environment, Population, Fire);
	if (Timing)
		Timing->Services.Add((FPlatformTime::Seconds() - ServiceStart) * 1000);
	Snapshot.OccupiedHomes = Services.OperationalHomes;
	Snapshot.BuiltShops = Services.BuiltShops;
	Snapshot.OperationalShops = Services.OperationalShops;
	Snapshot.CommercialOutput = Services.CommercialOutput;
	Snapshot.BuiltIndustries = Services.BuiltIndustries;
	Snapshot.OperationalIndustries = Services.OperationalIndustries;
	Snapshot.IndustrialOutput = Services.IndustrialOutput;
	Snapshot.BuiltParks = Services.BuiltParks;
	Snapshot.BuiltPower = Services.LegacyPowerBuildings;
	Snapshot.BuiltService = Services.LegacyServiceBuildings;
	Snapshot.Residents = Population ? FMath::Max(Services.Residents, Population->Citizens.Num()) : Services.Residents;
	Snapshot.ResidentialUpkeep = Services.ResidentialUpkeep;
	Snapshot.CommercialUpkeep = Services.CommercialUpkeep;
	Snapshot.IndustrialUpkeep = Services.IndustrialUpkeep;
	Snapshot.ParkUpkeep = Services.ParkUpkeep;
	Snapshot.CivicUpkeep = Services.CivicUpkeep;
	Snapshot.UtilityUpgradeUpkeep = Services.UtilityUpgradeUpkeep;
	Snapshot.Jobs = Services.Jobs;
	Snapshot.EmployedResidents = FMath::Min(Snapshot.Residents, Snapshot.Jobs);
	Snapshot.HouseholdGoodsDemand =
	    Snapshot.Residents > 0
	        ? FMath::DivideAndRoundUp(Snapshot.Residents, FMath::Max(1, Tuning.ResidentsPerGoodsUnit))
	        : 0;
	Snapshot.GoodsProduced = Snapshot.IndustrialOutput;
	Snapshot.RetailCapacity = Snapshot.CommercialOutput;
	Snapshot.HouseholdIncome =
	    SaturatingMultiply(Snapshot.EmployedResidents, Tuning.HouseholdIncomePerEmployedResident);
	const int32 AffordablePurchases = static_cast<int32>(
	    FMath::Min<int64>(MAX_int32, Snapshot.HouseholdIncome / FMath::Max(1, Tuning.HouseholdIncomePerGoodsPurchase)));
	Snapshot.GoodsSold = FMath::Min3(Snapshot.HouseholdGoodsDemand, Snapshot.GoodsProduced,
	                                 FMath::Min(Snapshot.RetailCapacity, AffordablePurchases));
	if (Population)
		Snapshot.GoodsSold = FCityPopulationSimulation::ForecastGoodsSales(
		    City, Snapshot, Tuning, Services, *Population, &Snapshot.HouseholdIncome, &Snapshot.GoodsProduced,
		    &Snapshot.PublicPayrollDue, &Snapshot.EmployedResidents, Timing ? &Timing->Forecast : nullptr,
		    bPreparePopulationQueries, &Snapshot.HouseholdSpending);
	Snapshot.UnmetGoodsDemand = FMath::Max(0, Snapshot.HouseholdGoodsDemand - Snapshot.GoodsSold);
	if (Population)
	{
		const int32 StoredShortage = FCityPopulationSimulation::StoredEssentialShortage(*Population, Snapshot.MonthId);
		if (StoredShortage >= 0) Snapshot.UnmetGoodsDemand = StoredShortage;
	}
	else Snapshot.HouseholdSpending = SaturatingMultiply(Snapshot.GoodsSold, Tuning.HouseholdIncomePerGoodsPurchase);
	// The synchronous AfterStep callback may consume this result while its
	// service inputs are unchanged. Each catch-up step replaces it; never retain
	// or reuse it after population, weather, development or incident updates.
	if (OutStepServices)
		*OutStepServices = MoveTemp(Services);
}

void FCityEconomySimulation::BillMonth(FCityPlacementModel& City, const FCityEconomyTuning& Tuning, const int64 MonthId)
{
	if (MonthId <= Snapshot.LastBilledMonthId)
	{
		return;
	}

	const FCityBudgetProjection Budget = GetProjectedBudget(City, Tuning);
	Snapshot.LastResidentialTaxRevenue = Budget.ResidentialTax;
	Snapshot.LastCommercialTaxRevenue = Budget.CommercialTax;
	Snapshot.LastIndustrialTaxRevenue = Budget.IndustrialTax;
	Snapshot.LastIncome = Budget.Income;
	Snapshot.LastPublicPayrollDue = Snapshot.PublicPayrollDue;
	Snapshot.LastUpkeep = Budget.Expenses - Budget.DebtService;
	Snapshot.UpkeepShortfall = City.ApplyMonthlyEconomy(Snapshot.LastIncome, Snapshot.LastUpkeep);
	// Ordinary operating bills have priority. Every paid civic salary is part
	// of this single treasury debit, then transferred to wallets at settlement.
	Snapshot.LastPublicPayrollPaid = FMath::Max<int64>(0, Snapshot.PublicPayrollDue - Snapshot.UpkeepShortfall);
	Snapshot.HouseholdIncome =
	    FMath::Max<int64>(0, Snapshot.HouseholdIncome - (Snapshot.PublicPayrollDue - Snapshot.LastPublicPayrollPaid));
	Snapshot.bGrowthBlockedByShortfall = Snapshot.UpkeepShortfall > 0;
	Snapshot.Loan.SettleMonth(City);
	Snapshot.LastBilledMonthId = MonthId;
}

int64 FCityEconomySimulation::CalculateTaxRevenue(const int64 Units, const int64 IncomeAtDefaultRate,
                                                  const int32 TaxBasisPoints)
{
	const int64 BaseRevenue = SaturatingMultiply(Units, IncomeAtDefaultRate);
	const int64 Whole = BaseRevenue / FCityEconomySnapshot::DefaultTaxBasisPoints;
	const int64 Remainder = BaseRevenue % FCityEconomySnapshot::DefaultTaxBasisPoints;
	return SaturatingAdd(SaturatingMultiply(Whole, TaxBasisPoints),
	                     SaturatingMultiply(Remainder, TaxBasisPoints) / FCityEconomySnapshot::DefaultTaxBasisPoints);
}

int64 FCityEconomySimulation::SaturatingMultiply(const int64 Left, const int64 Right)
{
	if (Left <= 0 || Right <= 0)
	{
		return 0;
	}
	return Left > MAX_int64 / Right ? MAX_int64 : Left * Right;
}

int64 FCityEconomySimulation::SaturatingAdd(const int64 Left, const int64 Right)
{
	const int64 SafeLeft = FMath::Max<int64>(0, Left);
	const int64 SafeRight = FMath::Max<int64>(0, Right);
	return SafeRight > MAX_int64 - SafeLeft ? MAX_int64 : SafeLeft + SafeRight;
}
