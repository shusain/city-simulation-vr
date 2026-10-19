#include "Simulation/CityScenarioSimulation.h"

#include "Simulation/CityEconomySimulation.h"

void FCityScenarioSimulation::Start(const int64 CurrentBilledMonthId)
{
	Snapshot.Phase = ECityScenarioPhase::Running;
	Snapshot.ProfitableMonthStreak = 0;
	Snapshot.LastObservedBilledMonthId = FMath::Max<int64>(0, CurrentBilledMonthId);
}

void FCityScenarioSimulation::Advance(const FCityEconomySnapshot& Economy)
{
	if (Snapshot.Phase != ECityScenarioPhase::Running ||
	    Economy.LastBilledMonthId <= Snapshot.LastObservedBilledMonthId)
	{
		return;
	}
	Snapshot.LastObservedBilledMonthId = Economy.LastBilledMonthId;
	const bool bGoalPopulation = Economy.Residents >= ResidentGoal;
	const bool bProfitable = Economy.LastIncome - Economy.LastUpkeep - Economy.Loan.LastPrincipalPaid -
	                            Economy.Loan.LastInterestPaid - Economy.Policies.Paid() > 0 && Economy.Loan.Arrears() == 0;
	Snapshot.ProfitableMonthStreak = bGoalPopulation && bProfitable ? Snapshot.ProfitableMonthStreak + 1 : 0;
	if (Snapshot.ProfitableMonthStreak >= ProfitableMonthGoal)
	{
		Snapshot.ProfitableMonthStreak = ProfitableMonthGoal;
		Snapshot.Phase = ECityScenarioPhase::Succeeded;
	}
}

bool FCityScenarioSimulation::Restore(const FCityScenarioSnapshot& InSnapshot, FString& OutError)
{
	if (InSnapshot.Phase > ECityScenarioPhase::Succeeded || InSnapshot.ProfitableMonthStreak < 0 ||
	    InSnapshot.ProfitableMonthStreak > ProfitableMonthGoal || InSnapshot.LastObservedBilledMonthId < 0 ||
	    (InSnapshot.Phase == ECityScenarioPhase::NotStarted &&
	     (InSnapshot.ProfitableMonthStreak != 0 || InSnapshot.LastObservedBilledMonthId != 0)) ||
	    (InSnapshot.Phase == ECityScenarioPhase::Running && InSnapshot.ProfitableMonthStreak >= ProfitableMonthGoal) ||
	    (InSnapshot.Phase == ECityScenarioPhase::Succeeded && InSnapshot.ProfitableMonthStreak != ProfitableMonthGoal))
	{
		OutError = TEXT("Invalid saved scenario progress");
		return false;
	}
	Snapshot = InSnapshot;
	return true;
}

FString FCityScenarioSimulation::Describe(const int32 CurrentResidents) const
{
	if (Snapshot.Phase == ECityScenarioPhase::NotStarted)
	{
		return TEXT("Scenario: select START");
	}
	if (Snapshot.Phase == ECityScenarioPhase::Succeeded)
	{
		return TEXT("SCENARIO COMPLETE: 100 residents + 3 profitable months");
	}
	return FString::Printf(TEXT("Objective: residents %d/100 | profitable months %d/3"),
	                       FMath::Min(CurrentResidents, ResidentGoal), Snapshot.ProfitableMonthStreak);
}
