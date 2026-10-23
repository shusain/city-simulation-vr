#include "Presentation/CityWaterEffects.h"
#include "Presentation/CityTerrainSurface.h"
#include "Presentation/CityBeachScenery.h"
#include "Simulation/CityTerrain.h"

namespace
{
	// Search all nearby contour segments, including banks across tile boundaries.
	// Beyond 16m the approved field is entirely open water; exact distance is immaterial.
	struct FShoreField
	{
		const TArray<TPair<FVector, FVector>>& Segments;
		TMap<FIntPoint, TArray<int32>> Buckets;
		explicit FShoreField(const TArray<TPair<FVector, FVector>>& In) : Segments(In)
		{
			for (int32 I = 0; I < Segments.Num(); ++I)
			{
				const auto& S = Segments[I];
				for (int32 Y = FMath::FloorToInt(FMath::Min(S.Key.Y, S.Value.Y) / 800.0);
				     Y <= FMath::FloorToInt(FMath::Max(S.Key.Y, S.Value.Y) / 800.0); ++Y)
					for (int32 X = FMath::FloorToInt(FMath::Min(S.Key.X, S.Value.X) / 800.0);
					     X <= FMath::FloorToInt(FMath::Max(S.Key.X, S.Value.X) / 800.0); ++X)
						Buckets.FindOrAdd({X, Y}).Add(I);
			}
		}
		double Distance(const FVector& P) const
		{
			double Best = 1600.0 * 1600.0;
			const FIntPoint Center(FMath::FloorToInt(P.X / 800.0), FMath::FloorToInt(P.Y / 800.0));
			for (int32 Y = -2; Y <= 2; ++Y)
				for (int32 X = -2; X <= 2; ++X)
					if (const auto* Indices = Buckets.Find(Center + FIntPoint(X, Y)))
						for (const int32 I : *Indices)
						{
							const FVector2D A(Segments[I].Key), B(Segments[I].Value), Q(P);
							const FVector2D D = B - A;
							const double T = FMath::Clamp(FVector2D::DotProduct(Q - A, D) / D.SizeSquared(), 0.0, 1.0);
							Best = FMath::Min(Best, (Q - (A + D * T)).SizeSquared());
						}
			return FMath::Sqrt(Best);
		}
	};
	template <typename F> void Tessellate(const FVector& A, const FVector& B, const FVector& C, F Emit)
	{
		const double Span = FMath::Max3(FMath::Abs(A.X - B.X), FMath::Abs(A.X - C.X), FMath::Abs(B.X - C.X));
		const double SpanY = FMath::Max3(FMath::Abs(A.Y - B.Y), FMath::Abs(A.Y - C.Y), FMath::Abs(B.Y - C.Y));
		const int32 N = FMath::Max(1, FMath::CeilToInt((FMath::Max(Span, SpanY) - 1.e-6) / 100.0));
		auto Point = [&](int32 X, int32 Y) { return A + (B - A) * (double(X) / N) + (C - A) * (double(Y) / N); };
		for (int32 Y = 0; Y < N; ++Y)
			for (int32 X = 0; X < N - Y; ++X)
			{
				Emit(Point(X, Y), Point(X + 1, Y), Point(X, Y + 1));
				if (X + Y + 1 < N)
					Emit(Point(X + 1, Y), Point(X + 1, Y + 1), Point(X, Y + 1));
			}
	}
	FVector2D ShoreUV(const FVector& P, double SignedDistance)
	{
		return {(SignedDistance / 100.0 + 4.0) / 8.0, .5 - P.X / 4800.0};
	}
} // namespace
FCityWaterEffects FCityWaterEffects::Build(const FCityTerrainHeightfield& Terrain, const FCityPlacementModel& City,
                                           FCityWaterMesh& Water)
{
	FCityWaterEffects Result;
	const FShoreField Field(Water.ShoreSegments);
	Water.ShoreUVs.Reserve(Water.Vertices.Num());
	for (const FVector& P : Water.Vertices)
		Water.ShoreUVs.Add(ShoreUV(P, -Field.Distance(P)));

	const double Level = Terrain.GetSeaLevelCm();
	for (int32 Y = 0; Y + 1 < Terrain.GetSampleHeight(); ++Y)
		for (int32 X = 0; X + 1 < Terrain.GetSampleWidth(); ++X)
		{
			if (!FCityBeachScenery::IsClear(City, {X / 2, Y / 2}))
				continue;
			FCityTerrainSurface::Facets(
			    Terrain, X, Y,
			    [&](const FVector& A, const FVector& B, const FVector& C)
			    {
				    const FVector Points[] = {A, B, C};
				    double Distances[3];
				    bool bNear = false;
				    for (int32 I = 0; I < 3; ++I)
				    {
					    Distances[I] = Field.Distance(Points[I]) * (Points[I].Z < Level + 2.0 ? -1.0 : 1.0);
					    bNear |= Distances[I] > -650.0 && Distances[I] < 400.0;
				    }
				    if (!bNear)
					    return;
				    Tessellate(A, B, C,
				               [&](const FVector& P0, const FVector& P1, const FVector& P2)
				               {
					               const int32 Base = Result.Foam.Vertices.Num();
					               for (const FVector P : {P0, P1, P2})
					               {
						               const double D = Field.Distance(P) * (P.Z < Level + 2.0 ? -1.0 : 1.0);
						               Result.Foam.Vertices.Add({P.X, P.Y, FMath::Max(Level + 4.0, P.Z + 3.5)});
						               Result.Foam.UVs.Add(FVector2D(P) / 800.0);
						               Result.Foam.ShoreUVs.Add(ShoreUV(P, D));
						               Result.Foam.DepthUVs.Add({FMath::Max(0.0, Level - P.Z) / 800.0, 0});
						               Result.Foam.Colors.Add(FColor::White);
					               }
					               Result.Foam.Triangles.Append({Base, Base + 1, Base + 2});
				               });
			    });
		}
	// Even contour-local spacing follows shore length, never a citywide count cap.
	for (int32 I = 0; I < Water.ShoreSegments.Num(); ++I)
	{
		const auto& S = Water.ShoreSegments[I];
		const double Length = FVector::Dist2D(S.Key, S.Value);
		const uint32 Hash = GetTypeHash(FIntPoint(FMath::RoundToInt(S.Key.X), FMath::RoundToInt(S.Key.Y)));
		// Deterministic sample of short refined facets; one site per ~150cm.
		if ((Hash % 10000) / 10000.0 > FMath::Min(1.0, Length / 150.0))
			continue;
		const FVector P = (S.Key + S.Value) * .5;
		if (!FCityBeachScenery::IsClear(
		        City, FCityCellCoord(int32(FMath::FloorToInt(P.X / 800.0)), int32(FMath::FloorToInt(P.Y / 800.0)))))
			continue;
		for (int32 Drop = 0; Drop < 3; ++Drop)
		{
			const uint32 H = HashCombine(Hash, GetTypeHash(Drop));
			const int32 Base = Result.Splashes.Vertices.Num();
			const FVector Offset((int32(H % 101) - 50) * .5, (int32((H / 101) % 101) - 50) * .5, 3);
			const FVector Corners[] = {{-2, -2, 0}, {2, -2, 0}, {0, 2, 0}, {0, 0, 6}};
			for (const FVector& Corner : Corners)
			{
				Result.Splashes.Vertices.Add(P + Offset + Corner);
				Result.Splashes.UVs.Add({(H % 1000) / 1000.0 * .045, ((H / 1000) % 1000) / 1000.0});
				Result.Splashes.ShoreUVs.Add(ShoreUV(P, 0));
				Result.Splashes.DepthUVs.Add(FVector2D::ZeroVector);
				Result.Splashes.Colors.Add(FColor(64 + H % 128, 64 + (H / 128) % 128, 153 + (H / 16384) % 103, 255));
			}
			Result.Splashes.Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 1, Base + 3, Base + 1, Base + 2,
			                                  Base + 3, Base + 2, Base, Base + 3});
		}
	}
	return Result;
}
