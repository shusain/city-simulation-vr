#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityWaterEffects.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterEffectsTest, "CitySim.Presentation.WaterEffectsGeometry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterEffectsTest::RunTest(const FString&)
{
	for (const int32 Seed : {6, 5})
	{
		FCityTerrainGenerationParameters P;
		P.GeneratorVersion = 2;
		P.WaterRulesVersion = 1;
		P.Seed = Seed;
		P.BroadElevationAmplitudeCm = 0;
		P.HillAmplitudeCm = 0;
		P.RidgeAmplitudeCm = 0;
		P.SeaLevelCm = -160;
		P.QuantizationCm = 1;
		FCityTerrainHeightfield Terrain;
		Terrain.Generate(P);
		FCityPlacementModel City(100000);
		const auto Heights = Terrain.GetQuantizedHeightsCm();
		auto Water = FCityWaterMesh::Build(Terrain, true, true);
		auto Area = [](const FCityWaterMesh& M)
		{
			double Sum = 0;
			for (int32 I = 0; I < M.Triangles.Num(); I += 3)
				Sum += FMath::Abs(FVector::CrossProduct(M.Vertices[M.Triangles[I + 1]] - M.Vertices[M.Triangles[I]],
				                                        M.Vertices[M.Triangles[I + 2]] - M.Vertices[M.Triangles[I]])
				                      .Z) *
				       .5;
			return Sum;
		};
		const auto Original = FCityWaterMesh::Build(Terrain);
		TestTrue(TEXT("wave subdivision preserves clipped coverage"),
		         FMath::Abs(Area(Water) - Area(Original)) < FMath::Max(1.0, Area(Original)) * 1.e-8);
		const auto Effects = FCityWaterEffects::Build(Terrain, City, Water);
		TestTrue(TEXT("actual shoreline and both batches"), !Water.ShoreSegments.IsEmpty() &&
		                                                        !Effects.Foam.Vertices.IsEmpty() &&
		                                                        !Effects.Splashes.Vertices.IsEmpty());
		for (const auto* Mesh : TArray<const FCityWaterMesh*>{&Water, &Effects.Foam, &Effects.Splashes})
		{
			TestEqual(TEXT("shore UV per vertex"), Mesh->ShoreUVs.Num(), Mesh->Vertices.Num());
			TestEqual(TEXT("depth UV per vertex"), Mesh->DepthUVs.Num(), Mesh->Vertices.Num());
			for (int32 I = 0; I < Mesh->Vertices.Num(); ++I)
			{
				if (Mesh->Vertices[I].ContainsNaN() || Mesh->ShoreUVs[I].ContainsNaN() ||
				    Mesh->DepthUVs[I].ContainsNaN())
				{
					AddError(TEXT("nonfinite effect field"));
					return false;
				}
			}
		}
		TestTrue(TEXT("no terrain authority mutation"), Heights == Terrain.GetQuantizedHeightsCm());
		bool bCoast = false, bOpen = false;
		for (int32 I = 0; I < Water.Vertices.Num(); ++I)
		{
			const double D = Water.ShoreUVs[I].X * 8.0 - 4.0;
			bCoast |= D > -.2;
			bOpen |= D <= -12.0 || Water.DepthUVs[I].X * 8 >= 2.2;
		}
		TestTrue(TEXT("real coastal and offshore regions"), bCoast && bOpen);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterWeatherTest, "CitySim.Presentation.WaterWeatherPhase",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterWeatherTest::RunTest(const FString&)
{
	FCityEnvironmentSnapshot Calm, Storm;
	Storm.WindStrength = 2;
	Storm.bPrecipitation = true;
	TestTrue(TEXT("storm fixture obeys real weather range"), Storm.IsValid());
	TestTrue(TEXT("actual weather calls this a storm"), Storm.DescribeWeather().Contains(TEXT("STORM")));
	FCityAtmosphereMotion Motion;
	Motion.Reset(Calm);
	TestEqual(TEXT("calm endpoint"), FCityWaterEffects::Strength(Motion), 0.0);
	Motion.PhaseSeconds = 1.5;
	Motion.Advance(.1, Storm);
	TestTrue(TEXT("storm blend continuous"),
	         FCityWaterEffects::Strength(Motion) > 0 && FCityWaterEffects::Strength(Motion) < 1);
	TestTrue(TEXT("weather changes never restart arrival"), FMath::IsNearlyEqual(Motion.PhaseSeconds, 1.6));
	Motion.Advance(60, Storm);
	TestTrue(TEXT("storm approaches reviewed endpoint"), FCityWaterEffects::Strength(Motion) > .999);
	Motion.Advance(60, Calm);
	TestTrue(TEXT("return calm"), FCityWaterEffects::Strength(Motion) < .001);
	const auto Before = Motion.PhaseSeconds;
	Motion.Advance(0, Storm);
	TestEqual(TEXT("paused clock stable"), Motion.PhaseSeconds, Before);
	return true;
}
#endif
