#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Presentation/CityAtmosphereMotion.h"
#include "Presentation/CityWeatherCoverage.h"
#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWeatherCoveragePatternTest, "CitySim.Weather.CoveragePatterns",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWeatherCoveragePatternTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters P;
	P.Seed = 7429;
	P.GeneratorVersion = 2;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(P);
	const uint32 GeometryBefore = FCrc::MemCrc32(Terrain.GetQuantizedHeightsCm().GetData(),
	                                             Terrain.GetQuantizedHeightsCm().Num() * sizeof(int32));
	double MinSnow = 1.0, MaxSnow = 0.0, MinWet = 1.0, MaxWet = 0.0, MaxNeighborChange = 0.0;
	int32 ChangedSeeds = 0, PartialSnow = 0, BareSnow = 0;
	for (int32 Y = 0; Y <= 64; ++Y)
		for (int32 X = 0; X <= 64; ++X)
		{
			const FVector2D Cell(X, Y);
			const FVector Normal = Terrain.GetSurfaceNormal(X, Y);
			const FVector2D A = FCityWeatherCoverage::Sample(Terrain, Cell, Normal);
			const FVector2D B = FCityWeatherCoverage::Sample(Terrain, Cell, Normal);
			TestTrue(TEXT("same seed/position produces identical coverage, including shared chunk edges"), A == B);
			TestTrue(TEXT("cosmetic channels remain finite and bounded"), A.X >= 0 && A.X <= 1 && A.Y >= 0 && A.Y <= 1);
			const FVector2D Other = FCityWeatherCoverage::Evaluate(
			    P.Seed + 1, Cell, Terrain.SampleHeightCm(X, Y) - Terrain.GetSeaLevelCm(), Normal);
			ChangedSeeds += !A.Equals(Other, .01);
			MinSnow = FMath::Min(MinSnow, A.X);
			MaxSnow = FMath::Max(MaxSnow, A.X);
			MinWet = FMath::Min(MinWet, A.Y);
			MaxWet = FMath::Max(MaxWet, A.Y);
			const double Coverage = FCityWeatherCoverage::SnowAmount(.45, A.X);
			PartialSnow += Coverage > .05 && Coverage < .8;
			BareSnow += Coverage == 0;
			const FVector2D Nearby = FCityWeatherCoverage::Evaluate(
			    P.Seed, Cell + FVector2D(.01, .01), Terrain.SampleHeightCm(X, Y) - Terrain.GetSeaLevelCm(), Normal);
			MaxNeighborChange = FMath::Max(MaxNeighborChange, (Nearby - A).Size());
		}
	TestTrue(TEXT("snow and wet treatment vary visibly across the board"),
	         MaxSnow - MinSnow > .3 && MaxWet - MinWet > .3);
	TestTrue(TEXT("seed changes pattern"), ChangedSeeds > 1000);
	TestTrue(TEXT("partial accumulation has covered patches and exposed terrain"), PartialSnow > 100 && BareSnow > 100);
	TestTrue(TEXT("patch interpolation has no seams at cell/chunk boundaries"), MaxNeighborChange < .02);
	TestEqual(TEXT("cosmetic sampling leaves saved terrain geometry untouched"),
	          FCrc::MemCrc32(Terrain.GetQuantizedHeightsCm().GetData(),
	                         Terrain.GetQuantizedHeightsCm().Num() * sizeof(int32)),
	          GeometryBefore);
	const FVector2D Low = FCityWeatherCoverage::Evaluate(3, {12, 18}, 100, FVector::UpVector);
	const FVector2D High = FCityWeatherCoverage::Evaluate(3, {12, 18}, 3400, FVector::UpVector);
	const FVector2D Steep = FCityWeatherCoverage::Evaluate(3, {12, 18}, 3400, FVector(0, .8, .6));
	TestTrue(TEXT("higher land retains more snow, lowlands retain more wetness"), High.X > Low.X && Low.Y > High.Y);
	TestTrue(TEXT("steep exposed faces retain less snow and wetness"), Steep.X < High.X && Steep.Y < High.Y);
	TestTrue(TEXT("cool north-facing slope retains more snow"),
	         FCityWeatherCoverage::Evaluate(3, {12, 18}, 3400, FVector(0, -.8, .6)).X > Steep.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWeatherCoverageTransitionTest, "CitySim.Weather.CoverageTransitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWeatherCoverageTransitionTest::RunTest(const FString& Parameters)
{
	for (const double Retention : {.0, .25, .5, .75, 1.0})
	{
		TestEqual(TEXT("clear weather contributes no snow or wet tint"),
		          FCityWeatherCoverage::SnowAmount(0, Retention) + FCityWeatherCoverage::WetAmount(0, Retention), 0.0);
		double Previous = 0.0;
		for (int32 Step = 0; Step <= 100; ++Step)
		{
			const double Amount = FCityWeatherCoverage::SnowAmount(Step / 100.0, Retention);
			TestTrue(TEXT("accumulation grows monotonically and remains below bounded tint cap"),
			         Amount >= Previous && Amount <= .82);
			TestTrue(TEXT("small weather changes preserve gradual material transitions"), Amount - Previous <= .018);
			Previous = Amount;
		}
	}
	FCityEnvironmentSnapshot Weather;
	Weather.Season = ECitySeason::Winter;
	Weather.Temperature = ECityTemperatureBand::Cold;
	Weather.bPrecipitation = true;
	FCityAtmosphereMotion Motion;
	Motion.Advance(1, Weather);
	const double First = FCityWeatherCoverage::SnowAmount(Motion.SnowCover, .8);
	Motion.Advance(20, Weather);
	const double Accumulated = FCityWeatherCoverage::SnowAmount(Motion.SnowCover, .8);
	TestTrue(TEXT("existing visual weather clock accumulates terrain snow gradually"),
	         Motion.SnowCover < 1 && Accumulated > First);
	Weather.bPrecipitation = false;
	Weather.Season = ECitySeason::Summer;
	Weather.Temperature = ECityTemperatureBand::Hot;
	Motion.Advance(5, Weather);
	TestTrue(TEXT("warming melts coverage instead of switching all terrain instantly"),
	         FCityWeatherCoverage::SnowAmount(Motion.SnowCover, .8) < Accumulated && Motion.SnowCover > 0);
	Motion.Advance(600, Weather);
	TestEqual(TEXT("settled clear weather removes snow mask"), FCityWeatherCoverage::SnowAmount(Motion.SnowCover, .8),
	          0.0);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWeatherCoverageMaterialTest, "CitySim.Weather.CoverageMaterialContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWeatherCoverageMaterialTest::RunTest(const FString& Parameters)
{
	auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/CitySim/Art/Materials/M_CityTerrain.M_CityTerrain"));
	if (!TestNotNull(TEXT("terrain material exists"), Material))
		return false;
	auto* Surface = Cast<UMaterialExpressionLinearInterpolate>(Material->GetEditorOnlyData()->BaseColor.Expression);
	if (!TestNotNull(TEXT("placement grid remains the final color blend outside weather treatment"), Surface))
		return false;
	TestTrue(TEXT("grid and weather branches remain connected"),
	         Surface->A.Expression && Surface->B.Expression && Surface->Alpha.Expression);
	TSet<UMaterialExpression*> Visited;
	TArray<UMaterialExpression*> Pending;
	Pending.Add(Surface->B.Expression);
	TSet<FName> BoundParameters;
	bool bRetentionBound = false;
	while (!Pending.IsEmpty())
	{
		UMaterialExpression* Node = Pending.Pop(EAllowShrinking::No);
		if (!Node || Visited.Contains(Node))
			continue;
		Visited.Add(Node);
		if (const auto* Parameter = Cast<UMaterialExpressionScalarParameter>(Node))
			BoundParameters.Add(Parameter->ParameterName);
		if (const auto* Coordinates = Cast<UMaterialExpressionTextureCoordinate>(Node))
			bRetentionBound |= Coordinates->CoordinateIndex == 1;
		for (FExpressionInputIterator Input(Node); Input; ++Input)
			Pending.Add(Input->Expression);
	}
	TestTrue(TEXT("seeded UV1 retention reaches rendered terrain output"), bRetentionBound);
	TestTrue(TEXT("both existing visual weather uniforms reach terrain output"),
	         BoundParameters.Contains(TEXT("WeatherSnow")) && BoundParameters.Contains(TEXT("WeatherWet")));
	TestNotNull(TEXT("weather material roughness is bound"), Material->GetEditorOnlyData()->Roughness.Expression);
	return true;
}
#endif
#endif
