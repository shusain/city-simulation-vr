#pragma once
#include "CoreMinimal.h"
#include "Simulation/CityTerrain.h"

// Bounded presentation refinement. The saved heightfield and logical cell rules
// remain authoritative; steep banks use its existing bilinear query surface.
struct CITYSIMVR_API FCityTerrainSurface
{
	static FVector SampleNormal(const FCityTerrainHeightfield& T, int32 X, int32 Y)
	{
		return FVector(T.GetSampleHeightCm(X - 1, Y) - T.GetSampleHeightCm(X + 1, Y),
		               T.GetSampleHeightCm(X, Y - 1) - T.GetSampleHeightCm(X, Y + 1), 800.)
		    .GetSafeNormal();
	}
	static double BankShadingWeight(const FCityTerrainHeightfield& T, int32 X, int32 Y)
	{
		const double CellX = X * .5, CellY = Y * .5;
		const int32 CX = FMath::FloorToInt(CellX), CY = FMath::FloorToInt(CellY);
		// The carved upper rim can sit several cells behind the wet beach.
		// Cover the whole bank, then fade before reaching unrelated upland terrain.
		double DistanceSquared = 30.25;
		for (int32 J = CY - 5; J <= CY + 5; ++J)
			for (int32 I = CX - 5; I <= CX + 5; ++I)
				if (T.IsWaterCell(FCityCellCoord(I, J)))
					DistanceSquared =
					    FMath::Min(DistanceSquared, FMath::Square(CellX - I - .5) + FMath::Square(CellY - J - .5));
		return 1. - FMath::SmoothStep(3.5, 5.5, FMath::Sqrt(DistanceSquared));
	}
	static FVector ShadingNormal(const FCityTerrainHeightfield& T, int32 X, int32 Y)
	{
		const FVector Original = SampleNormal(T, X, Y);
		const double Weight = BankShadingWeight(T, X, Y);
		if (Weight == 0.)
			return Original;
		// Filter gradients before normalization: steep alternating sample facets otherwise
		// leave a repeating light/dark ridge even after subdividing the bank geometry.
		constexpr int32 Kernel[] = {1, 4, 6, 4, 1};
		double Dx = 0., Dy = 0.;
		for (int32 J = -2; J <= 2; ++J)
			for (int32 I = -2; I <= 2; ++I)
			{
				const double K = Kernel[I + 2] * Kernel[J + 2] / 256.;
				Dx += K * (T.GetSampleHeightCm(X + I - 1, Y + J) - T.GetSampleHeightCm(X + I + 1, Y + J));
				Dy += K * (T.GetSampleHeightCm(X + I, Y + J - 1) - T.GetSampleHeightCm(X + I, Y + J + 1));
			}
		return FMath::Lerp(Original, FVector(Dx, Dy, 800.).GetSafeNormal(), Weight).GetSafeNormal();
	}
	static int32 Subdivisions(const FCityTerrainHeightfield& T, int32 X, int32 Y)
	{
		const double A = T.GetSampleHeightCm(X, Y), B = T.GetSampleHeightCm(X + 1, Y);
		const double C = T.GetSampleHeightCm(X, Y + 1), D = T.GetSampleHeightCm(X + 1, Y + 1);
		const double Low = FMath::Min(FMath::Min(A, B), FMath::Min(C, D));
		const double High = FMath::Max(FMath::Max(A, B), FMath::Max(C, D));
		return Low < T.GetSeaLevelCm() + 600 && High > T.GetSeaLevelCm() - 200 && High - Low > 80 ? 4 : 1;
	}
	static FVector Point(const FCityTerrainHeightfield& T, int32 X, int32 Y, double U, double V, int32 N)
	{
		const double A = T.GetSampleHeightCm(X, Y), B = T.GetSampleHeightCm(X + 1, Y);
		const double C = T.GetSampleHeightCm(X, Y + 1), D = T.GetSampleHeightCm(X + 1, Y + 1);
		const double Z = N > 1        ? FMath::Lerp(FMath::Lerp(A, B, U), FMath::Lerp(C, D, U), V)
		                 : U + V <= 1 ? A + U * (B - A) + V * (C - A)
		                              : D + (1 - U) * (C - D) + (1 - V) * (B - D);
		return FVector((X + U) * 400., (Y + V) * 400., Z);
	}
	static double Height(const FCityTerrainHeightfield& T, double CellX, double CellY)
	{
		const double X = CellX * 2, Y = CellY * 2;
		const int32 I = FMath::FloorToInt(X), J = FMath::FloorToInt(Y), N = Subdivisions(T, I, J);
		const double U = (X - I) * N, V = (Y - J) * N;
		const int32 A = FMath::Min(FMath::FloorToInt(U), N - 1), B = FMath::Min(FMath::FloorToInt(V), N - 1);
		const double P = U - A, Q = V - B;
		const double H00 = Point(T, I, J, double(A) / N, double(B) / N, N).Z;
		const double H10 = Point(T, I, J, double(A + 1) / N, double(B) / N, N).Z;
		const double H01 = Point(T, I, J, double(A) / N, double(B + 1) / N, N).Z;
		const double H11 = Point(T, I, J, double(A + 1) / N, double(B + 1) / N, N).Z;
		return P + Q <= 1 ? H00 + P * (H10 - H00) + Q * (H01 - H00)
		                  : H11 + (1 - P) * (H01 - H11) + (1 - Q) * (H10 - H11);
	}
	template <typename Callback>
	static void Facets(const FCityTerrainHeightfield& T, int32 X, int32 Y, Callback Emit, bool bRefine = true)
	{
		const int32 N = bRefine ? Subdivisions(T, X, Y) : 1;
		for (int32 J = 0; J < N; ++J)
			for (int32 I = 0; I < N; ++I)
			{
				const FVector A = Point(T, X, Y, double(I) / N, double(J) / N, N);
				const FVector B = Point(T, X, Y, double(I + 1) / N, double(J) / N, N);
				const FVector C = Point(T, X, Y, double(I) / N, double(J + 1) / N, N);
				const FVector D = Point(T, X, Y, double(I + 1) / N, double(J + 1) / N, N);
				Emit(A, C, B);
				Emit(B, C, D);
			}
	}
};
