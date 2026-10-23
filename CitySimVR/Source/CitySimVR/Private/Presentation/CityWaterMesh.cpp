#include "Presentation/CityWaterMesh.h"
#include "Presentation/CityTerrainSurface.h"
#include "Interaction/CityBoardPicking.h"
#include "Simulation/CityTerrain.h"

void FCityWaterMesh::AddFacet(const FVector& A, const FVector& B, const FVector& C, const double WaterLevelCm)
{
	// Keep the existing 2cm lift, clipping at the *rendered* plane so its rim
	// contacts the actual ground triangle, including non-coplanar sample quads.
	const double Plane = WaterLevelCm + 2.0;
	const FVector Input[] = {A, B, C};
	FVector Polygon[4];
	int32 Count = 0;
	TArray<FVector> Crossings;
	for (int32 I = 0; I < 3; ++I)
	{
		const FVector& Start = Input[I];
		const FVector& End = Input[(I + 1) % 3];
		const bool bStartWet = Start.Z < Plane, bEndWet = End.Z < Plane;
		if (bStartWet)
			Polygon[Count++] = Start;
		if (bStartWet != bEndWet)
		{
			// Always interpolate low -> high. Adjacent facets traverse the shared
			// edge in opposite directions; canonical order gives identical bytes.
			const FVector& Low = Start.Z < End.Z ? Start : End;
			const FVector& High = Start.Z < End.Z ? End : Start;
			Polygon[Count++] = FMath::Lerp(Low, High, (Plane - Low.Z) / (High.Z - Low.Z));
			Polygon[Count - 1].Z = Plane;
			Crossings.Add(Polygon[Count - 1]);
		}
	}
	if (Crossings.Num() == 2 && FVector::DistSquared2D(Crossings[0], Crossings[1]) > 1.e-8)
		ShoreSegments.Emplace(Crossings[0], Crossings[1]);
	// A vertex exactly on the plane can appear twice. Discard zero-area fans
	// rather than removing a small positive shoreline and opening a crack.
	for (int32 I = 1; I + 1 < Count; ++I)
	{
		const FVector Fan[] = {Polygon[0], Polygon[I], Polygon[I + 1]};
		const double Area =
		    (Fan[1].X - Fan[0].X) * (Fan[2].Y - Fan[0].Y) - (Fan[1].Y - Fan[0].Y) * (Fan[2].X - Fan[0].X);
		if (FMath::Abs(Area) <= 1.e-8)
			continue;
		const int32 Base = Vertices.Num();
		for (const FVector& Ground : Fan)
		{
			Vertices.Add(FVector(Ground.X, Ground.Y, Plane));
			DepthUVs.Add(FVector2D(FMath::Max(0.0, WaterLevelCm - Ground.Z) / 800.0, 0));
			UVs.Add(FVector2D(Ground.X, Ground.Y) / CityBoardPicking::LogicalCellSizeCm);
			const double DeepAlpha = FMath::SmoothStep(50.0, 250.0, WaterLevelCm - Ground.Z);
			Colors.Add(
			    FMath::Lerp(FLinearColor(0.04, 0.34, 0.52, 0.70), FLinearColor(0.025, 0.16, 0.30, 0.78), DeepAlpha)
			        .ToFColorSRGB());
		}
		Triangles.Append({Base, Base + 1, Base + 2});
	}
}

FCityWaterMesh FCityWaterMesh::Build(const FCityTerrainHeightfield& Terrain, const bool bRefineBanks,
                                     const bool bWaveDetail)
{
	FCityWaterMesh Mesh;
	constexpr int32 Segments = FCityTerrainHeightfield::SamplesPerCell;
	// GenerateHydrology uses one sea-level datum for every current lake/river.
	// Evaluate *all* samples, not just IsWaterCell: a dry logical center can
	// still have submerged corners. This also prevents tile/chunk cutoff seams.
	for (int32 Y = 0; Y + 1 < Terrain.GetSampleHeight(); ++Y)
		for (int32 X = 0; X + 1 < Terrain.GetSampleWidth(); ++X)
		{
			const FCityCellCoord Cell(X / Segments, Y / Segments);
			const double Level =
			    Terrain.IsWaterCell(Cell) ? Terrain.GetWaterSurfaceHeightCm(Cell) : Terrain.GetSeaLevelCm();
			FCityTerrainSurface::Facets(
			    Terrain, X, Y,
			    [&](const FVector& A, const FVector& B, const FVector& C)
			    {
				    if (!bWaveDetail)
				    {
					    Mesh.AddFacet(A, B, C, Level);
					    return;
				    }
				    // Subdivide the ground BEFORE clipping. Shared 100cm grid edges
				    // then have identical wave vertices even where the bank cuts them.
				    const double Span =
				        FMath::Max3(FMath::Abs(A.X - B.X), FMath::Abs(A.X - C.X), FMath::Abs(B.X - C.X));
				    const double SpanY =
				        FMath::Max3(FMath::Abs(A.Y - B.Y), FMath::Abs(A.Y - C.Y), FMath::Abs(B.Y - C.Y));
				    const int32 N = FMath::Max(1, FMath::CeilToInt((FMath::Max(Span, SpanY) - 1.e-6) / 100.0));
				    auto Point = [&](int32 U, int32 V)
				    { return A + (B - A) * (double(U) / N) + (C - A) * (double(V) / N); };
				    for (int32 V = 0; V < N; ++V)
					    for (int32 U = 0; U < N - V; ++U)
					    {
						    Mesh.AddFacet(Point(U, V), Point(U + 1, V), Point(U, V + 1), Level);
						    if (U + V + 1 < N)
							    Mesh.AddFacet(Point(U + 1, V), Point(U + 1, V + 1), Point(U, V + 1), Level);
					    }
			    },
			    bRefineBanks);
		}
	return Mesh;
}
