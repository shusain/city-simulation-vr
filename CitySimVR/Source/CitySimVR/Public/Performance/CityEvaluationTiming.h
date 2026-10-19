#pragma once

#include "Performance/CityFrameTiming.h"

// Optional per-evaluation observers. No sampling or allocation in normal play.
struct FCityServiceEvaluationTiming
{
	FCityFrameTiming Providers, WaterNetwork, Consumers, PublicStaff, Education, Parks;
	uint64 PreparedWaterQueries = 0, ReferenceWaterQueries = 0;
	uint64 PreparedEducationQueries = 0, ReferenceEducationCitizens = 0;
};

struct FCityForecastTiming
{
	FCityFrameTiming Copy, Reconcile, Settlement, Aggregate;
	FCityFrameTiming PublicPayroll, PrivatePayroll, Purchases, HouseholdBalances;
	FCityFrameTiming ReconcileLedger, ReconcileHiring, ReconcileHousing;
	uint64 PreparedQueries = 0, ReferenceQueries = 0;
};

struct FCityEconomyEvaluationTiming
{
	FCityFrameTiming Services, Billing;
	FCityForecastTiming Forecast;
};

struct FCityNeedsEvaluationTiming
{
	FCityFrameTiming Market, Zones, Desirability;
	FCityForecastTiming Forecast;
};
