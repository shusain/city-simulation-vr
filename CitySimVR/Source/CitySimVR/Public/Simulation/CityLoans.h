#pragma once
#include "CoreMinimal.h"

class FCityPlacementModel;

// Integer dollars; one external civic lender, equal principal over 24 months.
struct CITYSIMVR_API FCityLoanState
{
	static constexpr int32 TermMonths = 24;
	int64 OriginalPrincipal = 0;
	int64 Principal = 0;
	int64 InterestDue = 0;
	int64 StartedMonth = 0;
	int32 MonthsRemaining = 0;
	int64 PendingBorrowed = 0;
	int64 PendingPrincipalPaid = 0;
	int64 PendingInterestPaid = 0;
	int64 LastBorrowed = 0;
	int64 LastPrincipalPaid = 0;
	int64 LastInterestPaid = 0;
	int64 LastInterestAccrued = 0;

	static bool IsOffer(int64 Amount);
	static int64 MonthlyInterest(int64 Balance)
	{
		return (Balance + 199) / 200;
	}
	int64 PrincipalDue(int32 Remaining) const;
	int64 Arrears() const
	{
		return InterestDue + PrincipalDue(MonthsRemaining);
	}
	int64 NextPayment() const;
	int64 Payoff() const
	{
		return Principal + InterestDue;
	}
	bool Borrow(FCityPlacementModel& City, int64 Amount, int64 SettledMonth, FString& Error);
	bool Repay(FCityPlacementModel& City, FString& Error);
	void SettleMonth(FCityPlacementModel& City);
	bool Validate(int64 SettledMonth, FString& Error) const;
	static FString Terms(int64 Amount);
	FString Summary(int64 Cash, int64 OperatingNet) const;
};
