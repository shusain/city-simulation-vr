#include "Simulation/CityIdentity.h"

bool FCityIdentity::IsValidId(const FString& Value)
{
	if (Value.Len() != 32)
		return false;
	for (TCHAR C : Value)
		if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f')))
			return false;
	return true;
}
bool FCityIdentity::Validate(FString& Error) const
{
	if (!FCityGameRevision::IsValid(CreatedGameRevision))
	{
		Error = TEXT("Missing or invalid creation revision");
		return false;
	}
	if (!IsValidId(Id) || Name.IsEmpty() || Name.Len() > 32 || Name != Name.TrimStartAndEnd() ||
	    RulesVersion != 1 || static_cast<uint8>(Difficulty) > 2 || static_cast<uint8>(DisasterFrequency) > 3)
	{
		Error = TEXT("Invalid city identity, name or difficulty rules version");
		return false;
	}
	for (TCHAR C : Name)
		if (C < 32 || C == 127)
		{
			Error = TEXT("City name contains control characters");
			return false;
		}
	return true;
}
FCityEconomyTuning FCityIdentity::Apply(const ECityDifficulty Preset,
                                        const FCityEconomyTuning& CatalogBaseline)
{
	FCityEconomyTuning Result = CatalogBaseline;
	Result.CrimePopulationThreshold = 100;
	Result.FirePopulationThreshold = 150;
	Result.CrimeGraceMonths = 2;
	Result.FireGraceMonths = 2;
	Result.CrimeCheckIntervalSteps = 5;
	Result.FireCheckIntervalSteps = 10;
	Result.FireRepairCostPercent = 25;
	Result.FireRepairSteps = 10;
	if (Preset == ECityDifficulty::Relaxed)
	{
		Result.CrimePopulationThreshold = 150;
		Result.FirePopulationThreshold = 225;
		Result.CrimeGraceMonths = Result.FireGraceMonths = 3;
		Result.CrimeCheckIntervalSteps = 10;
		Result.FireCheckIntervalSteps = 20;
		Result.FireRepairCostPercent = 15;
		Result.FireRepairSteps = 5;
	}
	else if (Preset == ECityDifficulty::Challenging)
	{
		Result.CrimePopulationThreshold = 75;
		Result.FirePopulationThreshold = 125;
		Result.CrimeGraceMonths = Result.FireGraceMonths = 1;
		Result.FireRepairCostPercent = 35;
		Result.FireRepairSteps = 15;
	}
	return Result;
}
int64 FCityIdentity::StartingFunds(const ECityDifficulty Preset)
{
	return Preset == ECityDifficulty::Relaxed ? 15000 : Preset == ECityDifficulty::Challenging ? 7500 : 10000;
}
const TCHAR* FCityIdentity::DifficultyName(const ECityDifficulty Preset)
{
	return Preset == ECityDifficulty::Relaxed       ? TEXT("RELAXED")
	       : Preset == ECityDifficulty::Challenging ? TEXT("CHALLENGING")
	                                                : TEXT("NORMAL");
}

const TCHAR* FCityIdentity::DisasterFrequencyName(const ECityDisasterFrequency Frequency)
{
	switch (Frequency)
	{
	case ECityDisasterFrequency::Off:
		return TEXT("OFF");
	case ECityDisasterFrequency::Low:
		return TEXT("LOW");
	case ECityDisasterFrequency::High:
		return TEXT("HIGH");
	default:
		return TEXT("MEDIUM");
	}
}
