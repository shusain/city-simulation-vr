#include "Simulation/CityLoans.h"
#include "Commands/CityPlacementModel.h"

bool FCityLoanState::IsOffer(int64 Amount)
{
	return Amount == 5000 || Amount == 10000 || Amount == 20000;
}

int64 FCityLoanState::PrincipalDue(int32 Remaining) const
{
	return FMath::Max<int64>(0, Principal - OriginalPrincipal * Remaining / TermMonths);
}

int64 FCityLoanState::NextPayment() const
{
	return PrincipalDue(FMath::Max(0, MonthsRemaining - 1)) + InterestDue + MonthlyInterest(Principal);
}

bool FCityLoanState::Borrow(FCityPlacementModel& City, int64 Amount, int64 SettledMonth, FString& Error)
{
	if (!IsOffer(Amount) || SettledMonth < 0 || Payoff() || City.GetFunds() > MAX_int64 - Amount ||
	    PendingBorrowed > MAX_int64 / 121 - Amount || PendingPrincipalPaid > MAX_int64 / 121 - Amount)
	{
		Error = TEXT("Borrowing unavailable: repay the current loan first.");
		return false;
	}
	// External civic lender -> treasury. Borrowing is never tax revenue.
	City.ApplyMonthlyEconomy(Amount, 0);
	OriginalPrincipal = Principal = Amount;
	StartedMonth = SettledMonth;
	MonthsRemaining = TermMonths;
	PendingBorrowed += Amount;
	return true;
}

bool FCityLoanState::Repay(FCityPlacementModel& City, FString& Error)
{
	if (!Payoff() || PendingInterestPaid > MAX_int64 / 121 - InterestDue || !City.TrySpendFunds(Payoff()))
	{
		Error = TEXT("Full repayment needs enough treasury cash for principal and accrued interest.");
		return false;
	}
	// Treasury -> external civic lender; no fee or future interest.
	PendingPrincipalPaid += Principal;
	PendingInterestPaid += InterestDue;
	OriginalPrincipal = Principal = InterestDue = StartedMonth = 0;
	MonthsRemaining = 0;
	return true;
}

void FCityLoanState::SettleMonth(FCityPlacementModel& City)
{
	LastBorrowed = PendingBorrowed;
	LastPrincipalPaid = PendingPrincipalPaid;
	LastInterestPaid = PendingInterestPaid;
	PendingBorrowed = PendingPrincipalPaid = PendingInterestPaid = 0;
	LastInterestAccrued = MonthlyInterest(Principal);
	InterestDue += LastInterestAccrued;
	MonthsRemaining = FMath::Max(0, MonthsRemaining - 1);
	const int64 InterestPaid = FMath::Min(City.GetFunds(), InterestDue);
	City.TrySpendFunds(InterestPaid);
	InterestDue -= InterestPaid;
	LastInterestPaid += InterestPaid;
	const int64 PrincipalPaid = FMath::Min(City.GetFunds(), PrincipalDue(MonthsRemaining));
	City.TrySpendFunds(PrincipalPaid);
	Principal -= PrincipalPaid;
	LastPrincipalPaid += PrincipalPaid;
	if (!Payoff())
	{
		OriginalPrincipal = StartedMonth = 0;
		MonthsRemaining = 0;
	}
}

bool FCityLoanState::Validate(int64 SettledMonth, FString& Error) const
{
	const int64 Flows[] = {PendingBorrowed,   PendingPrincipalPaid, PendingInterestPaid, LastBorrowed,
	                       LastPrincipalPaid, LastInterestPaid,     LastInterestAccrued};
	for (int64 Flow : Flows)
		if (Flow < 0 || Flow > MAX_int64 / 121)
		{
			Error = TEXT("Invalid city loan transfer.");
			return false;
		}
	const bool Active = Principal > 0 || InterestDue > 0;
	if (Principal < 0 || InterestDue < 0 || InterestDue > MAX_int64 / 121 - 20000 || StartedMonth < 0 ||
	    StartedMonth > SettledMonth || MonthsRemaining < 0 || MonthsRemaining > TermMonths ||
	    LastInterestAccrued > 100 ||
	    (Active && (!IsOffer(OriginalPrincipal) || Principal > OriginalPrincipal ||
	                MonthsRemaining != FMath::Max<int64>(0, TermMonths - (SettledMonth - StartedMonth)) ||
	                (InterestDue + 99) / 100 > SettledMonth - StartedMonth)) ||
	    (!Active && (OriginalPrincipal || StartedMonth || MonthsRemaining)))
	{
		Error = TEXT("Invalid city loan balance, term or settlement date.");
		return false;
	}
	return true;
}

FString FCityLoanState::Terms(int64 Amount)
{
	const int64 FirstPrincipal = Amount - Amount * (TermMonths - 1) / TermMonths;
	int64 Interest = 0;
	for (int32 Remaining = TermMonths; Remaining > 0; --Remaining)
		Interest += MonthlyInterest(Amount * Remaining / TermMonths);
	return FString::Printf(TEXT("External civic lender -> city: $%lld\n24 daily principal installments\n"
	                            "Interest: 0.5%% / day on remaining principal\n"
	                            "First payment $%lld; then declining\n"
	                            "On-time total $%lld (interest $%lld)\n"
	                            "Interest rounds up to whole dollars.\n"
	                            "Operating bills first; missed dues stay owed.\n"
	                            "Interest continues on unpaid principal.\n"
	                            "One loan at a time. Early payoff has no fee."),
	                       Amount, FirstPrincipal + MonthlyInterest(Amount), Amount + Interest, Interest);
}

FString FCityLoanState::Summary(int64 Cash, int64 OperatingNet) const
{
	return FString::Printf(TEXT("City treasury: $%lld\nExternal civic lender balance\n"
	                            "Principal $%lld | interest owed $%lld\n"
	                            "Scheduled days left: %d / 24\n"
	                            "Unpaid dues now: $%lld\nNext settlement due: $%lld\n"
	                            "Operating forecast $%lld; after debt $%lld\n"
	                            "Last settled: borrowed $%lld\nPaid principal $%lld | interest $%lld\n"
	                            "Full payoff now: $%lld\nBorrowing is debt, not tax income."),
	                       Cash, Principal, InterestDue, MonthsRemaining, Arrears(), NextPayment(),
	                       OperatingNet, OperatingNet - NextPayment(), LastBorrowed, LastPrincipalPaid,
	                       LastInterestPaid, Payoff());
}
