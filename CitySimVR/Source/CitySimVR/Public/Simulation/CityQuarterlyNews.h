#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityReportHistory.h"

// Read-only editions derived from the existing saved daily settlements.
// No generated story, second calendar, speculative forecast or duplicate ledger.
struct CITYSIMVR_API FCityQuarterlyEdition
{
	int64 QuarterId = 0, FirstMonth = 0, LastMonth = 0;
	int32 ObservedMonths = 0;
	int32 ObservedDays = 0;
	bool bHasSentiment = true, bHasMovement = false;
	bool bHasPolicies = true;
	int64 ReserveAssistance = 0, Bursaries = 0, PolicyAdministration = 0, UnfundedPrograms = 0;
	bool bHasLoans = true;
	int64 Borrowed = 0, PrincipalPaid = 0, InterestPaid = 0;
	int64 ResidentObservations = 0, HappinessTotal = 0;
	int64 TaxIncome = 0, Upkeep = 0, Relocated = 0, Departed = 0;
	FCityReportSample Latest;
	bool IsComplete() const
	{
		return ObservedMonths == 3 && ObservedDays == 3 * FCityCalendar::DaysPerMonth;
	}
	int64 AverageHappiness() const
	{
		return ResidentObservations ? HappinessTotal / ResidentObservations : 0;
	}
};

class CITYSIMVR_API FCityQuarterlyNews
{
  public:
	static TArray<FCityQuarterlyEdition> Editions(const FCityReportHistory& History);
	// Page zero is newest. Partial quarters retain observed totals, never annualized.
	static FString Body(const FCityReportHistory& History, int32 Page);
	static FString Footer(const FCityReportHistory& History, int32 Page);
};

struct CITYSIMVR_API FCityNewsArticle
{
	FString Section, Headline, Body;
};
struct CITYSIMVR_API FCityNewspaperEdition
{
	FString Masthead = TEXT("The City Chronicle");
	FString Dateline, Headline, Lead, Footer;
	TArray<FCityNewsArticle> Articles;
};
// Editorial presentation of recorded facts; never writes to the simulation.
CITYSIMVR_API FCityNewspaperEdition CityNewspaperEdition(const FCityReportHistory& History, int32 Page);
