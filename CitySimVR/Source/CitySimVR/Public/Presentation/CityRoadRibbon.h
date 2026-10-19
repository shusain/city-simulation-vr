#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"

// Derived geometry only. One shared component batches road-class sections.
class FCityTerrainHeightfield;
struct CITYSIMVR_API FCityRoadRibbonMesh
{
	const FCityTerrainHeightfield* Terrain = nullptr;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	static constexpr int32 Segments = 4;
	// Thin paving, not the former59cm floating road assembly. Keep the8cm curb.
	static constexpr double SurfaceLiftCm = 4.0;
	static constexpr double SidewalkLiftCm = SurfaceLiftCm + 8.0;
	static constexpr double AssemblyDropCm = 55.0;
	// Clip a convex patch to the rendered400cm terrain grid/diagonals. Height
	// may also supply a bridge/deck profile; this does not change terrain.
	void AppendPolygon(const TArray<FVector2D>& Polygon, TFunctionRef<double(double, double)> Height, double Lift);
	void AppendStraight(const FCityCellCoord& Cell, bool bHorizontal, double WidthCells,
	                    TFunctionRef<double(double, double)> Height, double StartWidth = -1, double EndWidth = -1);
	void AppendJunction(const FCityCellCoord& Cell, uint8 Mask, double WidthCells, const TArray<double>& PortalWidths,
	                    TFunctionRef<double(double, double)> Height);
	static double WidthAt(double Along, double Start, double Center, double End);

private:
	void AppendPatch(TFunctionRef<FVector2D(double, double)> Position, TFunctionRef<double(double, double)> Height);
};
