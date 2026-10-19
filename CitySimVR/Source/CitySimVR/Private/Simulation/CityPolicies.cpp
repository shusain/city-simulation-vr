#include "Simulation/CityPolicies.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
namespace
{
int64 Grant(ECityPolicy Policy, const FCityCitizenRecord& C, const FCityServiceSnapshot& S,
            const FCityEconomyTuning& T, bool EducationAvailable)
{
	if (C.IsDetained())
		return 0;
	if (Policy == ECityPolicy::ReserveAssistance)
	{
		const int64 Buffer = static_cast<int64>(FMath::Max(0, C.ConsumptionNeed)) *
		                     FMath::Max(0, T.HouseholdIncomePerGoodsPurchase);
		return FMath::Clamp<int64>(Buffer - C.AvailableMoney, 0, 5);
	}
	const bool Served = C.Education >= ECityEducationLevel::Secondary
	                        ? S.HigherEducationCitizenIds.Contains(C.Id)
	                        : S.EducationCitizenIds.Contains(C.Id);
	return EducationAvailable && !C.bOnMedicalLeave && C.Education < ECityEducationLevel::Higher && Served &&
	               C.AvailableMoney <= MAX_int64 / 4 - 2
	           ? 2
	           : 0;
}
} // namespace
const TCHAR* FCityPolicyState::Name(ECityPolicy Policy)
{
	return Policy == ECityPolicy::ReserveAssistance ? TEXT("Reserve assistance") : TEXT("School bursaries");
}
FCityPolicyQuote FCityPolicyState::Quote(ECityPolicy Policy, const FCityPopulationSnapshot& P,
                                         const FCityServiceSnapshot& S, const FCityEconomyTuning& T,
                                         bool EducationAvailable)
{
	FCityPolicyQuote Q;
	if (!IsValid(Policy))
		return Q;
	for (const auto& C : P.Citizens)
	{
		const int64 Amount = Grant(Policy, C, S, T, EducationAvailable);
		Q.Eligible += Amount > 0;
		Q.Grants += Amount;
	}
	return Q;
}
int64 FCityPolicyState::Forecast(const FCityPopulationSnapshot& P, const FCityServiceSnapshot& S,
                                 const FCityEconomyTuning& T, bool EducationAvailable) const
{
	int64 Total = 0;
	for (int32 I = 0; I < 2; ++I)
		if (Programs[I].bEnabled)
			Total += Quote(static_cast<ECityPolicy>(I), P, S, T, EducationAvailable).Total();
	return Total;
}
bool FCityPolicyState::SetEnabled(ECityPolicy Policy, bool Enabled)
{
	if (!IsValid(Policy))
		return false;
	Programs[static_cast<int32>(Policy)].bEnabled = Enabled;
	return true;
}
bool FCityPolicyState::SettleMonth(int64 Month, FCityPlacementModel& City, FCityPopulationSnapshot& P,
                                   const FCityServiceSnapshot& S, const FCityEconomyTuning& T,
                                   bool EducationAvailable)
{
	if (Month <= LastSettledMonth || Month <= 0)
		return false;
	for (auto& C : P.Citizens)
	{
		C.LastPolicyGrant = 0;
		C.LastPolicyGrantMonth = Month;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		auto& R = Programs[I];
		const bool Enabled = R.bEnabled;
		R = FCityPolicyRecord();
		R.bEnabled = R.bLastEnabled = Enabled;
		if (!Enabled)
			continue;
		const auto Policy = static_cast<ECityPolicy>(I);
		const auto Q = Quote(Policy, P, S, T, EducationAvailable);
		R.Eligible = Q.Eligible;
		R.GrantsDue = Q.Grants;
		if (!Q.Eligible || !City.TrySpendFunds(Q.Total()))
			continue;
		// Treasury -> external civic administration provider ($25 per active program).
		R.AdministrationPaid = Q.Administration();
		// Treasury -> eligible resident wallets. No tax/wage/goods creation.
		for (auto& C : P.Citizens)
		{
			const int64 Amount = Grant(Policy, C, S, T, EducationAvailable);
			C.AvailableMoney += Amount;
			C.LastPolicyGrant += Amount;
			R.Recipients += Amount > 0;
			R.GrantsPaid += Amount;
		}
	}
	LastSettledMonth = Month;
	return true;
}
int64 FCityPolicyState::Paid() const
{
	return Programs[0].GrantsPaid + Programs[0].AdministrationPaid + Programs[1].GrantsPaid +
	       Programs[1].AdministrationPaid;
}
int64 FCityPolicyState::Due() const
{
	return Programs[0].Due() + Programs[1].Due();
}
int64 FCityPolicyState::Unfunded() const
{
	return (Programs[0].Due() > 0 && Programs[0].AdministrationPaid == 0) +
	       (Programs[1].Due() > 0 && Programs[1].AdministrationPaid == 0);
}
bool FCityPolicyState::Validate(int64 SettledMonth, FString& Error) const
{
	if (LastSettledMonth < 0 || LastSettledMonth > SettledMonth)
	{
		Error = TEXT("Invalid policy settlement clock");
		return false;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const auto& R = Programs[I];
		const int64 MaxGrant = I == 0 ? 5 : 2;
		if (R.Eligible < 0 || R.Eligible > MAX_int32 || R.Recipients < 0 || R.Recipients > R.Eligible ||
		    R.GrantsDue < 0 || R.GrantsDue > R.Eligible * MaxGrant ||
		    (R.Eligible > 0 && R.GrantsDue < R.Eligible) || R.GrantsPaid < 0 ||
		    (R.GrantsPaid != 0 && R.GrantsPaid != R.GrantsDue) ||
		    (R.AdministrationPaid != 0 && R.AdministrationPaid != 25) ||
		    (R.AdministrationPaid == 25 && (!R.bLastEnabled || R.Eligible == 0 ||
		                                    R.Recipients != R.Eligible || R.GrantsPaid != R.GrantsDue)) ||
		    (R.AdministrationPaid == 0 && (R.GrantsPaid || R.Recipients)) ||
		    ((!R.bLastEnabled || LastSettledMonth == 0) &&
		     (R.Eligible || R.GrantsDue || R.AdministrationPaid)))
		{
			Error = TEXT("Invalid policy cost or recipient ledger");
			return false;
		}
	}
	return true;
}
FString FCityPolicyState::Summary(int64 Cash, int64 Cost) const
{
	FString Text = FString::Printf(TEXT("City cash $%lld | forecast policies $%lld / month\n"), Cash, Cost);
	if (!LastSettledMonth)
		Text += TEXT("No policy settlement recorded yet.\n");
	for (int32 I = 0; I < 2; ++I)
	{
		const auto& R = Programs[I];
		Text += FString::Printf(TEXT("%s: %s\n"), Name(static_cast<ECityPolicy>(I)),
		                        R.bEnabled ? TEXT("ENABLED") : TEXT("OFF"));
		if (LastSettledMonth)
			Text += FString::Printf(TEXT("Last day %lld/%lld paid; grants $%lld + admin $%lld\n"),
			                        R.Recipients, R.Eligible, R.GrantsPaid, R.AdministrationPaid);
		if (R.Due() > 0 && !R.AdministrationPaid)
			Text += TEXT("WARNING: unfunded last day; no benefit paid.\n");
	}
	return Text + TEXT("Bills and lender paid before optional programs.\nReserve assistance funded first, "
	                   "then bursaries.\nNo eligible recipients: no administration charge.\nChanges take "
	                   "effect at the next settlement.");
}
FString FCityPolicyState::Review(ECityPolicy Policy, bool Enable, const FCityPolicyQuote& Q, int64 Cash,
                                 int64 Net) const
{
	FString Text = FString::Printf(TEXT("%s %s\n"), Enable ? TEXT("ENABLE") : TEXT("DISABLE"), Name(Policy));
	Text += Policy == ECityPolicy::ReserveAssistance
	            ? TEXT("Treasury -> residents short of a basic purchase buffer.\nTop up by at most $5 per "
	                   "eligible resident / month.\n")
	            : TEXT("Treasury -> residents with an operating school place.\n$2 per served learner / "
	                   "month; no instant qualification.\nDetention/medical leave exclude bursaries.\n");
	return Text +
	       FString::Printf(TEXT("Eligible now %lld | grants $%lld + admin $%lld\nForecast cost $%lld / month "
	                            "| city cash $%lld\nBudget after change $%lld / month\nAdministration -> "
	                            "external civic provider.\nFull program funded or no charge/benefit.\nNo "
	                            "arrears for skipped optional programs.\nEnable/disable moves no money now."),
	                       Q.Eligible, Q.Grants, Q.Administration(), Enable ? Q.Total() : 0, Cash, Net);
}
