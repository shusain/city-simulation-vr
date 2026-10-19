#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityTerrainSurface.h"
#include "Presentation/CityRoadRibbon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainBankTest, "CitySim.Presentation.BankRefinement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTerrainBankTest::RunTest(const FString&)
{
	FCityTerrainHeightfield T;
	T.Generate(FCityTerrainGenerationParameters());
	const auto Heights = T.GetQuantizedHeightsCm();
	const auto Classes = T.GetWaterClasses();
	int32 Refined = 0, Coarse = 0;
	double MaxDeviation = 0;
	for (int32 Y = 0; Y + 1 < T.GetSampleHeight(); ++Y)
		for (int32 X = 0; X + 1 < T.GetSampleWidth(); ++X)
		{
			const int32 N = FCityTerrainSurface::Subdivisions(T, X, Y);
			Refined += N > 1;
			Coarse += N == 1;
			int32 Faces = 0;
			FCityTerrainSurface::Facets(
			    T, X, Y,
			    [&](const FVector& A, const FVector& B, const FVector& C)
			    {
				    ++Faces;
				    const FVector P = (A + B + C) / 3.;
				    TestTrue(TEXT("ground query lies exactly on each emitted facet"),
				             FMath::IsNearlyEqual(P.Z, FCityTerrainSurface::Height(T, P.X / 800., P.Y / 800.), 1.e-6));
				    TestTrue(TEXT("clockwise, finite ground faces"),
				             !P.ContainsNaN() && FVector::CrossProduct(B - A, C - A).Z < 0);
				    if (N > 1)
				    {
					    for (const FVector& V : {A, B, C})
						    TestTrue(TEXT("fine vertices use existing bilinear authority query"),
						             FMath::IsNearlyEqual(V.Z, T.SampleHeightCm(V.X / 800., V.Y / 800.), 1.e-6));
					    const FVector Old = FCityTerrainSurface::Point(T, X, Y, (P.X / 400.) - X, (P.Y / 400.) - Y, 1);
					    MaxDeviation = FMath::Max(MaxDeviation, FMath::Abs(P.Z - Old.Z));
				    }
			    });
			TestEqual(TEXT("bounded facets per sample quad"), Faces, 2 * N * N);
			for (int32 I = 0; I <= N; ++I)
			{
				const double U = double(I) / N;
				for (const FVector2D UV : {FVector2D(U, 0), FVector2D(U, 1), FVector2D(0, U), FVector2D(1, U)})
					TestTrue(TEXT("coarse and refined neighbours have identical boundary heights"),
					         FCityTerrainSurface::Point(T, X, Y, UV.X, UV.Y, N)
					             .Equals(FCityTerrainSurface::Point(T, X, Y, UV.X, UV.Y, 1), 1.e-8));
			}
			if (N > 1)
			{
				FCityRoadRibbonMesh Road;
				Road.Terrain = &T;
				Road.AppendPolygon(
				    {{X * .5, Y * .5}, {(X + 1) * .5, Y * .5}, {(X + 1) * .5, (Y + 1) * .5}, {X * .5, (Y + 1) * .5}},
				    [&](double A, double B) { return FCityTerrainSurface::Height(T, A, B); }, 4.);
				for (int32 I = 0; I < Road.Triangles.Num(); I += 3)
				{
					const FVector P = (Road.Vertices[Road.Triangles[I]] + Road.Vertices[Road.Triangles[I + 1]] +
					                   Road.Vertices[Road.Triangles[I + 2]]) /
					                  3.;
					TestTrue(
					    TEXT("bank paving interior follows refined ground without burial or floating"),
					    FMath::IsNearlyEqual(P.Z - FCityTerrainSurface::Height(T, P.X / 800., P.Y / 800.), 4., 1.e-6));
				}
			}
		}
	TestTrue(TEXT("refinement remains local with dry terrain unchanged"), Refined > 0 && Refined < Coarse);
	TestTrue(TEXT("bank geometry actually changes between old diagonal and bilinear surface"), MaxDeviation > 1);
	TestTrue(TEXT("rendering preserves authoritative samples and classification"),
	         Heights == T.GetQuantizedHeightsCm() && Classes == T.GetWaterClasses());
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_BANK_REFINEMENT RefinedQuads=%d CoarseQuads=%d MaxOldDeviationCm=%.3f"),
	       Refined, Coarse, MaxDeviation);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainBankShadingTest, "CitySim.Presentation.BankShadingContinuity",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityTerrainBankShadingTest::RunTest(const FString&)
{
	FCityTerrainHeightfield T;
	T.Generate(FCityTerrainGenerationParameters());
	const auto Heights = T.GetQuantizedHeightsCm();
	const auto Classes = T.GetWaterClasses();
	int32 Changed = 0, UnchangedDry = 0, BankSamples = 0;
	double BeforeEnergy = 0., AfterEnergy = 0.;
	for (int32 Y = 0; Y < T.GetSampleHeight(); ++Y)
		for (int32 X = 0; X < T.GetSampleWidth(); ++X)
		{
			const FVector Before = FCityTerrainSurface::SampleNormal(T, X, Y);
			const FVector After = FCityTerrainSurface::ShadingNormal(T, X, Y);
			const double Weight = FCityTerrainSurface::BankShadingWeight(T, X, Y);
			TestTrue(TEXT("finite unit bank shading"),
			         !After.ContainsNaN() && FMath::IsNearlyEqual(After.SizeSquared(), 1., 1.e-10));
			TestTrue(TEXT("deterministic shading"), After == FCityTerrainSurface::ShadingNormal(T, X, Y));
			Changed += !After.Equals(Before, 1.e-8);
			if (Weight == 0.)
			{
				++UnchangedDry;
				TestTrue(TEXT("terrain outside the bank envelope has its exact original normal"), After == Before);
			}
			if (Weight >= .8 && Before.Z < .98 && X > 0 && Y > 0 && X + 1 < T.GetSampleWidth() &&
			    Y + 1 < T.GetSampleHeight())
			{
				++BankSamples;
				for (const FIntPoint D : {FIntPoint(1, 0), FIntPoint(0, 1)})
				{
					const FVector OldCurve = FCityTerrainSurface::SampleNormal(T, X - D.X, Y - D.Y) - 2. * Before +
					                         FCityTerrainSurface::SampleNormal(T, X + D.X, Y + D.Y);
					const FVector NewCurve = FCityTerrainSurface::ShadingNormal(T, X - D.X, Y - D.Y) - 2. * After +
					                         FCityTerrainSurface::ShadingNormal(T, X + D.X, Y + D.Y);
					BeforeEnergy += OldCurve.SizeSquared();
					AfterEnergy += NewCurve.SizeSquared();
				}
			}
		}
	TestTrue(TEXT("localized smoothing changes banks while preserving dry terrain"),
	         Changed > 0 && UnchangedDry > Changed);
	TestTrue(TEXT("bank normal curvature decreases"),
	         BankSamples > 0 && BeforeEnergy > 0. && AfterEnergy < BeforeEnergy);
	TestTrue(TEXT("shading preserves authoritative heights and classification"),
	         Heights == T.GetQuantizedHeightsCm() && Classes == T.GetWaterClasses());
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_BANK_SHADING Changed=%d UnchangedDry=%d BankSamples=%d CurvatureRatio=%.6f"),
	       Changed, UnchangedDry, BankSamples, AfterEnergy / FMath::Max(BeforeEnergy, 1.e-20));
	return true;
}
#endif
