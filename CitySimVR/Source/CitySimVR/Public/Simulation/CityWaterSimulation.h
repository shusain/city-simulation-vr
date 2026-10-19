#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
struct FCityServiceEvaluationTiming;

enum class ECityWaterFailure : uint8
{
	None,
	Disconnected,
	LowPressure,
	SourceCapacity,
	SourceOutage
};

struct CITYSIMVR_API FCityWaterSource
{
	uint64 EntityId = 0;
	int32 Capacity = 0;
	int32 Used = 0;
	int32 YieldPercent = 100;
	int32 StaffPercent = 100;
	int32 SourcePressurePercent = 100;
	bool bOperational = false;
	TMap<FCityCellCoord, int32> RoadDistances;
};

// Derived each service evaluation; never persists allocations or invents stored water.
struct CITYSIMVR_API FCityWaterSnapshot
{
	int32 RulesVersion = 0;
	// Query preparation is scoped to current source budgets; never persisted.
	bool bPrepareQueries = true;
	FCityServiceEvaluationTiming* EvaluationTiming = nullptr;
	TArray<FCityWaterSource> Sources;
	TMap<FCityCellCoord, int32> RoadPressurePercent;
	TSet<FCityCellCoord> ActiveNetworkRoads;
	TSet<FCityCellCoord> PotentialNetworkRoads;
	TMap<uint64, int32> EntityDemand;
	TMap<uint64, int32> EntityDelivered;
	TMap<uint64, int32> EntityPressurePercent;
	TMap<uint64, ECityWaterFailure> EntityCauses;
};

class CITYSIMVR_API FCityWaterEvaluator
{
public:
	static constexpr int32 PressureLossPerRoadHop = 2;
	static constexpr int32 MinimumUsablePressurePercent = 20;
	static constexpr int32 FullDeliveryPressurePercent = 60;
	static void BuildNetwork(const FCityPlacementModel& City, FCityWaterSnapshot& Water, int32 LegacyHopBudget);
	// Mutates each finite source budget exactly once per consumer, in caller's priority order.
	static int32 Allocate(const FCityPlacementModel& City, FCityWaterSnapshot& Water, uint64 EntityId,
	                      const FCityCellCoord& Anchor, const FIntPoint& Size, int32 Demand);
	static int32 GetPressure(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
	                         const FCityCellCoord& Anchor, const FIntPoint& Size);
	static int32 GetAvailableAt(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
	                            const FCityCellCoord& Anchor, const FIntPoint& Size, int32 Demand);
	static ECityWaterFailure EvaluateAt(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
	                                    const FCityCellCoord& Anchor, const FIntPoint& Size, int32 Demand);
	static FString FailureToString(ECityWaterFailure Cause);
};
