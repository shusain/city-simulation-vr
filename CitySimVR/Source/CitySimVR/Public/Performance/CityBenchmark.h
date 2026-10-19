#pragma once

#include "CoreMinimal.h"
#include "Commands/CityPlacementModel.h"
#include "Performance/CityFrameTiming.h"
#include "Performance/CityEvaluationTiming.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityRoadNetwork.h"

class UCityBuildingCatalog;

struct CITYSIMVR_API FCityBenchmarkConfig
{
	int32 RequestedLots = 0;
	int32 RequestedRoadCells = 64;
	int32 RequestedVehicles = 32;
	FString SavedCityPath;
	int32 Speed = 0;
	FString Weather = TEXT("Natural");
	bool bPopulated = false;
	bool bLateGame = false;
	bool bReferenceServiceReuse = false;
	bool bReferenceCare = false;
	int32 CitizenRouteBudget = 32;
	int32 VisibleTraffic = 64;

	bool IsEnabled() const
	{
		return RequestedLots > 0 || !SavedCityPath.IsEmpty();
	}
};

// Allocated only for benchmark capture; no per-frame allocation or normal-play sampling.
struct FCityBenchmarkSimulationTiming
{
	FCityFrameTiming Ticks;
	FCityFrameTiming MonthlyTicks;
	FCityFrameTiming Derived, RoadNetwork, Services, Parks, Needs, FixedSteps;
	FCityFrameTiming InitialServices, PopulationReconcile, SchedulingTraffic, EnvironmentAdvance, DevelopmentGrowth,
	    Incidents;
	FCityRoadRoutingTiming Routing;
	FCityServiceEvaluationTiming ServiceEvaluation;
	FCityNeedsEvaluationTiming NeedsEvaluation;
	FCityEconomyEvaluationTiming EconomyEvaluation;
	FCityForecastTiming LivePopulationEvaluation;
	FCityFrameTiming TrafficSnapshotCopy, TrafficCompletions, DeliveryHolds, MonthlyPopulationCheck;
	FCityFrameTiming SchedulePrune, ScheduleHospital, ScheduleWaste, ScheduleEmergency, ScheduleOpenings,
	    ScheduleFreight, ScheduleCitizens;
	uint64 MaximumStepsPerTick = 0, CatchUpTicks = 0;
	uint64 ReusedInitialServices = 0, EvaluatedInitialServices = 0;
};

struct CITYSIMVR_API FCityBenchmarkResult
{
	FCityBenchmarkConfig Config;
	int32 PlacedLots = 0;
	int32 PlacedRoadCells = 0;
	int32 PlacedVehicles = 0;
};

class CITYSIMVR_API FCityBenchmark
{
public:
	static FCityTerrainGenerationParameters CapacityTerrainParameters();
	static bool ParseCommandLine(const TCHAR* CommandLine, FCityBenchmarkConfig& OutConfig, FString& OutError);
	static bool BuildScene(const FCityBenchmarkConfig& Config, const UCityBuildingCatalog& Catalog,
	                       FCityPlacementModelState& OutState, FCityBenchmarkResult& OutResult, FString& OutError,
	                       FCityGridDimensions Dimensions = FCityGridDimensions());
};
