#pragma once

#include "CoreMinimal.h"

// Shared constructor/runtime dimensions for the headset capacity and demand bars.
namespace CityHudLayout
{
	constexpr int32 DemandVisualBound = 40;
	constexpr float DemandBarCenterY = -10.0f;
	constexpr float DemandBarHalfWidth = 6.0f;
	constexpr float CapacityBarTopZ = -15.0f;
	constexpr float CapacityBarRowSpacing = 2.35f;
	constexpr float DemandBarTopZ = -28.25f;
	constexpr float DemandBarRowSpacing = 2.6f;
} // namespace CityHudLayout
