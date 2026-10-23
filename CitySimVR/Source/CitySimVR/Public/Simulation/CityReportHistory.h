#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityCalendar.h"
#include "CityReportHistory.generated.h"
UENUM(BlueprintType)
enum class ECityReportPeriod : uint8 { Daily, Weekly, Monthly, Annual, Hourly };

struct FCityEconomySnapshot;
struct FCityPopulationSnapshot;
struct FCityServiceSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityCrimeSnapshot;
struct FCityHealthSnapshot;
struct FCityWasteSnapshot;
// Stable order is part of the versioned save contract. Append new metrics.
enum class ECityReportMetric : uint8
{
	Population,
	FilledJobs,
	Homeless,
	Unemployed,
	ResidentNone,
	ResidentBasic,
	ResidentSecondary,
	ResidentHigher,
	StaffNone,
	StaffBasic,
	StaffSecondary,
	StaffHigher,
	Treasury,
	Income,
	Upkeep,
	WagesDue,
	WagesPaid,
	GoodsProduced,
	GoodsPurchased,
	GoodsExported,
	Inventory,
	PowerDemand,
	PowerCapacity,
	WaterDemand,
	WaterCapacity,
	Pollution,
	ResidentEducated,
	StaffEducated,
	Arsons,
	Arrests,
	Jailed,
	IllResidents,
	SevereIllness,
	Inpatients,
	RemotePatients,
	MedicalLeave,
	Recovered,
	HospitalCapacity,
	WastePiled,
	WasteCargo,
	WasteStorage,
	WasteGenerated,
	WasteCollected,
	WasteDisposed,
	WasteRecycled,
	WasteOverflow,
	RelocatedResidents,
	DepartedResidents,
	MovingExpenses,
	DepartureMoney,
	HappinessTotal,
	Discontented,
	SustainedDiscontent,
	SearchingResidents,
	ConcernHousing,
	ConcernServices,
	ConcernPurchases,
	ConcernReserves,
	ConcernPollution,
	ConcernSafety,
	ConcernHealth,
	ConcernWaste,
	ConcernTaxes,
	LoanPrincipal,
	LoanArrears,
	LoanBorrowed,
	LoanPrincipalPaid,
	LoanInterestPaid,
	LoanInterestAccrued,
	PolicyDue,
	PolicyAdministrationPaid,
	ReserveAssistancePaid,
	SchoolBursariesPaid,
	PolicyRecipientPayments,
	UnfundedPolicies,
	Count
};
struct CITYSIMVR_API FCityReportSample
{
	int64 MonthId = 0;
	int64 HourStartStep = 0, HourEndStep = 0; // Hourly observation window only.
	int32 ObservedMonths = 1;
	int32 ObservedDays = 1; // Derived monthly/annual coverage; saved raw rows each cover one day.
	bool bHasSafety = false;
	bool bHasHealth = false;
	bool bHasWaste = false;
	bool bHasMigration = false;
	bool bHasPolicies = false;
	bool bHasLoans = false;
	bool bHasSentiment = false;
	int64 Values[static_cast<int32>(ECityReportMetric::Count)] = {};
	int64 Get(ECityReportMetric Metric) const
	{
		return Values[static_cast<int32>(Metric)];
	}
};
class CITYSIMVR_API FCityReportHistory
{
public:
	static constexpr int32 MaximumMonths = 120 * FCityCalendar::DaysPerMonth; // Legacy name: saved daily rows.
	TArray<FCityReportSample> Months;
	static constexpr int32 MaximumHours = 5 * 24;
	TArray<FCityReportSample> Hours;
	bool bHourlyPrimed = false;
	int64 HourlyLastStep = 0, HourlyLastHour = 0, HourlyLastDay = 0;
	int64 HourlyLastPurchases = 0, HourlyLastExports = 0;
	bool ObserveHourly(const FCityEconomySnapshot& Economy, const FCityPopulationSnapshot& Population,
	    const FCityServiceSnapshot& Services, const FCityEnvironmentSnapshot& Environment, int64 Treasury,
	    int32 StepsPerDay, const FCityCrimeSnapshot* Crime = nullptr, const FCityHealthSnapshot* Health = nullptr,
	    const FCityWasteSnapshot* Waste = nullptr);
	bool ValidateHourly(int64 MaximumStep, int32 StepsPerDay, FString& OutError) const;
	bool Capture(const FCityEconomySnapshot& Economy, const FCityPopulationSnapshot& Population,
	             const FCityServiceSnapshot& Services, const FCityEnvironmentSnapshot& Environment, int64 Treasury,
	             const FCityCrimeSnapshot* Crime = nullptr, const FCityHealthSnapshot* Health = nullptr,
	             const FCityWasteSnapshot* Waste = nullptr);
	bool Validate(int64 SettledMonth, FString& OutError) const;
	TArray<FCityReportSample> Samples(ECityReportPeriod Period) const;
	TArray<FCityReportSample> Weekly() const;
	TArray<FCityReportSample> Monthly() const;
	static const TCHAR* PeriodName(ECityReportPeriod Period);
	static int32 PeriodDays(ECityReportPeriod Period);
	TArray<FCityReportSample> Annual() const;
	static const TCHAR* DisplayName(ECityReportMetric Metric);
	static const TCHAR* Name(ECityReportMetric Metric);
	static const TCHAR* Unit(ECityReportMetric Metric);
	static bool IsFlow(ECityReportMetric Metric);
private:
	static FCityReportSample Observe(const FCityEconomySnapshot& Economy, const FCityPopulationSnapshot& Population,
	    const FCityServiceSnapshot& Services, const FCityEnvironmentSnapshot& Environment, int64 Treasury,
	    const FCityCrimeSnapshot* Crime, const FCityHealthSnapshot* Health, const FCityWasteSnapshot* Waste);
	TArray<FCityReportSample> RollupDays(int32 Days) const;
};
