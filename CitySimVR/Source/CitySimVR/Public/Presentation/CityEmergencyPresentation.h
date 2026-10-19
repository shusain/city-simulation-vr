#pragma once

#include "Simulation/CityTrafficSimulation.h"

// Match the same interpolated road pose that the vehicle renderer displays.
// Authority can complete a trip while that pose is still approaching its end.
namespace CityEmergencyPresentation
{
	inline bool SampleArrivedVehicle(const FCityTrafficVehicleState& Vehicle, const FCityTrafficVehicleState* Previous,
	                                 const FCityRoadNetworkSnapshot& Network, double Alpha, FVector& OutPosition,
	                                 FVector& OutDirection)
	{
		if (!Vehicle.bCompleted)
			return false;
		FVector End, EndDirection;
		return FCityTrafficSimulation::SamplePosition(Vehicle, Previous, Network, Alpha, OutPosition, OutDirection) &&
		       FCityTrafficSimulation::SamplePosition(Vehicle, nullptr, Network, 1.0, End, EndDirection) &&
		       OutPosition.Equals(End, 0.000001);
	}
} // namespace CityEmergencyPresentation
