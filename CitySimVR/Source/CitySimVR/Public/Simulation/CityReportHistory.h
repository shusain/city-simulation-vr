#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityCalendar.h"
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
	bool Capture(const FCityEconomySnapshot& Economy, const FCityPopulationSnapshot& Population,
	             const FCityServiceSnapshot& Services, const FCityEnvironmentSnapshot& Environment, int64 Treasury,
	             const FCityCrimeSnapshot* Crime = nullptr, const FCityHealthSnapshot* Health = nullptr,
	             const FCityWasteSnapshot* Waste = nullptr);
	bool Validate(int64 SettledMonth, FString& OutError) const;
	TArray<FCityReportSample> Monthly() const;
	TArray<FCityReportSample> Annual() const;
	static const TCHAR* DisplayName(ECityReportMetric Metric);
	static const TCHAR* Name(ECityReportMetric Metric);
	static const TCHAR* Unit(ECityReportMetric Metric);
	static bool IsFlow(ECityReportMetric Metric);
};
