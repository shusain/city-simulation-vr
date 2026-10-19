#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityTerrain.h"

namespace
{
	uint64 TerrainDigest(const FCityTerrainHeightfield& Terrain)
	{
		uint64 Digest = 14695981039346656037ull;
		auto Add = [&](int32 Value)
		{
			for (int32 Shift = 0; Shift < 32; Shift += 8)
				Digest = (Digest ^ static_cast<uint8>(static_cast<uint32>(Value) >> Shift)) * 1099511628211ull;
		};
		for (int32 Value : Terrain.GetQuantizedHeightsCm())
			Add(Value);
		for (auto Value : Terrain.GetWaterClasses())
			Add(static_cast<int32>(Value));
		for (int32 Y = 0; Y < 64; ++Y)
			for (int32 X = 0; X < 64; ++X)
			{
				Add(Terrain.GetWaterSurfaceHeightCm({X, Y}));
				Add(Terrain.GetGroundwaterYieldPercent({X, Y}));
			}
		for (auto Cell : Terrain.GetRiverCells())
		{
			Add(Cell.X);
			Add(Cell.Y);
		}
		auto Entrance = Terrain.GetParameters().GetEntranceCell();
		Add(Entrance.X);
		Add(Entrance.Y);
		return Digest;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainLegacyDigestTest, "CitySim.Dimensions.LegacyTerrainDigests",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTerrainLegacyDigestTest::RunTest(const FString& Parameters)
{
	// Frozen pre-migration native results; never refresh to hide geometry drift.
	const uint64 Expected[3][7] = {
	    {3354536880650876349ull, 7646260402483297633ull, 2707877971381535293ull, 6304681757655162852ull,
	     4096624025099556675ull, 5131010441353212223ull, 10215446224902864807ull},
	    {14554388149802893528ull, 2383098345204703953ull, 4483246959442876177ull, 3212079955455095034ull,
	     17827469243933306862ull, 504105420145651714ull, 13323974802992485764ull},
	    {916339188367827548ull, 2677371077328223479ull, 10866815369164961833ull, 15127578565893775171ull,
	     15054214168589745147ull, 7559441988248441462ull, 13323974802992485764ull}};
	const int32 Seeds[] = {7429, 20261011, 100, 101, 102, 103, 107};
	for (int32 Version = 0; Version <= 2; ++Version)
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Seeds); ++Index)
		{
			const int32 Seed = Seeds[Index];
			FCityTerrainGenerationParameters Settings;
			Settings.GeneratorVersion = Version;
			Settings.Seed = Seed;
			FCityTerrainHeightfield Terrain;
			if (!TestTrue(TEXT("legacy terrain generated"), Terrain.Generate(Settings)))
				return false;
			UE_LOG(LogTemp, Display, TEXT("CITYSIM_TERRAIN_DIGEST Version=%d Seed=%d Rotation=%d Digest=%llu"), Version,
			       Seed, Settings.GetMapRotation(), TerrainDigest(Terrain));
			TestEqual(FString::Printf(TEXT("Frozen terrain version%d seed%d"), Version, Seed), TerrainDigest(Terrain),
			          Expected[Version][Index]);
		}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainCoexistingDimensionsTest, "CitySim.Dimensions.CoexistingTerrain",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTerrainCoexistingDimensionsTest::RunTest(const FString& Parameters)
{
	FCityTerrainGenerationParameters Settings;
	FCityTerrainHeightfield Legacy, Expanded;
	TestTrue(TEXT("legacy generated"), Legacy.Generate(Settings));
	const uint64 LegacyHash = TerrainDigest(Legacy);
	FCityPlacementModel LegacyCity, ExpandedCity(100000, FCityGridDimensions(80, 80));
	TestTrue(TEXT("matching legacy terrain assigned"), LegacyCity.SetTerrain(&Legacy));
	for (int32 Version = 0; Version <= 2; ++Version)
		for (int32 Seed : {100, 102, 107, 7429})
		{
			Settings.Dimensions = FCityGridDimensions(80, 80);
			Settings.GeneratorVersion = Version;
			Settings.Seed = Seed;
			TestTrue(TEXT("expanded terrain generated"), Expanded.Generate(Settings));
			TestEqual(TEXT("expanded samples span the full boundary"), Expanded.GetQuantizedHeightsCm().Num(),
			          161 * 161);
			TestEqual(TEXT("expanded hydrology has 6400 cells"), Expanded.GetWaterClasses().Num(), 6400);
			const auto Entry = Settings.GetEntranceCell();
			TestTrue(TEXT("expanded entrance lies on the expanded boundary"),
			         Settings.Dimensions.IsValidCell(Entry) &&
			             (Entry.X == 0 || Entry.Y == 0 || Entry.X == 79 || Entry.Y == 79));
			TestTrue(TEXT("expanded entrance remains dry and buildable"), Expanded.IsCellBuildable(Entry));
			for (const auto Cell : Expanded.GetRiverCells())
				TestTrue(TEXT("rotated river stays inside its own city"), Settings.Dimensions.IsValidCell(Cell));
			TestEqual(TEXT("expanded vertex queries use the final sample"), Expanded.GetVertexHeightCm(80, 80),
			          Expanded.GetQuantizedHeightsCm().Last());
			TestFalse(TEXT("outside expanded terrain cannot build"), Expanded.IsCellBuildable({80, 0}));
			TestTrue(TEXT("matching expanded terrain assigned"), ExpandedCity.SetTerrain(&Expanded));
			TestEqual(TEXT("placement entrance uses expanded terrain"), ExpandedCity.GetEntranceCell(), Entry);
			TestFalse(TEXT("foreign terrain rejected"), LegacyCity.SetTerrain(&Expanded));
			TestTrue(TEXT("rejected assignment preserves prior terrain"), LegacyCity.GetTerrain() == &Legacy);
			TestEqual(TEXT("other city terrain remains exact"), TerrainDigest(Legacy), LegacyHash);
		}
	Settings.GeneratorVersion = 0;
	Settings.BroadElevationAmplitudeCm = Settings.HillAmplitudeCm = Settings.RidgeAmplitudeCm = 0;
	TestTrue(TEXT("flat expanded span fixture generated"), Expanded.Generate(Settings));
	FCityPlacementModel RoadCity(100000, FCityGridDimensions(80, 80));
	TestTrue(TEXT("expanded span terrain assigned"), RoadCity.SetTerrain(&Expanded));
	auto Road = RoadCity.BeginRoadPreview({65, 5});
	RoadCity.UpdateRoadPreviewEnd(Road, {79, 5});
	Road.Profile = ECityRoadProfile::Elevated;
	TestTrue(TEXT("raised span fits entirely beyond legacy boundary"), RoadCity.CommitRoad(Road).IsSuccess());
	FString Error;
	TestTrue(TEXT("expanded raised geometry validates against terrain"), RoadCity.ValidateRoadStructures(Error));
	const auto Before = Expanded.GetQuantizedHeightsCm();
	Settings.Dimensions = FCityGridDimensions(MAX_int32, MAX_int32);
	TestFalse(TEXT("malformed terrain dimensions rejected before mutation"), Expanded.Generate(Settings));
	TestTrue(TEXT("failed generation retains valid surface"), Expanded.GetQuantizedHeightsCm() == Before);
	return true;
}

#endif
