#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"
#include "Performance/CityFrameTiming.h"

class FCityPlacementModel;

enum class ECityRoadClass : uint8
{
	Dirt,
	Local,
	Avenue,
	Highway
};

enum class ECityRoadAccess : uint8
{
	None = 0,
	Pedestrian = 1 << 0,
	Car = 1 << 1,
	Freight = 1 << 2,
	BuildingFrontage = 1 << 3
};
ENUM_CLASS_FLAGS(ECityRoadAccess)

struct CITYSIMVR_API FCityRoadDefinition
{
	FName Id = TEXT("LocalRoad");
	ECityRoadClass RoadClass = ECityRoadClass::Local;
	int32 ForwardLanes = 1;
	int32 ReverseLanes = 1;
	int32 SpeedUnitsPerStep = 4;
	int32 CapacityPerLane = 20;
	int64 PlacementCostPerCell = 10;
	int64 MonthlyUpkeepPerCell = 1;
	ECityRoadAccess Access = ECityRoadAccess::Pedestrian | ECityRoadAccess::Car | ECityRoadAccess::Freight |
	                         ECityRoadAccess::BuildingFrontage;

	bool IsValid() const;
	static FCityRoadDefinition Dirt();
	static FCityRoadDefinition Local();
	static FCityRoadDefinition Avenue();
	static FCityRoadDefinition Highway();
};

struct CITYSIMVR_API FCityRoadNode
{
	int32 Id = INDEX_NONE;
	FCityCellCoord Cell;
	uint8 NeighborMask = 0;
};

struct CITYSIMVR_API FCityRoadEdge
{
	int32 Id = INDEX_NONE;
	int32 StartNodeId = INDEX_NONE;
	int32 EndNodeId = INDEX_NONE;
	FName DefinitionId = TEXT("LocalRoad");
	ECityRoadClass RoadClass = ECityRoadClass::Local;
	int32 ForwardLanes = 1;
	int32 ReverseLanes = 1;
	int32 SpeedUnitsPerStep = 4;
	int32 Capacity = 40;
	ECityRoadAccess Access = ECityRoadAccess::None;
	TArray<FCityCellCoord> Cells;
};

enum class ECityRoadTurn : uint8
{
	Left,
	Straight,
	Right
};

struct CITYSIMVR_API FCityLaneConnector
{
	int32 Id = INDEX_NONE;
	int32 NodeId = INDEX_NONE;
	int32 IncomingEdgeId = INDEX_NONE;
	int32 OutgoingEdgeId = INDEX_NONE;
	int32 IncomingLane = 0;
	int32 OutgoingLane = 0;
	ECityRoadTurn Turn = ECityRoadTurn::Straight;
};

struct FCityRoadRoutingTiming
{
	FCityFrameTiming Preparation, Search, Conversion, TrafficUpdate, TrafficAdvance;
	uint64 PreparedSearches = 0, ReferenceBuilds = 0, IndexedSegments = 0;
};

struct FCityRoadRoutingData;

struct CITYSIMVR_API FCityRoadNetworkSnapshot
{
	FCityGridDimensions Dimensions;
	uint64 SourceRevision = 0;
	TArray<FCityRoadNode> Nodes;
	TArray<FCityRoadEdge> Edges;
	TArray<FCityLaneConnector> Connectors;
	// Non-owning capture observer, refreshed whenever the benchmark resets.
	FCityRoadRoutingTiming* RoutingTiming = nullptr;
	bool bUsePreparedRouting = true;

	bool IsValid(FString* OutError = nullptr) const;
	bool FindRoute(const FCityCellCoord& Start, const FCityCellCoord& End, ECityRoadAccess RequiredAccess,
	               TArray<FCityCellCoord>& OutCells) const;
	// Walking distance in road cells to every reachable frontage; one bounded graph search.
	void FindWalkingDistances(const FCityCellCoord& Start, TMap<FCityCellCoord, int32>& OutDistances) const;
	const FCityRoadEdge* FindRouteSegment(const FCityCellCoord& Start, const FCityCellCoord& End,
	                                      ECityRoadAccess Access, int32& OutCellIndex, bool& bOutReverse) const;
	// Builder snapshots are immutable. Hand-authored fixtures may explicitly
	// prepare again after editing their public edge arrays; unprepared ones work too.
	void PrepareRouting();

private:
	TSharedPtr<const FCityRoadRoutingData> RoutingData;
};

class CITYSIMVR_API FCityRoadNetworkBuilder
{
public:
	static void Build(const FCityPlacementModel& City, const FCityRoadDefinition& DefaultDefinition,
	                  FCityRoadNetworkSnapshot& OutNetwork);
};
