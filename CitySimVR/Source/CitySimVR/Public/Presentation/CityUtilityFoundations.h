#pragma once

#include "CoreMinimal.h"

class FCityTerrainHeightfield;

// An imported contour mesh contains separate closed structural columns. Its
// local top is0 and bottom is-100cm; only its vertical depth changes at runtime.
struct FCityUtilityFoundationGroup
{
    const TCHAR* AssetKey;
    const TCHAR* Mesh;
    double TopCm;
    FVector2D MinCm;
    FVector2D MaxCm;
};

namespace CityUtilityFoundations
{
    CITYSIMVR_API TConstArrayView<FCityUtilityFoundationGroup> All();
    CITYSIMVR_API TConstArrayView<FCityUtilityFoundationGroup> Find(const FString& MeshName);
    CITYSIMVR_API FTransform MakeTransform(const FCityUtilityFoundationGroup& Group,
        const FTransform& BuildingPose, const FCityTerrainHeightfield& Terrain);
}
