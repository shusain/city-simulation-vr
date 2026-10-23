#include "Simulation/CityQuarterlyNews.h"

FCityNewspaperEdition CityNewspaperEdition(const FCityReportHistory& History, int32 Page)
{
	FCityNewspaperEdition Paper;
	const auto Editions = FCityQuarterlyNews::Editions(History);
	if (Editions.IsEmpty())
	{
		Paper.Dateline = TEXT("INAUGURAL EDITION  |  AWAITING THE FIRST SETTLEMENT");
		Paper.Headline = TEXT("A city begins its story");
		Paper.Lead = TEXT("The Chronicle's first city report will appear after the next daily settlement. "
		                  "Each edition follows three city months, with partial quarters clearly marked.");
		Paper.Footer = TEXT("Local affairs, citizen wellbeing and the public purse.");
		return Paper;
	}
	Page = FMath::Clamp(Page, 0, Editions.Num() - 1);
	const int32 Index = Editions.Num() - 1 - Page;
	const auto& E = Editions[Index];
	const auto& L = E.Latest;
	const int64 People = L.Get(ECityReportMetric::Population);
	Paper.Dateline = FString::Printf(TEXT("YEAR %lld  |  QUARTER %lld  |  %s  |  %d OF 3 MONTHS OBSERVED | %d/%d DAYS"),
	                                 (E.QuarterId - 1) / 4 + 1, (E.QuarterId - 1) % 4 + 1,
	                                 E.IsComplete() ? TEXT("COMPLETE EDITION") : TEXT("PARTIAL EDITION"),
	                                 E.ObservedMonths, E.ObservedDays, 3 * FCityCalendar::DaysPerMonth);
	if (E.bHasMovement && E.Departed > 0)
		Paper.Headline = FString::Printf(TEXT("%lld residents leave the city"), E.Departed);
	else if (!E.bHasSentiment)
		Paper.Headline = TEXT("A quarter with gaps in the mood record");
	else if (!E.ResidentObservations)
		Paper.Headline = TEXT("An empty census this quarter");
	else if (L.Get(ECityReportMetric::Discontented) * 2 >= People && People)
		Paper.Headline = TEXT("Citizen mood under strain");
	else
		Paper.Headline = TEXT("The quarter in city life");
	if (!E.bHasSentiment)
		Paper.Lead = TEXT("Some observed days have no recorded citizen sentiment. This edition reports the "
		                  "available facts without filling in the missing mood history.");
	else if (!E.ResidentObservations)
		Paper.Lead = TEXT("No residents were present in the observed daily snapshots. The housing and "
		                  "treasury reports below describe what the city recorded.");
	else
	{
		Paper.Lead = FString::Printf(
		    TEXT("Recorded mood averaged %lld/100 across the observed daily resident samples. At month %lld's close, %lld of "
		         "%lld residents were discontented and %lld were searching for another home."),
		    E.AverageHappiness(), E.LastMonth, L.Get(ECityReportMetric::Discontented), People,
		    L.Get(ECityReportMetric::SearchingResidents));
		if (Index > 0 && E.IsComplete() && Editions[Index - 1].IsComplete() &&
		    Editions[Index - 1].bHasSentiment && Editions[Index - 1].ResidentObservations)
		{
			const int64 Change = E.AverageHappiness() - Editions[Index - 1].AverageHappiness();
			Paper.Lead += FString::Printf(
			    TEXT(" Mean mood changed by %+lld points from the previous complete quarter."), Change);
		}
	}
	static const TCHAR* Headlines[] = {
	    TEXT("Housing pressures come into focus"), TEXT("Utility access draws concern"),
	    TEXT("Shopping tops the everyday agenda"), TEXT("Household savings feel the squeeze"),
	    TEXT("Pollution weighs on local life"),    TEXT("Safety remains a citizen concern"),
	    TEXT("Health needs call for attention"),   TEXT("Waste troubles residents"),
	    TEXT("Tax burden draws concern")};
	static const TCHAR* Descriptions[] = {TEXT("housing access"),
	                                      TEXT("water or power access"),
	                                      TEXT("unmet purchases"),
	                                      TEXT("cash reserves"),
	                                      TEXT("pollution"),
	                                      TEXT("safety"),
	                                      TEXT("health"),
	                                      TEXT("waste"),
	                                      TEXT("taxes")};
	static const TCHAR* Remedies[] = {TEXT("Inspect homes and available housing."),
	                                  TEXT("Review operating water and power supplies."),
	                                  TEXT("Check shop access, stock and purchasing power."),
	                                  TEXT("Review wages, purchases and recurring costs."),
	                                  TEXT("Inspect polluted homes and nearby industry."),
	                                  TEXT("Review police and fire coverage."),
	                                  TEXT("Check hospital access, staff and treatment places."),
	                                  TEXT("Review waste collection and depot capacity."),
	                                  TEXT("Review tax rates alongside city funding needs.")};
	TArray<int32> Ranked;
	if (L.bHasSentiment)
		for (int32 I = 0; I < 9; ++I)
			if (L.Get(static_cast<ECityReportMetric>(static_cast<int32>(ECityReportMetric::ConcernHousing) +
			                                         I)))
				Ranked.Add(I);
	Ranked.StableSort([&L](int32 A, int32 B) {
		return L.Get(static_cast<ECityReportMetric>(static_cast<int32>(ECityReportMetric::ConcernHousing) +
		                                            A)) >
		       L.Get(
		           static_cast<ECityReportMetric>(static_cast<int32>(ECityReportMetric::ConcernHousing) + B));
	});
	for (int32 N = 0; N < FMath::Min(2, Ranked.Num()); ++N)
	{
		const int32 I = Ranked[N];
		Paper.Articles.Add(
		    {TEXT("CITIZEN AFFAIRS"), Headlines[I],
		     FString::Printf(TEXT("At month %lld's close, %lld residents recorded concerns about %s. %s"),
		                     E.LastMonth,
		                     L.Get(static_cast<ECityReportMetric>(
		                         static_cast<int32>(ECityReportMetric::ConcernHousing) + I)),
		                     Descriptions[I], Remedies[I])});
	}
	if (Ranked.IsEmpty())
		Paper.Articles.Add({TEXT("CITIZEN AFFAIRS"), TEXT("The latest wellbeing report"),
		                    !L.bHasSentiment
		                        ? TEXT("No citizen concern snapshot was recorded for the closing month. "
		                               "Current home inspection remains available.")
		                    : !People ? TEXT("The closing snapshot contains no residents. Empty homes retain "
		                                     "their buildings and can be inspected.")
		                              : TEXT("The closing snapshot records no happiness concerns. Inspect "
		                                     "current homes to follow their wellbeing.")});
	if (Paper.Articles.Num() < 2)
		Paper.Articles.Add(
		    {TEXT("LOCAL ECONOMY"), TEXT("Work and population in brief"),
		     FString::Printf(
		         TEXT("The closing-month census records %lld residents and %lld filled jobs. Employment is "
		              "one part of city life; the mood report records wellbeing separately."),
		         People, L.Get(ECityReportMetric::FilledJobs))});
	Paper.Articles.Add(
	    {TEXT("HOUSING DESK"),
	     E.bHasMovement && E.Departed ? TEXT("Departures reshape the quarter")
	                                  : TEXT("Households on the move"),
	     E.bHasMovement
	         ? FString::Printf(
	               TEXT("%lld residents relocated within the city and %lld left during the observed months. "
	                    "Housing Events retains the recorded decisions and reasons."),
	               E.Relocated, E.Departed)
	         : TEXT("The preceding migration record is unavailable, so this edition cannot report period "
	                "movement totals. Housing Events shows retained decisions.")});
    FString Finance=FString::Printf(TEXT("Recorded taxes $%lld; upkeep due $%lld; closing cash $%lld."),E.TaxIncome,E.Upkeep,L.Get(ECityReportMetric::Treasury));
    Finance+=E.bHasLoans ? FString::Printf(TEXT(" Civic lender: borrowed $%lld; paid $%lld principal + $%lld interest; closing principal $%lld, dues $%lld."),E.Borrowed,E.PrincipalPaid,E.InterestPaid,L.Get(ECityReportMetric::LoanPrincipal),L.Get(ECityReportMetric::LoanArrears)) : TEXT(" Loan history unavailable.");
    Finance+=E.bHasPolicies ? FString::Printf(TEXT(" Policies: reserve aid $%lld, bursaries $%lld, administration $%lld; %lld unfunded program-months."),E.ReserveAssistance,E.Bursaries,E.PolicyAdministration,E.UnfundedPrograms) : TEXT(" Policy history unavailable.");
    Paper.Articles.Add({TEXT("PUBLIC FINANCES"),E.bHasPolicies && E.UnfundedPrograms>0 ? TEXT("Policy funding falls short") : TEXT("Inside the public purse"),Finance});
	Paper.Footer = FString::Printf(TEXT("Edition %d of %d  |  Recorded months %lld-%lld  |  Citizen concerns "
	                                    "are closing-month snapshots and may overlap."),
	                               Page + 1, Editions.Num(), E.FirstMonth, E.LastMonth);
	return Paper;
}
