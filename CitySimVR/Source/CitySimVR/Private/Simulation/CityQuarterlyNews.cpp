#include "Simulation/CityQuarterlyNews.h"

TArray<FCityQuarterlyEdition> FCityQuarterlyNews::Editions(const FCityReportHistory& History)
{
	TArray<FCityQuarterlyEdition> Result;
	const FCityReportSample* Previous = nullptr;
	for (const auto& Month : History.Monthly())
	{
		const int64 Quarter = (Month.MonthId - 1) / 3 + 1;
		if (Result.IsEmpty() || Result.Last().QuarterId != Quarter)
		{
			FCityQuarterlyEdition Edition;
			Edition.QuarterId = Quarter;
			Edition.FirstMonth = Month.MonthId;
			// A retained preceding settlement is needed to subtract lifetime movement.
			Edition.bHasMovement =
			    Month.bHasMigration &&
			    (Month.MonthId == 1 ||
			     (Previous && Previous->MonthId + 1 == Month.MonthId && Previous->bHasMigration));
			if (Edition.bHasMovement && Previous)
			{
				Edition.Relocated = -Previous->Get(ECityReportMetric::RelocatedResidents);
				Edition.Departed = -Previous->Get(ECityReportMetric::DepartedResidents);
			}
			Result.Add(Edition);
		}
		auto& Edition = Result.Last();
		++Edition.ObservedMonths;
		Edition.ObservedDays += Month.ObservedDays;
		Edition.LastMonth = Month.MonthId;
		Edition.Latest = Month;
		Edition.bHasSentiment &= Month.bHasSentiment;
		Edition.bHasMovement &= Month.bHasMigration;
		Edition.ResidentObservations += Month.Get(ECityReportMetric::Population);
		Edition.HappinessTotal += Month.Get(ECityReportMetric::HappinessTotal);
		Edition.TaxIncome += Month.Get(ECityReportMetric::Income);
		Edition.Upkeep += Month.Get(ECityReportMetric::Upkeep);
		Edition.bHasPolicies &= Month.bHasPolicies;
		Edition.ReserveAssistance+=Month.Get(ECityReportMetric::ReserveAssistancePaid);
		Edition.Bursaries+=Month.Get(ECityReportMetric::SchoolBursariesPaid);
		Edition.PolicyAdministration+=Month.Get(ECityReportMetric::PolicyAdministrationPaid);
		Edition.UnfundedPrograms+=Month.Get(ECityReportMetric::UnfundedPolicies);
		Edition.bHasLoans &= Month.bHasLoans;
		Edition.Borrowed += Month.Get(ECityReportMetric::LoanBorrowed);
		Edition.PrincipalPaid += Month.Get(ECityReportMetric::LoanPrincipalPaid);
		Edition.InterestPaid += Month.Get(ECityReportMetric::LoanInterestPaid);
		Previous = &Month;
	}
	for (auto& Edition : Result)
		if (Edition.bHasMovement)
		{
			Edition.Relocated += Edition.Latest.Get(ECityReportMetric::RelocatedResidents);
			Edition.Departed += Edition.Latest.Get(ECityReportMetric::DepartedResidents);
		}
	return Result;
}

FString FCityQuarterlyNews::Body(const FCityReportHistory& History, int32 Page)
{
	const auto Editions = FCityQuarterlyNews::Editions(History);
	if (Editions.IsEmpty())
		return TEXT("No settled months yet.\nCity news begins at the next daily settlement.\nEach quarter "
		            "covers three city months.");
	const int32 Index = Editions.Num() - 1 - FMath::Clamp(Page, 0, Editions.Num() - 1);
	const auto& E = Editions[Index];
	const auto& L = E.Latest;
	FString Text = FString::Printf(TEXT("Year %lld Q%lld | %s | %d/3 months observed\n"),
	                               (E.QuarterId - 1) / 4 + 1, (E.QuarterId - 1) % 4 + 1,
	                               E.IsComplete() ? TEXT("COMPLETE") : TEXT("PARTIAL"), E.ObservedMonths);
	if (!E.bHasSentiment)
		Text += TEXT("Mood: not recorded for every observed month\n");
	else if (!E.ResidentObservations)
		Text += TEXT("Mood: no residents observed\n");
	else
	{
		FString Trend = TEXT("prior full-quarter comparison unavailable");
		if (Index > 0 && E.IsComplete() && Editions[Index - 1].IsComplete() &&
		    E.QuarterId == Editions[Index - 1].QuarterId + 1 && Editions[Index - 1].bHasSentiment &&
		    Editions[Index - 1].ResidentObservations)
			Trend = FString::Printf(TEXT("%+lld vs prior quarter"),
			                        E.AverageHappiness() - Editions[Index - 1].AverageHappiness());
		Text += FString::Printf(TEXT("Mean mood %lld/100 | %s\n"), E.AverageHappiness(), *Trend);
	}
	if (L.bHasSentiment && L.Get(ECityReportMetric::Population))
	{
		Text += FString::Printf(TEXT("Latest: %lld/%lld discontented; %lld searching\n"),
		                        L.Get(ECityReportMetric::Discontented), L.Get(ECityReportMetric::Population),
		                        L.Get(ECityReportMetric::SearchingResidents));
		static const TCHAR* Labels[] = {TEXT("housing"), TEXT("utilities"), TEXT("shopping"),
		                                TEXT("savings"), TEXT("pollution"), TEXT("safety"),
		                                TEXT("health"),  TEXT("waste"),     TEXT("taxes")};
		static const TCHAR* Remedies[] = {
		    TEXT("inspect homes"),     TEXT("check water/power"),  TEXT("check shops/pay"),
		    TEXT("check wages/costs"), TEXT("review pollution"),   TEXT("check police/fire"),
		    TEXT("check hospitals"),   TEXT("check waste depots"), TEXT("review taxes")};
		TArray<int32> Ranked;
		for (int32 I = 0; I < UE_ARRAY_COUNT(Labels); ++I)
			if (L.Get(static_cast<ECityReportMetric>(static_cast<int32>(ECityReportMetric::ConcernHousing) +
			                                         I)))
				Ranked.Add(I);
		Ranked.StableSort([&L](int32 A, int32 B) {
			return L.Get(static_cast<ECityReportMetric>(
			           static_cast<int32>(ECityReportMetric::ConcernHousing) + A)) >
			       L.Get(static_cast<ECityReportMetric>(
			           static_cast<int32>(ECityReportMetric::ConcernHousing) + B));
		});
		if (Ranked.IsEmpty())
			Text += TEXT("Latest concerns: none recorded\n");
		else
			for (int32 N = 0; N < FMath::Min(2, Ranked.Num()); ++N)
			{
				const int32 I = Ranked[N];
				Text += FString::Printf(TEXT("%s: %lld people | %s\n"), Labels[I],
				                        L.Get(static_cast<ECityReportMetric>(
				                            static_cast<int32>(ECityReportMetric::ConcernHousing) + I)),
				                        Remedies[I]);
			}
	}
	else
		Text += L.bHasSentiment ? TEXT("Latest concerns: no residents\n")
		                        : TEXT("Latest concerns: not recorded\n");
	Text += E.bHasMovement ? FString::Printf(TEXT("Housing: %lld moved; %lld left in observed months\n"),
	                                         E.Relocated, E.Departed)
	                       : TEXT("Housing totals: preceding record unavailable\n");
	Text += FString::Printf(
	    TEXT("Tax $%lld | upkeep $%lld (observed totals)\nCash $%lld | residents %lld | jobs %lld"),
	    E.TaxIncome, E.Upkeep, L.Get(ECityReportMetric::Treasury), L.Get(ECityReportMetric::Population),
	    L.Get(ECityReportMetric::FilledJobs));
	return Text;
}

FString FCityQuarterlyNews::Footer(const FCityReportHistory& History, int32 Page)
{
	const auto Editions = FCityQuarterlyNews::Editions(History);
	if (Editions.IsEmpty())
		return TEXT("Recorded facts only; no backdated news.");
	Page = FMath::Clamp(Page, 0, Editions.Num() - 1);
	const auto& E = Editions[Editions.Num() - 1 - Page];
	return FString::Printf(TEXT("%d/%d | months %lld-%lld | concerns overlap; latest snapshot"), Page + 1,
	                       Editions.Num(), E.FirstMonth, E.LastMonth);
}
