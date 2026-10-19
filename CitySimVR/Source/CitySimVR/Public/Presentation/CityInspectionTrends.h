#pragma once
#include "CoreMinimal.h"

class FCityPlacementModel;
struct FCityPopulationSnapshot;
struct FCityUiSnapshot;

// Presentation-only observations. Never substitutes a target for measured change.
class CITYSIMVR_API FCityInspectionTrends
{
public:
	void Observe(const FCityPlacementModel& City, const FCityPopulationSnapshot& Population,
	             int64 Step, int64 Month, uint64 Generation);
	void Present(uint64 EntityId, FCityUiSnapshot& Out) const;

private:
	struct FSample { int32 Residents = 0; int64 Happiness = 0; };
	TMap<uint64, FSample> Reference, Current;
	int64 LastStep = -1, ReferenceStep = -1, LastMonth = -1;
	uint64 CityGeneration = MAX_uint64;
};
