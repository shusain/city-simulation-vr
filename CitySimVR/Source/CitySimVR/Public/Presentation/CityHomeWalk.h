#pragma once
#include "CoreMinimal.h"
class FCityTerrainHeightfield;

// Residential terrain approaches. Presentation only; parcel/access authority is unchanged.
struct CITYSIMVR_API FCityHomeWalk
{
	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	int32 StepCount = 0;
	int32 ActualStepCount = 0;
	TArray<FVector> RailVertices, RailNormals;
	TArray<int32> RailTriangles;
	TArray<FVector2D> RailUVs;
	void Build(const FVector& Door, const FVector& Sidewalk, const FCityTerrainHeightfield& Terrain,
	           TFunctionRef<double(const FVector&)> SidewalkHeight, double LandingCm = 50., double GroundReserveCm = 100., double WidthCm = 130.);
	// A level doorway landing, a flight along the frontage and a terrain apron
	// turning onto the public sidewalk. Sidewalk identifies the supported side.
	void BuildFrontage(const FVector& Door, const FVector& Sidewalk, const FVector& Outward,
	                   const FCityTerrainHeightfield& Terrain, TFunctionRef<double(const FVector&)> SidewalkHeight,
	                   double WidthCm, double DoorWidthCm, double LandingCm, double GroundReserveCm);
	static double GroundHeight(const FCityTerrainHeightfield& Terrain, const FVector& P);
};
