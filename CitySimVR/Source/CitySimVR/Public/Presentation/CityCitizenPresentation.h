#pragma once
#include "CoreMinimal.h"
#include "Presentation/CityRoadRibbon.h"

namespace CityCitizenPresentation
{
	// Match the park citizen's neutral 1.43m height. Imported street feet start
	// 12.5cm above the pivot; move their support plane with the lowered road assembly.
	inline constexpr double StreetUniformScale = 1.15;
	inline constexpr double ImportedFootMinCm = 12.5;
	inline constexpr double StreetFootSupportCm = FCityRoadRibbonMesh::SidewalkLiftCm;
	inline constexpr double StreetOriginOffsetCm = StreetFootSupportCm - ImportedFootMinCm * StreetUniformScale;

	// Cosmetic selection from saved citizen/trip identity. No new save field.
	inline int32 VariantIndex(uint64 Identity, bool bRefined)
	{
		return bRefined && Identity ? static_cast<int32>((Identity - 1) % 3) : 0;
	}
} // namespace CityCitizenPresentation
