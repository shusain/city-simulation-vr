#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGameRevision.h"

enum class ECityDifficulty : uint8
{
	Relaxed,
	Normal,
	Challenging
};
// Natural hazards only; ordinary building fires use their separate safety rules.
enum class ECityDisasterFrequency : uint8
{
	Off,
	Low,
	Medium,
	High
};

// Versioned authority: preset v1 keeps the accepted damage/spread timings.
struct CITYSIMVR_API FCityIdentity
{
	FString Id = TEXT("00000000000000000000000000000001");
	FString Name = TEXT("Existing City");
	FString CreatedGameRevision = FCityGameRevision::Current();
	ECityDifficulty Difficulty = ECityDifficulty::Normal;
	int32 RulesVersion = 1;
	ECityDisasterFrequency DisasterFrequency = ECityDisasterFrequency::Medium;
	static const TCHAR* DisasterFrequencyName(ECityDisasterFrequency Frequency);
	bool Validate(FString& Error) const;
	static bool IsValidId(const FString& Value);
	static FCityEconomyTuning Apply(ECityDifficulty Preset, const FCityEconomyTuning& CatalogBaseline);
	static int64 StartingFunds(ECityDifficulty Preset);
	static const TCHAR* DifficultyName(ECityDifficulty Preset);
};
