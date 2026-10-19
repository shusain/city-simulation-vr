#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"
#include "Simulation/CityPopulationSimulation.h"

class FCityPlacementModel;
class UCityBuildingCatalog;
struct FCityRoadNetworkSnapshot;
struct FCityServiceSnapshot;
struct FCityDevelopmentSnapshot;

struct CITYSIMVR_API FCityWasteSite
{
	uint64 EntityId = 0;
	int32 Units = 0, Capacity = 60, GenerationRemainder = 0;
	int64 LastCollectionStep = 0;
};
struct CITYSIMVR_API FCityWasteDepot
{
	uint64 EntityId = 0;
	int32 Stored = 0;
	int64 Processed = 0, Recycled = 0;
};
struct CITYSIMVR_API FCityWasteTruck
{
	uint64 Id = 0, DepotId = 0, CurrentEntityId = 0, TripId = 0;
	int32 Cargo = 0;
	int64 LastWorkStep = 0;
	// Ordered/reserved stops survive save/resume. One trip at a time follows them.
	TArray<uint64> Stops;
};
struct CITYSIMVR_API FCityWasteSnapshot
{
	FCityGridDimensions Dimensions;
	int64 LastStep = 0, IntroductionStep = -1, Generated = 0, Collected = 0;
	int64 Disposed = 0, Recycled = 0, Overflow = 0, Removed = 0;
	uint64 NextTruckId = 1;
	TArray<FCityWasteSite> Sites;
	TArray<FCityWasteDepot> Depots;
	TArray<FCityWasteTruck> Trucks;
	const FCityWasteSite* Find(uint64 Id) const
	{
		return Sites.FindByPredicate([Id](const auto& S) { return S.EntityId == Id; });
	}
	int64 Piled() const;
	int64 InTrucks() const;
	int64 InDepots() const;
	int32 Pressure(uint64 Id, int32 StepsPerMonth) const;
};
struct CITYSIMVR_API FCityWasteFacilityPolicy
{
	int32 Range = 64, Fleet = 0, Storage = 2400, ProcessingPerStep = 80, StaffPercent = 0;
	bool bRecycling = false;
};
struct FCityWasteTuning
{
	int32 PopulationThreshold = 350;
	static constexpr int32 GraceMonths = 6;
	static constexpr int32 TruckCapacity = 200, PickupPerStep = 50, UnloadPerStep = 100, MaxStops = 4;
};
class CITYSIMVR_API FCityWasteSimulation
{
public:
	const FCityWasteSnapshot& GetSnapshot() const
	{
		return State;
	}
	void Reset(int64 Step = 0)
	{
		State = {};
		State.LastStep = Step;
		ReconciledRevision = MAX_uint64;
	}
	bool NeedsReconcile(uint64 Revision) const
	{
		return ReconciledRevision != Revision;
	}
	bool RestoreState(const FCityWasteSnapshot& Snapshot, FString& Error);
	static bool ValidateState(const FCityWasteSnapshot& Snapshot, int64 MaximumStep, FString& Error);
	static bool ValidateReferences(const FCityWasteSnapshot& Snapshot, const FCityPlacementModel& City,
	                               const FCityPopulationSnapshot& People, FString& Error);
	static bool IsFacility(FName Id)
	{
		return Id == TEXT("WasteDepot") || Id == TEXT("Recycling");
	}
	static FCityWasteFacilityPolicy Policy(uint64 Id, const FCityPlacementModel& City,
	                                       const UCityBuildingCatalog* Catalog,
	                                       const FCityDevelopmentSnapshot& Development,
	                                       const FCityServiceSnapshot& Services, bool Unpaid);
	// Arrivals are consumed before/after traffic, but generation/work runs once per fixed step.
	void Update(int64 Step, int32 StepsPerMonth, const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	            const FCityRoadNetworkSnapshot& Network, const FCityServiceSnapshot& Services,
	            const FCityDevelopmentSnapshot& Development, FCityPopulationSnapshot& People, bool Unpaid,
	            const FCityWasteTuning& Tuning = FCityWasteTuning());
	bool AddTestPile(uint64 EntityId);

private:
	FCityWasteSnapshot State;
	uint64 ReconciledRevision = MAX_uint64;
};
