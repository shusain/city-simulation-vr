#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"
#include "Simulation/CityIdentity.h"
class FCityPlacementModel;
class FCityFireSimulation;
struct FCityDevelopmentSnapshot;
enum class ECityNaturalDisaster : uint8
{
	Earthquake = 1,
	Tornado = 2
};
struct CITYSIMVR_API FCityDisasterEvent
{
	FCityGridDimensions Dimensions;
	int64 Id = 0;
	ECityNaturalDisaster Kind = ECityNaturalDisaster::Earthquake;
	FCityCellCoord Center;
	int32 Direction = 0;
	int64 WarningStep = 0, StartStep = 5, EndStep = 11;
	bool bTest = false;
	// Profile 1 preserves schema 41 events; profile 2 plans a city-scaled impact.
	int32 Profile = 1, SeverityPercent = 0, InitialBuildings = 0;
	int32 RadiusSquared = 0;
	TArray<double> TravelDistances;
	TArray<uint64> PlannedTargets;
	TArray<FCityCellCoord> Path;
	TArray<uint64> Affected;
};
struct CITYSIMVR_API FCityDisasterSnapshot
{
	FCityGridDimensions Dimensions;
	int64 LastFixedStepId = 0, NextCheckStep = 0, TotalEvents = 0;
	uint32 RandomState = 0x44495341u;
	TArray<FCityDisasterEvent> Events;
	const FCityDisasterEvent* Current() const;
	FString Describe() const;
};
// One fixed-step authority; no per-event Actors or runtime physics damage.
class CITYSIMVR_API FCityDisasterSimulation
{
public:
	static constexpr int32 MaximumEvents = 16;
	static constexpr int32 MaximumAffected = FCityGrid::Width * FCityGrid::Height;
	void Reset(int64 Step = 0, uint32 Seed = 1, FCityGridDimensions Dimensions = FCityGridDimensions());
	void SetFrequency(ECityDisasterFrequency Frequency, int32 StepsPerMonth);
	bool StartTest(ECityNaturalDisaster Kind, const FCityCellCoord& Center, int32 WindDirection);
	void AdvanceToStep(int64 Step, const FCityPlacementModel& City, const FCityDevelopmentSnapshot& Development,
	                   FCityFireSimulation& Damage, const FCityIdentity& Identity, int32 Residents, int32 StepsPerMonth,
	                   int32 WindDirection);
	const FCityDisasterSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}
	bool RestoreState(const FCityDisasterSnapshot& State, int64 MaximumStep, FString& Error);
	static bool ValidateState(const FCityDisasterSnapshot& State, int64 MaximumStep, FString& Error);
	static const TCHAR* Name(ECityNaturalDisaster Kind);
	static FVector2D ContinuousPosition(const FCityDisasterEvent& Event, double Step);
	static FCityCellCoord Position(const FCityDisasterEvent& Event, double Step);
	static int32 CheckInterval(ECityDisasterFrequency Frequency, int32 StepsPerMonth);

private:
	FCityDisasterSnapshot Snapshot;
	uint32 NextRandom();
	void PrepareEvent(const FCityPlacementModel& City, const TArray<uint64>& Targets);
	void Begin(ECityNaturalDisaster Kind, const FCityCellCoord& Center, int32 Direction, bool bTest);
};
