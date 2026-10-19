#pragma once
#include "Presentation/CityHomeWalk.h"

// Continuous vehicle paving; no stairs or changes to authoritative freight routes.
struct CITYSIMVR_API FCityIndustrialDriveway : FCityHomeWalk
{
    void Build(const FVector& Bay, const FVector& PublicEdge, const FVector& Outward,
        const FCityTerrainHeightfield& Terrain, TFunctionRef<double(const FVector&)> PublicHeight,
        double WidthCm, double GroundReserveCm=175., double TerrainLiftCm=6., bool bLevelStart=true);
};
