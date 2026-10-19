#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityWaterMesh.h"
#include "Presentation/CityTerrainSurface.h"
#include "Simulation/CityTerrain.h"

namespace
{
	double ShoreArea(const FCityWaterMesh& Mesh)
	{
		double Area = 0;
		for (int32 I = 0; I < Mesh.Triangles.Num(); I += 3)
		{
			const FVector A = Mesh.Vertices[Mesh.Triangles[I]], B = Mesh.Vertices[Mesh.Triangles[I + 1]],
			              C = Mesh.Vertices[Mesh.Triangles[I + 2]];
			Area += FMath::Abs(FVector::CrossProduct(B - A, C - A).Z) * .5;
		}
		return Area;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterFacetTest, "CitySim.Presentation.WaterFacetIntersection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterFacetTest::RunTest(const FString&)
{
	// Plane is level+2. An oblique half-submerged triangle has known 1/4 area.
	FCityWaterMesh One;
	One.AddFacet({0, 0, -100}, {0, 400, 100}, {400, 0, 100}, -2);
	TestEqual(TEXT("one wet corner has one triangle"), One.Triangles.Num(), 3);
	TestTrue(TEXT("diagonal contour preserves analytical clipped area"), FMath::IsNearlyEqual(ShoreArea(One), 20000.));
	TestTrue(TEXT("intersection crosses the tile interior"),
	         One.Vertices.Contains(FVector(200, 0, 0)) && One.Vertices.Contains(FVector(0, 200, 0)));
	FCityWaterMesh Two;
	Two.AddFacet({0, 0, 100}, {0, 400, -100}, {400, 0, -100}, -2);
	TestEqual(TEXT("two wet corners triangulate a quad"), Two.Triangles.Num(), 6);
	TestTrue(TEXT("complement covers the remaining three quarters"), FMath::IsNearlyEqual(ShoreArea(Two), 60000.));
	FCityWaterMesh Dry;
	Dry.AddFacet({0, 0, 0}, {0, 400, 0}, {400, 0, 0}, -2);
	Dry.AddFacet({0, 0, 0}, {0, 400, 100}, {400, 0, 100}, -2);
	TestTrue(TEXT("on-plane or dry facets make no floating sheet"), Dry.Vertices.IsEmpty());
	FCityWaterMesh Touch;
	Touch.AddFacet({0, 0, 0}, {0, 400, -100}, {400, 0, -100}, -2);
	TestEqual(TEXT("exact-plane corner emits no degenerate fan"), Touch.Triangles.Num(), 3);
	TestTrue(TEXT("exact-plane corner preserves area"), FMath::IsNearlyEqual(ShoreArea(Touch), 80000.));
	FCityWaterMesh Adjacent;
	const FVector Low(0, 400, -73), High(400, 0, 121);
	Adjacent.AddFacet({0, 0, -40}, Low, High, -2);
	Adjacent.AddFacet(High, Low, {400, 400, 80}, -2);
	const FVector Intersection = FMath::Lerp(Low, High, 73. / 194.);
	int32 Matches = 0;
	for (const auto& V : Adjacent.Vertices)
		Matches += V == FVector(Intersection.X, Intersection.Y, 0);
	TestTrue(TEXT("oppositely traversed shared edge has bit-identical intersections"), Matches >= 2);
	for (const auto& Mesh : {One, Two, Touch, Adjacent})
		for (int32 I = 0; I < Mesh.Vertices.Num(); I += 3)
			TestTrue(
			    TEXT("all emitted triangles retain ground winding and positive area"),
			    FVector::CrossProduct(Mesh.Vertices[I + 1] - Mesh.Vertices[I], Mesh.Vertices[I + 2] - Mesh.Vertices[I])
			            .Z < 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterContoursTest, "CitySim.Presentation.WaterTerrainContours",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterContoursTest::RunTest(const FString&)
{
	for (const int32 Seed : {7429, 1, 5, 6})
	{
		FCityTerrainGenerationParameters P;
		P.Seed = Seed;
		P.GeneratorVersion = Seed == 7429 ? 0 : 2;
		P.WaterRulesVersion = 1;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(P);
		const auto Heights = Terrain.GetQuantizedHeightsCm();
		const auto Classes = Terrain.GetWaterClasses();
		const auto Rivers = Terrain.GetRiverCells();
		const auto Mesh = FCityWaterMesh::Build(Terrain);
		const auto Again = FCityWaterMesh::Build(Terrain);
		TestTrue(TEXT("rendering leaves terrain, classes and river authority unchanged"),
		         Heights == Terrain.GetQuantizedHeightsCm() && Classes == Terrain.GetWaterClasses() &&
		             Rivers == Terrain.GetRiverCells());
		TestTrue(TEXT("regeneration is exact"), Mesh.Vertices == Again.Vertices && Mesh.Colors == Again.Colors &&
		                                            Mesh.Triangles == Again.Triangles && Mesh.UVs == Again.UVs);
		TestTrue(TEXT("fixture has water"), !Mesh.Vertices.IsEmpty());
		TestTrue(TEXT("geometry bounded by 256 triangles per logical tile"),
		         Mesh.Triangles.Num() <= P.Dimensions.GetCellCount() * 768);
		int32 Crossings = 0, OutsideWaterCells = 0;
		bool bSubmerged = true, bFinite = true;
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const auto& V = Mesh.Vertices[I];
			bFinite &= !V.ContainsNaN() && !Mesh.UVs[I].ContainsNaN();
			const double X = V.X / 400., Y = V.Y / 400.;
			const int32 Sx = FMath::Clamp(FMath::FloorToInt(X), 0, Terrain.GetSampleWidth() - 2);
			const int32 Sy = FMath::Clamp(FMath::FloorToInt(Y), 0, Terrain.GetSampleHeight() - 2);
			// Independent fine-facet interpolation, using the original bilinear
			// samples only where the bank refinement selects a finer grid.
			const int32 N = FCityTerrainSurface::Subdivisions(Terrain, Sx, Sy);
			const double U = (X - Sx) * N, W = (Y - Sy) * N;
			const int32 Iu = FMath::Min(FMath::FloorToInt(U), N - 1), Iv = FMath::Min(FMath::FloorToInt(W), N - 1);
			const double Tx = U - Iu, Ty = W - Iv;
			auto Z = [&](int32 A, int32 B)
			{ return Terrain.SampleHeightCm((Sx + double(A) / N) / 2., (Sy + double(B) / N) / 2.); };
			const double A = Z(Iu, Iv), B = Z(Iu + 1, Iv), C = Z(Iu, Iv + 1), D = Z(Iu + 1, Iv + 1);
			const double Ground =
			    Tx + Ty <= 1 ? A + Tx * (B - A) + Ty * (C - A) : D + (1 - Tx) * (C - D) + (1 - Ty) * (B - D);
			bSubmerged &= Ground <= V.Z + 1.e-6 && V.Z == Terrain.GetSeaLevelCm() + 2.;
			Crossings += !FMath::IsNearlyEqual(X, FMath::RoundToDouble(X), 1.e-8) ||
			             !FMath::IsNearlyEqual(Y, FMath::RoundToDouble(Y), 1.e-8);
			OutsideWaterCells += !Terrain.IsWaterCell({FMath::Min(int32(V.X / 800), P.Dimensions.GetWidth() - 1),
			                                           FMath::Min(int32(V.Y / 800), P.Dimensions.GetHeight() - 1)});
		}
		TestTrue(TEXT("water never floats above a dry facet or uses an invalid coordinate"), bSubmerged && bFinite);
		TestTrue(TEXT("shore cuts diagonally through sample edges"), Crossings > 0);
		TestTrue(TEXT("submerged shoreline portions extend beyond center-classified water"), OutsideWaterCells > 0);
		UE_LOG(LogTemp, Display,
		       TEXT("CITYSIM_SHORE_CONTOURS Seed=%d Vertices=%d Crossings=%d ShoreVertices=%d Area=%.0f"), Seed,
		       Mesh.Vertices.Num(), Crossings, OutsideWaterCells, ShoreArea(Mesh));
	}
	return true;
}
#endif
