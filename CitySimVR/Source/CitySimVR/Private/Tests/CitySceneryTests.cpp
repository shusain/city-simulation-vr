#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityScenery.h"
#include "Persistence/CityPersistence.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityScenerySuitabilityTest, "CitySim.Presentation.TerrainScenerySuitability",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityScenerySuitabilityTest::RunTest(const FString& Parameters)
{
	FCityScenerySample Lowland;
	Lowland.WarmthPercent = 55;
	FCityScenerySample Mountain = Lowland;
	Mountain.MountainAffinityPercent = 100;
	FCityScenerySample Coast = Lowland;
	Coast.ShoreDistanceCells = 1;
	Coast.WarmthPercent = 80;
	FCityScenerySample ColdCoast = Coast;
	ColdCoast.WarmthPercent = 30;
	int32 LowlandPines = 0, MountainPines = 0, WarmPalms = 0;
	for (uint32 Choice = 0; Choice < 1000; ++Choice)
	{
		LowlandPines += FCitySceneryField::SelectKind(Lowland, Choice) == ECitySceneryKind::Conifer;
		MountainPines += FCitySceneryField::SelectKind(Mountain, Choice) == ECitySceneryKind::Conifer;
		WarmPalms += FCitySceneryField::SelectKind(Coast, Choice) == ECitySceneryKind::Palm;
		TestTrue(TEXT("cold shore does not imply tropical vegetation"),
		         FCitySceneryField::SelectKind(ColdCoast, Choice) != ECitySceneryKind::Palm);
	}
	TestTrue(TEXT("mountain proximity strongly favors pines"), MountainPines > LowlandPines * 5);
	TestTrue(TEXT("warm low shores favor palm variety"), WarmPalms > 500);
	Mountain.Slope = ECityTerrainSlope::Steep;
	TestTrue(TEXT("exposed steep terrain uses rocks"),
	         FCitySceneryField::SelectKind(Mountain, 0) == ECitySceneryKind::Rock);
	TSet<int32> Climates;
	int32 Palms = 0, MountainTrees = 0, MountainPinesTotal = 0;
	bool bWarmFixture = false, bCoolFixture = false, bMountainFixture = false;
	for (int32 Seed = 1; Seed <= 96; ++Seed)
	{
		FCityTerrainGenerationParameters P;
		P.GeneratorVersion = 2;
		P.SceneryVersion = 1;
		P.Seed = Seed;
		FCityTerrainHeightfield Terrain, Reloaded;
		Terrain.Generate(P);
		Reloaded.Generate(P);
		const auto Heights = Terrain.GetQuantizedHeightsCm();
		const auto Water = Terrain.GetWaterClasses();
		FCitySceneryField Field;
		Field.Build(Terrain);
		const auto First = FCitySceneryField::SelectCandidates(Terrain, P.GetEntranceCell(), 420, 100);
		const auto Second = FCitySceneryField::SelectCandidates(Reloaded, P.GetEntranceCell(), 420, 100);
		TestTrue(TEXT("reload reproduces exact scenery species and candidates"), First == Second);
		TestTrue(TEXT("scenery cannot change terrain or water"),
		         Heights == Terrain.GetQuantizedHeightsCm() && Water == Terrain.GetWaterClasses());
		Climates.Add(FCitySceneryField::ClimateWarmthPercent(Seed));
		int32 Trees = 0, Rocks = 0, SeedPalms = 0, SeedMountainPines = 0, ShoreCells = 0, HighlandCells = 0;
		for (int32 Y = 0; Y < 64; ++Y)
			for (int32 X = 0; X < 64; ++X)
			{
				const auto& Sample = Field.GetSample({X, Y});
				TestTrue(TEXT("seeded warmth is bounded"), Sample.WarmthPercent >= 0 && Sample.WarmthPercent <= 100);
				ShoreCells += Terrain.GetWaterClass({X, Y}) == ECityWaterClass::Land && Sample.ShoreDistanceCells <= 3;
				HighlandCells += Terrain.GetWaterClass({X, Y}) == ECityWaterClass::Land && Sample.ElevationCm >= 2000;
			}
		for (const auto& Candidate : First)
		{
			const auto& Sample = Field.GetSample(Candidate.Cell);
			Trees += Candidate.Kind != ECitySceneryKind::Rock;
			Rocks += Candidate.Kind == ECitySceneryKind::Rock;
			TestTrue(TEXT("all scenery stays on dry land"),
			         Terrain.GetWaterClass(Candidate.Cell) == ECityWaterClass::Land);
			if (Candidate.Kind == ECitySceneryKind::Palm)
			{
				++Palms;
				++SeedPalms;
				TestTrue(TEXT("palms require actual warm low shore suitability"),
				         Sample.WarmthPercent >= 65 && Sample.ShoreDistanceCells <= 3 && Sample.ElevationCm <= 1000 &&
				             Sample.Slope != ECityTerrainSlope::Steep);
			}
			if (Sample.MountainAffinityPercent >= 70 && Candidate.Kind != ECitySceneryKind::Rock)
			{
				++MountainTrees;
				MountainPinesTotal += Candidate.Kind == ECitySceneryKind::Conifer;
				SeedMountainPines += Candidate.Kind == ECitySceneryKind::Conifer;
			}
		}
		TestTrue(TEXT("all species share the unchanged tree and rock budgets"), Trees <= 420 && Rocks <= 100);
		auto WriteFixture = [&](const FString& Profile)
		{
			FCitySaveDocument Doc;
			Doc.Terrain = P;
			Doc.Placement = FCityPlacementModel(10000).ExportState();
			Doc.bSimulationPaused = true;
			Doc.Identity.Name = Profile + TEXT(" scenery preview");
			FString Json, Error;
			TestTrue(TEXT("isolated scenery fixture serializes"), FCityPersistence::Serialize(Doc, Json, Error));
			TestTrue(TEXT("fixture writes outside player saves"),
			         FCityPersistence::WriteAtomic(
			             FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Artifacts/Preview/Scenery-") +
			                                               Profile + TEXT(".json")),
			             Json, Error));
		};
		if (!bWarmFixture && SeedPalms >= 5)
		{
			WriteFixture(TEXT("WarmCoast"));
			bWarmFixture = true;
		}
		if (!bCoolFixture && FCitySceneryField::ClimateWarmthPercent(Seed) == 30 && ShoreCells > 20)
		{
			WriteFixture(TEXT("CoolCoast"));
			bCoolFixture = true;
		}
		// Rolling hills can also have mountain-affinity pines. The visual mountain
		// fixture must contain a real massif, not merely a few high hill cells.
		if (!bMountainFixture && P.DescribeLandscape().StartsWith(TEXT("Mountain")) && HighlandCells > 0 && Rocks > 0)
		{
			WriteFixture(TEXT("Mountains"));
			bMountainFixture = true;
			AddInfo(FString::Printf(
			    TEXT("Mountain scenery fixture seed%d: %d highland cells, %d foothill pines, %d exposed rocks"), Seed,
			    HighlandCells, SeedMountainPines, Rocks));
		}
		P.SceneryVersion = 0;
		Terrain.Generate(P);
		TestTrue(TEXT("scenery version cannot move saved terrain or hydrology"),
		         Heights == Terrain.GetQuantizedHeightsCm() && Water == Terrain.GetWaterClasses());
		const auto Legacy = FCitySceneryField::SelectCandidates(Terrain, P.GetEntranceCell(), 420, 100);
		for (const auto& Candidate : Legacy)
		{
			const bool bRock = Terrain.GetCellSlope(Candidate.Cell) == ECityTerrainSlope::Steep ||
			                   Terrain.GetCellCenterHeightCm(Candidate.Cell) > 1300;
			TestTrue(TEXT("legacy scenery retains original species rule"),
			         Candidate.Kind == (bRock                     ? ECitySceneryKind::Rock
			                            : Candidate.Hash % 3 == 0 ? ECitySceneryKind::Conifer
			                                                      : ECitySceneryKind::Broadleaf));
		}
	}
	TestEqual(TEXT("seed sample includes cool temperate and warm climates"), Climates.Num(), 3);
	TestTrue(TEXT("generated warm coasts contain palms"), Palms > 20 && bWarmFixture && bCoolFixture);
	TestTrue(TEXT("generated mountain foothills predominantly use pines"),
	         MountainTrees > 0 && MountainPinesTotal * 100 > MountainTrees * 65);
	TestTrue(TEXT("representative mountain massif fixture generated"), bMountainFixture);
	AddInfo(
	    FString::Printf(TEXT("96 seeds: %d palms; %d/%d mountain-affinity trees are pines; tree cap420 rock cap100"),
	                    Palms, MountainPinesTotal, MountainTrees));
	return true;
}
#endif
