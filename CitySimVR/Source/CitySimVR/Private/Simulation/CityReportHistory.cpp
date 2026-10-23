#include "Simulation/CityReportHistory.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityWasteSimulation.h"
namespace
{
const TCHAR* Names[] = {TEXT("Population"),
                        TEXT("FilledJobs"),
                        TEXT("Homeless"),
                        TEXT("Unemployed"),
                        TEXT("ResidentNone"),
                        TEXT("ResidentBasic"),
                        TEXT("ResidentSecondary"),
                        TEXT("ResidentHigher"),
                        TEXT("StaffNone"),
                        TEXT("StaffBasic"),
                        TEXT("StaffSecondary"),
                        TEXT("StaffHigher"),
                        TEXT("Treasury"),
                        TEXT("Income"),
                        TEXT("Upkeep"),
                        TEXT("WagesDue"),
                        TEXT("WagesPaid"),
                        TEXT("GoodsProduced"),
                        TEXT("GoodsPurchased"),
                        TEXT("GoodsExported"),
                        TEXT("Inventory"),
                        TEXT("PowerDemand"),
                        TEXT("PowerCapacity"),
                        TEXT("WaterDemand"),
                        TEXT("WaterCapacity"),
                        TEXT("Pollution"),
                        TEXT("ResidentEducated"),
                        TEXT("StaffEducated"),
                        TEXT("Arsons"),
                        TEXT("Arrests"),
                        TEXT("Jailed"),
                        TEXT("IllResidents"),
                        TEXT("SevereIllness"),
                        TEXT("Inpatients"),
                        TEXT("RemotePatients"),
                        TEXT("MedicalLeave"),
                        TEXT("Recovered"),
                        TEXT("HospitalCapacity"),
                        TEXT("WastePiled"),
                        TEXT("WasteCargo"),
                        TEXT("WasteStorage"),
                        TEXT("WasteGenerated"),
                        TEXT("WasteCollected"),
                        TEXT("WasteDisposed"),
                        TEXT("WasteRecycled"),
                        TEXT("WasteOverflow"),
                        TEXT("RelocatedResidents"),
                        TEXT("DepartedResidents"),
                        TEXT("MovingExpenses"),
                        TEXT("DepartureMoney"),
                        TEXT("HappinessTotal"),
                        TEXT("Discontented"),
                        TEXT("SustainedDiscontent"),
                        TEXT("SearchingResidents"),
                        TEXT("ConcernHousing"),
                        TEXT("ConcernServices"),
                        TEXT("ConcernPurchases"),
                        TEXT("ConcernReserves"),
                        TEXT("ConcernPollution"),
                        TEXT("ConcernSafety"),
                        TEXT("ConcernHealth"),
                        TEXT("ConcernWaste"),
                        TEXT("ConcernTaxes"),TEXT("LoanPrincipal"),TEXT("LoanArrears"),TEXT("LoanBorrowed"),TEXT("LoanPrincipalPaid"),TEXT("LoanInterestPaid"),TEXT("LoanInterestAccrued"),TEXT("PolicyDue"),TEXT("PolicyAdministrationPaid"),TEXT("ReserveAssistancePaid"),TEXT("SchoolBursariesPaid"),TEXT("PolicyRecipientPayments"),TEXT("UnfundedPolicies")};
const TCHAR* Labels[] = {TEXT("Population"),
                         TEXT("Filled jobs"),
                         TEXT("Homeless residents"),
                         TEXT("Unemployed residents"),
                         TEXT("Residents: no education"),
                         TEXT("Residents: basic"),
                         TEXT("Residents: secondary"),
                         TEXT("Residents: higher"),
                         TEXT("Staff: no education"),
                         TEXT("Staff: basic"),
                         TEXT("Staff: secondary"),
                         TEXT("Staff: higher"),
                         TEXT("Treasury"),
                         TEXT("Tax income"),
                         TEXT("Upkeep due"),
                         TEXT("Wages due"),
                         TEXT("Wages paid"),
                         TEXT("Goods produced"),
                         TEXT("Goods purchased"),
                         TEXT("Goods exported"),
                         TEXT("Business inventory"),
                         TEXT("Power demand"),
                         TEXT("Power capacity"),
                         TEXT("Water demand"),
                         TEXT("Water capacity"),
                         TEXT("Total pollution"),
                         TEXT("Educated residents"),
                         TEXT("Educated staff"),
                         TEXT("Arson incidents to date"),
                         TEXT("Arrests to date"),
                         TEXT("Residents in jail"),
                         TEXT("Ill residents"),
                         TEXT("Severe illness"),
                         TEXT("Hospital patients"),
                         TEXT("Home recovery"),
                         TEXT("Medical leave"),
                         TEXT("Recovered to date"),
                         TEXT("Treatment capacity"),
                         TEXT("Uncollected waste"),
                         TEXT("Truck cargo"),
                         TEXT("Depot waste storage"),
                         TEXT("Waste generated to date"),
                         TEXT("Waste collected to date"),
                         TEXT("Waste disposed to date"),
                         TEXT("Waste recycled to date"),
                         TEXT("Waste overflow to date"),
                         TEXT("Residents relocated to date"),
                         TEXT("Residents departed to date"),
                         TEXT("Moving fees to external movers"),
                         TEXT("Money carried out of city"),
                         TEXT("Recorded happiness points"),
                         TEXT("Discontented residents"),
                         TEXT("Sustained discontent"),
                         TEXT("Home-search residents"),
                         TEXT("Housing concerns"),
                         TEXT("Utility concerns"),
                         TEXT("Shopping concerns"),
                         TEXT("Savings concerns"),
                         TEXT("Pollution concerns"),
                         TEXT("Safety concerns"),
                         TEXT("Health concerns"),
                         TEXT("Waste concerns"),
                         TEXT("Tax concerns"),TEXT("Loan principal"),TEXT("Loan unpaid dues"),TEXT("Borrowed from civic lender"),TEXT("Principal paid to civic lender"),TEXT("Interest paid to civic lender"),TEXT("Interest charged by civic lender"),TEXT("Optional policies planned"),TEXT("Civic administration paid"),TEXT("Reserve assistance paid"),TEXT("School bursaries paid"),TEXT("Policy recipient-payments"),TEXT("Unfunded programs")};
const TCHAR* Units[] = {TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("dollars"),         TEXT("dollars"),   TEXT("dollars"),
                        TEXT("dollars"),   TEXT("dollars"),   TEXT("units"),
                        TEXT("units"),     TEXT("units"),     TEXT("units"),
                        TEXT("power units"),     TEXT("power units"),     TEXT("water units"),
                        TEXT("water units"),     TEXT("pollution units"), TEXT("people"),
                        TEXT("people"),          TEXT("incidents"),       TEXT("arrests"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("places"),          TEXT("units"),
                        TEXT("units"),           TEXT("units"),           TEXT("units"),
                        TEXT("units"),           TEXT("units"),           TEXT("units"),
                        TEXT("units"),           TEXT("people"),          TEXT("people"),
                        TEXT("dollars to date"), TEXT("dollars to date"), TEXT("happiness points"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),
                        TEXT("people"),          TEXT("people"),          TEXT("people"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("dollars"),TEXT("payments"),TEXT("program-days")};
const bool Flows[] = {false, false, false, false, false, false, false, false, false, false, false,
                      false, false, true,  true,  true,  true,  true,  true,  true,  false, false,
                      false, false, false, false, false, false, false, false, false, false, false,
                      false, false, false, false, false, false, false, false, false, false, false,
                      false, false, false, false, false, false, false, false, false, false, false,
                      false, false, false, false, false, false, false, false,false,false,true,true,true,true,true,true,true,true,true,true};
} // namespace
static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(ECityReportMetric::Count));
static_assert(UE_ARRAY_COUNT(Labels) == UE_ARRAY_COUNT(Names));
static_assert(UE_ARRAY_COUNT(Units) == UE_ARRAY_COUNT(Names));
static_assert(UE_ARRAY_COUNT(Flows) == UE_ARRAY_COUNT(Names));
const TCHAR* FCityReportHistory::DisplayName(ECityReportMetric M)
{
	return Labels[static_cast<int32>(M)];
}
const TCHAR* FCityReportHistory::Name(ECityReportMetric M)
{
	return Names[static_cast<int32>(M)];
}
const TCHAR* FCityReportHistory::Unit(ECityReportMetric M)
{
	return Units[static_cast<int32>(M)];
}
bool FCityReportHistory::IsFlow(ECityReportMetric M)
{
	return Flows[static_cast<int32>(M)];
}
bool FCityReportHistory::Capture(const FCityEconomySnapshot& E, const FCityPopulationSnapshot& P,
                                 const FCityServiceSnapshot& S, const FCityEnvironmentSnapshot& Environment,
                                 int64 Treasury, const FCityCrimeSnapshot* Crime,
                                 const FCityHealthSnapshot* Health, const FCityWasteSnapshot* Waste)
{
	if (E.LastBilledMonthId <= 0 || E.LastBilledMonthId != P.LastSettledMonthId ||
	    (!Months.IsEmpty() && E.LastBilledMonthId <= Months.Last().MonthId))
		return false;
	FCityReportSample Sample = Observe(E, P, S, Environment, Treasury, Crime, Health, Waste);
	Months.Add(Sample);
	if (Months.Num() > MaximumMonths)
		Months.RemoveAt(0, Months.Num() - MaximumMonths);
	return true;
}
FCityReportSample FCityReportHistory::Observe(const FCityEconomySnapshot& E, const FCityPopulationSnapshot& P,
    const FCityServiceSnapshot& S, const FCityEnvironmentSnapshot& Environment, int64 Treasury,
    const FCityCrimeSnapshot* Crime, const FCityHealthSnapshot* Health, const FCityWasteSnapshot* Waste)
{
	FCityReportSample Sample;
	Sample.MonthId = E.LastBilledMonthId;
	const auto Set = [&Sample](ECityReportMetric M, int64 V) { Sample.Values[static_cast<int32>(M)] = V; };
	Sample.bHasPolicies=E.Policies.LastSettledMonth==E.LastBilledMonthId;
	if(Sample.bHasPolicies)
	{
		Set(ECityReportMetric::PolicyDue,E.Policies.Due());
		Set(ECityReportMetric::PolicyAdministrationPaid,E.Policies.Programs[0].AdministrationPaid+E.Policies.Programs[1].AdministrationPaid);
		Set(ECityReportMetric::ReserveAssistancePaid,E.Policies.Programs[0].GrantsPaid);
		Set(ECityReportMetric::SchoolBursariesPaid,E.Policies.Programs[1].GrantsPaid);
		Set(ECityReportMetric::PolicyRecipientPayments,E.Policies.Programs[0].Recipients+E.Policies.Programs[1].Recipients);
		Set(ECityReportMetric::UnfundedPolicies,E.Policies.Unfunded());
	}
	Sample.bHasLoans = true;
	Set(ECityReportMetric::LoanPrincipal, E.Loan.Principal);
	Set(ECityReportMetric::LoanArrears, E.Loan.Arrears());
	Set(ECityReportMetric::LoanBorrowed, E.Loan.LastBorrowed);
	Set(ECityReportMetric::LoanPrincipalPaid, E.Loan.LastPrincipalPaid);
	Set(ECityReportMetric::LoanInterestPaid, E.Loan.LastInterestPaid);
	Set(ECityReportMetric::LoanInterestAccrued, E.Loan.LastInterestAccrued);
	Set(ECityReportMetric::Population, P.Citizens.Num());
	Sample.bHasMigration = P.LastMigrationMonth == E.LastBilledMonthId;
	Set(ECityReportMetric::RelocatedResidents, P.RelocatedResidents);
	Set(ECityReportMetric::DepartedResidents, P.DepartedResidents);
	Set(ECityReportMetric::MovingExpenses, P.MovingExpenses);
	Set(ECityReportMetric::DepartureMoney, P.DepartureMoney);
	Sample.bHasSentiment = !P.Citizens.ContainsByPredicate(
	    [&E](const auto& Citizen) { return Citizen.LastHappinessMonth != E.LastBilledMonthId; });
	for (const auto& Citizen : P.Citizens)
	{
		++Sample.Values[static_cast<int32>(ECityReportMetric::ResidentNone) +
		                static_cast<int32>(Citizen.Education)];
		if (Sample.bHasSentiment)
		{
			Sample.Values[static_cast<int32>(ECityReportMetric::HappinessTotal)] += Citizen.Happiness;
			Sample.Values[static_cast<int32>(ECityReportMetric::Discontented)] +=
			    Citizen.Happiness < FCityHappinessEvaluator::DiscontentThreshold;
			Sample.Values[static_cast<int32>(ECityReportMetric::SustainedDiscontent)] +=
			    Citizen.Happiness < FCityHappinessEvaluator::DiscontentThreshold &&
			    Citizen.DiscontentMonths >= FCityHappinessEvaluator::SustainedMonths;
			Sample.Values[static_cast<int32>(ECityReportMetric::SearchingResidents)] +=
			    FCityHousingHistory::IsSearching(static_cast<uint8>(Citizen.HousingStatus));
			for (int32 I = 0; I < 9; ++I)
				Sample.Values[static_cast<int32>(ECityReportMetric::ConcernHousing) + I] +=
				    (Citizen.HappinessConcerns & (1 << I)) != 0;
		}
		if (Citizen.IsHomeless())
			++Sample.Values[static_cast<int32>(ECityReportMetric::Homeless)];
		if (!Citizen.WorkplaceEntityId && !Citizen.IsDetained())
			++Sample.Values[static_cast<int32>(ECityReportMetric::Unemployed)];
	}
	Set(ECityReportMetric::ResidentEducated, P.Citizens.Num() - Sample.Get(ECityReportMetric::ResidentNone));
	const auto Staff = P.BuildWorkforceEducation();
	Set(ECityReportMetric::StaffEducated, Staff.Educated());
	Set(ECityReportMetric::FilledJobs, Staff.Total());
	Set(ECityReportMetric::StaffNone, Staff.Uneducated);
	Set(ECityReportMetric::StaffBasic, Staff.Basic);
	Set(ECityReportMetric::StaffSecondary, Staff.Secondary);
	Set(ECityReportMetric::StaffHigher, Staff.Higher);
	Set(ECityReportMetric::Treasury, Treasury);
	Set(ECityReportMetric::Income, E.LastIncome);
	Set(ECityReportMetric::Upkeep, E.LastUpkeep);
	for (const auto& B : P.Businesses)
	{
		Sample.Values[static_cast<int32>(ECityReportMetric::Inventory)] += B.Inventory;
		Sample.Values[static_cast<int32>(ECityReportMetric::GoodsProduced)] += B.ProducedGoods;
		Sample.Values[static_cast<int32>(ECityReportMetric::WagesDue)] += B.PayrollDue;
		Sample.Values[static_cast<int32>(ECityReportMetric::WagesPaid)] += B.PayrollPaid;
	}
	for (const auto& B : P.PublicEmployers)
	{
		Sample.Values[static_cast<int32>(ECityReportMetric::WagesDue)] += B.PayrollDue;
		Sample.Values[static_cast<int32>(ECityReportMetric::WagesPaid)] += B.PayrollPaid;
	}
	Set(ECityReportMetric::GoodsPurchased, P.LastMonthSales);
	Set(ECityReportMetric::GoodsExported, P.LastMonthExports);
	Set(ECityReportMetric::PowerDemand, S.PowerDemand);
	Set(ECityReportMetric::PowerCapacity, S.PowerCapacity);
	Set(ECityReportMetric::WaterDemand, S.ServiceDemand);
	Set(ECityReportMetric::WaterCapacity, S.ServiceCapacity);
	Set(ECityReportMetric::Pollution, Environment.TotalPollution);
	Sample.bHasSafety = Crime != nullptr;
	if (Crime)
	{
		Set(ECityReportMetric::Arsons, Crime->TotalArsons);
		Set(ECityReportMetric::Arrests, Crime->TotalArrests);
	}
	Set(ECityReportMetric::Jailed, S.DetainedResidents);
	Sample.bHasHealth = Health != nullptr;
	if (Health)
	{
		Set(ECityReportMetric::IllResidents, Health->Patients.Num());
		Set(ECityReportMetric::Recovered, Health->TotalRecovered);
		for (const auto& Patient : Health->Patients)
		{
			Sample.Values[static_cast<int32>(ECityReportMetric::SevereIllness)] +=
			    Patient.Illness == ECityIllness::Severe;
			Sample.Values[static_cast<int32>(ECityReportMetric::Inpatients)] +=
			    Patient.Care == ECityHealthCare::Hospital;
			Sample.Values[static_cast<int32>(ECityReportMetric::RemotePatients)] +=
			    Patient.Care == ECityHealthCare::HomeRecovery;
			Sample.Values[static_cast<int32>(ECityReportMetric::MedicalLeave)] += Patient.InterruptsWork();
		}
		for (const auto& Capacity : S.HospitalCapacities)
			Sample.Values[static_cast<int32>(ECityReportMetric::HospitalCapacity)] += Capacity.Value;
	}
	Sample.bHasWaste = Waste != nullptr;
	if (Waste)
	{
		Set(ECityReportMetric::WastePiled, Waste->Piled());
		Set(ECityReportMetric::WasteCargo, Waste->InTrucks());
		Set(ECityReportMetric::WasteStorage, Waste->InDepots());
		Set(ECityReportMetric::WasteGenerated, Waste->Generated);
		Set(ECityReportMetric::WasteCollected, Waste->Collected);
		Set(ECityReportMetric::WasteDisposed, Waste->Disposed);
		Set(ECityReportMetric::WasteRecycled, Waste->Recycled);
		Set(ECityReportMetric::WasteOverflow, Waste->Overflow);
	}
	return Sample;
}
bool FCityReportHistory::ObserveHourly(const FCityEconomySnapshot& E, const FCityPopulationSnapshot& P,
    const FCityServiceSnapshot& S, const FCityEnvironmentSnapshot& Environment, const int64 Treasury,
    const int32 StepsPerDay, const FCityCrimeSnapshot* Crime, const FCityHealthSnapshot* Health,
    const FCityWasteSnapshot* Waste)
{
    const int64 Hour = FCityCalendar::HourIndex(E.FixedStepId, StepsPerDay);
    const auto Prime = [&]() {
        bHourlyPrimed = true; HourlyLastStep = E.FixedStepId; HourlyLastHour = Hour;
        HourlyLastDay = E.LastBilledMonthId; HourlyLastPurchases = P.TotalArrivalPurchases;
        HourlyLastExports = P.ExportsThisMonth;
    };
    if (!bHourlyPrimed) { Prime(); return false; }
    if (E.FixedStepId <= HourlyLastStep || Hour <= HourlyLastHour) return false;
    // Never manufacture missed hours after a clock discontinuity or late observation start.
    if (Hour != HourlyLastHour + 1 || E.FixedStepId != FCityCalendar::HourEndStep(Hour, StepsPerDay))
    { Prime(); return false; }
    auto Sample = Observe(E, P, S, Environment, Treasury, Crime, Health, Waste);
    Sample.MonthId = Hour; Sample.ObservedDays = 0;
    Sample.HourStartStep = HourlyLastStep; Sample.HourEndStep = E.FixedStepId;
    const bool Settled = E.LastBilledMonthId > HourlyLastDay;
    for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
        if (Flows[I] && !Settled) Sample.Values[I] = 0;
    Sample.Values[static_cast<int32>(ECityReportMetric::GoodsPurchased)] =
        FMath::Max<int64>(0, P.TotalArrivalPurchases - HourlyLastPurchases);
    Sample.Values[static_cast<int32>(ECityReportMetric::GoodsExported)] = FMath::Max<int64>(0,
        (Settled ? P.LastMonthExports : 0) + P.ExportsThisMonth - HourlyLastExports);
    Hours.Add(Sample);
    if (Hours.Num() > MaximumHours) Hours.RemoveAt(0, Hours.Num() - MaximumHours);
    Prime();
    return true;
}

bool FCityReportHistory::ValidateHourly(const int64 MaximumStep, const int32 StepsPerDay, FString& Error) const
{
    if (MaximumStep < 0 || MaximumStep > MAX_int64 / 48 || StepsPerDay < 24 ||
        Hours.Num() > MaximumHours || HourlyLastStep < 0 || HourlyLastStep > MaximumStep ||
        HourlyLastHour != FCityCalendar::HourIndex(HourlyLastStep, StepsPerDay) ||
        HourlyLastDay != FCityCalendar::DayIndex(HourlyLastStep, StepsPerDay) ||
        HourlyLastPurchases < 0 || HourlyLastExports < 0 ||
        (!bHourlyPrimed && (!Hours.IsEmpty() || HourlyLastStep || HourlyLastPurchases || HourlyLastExports)))
    { Error = TEXT("Invalid hourly report observation baseline"); return false; }
    int64 Previous = 0;
    for (const auto& H : Hours)
    {
        if (H.MonthId <= Previous || H.MonthId > HourlyLastHour || H.HourStartStep < FCityCalendar::HourEndStep(H.MonthId - 1, StepsPerDay) ||
            H.HourEndStep != FCityCalendar::HourEndStep(H.MonthId, StepsPerDay) ||
            H.HourStartStep >= H.HourEndStep || H.HourEndStep > HourlyLastStep)
        { Error = TEXT("Invalid hourly report window"); return false; }
        for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
            if ((I != static_cast<int32>(ECityReportMetric::Treasury) && H.Values[I] < 0) ||
                H.Values[I] > MAX_int64 / (MaximumMonths + 1))
            { Error = TEXT("Invalid hourly report metric"); return false; }
        Previous = H.MonthId;
    }
    return true;
}

bool FCityReportHistory::Validate(int64 SettledMonth, FString& Error) const
{
	if (Months.Num() > MaximumMonths)
	{
		Error = TEXT("Report history exceeds limit");
		return false;
	}
	int64 Previous = 0;
	for (const auto& S : Months)
	{
		if (S.MonthId <= Previous || (Previous > 0 && S.MonthId != Previous + 1) ||
		    S.MonthId > SettledMonth || S.ObservedMonths != 1)
		{
			Error = TEXT("Invalid report month sequence");
			return false;
		}
		for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
			if (I != static_cast<int32>(ECityReportMetric::Treasury) &&
			    (S.Values[I] < 0 || S.Values[I] > MAX_int64 / (MaximumMonths + 1)))
			{
				Error = TEXT("Invalid report metric");
				return false;
			}
		const int64 People = S.Get(ECityReportMetric::Population);
		const int64 Staff = S.Get(ECityReportMetric::StaffNone) + S.Get(ECityReportMetric::StaffBasic) +
		                    S.Get(ECityReportMetric::StaffSecondary) + S.Get(ECityReportMetric::StaffHigher);
		const int64 Residents =
		    S.Get(ECityReportMetric::ResidentNone) + S.Get(ECityReportMetric::ResidentBasic) +
		    S.Get(ECityReportMetric::ResidentSecondary) + S.Get(ECityReportMetric::ResidentHigher);
		if (S.Get(ECityReportMetric::ResidentEducated) != People - S.Get(ECityReportMetric::ResidentNone) ||
		    S.Get(ECityReportMetric::StaffEducated) != Staff - S.Get(ECityReportMetric::StaffNone) ||
		    People != Residents || Staff != S.Get(ECityReportMetric::FilledJobs) ||
		    Staff + S.Get(ECityReportMetric::Unemployed) + S.Get(ECityReportMetric::Jailed) != People ||
		    S.Get(ECityReportMetric::Homeless) > People ||
		    S.Get(ECityReportMetric::WagesPaid) > S.Get(ECityReportMetric::WagesDue))
		{
			Error = TEXT("Report census or payroll does not reconcile");
			return false;
		}
		for (int32 I = static_cast<int32>(ECityReportMetric::HappinessTotal);
		     I < static_cast<int32>(ECityReportMetric::LoanPrincipal); ++I)
			if ((!S.bHasSentiment && S.Values[I] != 0) ||
			    (I == static_cast<int32>(ECityReportMetric::HappinessTotal) ? S.Values[I] > People * 100
			                                                                : S.Values[I] > People))
			{
				Error = TEXT("Report sentiment does not reconcile");
				return false;
			}
		if (S.Get(ECityReportMetric::SustainedDiscontent) > S.Get(ECityReportMetric::Discontented))
		{
			Error = TEXT("Sustained discontent exceeds discontent");
			return false;
		}
		for (int32 I = static_cast<int32>(ECityReportMetric::LoanPrincipal);
		     I < static_cast<int32>(ECityReportMetric::PolicyDue); ++I)
			if (!S.bHasLoans && S.Values[I])
			{ Error = TEXT("Unrecorded loan history has values"); return false; }
		for(int32 I=static_cast<int32>(ECityReportMetric::PolicyDue);I<static_cast<int32>(ECityReportMetric::Count);++I)
			if(!S.bHasPolicies && S.Values[I]) { Error=TEXT("Unrecorded policy history has values");return false; }
		Previous = S.MonthId;
	}
	return true;
}
const TCHAR* FCityReportHistory::PeriodName(const ECityReportPeriod Period)
{
    switch (Period)
    {
    case ECityReportPeriod::Hourly: return TEXT("Hour");
    case ECityReportPeriod::Daily: return TEXT("Day");
    case ECityReportPeriod::Weekly: return TEXT("Week");
    case ECityReportPeriod::Annual: return TEXT("Year");
    default: return TEXT("Month");
    }
}
int32 FCityReportHistory::PeriodDays(const ECityReportPeriod Period)
{
    switch (Period)
    {
    case ECityReportPeriod::Hourly: return 0;
    case ECityReportPeriod::Daily: return 1;
    case ECityReportPeriod::Weekly: return FCityCalendar::DaysPerWeek;
    case ECityReportPeriod::Annual: return FCityCalendar::DaysPerMonth * FCityCalendar::MonthsPerYear;
    default: return FCityCalendar::DaysPerMonth;
    }
}
TArray<FCityReportSample> FCityReportHistory::Samples(const ECityReportPeriod Period) const
{
    switch (Period)
    {
    case ECityReportPeriod::Hourly: return Hours;
    case ECityReportPeriod::Daily: return Months;
    case ECityReportPeriod::Weekly: return Weekly();
    case ECityReportPeriod::Annual: return Annual();
    default: return Monthly();
    }
}
TArray<FCityReportSample> FCityReportHistory::Weekly() const
{
    return RollupDays(FCityCalendar::DaysPerWeek);
}
TArray<FCityReportSample> FCityReportHistory::Monthly() const
{
    return RollupDays(FCityCalendar::DaysPerMonth);
}
TArray<FCityReportSample> FCityReportHistory::RollupDays(const int32 Days) const
{
    TArray<FCityReportSample> Result;
    for (const auto& Day : Months)
    {
        const int64 Month = (Day.MonthId - 1) / Days + 1;
        const bool First = Result.IsEmpty() || Result.Last().MonthId != Month;
        if (First)
        {
            FCityReportSample S = Day; S.MonthId = Month; S.ObservedDays = 0;
            for (auto& Value : S.Values) Value = 0;
            Result.Add(S);
        }
        auto& S = Result.Last();
        ++S.ObservedDays;
        S.bHasSafety = Day.bHasSafety; S.bHasHealth = Day.bHasHealth;
        S.bHasWaste = Day.bHasWaste; S.bHasMigration = Day.bHasMigration;
        S.bHasSentiment = First ? Day.bHasSentiment : S.bHasSentiment && Day.bHasSentiment;
        S.bHasLoans = First ? Day.bHasLoans : S.bHasLoans && Day.bHasLoans;
        S.bHasPolicies = First ? Day.bHasPolicies : S.bHasPolicies && Day.bHasPolicies;
        for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
            if (Flows[I]) S.Values[I] += Day.Values[I]; else S.Values[I] = Day.Values[I];
    }
    return Result;
}
TArray<FCityReportSample> FCityReportHistory::Annual() const
{
	TArray<FCityReportSample> Result;
	for (const auto& M : Monthly())
	{
		const int64 Year = (M.MonthId - 1) / 12 + 1;
		if (Result.IsEmpty() || Result.Last().MonthId != Year)
		{
			FCityReportSample S;
			S.MonthId = Year;
			S.ObservedMonths = 0; S.ObservedDays = 0;
			Result.Add(S);
		}
		auto& S = Result.Last();
		++S.ObservedMonths; S.ObservedDays += M.ObservedDays;
		S.bHasSafety = M.bHasSafety;
		S.bHasHealth = M.bHasHealth;
		S.bHasWaste = M.bHasWaste;
		S.bHasMigration = M.bHasMigration;
		S.bHasPolicies = S.ObservedMonths==1 ? M.bHasPolicies : S.bHasPolicies && M.bHasPolicies;
		S.bHasLoans = S.ObservedMonths == 1 ? M.bHasLoans : S.bHasLoans && M.bHasLoans;
		S.bHasSentiment = S.ObservedMonths == 1 ? M.bHasSentiment : S.bHasSentiment && M.bHasSentiment;
		for (int32 I = 0; I < static_cast<int32>(ECityReportMetric::Count); ++I)
			if (Flows[I])
				S.Values[I] += M.Values[I];
			else
				S.Values[I] = M.Values[I];
	}
	return Result;
}
