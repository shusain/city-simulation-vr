#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityRoadRibbon.h"
#include "Presentation/CityRoadPresentation.h"
#include "Presentation/CityHomeWalk.h"
#include "Simulation/CityTerrain.h"
#include "Persistence/CityPersistence.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadRibbonTest, "CitySim.Roads.SampledGroundPaving",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRoadRibbonTest::RunTest(const FString& Parameters)
{
	FCityTerrainHeightfield Terrain;
	FCityTerrainGenerationParameters Config;
	Config.Seed = 19041;
	TestTrue(TEXT("Generate uneven fixture"), Terrain.Generate(Config));
	for (const FName Id :
	     {FName(TEXT("DirtRoad")), FName(TEXT("LocalRoad")), FName(TEXT("Avenue")), FName(TEXT("Highway"))})
		for (const bool bHorizontal : {false, true})
		{
			FCityRoadRibbonMesh Mesh;
			Mesh.Terrain = &Terrain;
			const auto Height = [&](double X, double Y) { return FCityHomeWalk::GroundHeight(Terrain,FVector(X*800,Y*800,0)); };
			Mesh.AppendStraight({30, 30}, bHorizontal, CityRoadPresentation::SurfaceWidth(Id), Height);
			Mesh.AppendStraight(bHorizontal ? FCityCellCoord(31, 30) : FCityCellCoord(30, 31), bHorizontal,
			                    CityRoadPresentation::SurfaceWidth(Id), Height);
            TestTrue(TEXT("Clipped paving remains bounded per two cells"),Mesh.Vertices.Num()>0 && Mesh.Vertices.Num()<400);
            // Independent build on either side must expose identical shared portal positions.
			FCityRoadRibbonMesh Left, Right;
			Left.Terrain = Right.Terrain = &Terrain;
			Left.AppendStraight({30,30},bHorizontal,CityRoadPresentation::SurfaceWidth(Id),Height);
            Right.AppendStraight(bHorizontal?FCityCellCoord(31,30):FCityCellCoord(30,31),bHorizontal,CityRoadPresentation::SurfaceWidth(Id),Height);
            int32 Shared=0;
            for(const auto& V:Left.Vertices) if(FMath::IsNearlyEqual(bHorizontal?V.X:V.Y,31*800.,.001)) {
                ++Shared; TestTrue(TEXT("Adjacent paving cells share exact portals"),Right.Vertices.ContainsByPredicate([&](const FVector& P){return P.Equals(V,.001);}));
            }
            TestTrue(TEXT("Portal has both road edges"),Shared>=2);
			for (int32 I = 0; I < Mesh.Triangles.Num(); I += 3)
			{
				const FVector A = Mesh.Vertices[Mesh.Triangles[I]], B = Mesh.Vertices[Mesh.Triangles[I + 1]],
				              C = Mesh.Vertices[Mesh.Triangles[I + 2]];
				TestTrue(TEXT("Top faces use Unreal clockwise winding"), FVector::CrossProduct(B - A, C - A).Z < 0);
			}
            for(int32 I=0; I<Mesh.Triangles.Num(); I+=3) {
                const FVector P=(Mesh.Vertices[Mesh.Triangles[I]]+Mesh.Vertices[Mesh.Triangles[I+1]]+Mesh.Vertices[Mesh.Triangles[I+2]])/3.;
                TestTrue(TEXT("Paving triangle interior remains exactly4cm above actual terrain"),FMath::IsNearlyEqual(P.Z-FCityHomeWalk::GroundHeight(Terrain,P),4.,.001));
            }
			for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
			{
				const auto& V = Mesh.Vertices[I];
				TestTrue(TEXT("Every paving vertex follows the shared height sampler"),
				         FMath::IsNearlyEqual(V.Z, Height(V.X / 800., V.Y / 800.) + FCityRoadRibbonMesh::SurfaceLiftCm, .0001));
				TestTrue(TEXT("Sampled normals face upward"), Mesh.Normals[I].Z > 0);
			}
		}
    FCityRoadRibbonMesh Taper;
	Taper.Terrain = &Terrain;
	Taper.AppendStraight({30,30},true,.98,[](double,double){return 0.;},.52,.68);
    TestTrue(TEXT("Mixed-width road retains its widest class midpoint"),Taper.Vertices.ContainsByPredicate([](const FVector& P){return FMath::IsNearlyEqual(P.X,30.5*800.,.001) && FMath::IsNearlyEqual(P.Y,(30.5+.49)*800.,.001);}));
	// Every corner/T/cross orientation is a disjoint center patch plus its open approaches.
	for (const uint8 Mask : {3, 6, 9, 12, 7, 11, 13, 14, 15})
		for (const double Width : {.52, .68, .88, .98})
		{
			FCityRoadRibbonMesh Node;
			Node.Terrain = &Terrain;
			TArray<double> Portals = {(Width + .52) * .5, (Width + .98) * .5, (Width + .68) * .5, (Width + .88) * .5};
			Node.AppendJunction({30, 30}, Mask, Width, Portals,
			                    [&](double X, double Y) { return FCityHomeWalk::GroundHeight(Terrain,FVector(X*800,Y*800,0)); });
            TestTrue(TEXT("Clipped junction has bounded geometry"),Node.Vertices.Num()>0 && Node.Vertices.Num()<1200);
			for (const auto& V : Node.Vertices)
				TestTrue(TEXT("Junction remains inside its logical road cell"),
				         V.X >= 30 * 800 && V.X <= 31 * 800 && V.Y >= 30 * 800 && V.Y <= 31 * 800);
			for (int32 I = 0; I < Node.Triangles.Num(); I += 3)
				TestTrue(TEXT("All junction faces point upward"),
				         FVector::CrossProduct(Node.Vertices[Node.Triangles[I + 1]] - Node.Vertices[Node.Triangles[I]],
				                               Node.Vertices[Node.Triangles[I + 2]] - Node.Vertices[Node.Triangles[I]])
				                 .Z < 0);
			if (Mask & 2)
			{
				FCityRoadRibbonMesh Next;
				Next.Terrain = &Terrain;
				Next.AppendStraight(
				    {31, 30}, true, .98, [&](double X, double Y) { return FCityHomeWalk::GroundHeight(Terrain,FVector(X*800,Y*800,0)); }, Portals[1],
				    .98);
                for(const auto& V:Node.Vertices) if(FMath::IsNearlyEqual(V.X,31*800.,.001))
                    TestTrue(TEXT("Mixed-width junction matches adjacent straight portal"),Next.Vertices.ContainsByPredicate([&](const FVector& P){return P.Equals(V,.001);}));
			}
		}
	// Generate a native road-command fixture for the actual PIE inspection, separate from user saves.
	FCityPlacementModel City(100000);
	int32 Y = 22;
	for (const auto& Definition : {FCityRoadDefinition::Dirt(), FCityRoadDefinition::Local(),
	                               FCityRoadDefinition::Avenue(), FCityRoadDefinition::Highway()})
	{
		auto Road = City.BeginRoadPreview({10, Y}, Definition.Id, Definition.PlacementCostPerCell);
		City.UpdateRoadPreviewEnd(Road, {45, Y});
		TestTrue(TEXT("Visual fixture uses valid road commands"), City.CommitRoad(Road).IsSuccess());
		Y += 3;
	}
	// Add corner, T, cross and mixed-width junctions to the native visual fixture.
	for (const int32 X : {15, 26, 37})
	{
		auto Road = City.BeginRoadPreview({X, 22}, TEXT("Highway"), 60);
		City.UpdateRoadPreviewEnd(Road, {X, 34});
		TestTrue(TEXT("Junction fixture commits"), City.CommitRoad(Road).IsSuccess());
	}
	auto CornerRoad = City.BeginRoadPreview({45, 22}, TEXT("Highway"), 60);
	City.UpdateRoadPreviewEnd(CornerRoad, {45, 25});
	TestTrue(TEXT("Corner fixture commits"), City.CommitRoad(CornerRoad).IsSuccess());
	FCitySaveDocument Document;
	Document.Placement = City.ExportState();
	Document.Terrain = Config;
	Document.Terrain.SeaLevelCm = -2500;
	Document.bSimulationPaused = true;
	FString Json, Error;
	const FString Fixture =
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/Roads-JunctionFixture.json"));
	TestTrue(TEXT("Native fixture serializes"), FCityPersistence::Serialize(Document, Json, Error));
	TestTrue(TEXT("Native fixture stays separate from user city"), FCityPersistence::WriteAtomic(Fixture, Json, Error));
	return true;
}
#endif
