#pragma once

#include "CoreMinimal.h"

struct FCityEconomySnapshot;

enum class ECityScenarioPhase : uint8
{
	NotStarted,
	Running,
	Succeeded
};

struct CITYSIMVR_API FCityScenarioSnapshot
{
	ECityScenarioPhase Phase = ECityScenarioPhase::NotStarted;
	int32 ProfitableMonthStreak = 0;
	int64 LastObservedBilledMonthId = 0;
};

class CITYSIMVR_API FCityScenarioSimulation
{
public:
	static constexpr int32 ResidentGoal = 100;
	static constexpr int32 ProfitableMonthGoal = 3;

	void Start(int64 CurrentBilledMonthId = 0);
	void Advance(const FCityEconomySnapshot& Economy);
	bool Restore(const FCityScenarioSnapshot& InSnapshot, FString& OutError);
	const FCityScenarioSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}
	FString Describe(int32 CurrentResidents) const;

private:
	FCityScenarioSnapshot Snapshot;
};
