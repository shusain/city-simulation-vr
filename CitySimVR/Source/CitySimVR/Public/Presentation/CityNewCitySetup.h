#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityIdentity.h"

using ECityDifficultyDraft = ECityDifficulty;

// Presentation draft consumes the shared versioned difficulty evaluator.
// Opening or selecting a proposal never changes an existing city's rules.
struct CITYSIMVR_API FCityNewCitySetup
{
	ECityDifficultyDraft Difficulty = ECityDifficultyDraft::Normal;
	int64 StartingFunds = 10000;
	FCityEconomyTuning Tuning;

	static FCityNewCitySetup Proposal(ECityDifficultyDraft Difficulty, const FCityEconomyTuning& Baseline);
	FString Summary() const;
};
