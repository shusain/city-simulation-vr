#pragma once
#include "CoreMinimal.h"

class FCityPlacementModel;
struct FCityServiceSnapshot;
struct FCityPopulationSnapshot;
struct FCityEconomyTuning;
struct FCityDevelopmentSnapshot;
struct FCityBuildingDefinition;

// Contextual, read-only explanations of current service allocations and hiring.
class CITYSIMVR_API FCityServiceGuidance
{
public:
	static FString Range(const FCityBuildingDefinition& Definition, const FCityEconomyTuning& Tuning);
	static FString Water(const FCityPlacementModel& City, const FCityServiceSnapshot& Services, uint64 EntityId);
	static FString Education(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                         const FCityPopulationSnapshot& People, const FCityEconomyTuning& Tuning,
	                         const FCityDevelopmentSnapshot& Development, uint64 HomeId);
	static FString Employment(const FCityPopulationSnapshot& People, const FCityServiceSnapshot& Services,
	                          uint64 EntityId, bool bHome);
};
