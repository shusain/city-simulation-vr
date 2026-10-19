#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
struct FCityPlacedEntity;
struct FCityBuildingDefinition;
struct FCityServiceSnapshot;
struct FCityDevelopmentSnapshot;
struct FCityEnvironmentSnapshot;
struct FCityEconomyTuning;
struct FCityPopulationSnapshot;

struct CITYSIMVR_API FCityFireIncident
{
	int64 Id = 0;
	uint64 EntityId = 0;
	uint64 SpreadFromEntityId = 0;
	int64 StartStep = 0;
	int64 ResolvedStep = -1;
	int32 ResponseSteps = 0;
	bool bTest = false;
	bool bArson = false;
	uint64 OffenderCitizenId = 0;
};

struct CITYSIMVR_API FCityFireDamage
{
	int32 Damage = 0;
	int64 RepairCompleteStep = -1;
	int32 Cause = 0; // 0 fire, 1 earthquake, 2 tornado; shared structural repair record.
};

struct CITYSIMVR_API FCityFireSnapshot
{
	FCityGridDimensions Dimensions;
	int64 LastFixedStepId = 0;
	int64 ActivationStep = -1;
	int64 GraceEndStep = -1;
	int64 NextIgnitionStep = 0;
	uint32 RandomState = 0x46495245u;
	int64 TotalIncidents = 0;
	int64 PreventedIncidents = 0;
	TArray<FCityFireIncident> RecentIncidents;
	TMap<uint64, FCityFireDamage> DamagedBuildings;

	// Derived diagnostics/allocations, rebuilt in stable entity order.
	int32 ActiveIncidents = 0;
	int32 OperationalStations = 0;
	int32 ProtectionDemand = 0;
	int32 ProtectedDemand = 0;
	int32 AverageRisk = 0;
	TArray<int32> CellRisk;
	TArray<uint8> CellStatus; // 1 risk, 4 burning, 5 damaged, 6 repairing.
	TMap<uint64, int32> EntityRisk;
	TMap<uint64, int32> UnprotectedRisk;
	TMap<uint64, int32> CrimeRiskContribution;
	TMap<uint64, int32> IndustryRiskContribution;
	TMap<uint64, uint64> EnRouteStations;
	TMap<uint64, int32> EntityProtection;
	TMap<uint64, int32> StationUsedCapacity;
	TMap<uint64, uint64> ResponseStations;

	bool IsIntroduced() const
	{
		return ActivationStep >= 0;
	}
	bool IsInGrace() const
	{
		return IsIntroduced() && LastFixedStepId < GraceEndStep;
	}
	bool IsBurning(uint64 EntityId) const;
	int32 FireDamageCount() const
	{
		int32 Count = 0;
		for (const auto& Pair : DamagedBuildings)
			Count += Pair.Value.Cause == 0 ? 1 : 0;
		return Count;
	}
	bool IsDisabled(uint64 EntityId) const
	{
		return DamagedBuildings.Contains(EntityId) || IsBurning(EntityId);
	}
	FString Describe() const;
};

class CITYSIMVR_API FCityFireSimulation
{
public:
	static constexpr int32 MaximumActiveIncidents = 3;
	static constexpr int32 MaximumRecentIncidents = 32;
	static constexpr int32 MaximumDamagedBuildings = 8;
	static constexpr int32 MaximumStructuralDamage = FCityGrid::Width * FCityGrid::Height;
	static constexpr int32 MaximumBurnSteps = 50;
	static constexpr int32 ResponseStepsToExtinguish = 3;
	void MarkArson(uint64 EntityId, uint64 CitizenId);
	bool StartArsonIgnition(uint64 EntityId, FString& Status);
	void Reset(int64 Step = 0);
	bool StartTest(uint64 EntityId, FString& OutStatus);
	bool ApplyNaturalDamage(uint64 EntityId, int32 Cause, int32 Damage);
	void RestoreFireproofStations(const FCityPlacementModel& City);
	void Refresh(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	             const FCityDevelopmentSnapshot& Development, const FCityEnvironmentSnapshot& Weather,
	             const FCityEconomyTuning& Tuning, bool bUpkeepShortfall,
	             const FCityPopulationSnapshot* Population = nullptr);
	void AdvanceToStep(int64 Step, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                   const FCityDevelopmentSnapshot& Development, const FCityEnvironmentSnapshot& Weather,
	                   const FCityEconomyTuning& Tuning, bool bUpkeepShortfall,
	                   const FCityPopulationSnapshot* Population = nullptr);
	bool Repair(uint64 EntityId, FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
	            const FCityEconomyTuning& Tuning, FString& OutStatus);
	int64 RepairCost(uint64 EntityId, const FCityBuildingDefinition& Definition,
	                 const FCityEconomyTuning& Tuning) const;
	static bool AreAdjacent(const FCityPlacedEntity& A, const FCityPlacedEntity& B);
	static int32 SpreadChance(const FCityPlacedEntity& Source, const FCityPlacedEntity& Target,
	                          const FCityEnvironmentSnapshot& Weather, const FCityEconomyTuning& Tuning,
	                          int32 Protection);
	static bool IsFireTarget(FName DefinitionId, const FCityEconomyTuning& Tuning);
	static bool ValidateState(const FCityFireSnapshot& State, int64 MaximumStep, FString& OutError);
	bool RestoreState(const FCityFireSnapshot& State, int64 MaximumStep, FString& OutError);
	const FCityFireSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

private:
	uint32 NextRandom();
	void Ignite(uint64 EntityId, uint64 SourceId);
	FCityFireSnapshot Snapshot;
};
