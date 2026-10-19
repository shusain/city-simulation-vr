#pragma once
#include "CoreMinimal.h"
class FCityPlacementModel;
struct FCityEconomyTuning;
struct FCityServiceSnapshot;
struct FCityPopulationSnapshot;
struct FCityDevelopmentSnapshot;
class CITYSIMVR_API FCityEducationEvaluator
{
public:
	static void Build(const FCityPlacementModel& City, FCityServiceSnapshot& Services, const FCityEconomyTuning& Tuning,
	                  const FCityPopulationSnapshot* Population, const FCityDevelopmentSnapshot* Development,
	                  bool bPrepareQueries = true, struct FCityServiceEvaluationTiming* Timing = nullptr);
};
