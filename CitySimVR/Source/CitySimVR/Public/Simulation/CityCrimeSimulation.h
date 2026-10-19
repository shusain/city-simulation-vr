#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

class FCityPlacementModel;
struct FCityServiceSnapshot;
struct FCityEconomyTuning;
struct FCityPopulationSnapshot;
struct FCityDevelopmentSnapshot;
class FCityPopulationSimulation;
class FCityFireSimulation;
enum class ECityArsonOutcome : uint8
{
	Pending,
	Escaped,
	JailFull,
	Detained,
	Released,
	ReleasedEarly
};

struct CITYSIMVR_API FCityCrimeIncident
{
	int64 Id = 0;
	uint64 EntityId = 0;
	int64 StartStep = 0;
	int64 EndStep = 0;
	bool bTest = false;
	bool bArson = false;
	uint64 OffenderCitizenId = 0, ArrestStationId = 0;
	int64 CaptureStep = -1;
	ECityArsonOutcome ArsonOutcome = ECityArsonOutcome::Pending;
};

struct CITYSIMVR_API FCityCrimeSnapshot
{
	FCityGridDimensions Dimensions;
	// Authoritative saved counters. Housing displacement is an aggregate,
	// separate from the existing housed citizen/wallet records.
	int64 LastFixedStepId = 0;
	int64 ActivationStep = -1;
	int64 GraceEndStep = -1;
	uint32 RandomState = 0x4352494du;
	int32 PreviousHousedResidents = 0;
	int32 HomelessResidents = 0;
	int64 TotalIncidents = 0;
	int64 PreventedIncidents = 0;
	int64 LastArsonStep = -1, TotalArsons = 0, TotalArrests = 0;
	TArray<FCityCrimeIncident> RecentIncidents;

	// Rebuilt from current commands/services; never persisted as duplicate authority.
	int32 HousedResidents = 0;
	int32 JobShortage = 0;
	int32 BaseRisk = 0;
	int32 AverageRisk = 0;
	int32 OperationalStations = 0;
	int32 ProtectionDemand = 0;
	int32 ProtectedDemand = 0;
	int32 ActiveIncidents = 0;
	TArray<int32> CellRisk;
	TMap<uint64, int32> EntityRisk;
	TMap<uint64, int32> EntityProtection;
	TMap<uint64, int32> StationUsedCapacity;

	bool IsIntroduced() const
	{
		return ActivationStep >= 0;
	}
	bool IsInGrace() const
	{
		return IsIntroduced() && LastFixedStepId < GraceEndStep;
	}
	int32 GetRisk(const FCityCellCoord& Cell) const;
	FString Describe() const;
};

class CITYSIMVR_API FCityCrimeSimulation
{
public:
	static constexpr int32 MaximumRecentIncidents = 32;
	static constexpr int32 MaximumActiveIncidents = 2;
	static constexpr int32 IncidentDurationSteps = 5;
	static constexpr int32 MaximumResidents = FCityGrid::CellCount * 1024;
	static constexpr int32 ArsonEscapeSteps = 10;
	static int32 JailCapacity(const FCityDevelopmentSnapshot& Development, uint64 StationId);
	static const TCHAR* OutcomeName(ECityArsonOutcome Outcome);
	bool LinkArson(uint64 EntityId, const FCityPopulationSnapshot& Population, FCityFireSimulation& Fire);
	void RefreshCustody(const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
	                    FCityPopulationSimulation& Population, const FCityFireSimulation& Fire);
	void AdvanceArson(const FCityPlacementModel& City, const FCityDevelopmentSnapshot& Development,
	                  const FCityEconomyTuning& Tuning, const FCityServiceSnapshot& Services, bool bUnpaid,
	                  FCityPopulationSimulation& Population, FCityFireSimulation& Fire);
	void Reset(int64 Step = 0, int32 HousedResidents = 0);
	bool StartTest(uint64 EntityId, FString& OutStatus);
	void Refresh(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	             const FCityDevelopmentSnapshot& Development, const FCityEconomyTuning& Tuning, bool bUpkeepShortfall);
	void AdvanceToStep(int64 Step, const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                   const FCityDevelopmentSnapshot& Development, const FCityEconomyTuning& Tuning,
	                   bool bUpkeepShortfall, const FCityPopulationSnapshot* Population = nullptr);
	bool RestoreState(const FCityCrimeSnapshot& State, int64 MaximumStep, FString& OutError);
	static bool ValidateState(const FCityCrimeSnapshot& State, int64 MaximumStep, FString& OutError);
	const FCityCrimeSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

private:
	uint32 NextRandom();
	FCityCrimeSnapshot Snapshot;
};
