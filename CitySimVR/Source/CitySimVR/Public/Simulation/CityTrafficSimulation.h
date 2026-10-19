#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityPopulationSimulation.h"

class FCityPlacementModel;
class UCityBuildingCatalog;

// Fixed-point longitudinal movement keeps traffic deterministic and independent
// of render frame rate. One logical road cell is TrafficUnitsPerCell units.
struct CITYSIMVR_API FCityTrafficRouteLeg
{
	int32 EdgeId = INDEX_NONE;
	bool bReverse = false;
	int32 LaneIndex = 0;
};

struct CITYSIMVR_API FCityTrafficVehicleState
{
	uint64 Id = 0;
	TArray<FCityTrafficRouteLeg> Route;
	int32 RouteLegIndex = 0;
	int32 DistanceUnits = 0;
	int32 SpeedUnitsPerStep = 0;
	int32 LengthUnits = 350;
	bool bCompleted = false;
	int32 StartDistanceUnits = 0;
	int32 EndDistanceUnits = -1;
	ECityTripVehicleType VehicleType = ECityTripVehicleType::Car;
	uint64 SourceId = 0;
	uint64 OriginEntityId = 0;
	uint64 DestinationEntityId = 0;
	bool bTransitTrip = false;
	bool bReturning = false;
	// Response priority excludes trips returning to the station. Yield is a
	// curbside state, interpolated by the existing presentation pool. Ordinary
	// vehicles may crawl to clear a responder's pull-over slot; crews stop fully.
	bool bEmergencyResponding = false;
	bool bYielding = false;
	int64 CompletedStep = 0;
	// Only used by waiting departures; movement/arrival records stay separate.
	int64 DepartureRequestedStep = 0;
	int32 PedestrianTrack = 0;
};

struct CITYSIMVR_API FCityTrafficLaneLoad
{
	int32 EdgeId = INDEX_NONE;
	bool bReverse = false;
	int32 LaneIndex = 0;
	int32 VehicleCount = 0;
	int32 Capacity = 1;
	int32 AverageSpeedUnitsPerStep = 0;
	int32 CongestionPermille = 0;
	bool bPedestrian = false;
};

struct CITYSIMVR_API FCityTrafficSnapshot
{
	int64 FixedStepId = 0;
	uint64 NetworkFingerprint = 0;
	TArray<FCityTrafficVehicleState> Vehicles;
	TArray<FCityTrafficVehicleState> WaitingDepartures;
	TArray<FCityTrafficLaneLoad> LaneLoads;
};

class CITYSIMVR_API FCityTrafficSimulation
{
  public:
	static constexpr int32 TrafficUnitsPerCell = 1000;
	static constexpr int32 AccelerationUnitsPerStep = 1000;
	static constexpr int32 SafeFollowingGapUnits = 500;
	static constexpr int32 PedestrianGapUnits = 70;
	static int32 FollowingGap(const FCityTrafficVehicleState& V)
	{
		return V.VehicleType == ECityTripVehicleType::Walk ? PedestrianGapUnits : SafeFollowingGapUnits;
	}
	static constexpr int32 StopLineGapUnits = 100;

	void Reset();
	// Capacity experiments only; ordinary scheduling remains 32 citizens/16 transit trips.
	void SetBenchmarkCitizenRouteBudget(int32 Budget)
	{
		CitizenRouteBudget = FMath::Clamp(Budget, 1, 2048);
	}
	bool AddVehicle(const FCityTrafficVehicleState& Vehicle, const FCityRoadNetworkSnapshot& Network,
	                FString* OutError = nullptr);
	void AdvanceOneStep(const FCityRoadNetworkSnapshot& Network, int64 FixedStepId);
	bool AdvanceToStep(const FCityRoadNetworkSnapshot& Network, int64 FixedStepId, FString& OutError);
	bool RestoreState(const FCityTrafficSnapshot& State, const FCityRoadNetworkSnapshot& Network,
	                  int64 MaximumFixedStepId, FString& OutError);
	static uint64 GetNetworkFingerprint(const FCityRoadNetworkSnapshot& Network);
	void ReconcileNetwork(const FCityRoadNetworkSnapshot& Network);
	void UpdateTrips(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                 const FCityPopulationSnapshot& Population, const FCityRoadNetworkSnapshot& Network,
	                 int64 Step);
	static bool FindPortal(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	                       uint64 EntityId, FCityCellCoord& OutCell);
	static bool BuildRoute(const FCityRoadNetworkSnapshot& Network, const FCityCellCoord& Start,
	                       const FCityCellCoord& End, FCityTrafficVehicleState& Vehicle);
	static bool SamplePosition(const FCityTrafficVehicleState& Vehicle,
	                           const FCityTrafficVehicleState* Previous,
	                           const FCityRoadNetworkSnapshot& Network, double Alpha,
	                           FVector& OutGridPosition, FVector& OutDirection);
	static double SampleMotorLaneOffsetCm(const FCityTrafficVehicleState& Vehicle,
	                                      const FCityTrafficVehicleState* Previous, double Alpha,
	                                      FName RoadDefinitionId);
	static double SamplePedestrianOffsetCm(const FCityTrafficVehicleState& Vehicle,
	                                      const FCityTrafficVehicleState* Previous, double Alpha, double WidthCm);
	static FVector SampleMotorHeading(const FCityTrafficVehicleState& Vehicle,
	                                 const FCityTrafficVehicleState* Previous, double Alpha,
	                                 FName RoadDefinitionId, const FVector& Forward, double TravelCm);
	const FCityTrafficSnapshot& GetSnapshot() const
	{
		return Snapshot;
	}

	static int32 GetEdgeLengthUnits(const FCityRoadEdge& Edge);
	static int32 GetSpeedLimitUnitsPerStep(const FCityRoadEdge& Edge);

  private:
	static bool ValidateLeg(const FCityTrafficRouteLeg& Leg, const FCityRoadNetworkSnapshot& Network);
	static int32 GetDepartureNode(const FCityTrafficRouteLeg& Leg, const FCityRoadEdge& Edge);
	static int32 GetArrivalNode(const FCityTrafficRouteLeg& Leg, const FCityRoadEdge& Edge);
	static const FCityLaneConnector* FindConnector(const FCityTrafficRouteLeg& Incoming,
	                                               const FCityTrafficRouteLeg& Outgoing,
	                                               const FCityRoadNetworkSnapshot& Network);
	void RebuildLaneLoads(const FCityRoadNetworkSnapshot& Network);
	void AdmitDepartures(const FCityRoadNetworkSnapshot& Network);
	int32 DepartureStopLimit(const FCityTrafficRouteLeg& Leg, bool bPedestrian,
	                         int32 Position, bool bEmergency, int32 DestinationDistance = MAX_int32,
	                         int32 PedestrianTrack = 0) const;

	FCityTrafficSnapshot Snapshot;
	int32 CitizenRouteBudget = 32;
};
