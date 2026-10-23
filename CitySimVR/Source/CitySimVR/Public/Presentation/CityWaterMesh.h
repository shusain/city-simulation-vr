#pragma once

#include "CoreMinimal.h"

class FCityTerrainHeightfield;

// Cosmetic intersection with the rendered terrain facets; never a placement/query surface.
struct CITYSIMVR_API FCityWaterMesh
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FVector2D> ShoreUVs;
	TArray<FVector2D> DepthUVs;
	TArray<TPair<FVector, FVector>> ShoreSegments;

	static FCityWaterMesh Build(const FCityTerrainHeightfield& Terrain, bool bRefineBanks = true, bool bWaveDetail = false);
	// Input winding matches the terrain (A,C,B / B,C,D), positions in logical cm.
	void AddFacet(const FVector& A, const FVector& B, const FVector& C, double WaterLevelCm);
};
