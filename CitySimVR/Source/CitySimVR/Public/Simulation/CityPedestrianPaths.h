#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityTrafficSimulation.h"

struct CITYSIMVR_API FCityCrosswalk
{
	FCityCellCoord Cell;
	FIntVector A, B; // XY: 1000 units/cell, Z: road level (not elevation).
	bool bPainted = true;
};

// Derived sidewalk geometry. Road accessibility and trip settlement remain authoritative.
namespace CityPedestrianPaths
{
	CITYSIMVR_API TArray<FCityCrosswalk> Crossings(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog);
	CITYSIMVR_API bool Build(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
	    const FCityRoadNetworkSnapshot& Network, const TArray<FCityCrosswalk>& Crossings,
	    uint64 Origin, uint64 Destination, FCityTrafficVehicleState& Vehicle);
	CITYSIMVR_API int32 Length(const TArray<FIntVector>& Points);
	CITYSIMVR_API bool Sample(const TArray<FIntVector>& Points, double Distance, FVector& Position,
	    FVector& Direction, int32* Segment = nullptr, double* Fraction = nullptr);
	CITYSIMVR_API bool HasRoom(const FCityTrafficVehicleState& Vehicle, int32 Distance,
	    const TArray<FCityTrafficVehicleState>& Others);
	CITYSIMVR_API void UpdateRoadPosition(FCityTrafficVehicleState& Vehicle, const FCityRoadNetworkSnapshot& Network);
}
