#pragma once
#include "CoreMinimal.h"

class FCityPlacementModel;
struct FCityFireSnapshot;
struct FCityCrimeSnapshot;
struct FCityWasteSnapshot;
struct FCityServiceSnapshot;
struct FCityHealthSnapshot;
struct FCityPopulationSnapshot;
class UCitySimulationSubsystem;

struct FCityConditionIndicator
{
	uint64 EntityId = 0;
	TArray<uint64> EntityIds;
	FBox2D RegionBounds{ForceInit};
	bool bNeedsRepair = false;
	bool bRepairing = false;
	bool bCrime = false;
	bool bFire = false;
	// Current authoritative operation failure; zero means no failure.
	uint8 OperationFailure = 0;
	int32 SettledConcerns = 0;
	bool bSupplyEmpty = false, bRetailStockout = false;
	bool bSustainedDiscontent = false;
	FName Symbol() const;
	int32 Priority() const;
	int32 CauseCount() const;
	uint32 CauseMask() const;
	TArray<FName> Symbols() const;
	// Brief cues come from the existing monthly housing history, not inferred causes.
	uint8 RecentHousingKind = 0; // kind + 1; zero means none.
	int32 RecentHousingResidents = 0;
	int64 RecentHousingMonth = 0;
	int32 IllResidents = 0, PatientsInCare = 0;
	int32 WasteUnits = 0;
	int32 SearchingResidents = 0, LongestDiscontent = 0;
	bool bHousingCareHold = false, bMovingFeeUnaffordable = false;
	int64 RepairDays = 0;
	int32 RepairPercent = 0;
	int64 CrimeDays = 0;
	FString Title(int64 RepairCost) const;
	FString Detail(int32 StepsPerDay = 150) const;
};

// Read-only, consolidated presentation of current conditions, never risk/history.
class CITYSIMVR_API FCityConditionIndicators
{
public:
	static constexpr int32 MaximumBadges = 16;
	// A compact region, not a chain that can connect markers across the city.
	static constexpr double MaximumClusterDiameterCells = 8.0;
	static constexpr int32 MaximumLeaders = 64 * 64;
	static constexpr double HalfWidth = 3.5;
	static constexpr double HalfHeight = 3.0;
	static TArray<FCityConditionIndicator> Build(
	    const FCityPlacementModel& City, const FCityFireSnapshot& Fire, const FCityCrimeSnapshot& Crime,
	    int32 RepairDuration, const FCityHealthSnapshot* Health = nullptr,
	    const FCityPopulationSnapshot* People = nullptr, uint64 ExcludedEntityId = 0,
	    const FCityWasteSnapshot* Waste = nullptr, const FCityServiceSnapshot* Services = nullptr,
	    int32 Limit = MaximumBadges, double ClusterDiameter = MaximumClusterDiameterCells,
	    int32* OutAffectedLots = nullptr);
	static bool Hit(const FTransform& Pose, const FVector& Origin, const FVector& Direction, double& Distance);
	static FString Explain(const UCitySimulationSubsystem& Sim, uint64 EntityId);
	// One current blocker, shown only for an exhausted household in its supplies accordion.
	static FString ShoppingBlocker(const UCitySimulationSubsystem& Sim, uint64 EntityId);
	static uint64 RelatedLot(const UCitySimulationSubsystem& Sim, uint64 EntityId, FString& Label);
};
