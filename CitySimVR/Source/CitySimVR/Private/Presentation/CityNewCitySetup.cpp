#include "Presentation/CityNewCitySetup.h"

FCityNewCitySetup FCityNewCitySetup::Proposal(const ECityDifficultyDraft Difficulty, const FCityEconomyTuning& Baseline)
{
	FCityNewCitySetup Result;
	Result.Difficulty = Difficulty;
	Result.Tuning = FCityIdentity::Apply(Difficulty, Baseline);
	Result.StartingFunds = FCityIdentity::StartingFunds(Difficulty);
	return Result;
}

FString FCityNewCitySetup::Summary() const
{
	return FString::Printf(
	    TEXT("%s | Starting funds $%lld\n")
	        TEXT("Crime: %d residents / %d days grace\nFire: %d residents / %d days grace\n") TEXT(
	            "Checks: crime %d / fire %d game minutes (chances)\nRepairs: %d%% / %d game minutes\nReview before creating; current city unchanged"),
	    FCityIdentity::DifficultyName(Difficulty), StartingFunds, Tuning.CrimePopulationThreshold,
	    Tuning.CrimeGraceMonths, Tuning.FirePopulationThreshold, Tuning.FireGraceMonths, Tuning.CrimeCheckIntervalSteps * 1440 / FMath::Max(1, Tuning.FixedStepsPerMonth),
	    Tuning.FireCheckIntervalSteps * 1440 / FMath::Max(1, Tuning.FixedStepsPerMonth), Tuning.FireRepairCostPercent, Tuning.FireRepairSteps * 1440 / FMath::Max(1, Tuning.FixedStepsPerMonth));
}
