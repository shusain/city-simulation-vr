#pragma once
#include "CoreMinimal.h"
struct FCityPopulationSnapshot;
struct FCityCitizenRecord;
struct FCityHouseholdRecord;
struct FCityServiceSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityEconomySnapshot;
struct FCityEconomyTuning;
struct FCityWasteSnapshot;
class FCityPlacementModel;

enum class ECityHappinessConcern : int32
{
	Housing = 1 << 0,
	Services = 1 << 1,
	Purchases = 1 << 2,
	Reserves = 1 << 3,
	Pollution = 1 << 4,
	Safety = 1 << 5,
	Health = 1 << 6,
	Waste = 1 << 7,
	Taxes = 1 << 8
};

struct CITYSIMVR_API FCityHappinessSummary
{
	int32 Residents = 0, Average = 0, Target = 0, Discontented = 0, Sustained = 0, Concerns = 0;
};

struct FCityHappinessAssessment
{
	int32 Target = 85, Concerns = 0;
	void Penalize(int32 Amount, ECityHappinessConcern Concern)
	{
		Target -= Amount;
		if (Amount > 0)
			Concerns |= static_cast<int32>(Concern);
	}
};

class CITYSIMVR_API FCityHappinessEvaluator
{
public:
	static constexpr int32 InitialHappiness = 60;
	static constexpr int32 DiscontentThreshold = 50;
	static constexpr int32 SustainedMonths = 3;
	static constexpr int32 MaximumMonthlyChange = 10;
	static constexpr int32 ValidConcerns = (1 << 9) - 1;
	static bool ValidateCitizen(const FCityCitizenRecord& Citizen, FString& Error);
	static FCityHappinessSummary Summarize(const FCityPopulationSnapshot& Population, uint64 HomeId = 0);
	static FString DescribeConcerns(int32 Concerns);
	// A move preserves actual purchases and uses remaining money after its fee.
	static FCityHappinessAssessment AssessHome(const FCityHouseholdRecord& Household, const FCityPlacementModel& City,
	                                           const FCityServiceSnapshot& Services,
	                                           const FCityEnvironmentSnapshot& Environment,
	                                           const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
	                                           const FCityWasteSnapshot& Waste);
};
