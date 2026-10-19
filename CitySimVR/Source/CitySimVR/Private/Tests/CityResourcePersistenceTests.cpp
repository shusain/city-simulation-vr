#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityScenery.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGroundwaterRepeatabilityTest,
                                 "CitySim.Water.GroundwaterVariationAndFrozenGeometry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityGroundwaterRepeatabilityTest::RunTest(const FString& Parameters)
{
	TSet<int32> SiteYields;
	int32 LargestSpatialRange = 0;
	for (int32 Generator = 0; Generator <= 2; ++Generator)
		for (int32 Seed = 1; Seed <= 12; ++Seed)
		{
			FCityTerrainGenerationParameters P;
			P.GeneratorVersion = Generator;
			P.Seed = Seed;
			FCityTerrainHeightfield Legacy, Modern, Reloaded;
			TestTrue(TEXT("legacy terrain generates"), Legacy.Generate(P));
			P.WaterRulesVersion = 1;
			P.SceneryVersion = 1;
			TestTrue(TEXT("resource/scenery version terrain generates"), Modern.Generate(P));
			TestTrue(TEXT("resource rules leave all quantized heights and hydrology identical"),
			         Legacy.GetQuantizedHeightsCm() == Modern.GetQuantizedHeightsCm() &&
			             Legacy.GetWaterClasses() == Modern.GetWaterClasses() &&
			             Legacy.GetRiverCells() == Modern.GetRiverCells());
			FCitySaveDocument Doc, Read;
			Doc.Placement = FCityPlacementModel(10000).ExportState();
			Doc.Terrain = P;
			FString Json, Error;
			if (!TestTrue(TEXT("resource terrain document round-trips"),
			              FCityPersistence::Serialize(Doc, Json, Error) &&
			                  FCityPersistence::Deserialize(Json, Read, Error)))
				return false;
			TestTrue(TEXT("resource versions and seed persist exactly"), Read.Terrain == P);
			TestTrue(TEXT("saved resource terrain regenerates"), Reloaded.Generate(Read.Terrain));
			TestTrue(TEXT("round-trip geometry and water remain exact"),
			         Modern.GetQuantizedHeightsCm() == Reloaded.GetQuantizedHeightsCm() &&
			             Modern.GetWaterClasses() == Reloaded.GetWaterClasses());
			int32 Minimum = MAX_int32, Maximum = MIN_int32;
			for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
				for (int32 X = 0; X < FCityGrid::Width; ++X)
				{
					const FCityCellCoord Cell(X, Y);
					const int32 Yield = Modern.GetGroundwaterYieldPercent(Cell);
					TestTrue(TEXT("renewable groundwater yield stays within60..140percent"),
					         Yield >= 60 && Yield <= 140);
					TestEqual(TEXT("same saved seed/site reproduces yield"),
					          Reloaded.GetGroundwaterYieldPercent(Cell), Yield);
					TestEqual(TEXT("rule opt-in never changes terrain water surface"),
					          Modern.GetWaterSurfaceHeightCm(Cell), Legacy.GetWaterSurfaceHeightCm(Cell));
					Minimum = FMath::Min(Minimum, Yield);
					Maximum = FMath::Max(Maximum, Yield);
				}
			LargestSpatialRange = FMath::Max(LargestSpatialRange, Maximum - Minimum);
			SiteYields.Add(Modern.GetGroundwaterYieldPercent({17, 29}));
		}
	TestTrue(TEXT("water table varies materially between sites"), LargestSpatialRange >= 30);
	TestTrue(TEXT("same site has distinct groundwater yields across seeds"), SiteYields.Num() >= 5);
	AddInfo(FString::Printf(TEXT("36 frozen generator/seed combinations: groundwater largest spatial "
	                             "range%d; fixed-site distinct yields%d"),
	                        LargestSpatialRange, SiteYields.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityResourceRulePersistenceTest,
                                 "CitySim.Persistence.ResourceRules58AndLegacy57Migration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityResourceRulePersistenceTest::RunTest(const FString& Parameters)
{
	FCitySaveDocument Doc, Read;
	Doc.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	Doc.Placement = FCityPlacementModel(10000).ExportState();
	Doc.Terrain.GeneratorVersion = 2;
	Doc.Terrain.Seed = 17;
	Doc.Terrain.WaterRulesVersion = 1;
	Doc.Terrain.SceneryVersion = 1;
	FString Json, Error;
	if (!TestTrue(TEXT("schema58 versions save and reload"),
	              FCityPersistence::Serialize(Doc, Json, Error) &&
	                  FCityPersistence::Deserialize(Json, Read, Error)))
		return false;
	TestTrue(TEXT("both opt-in versions preserved"),
	         Read.SchemaVersion == FCitySaveDocument::CurrentSchemaVersion &&
	             Read.Terrain.WaterRulesVersion == 1 && Read.Terrain.SceneryVersion == 1 &&
	             Read.Terrain == Doc.Terrain);
	const FString ModernJson = Json;
	for (const FString& Field : {FString(TEXT("waterRulesVersion")), FString(TEXT("sceneryVersion"))})
	{
		for (const int32 Invalid : {-1, 2})
		{
			TSharedPtr<FJsonObject> Root;
			if (!TestTrue(TEXT("valid modern JSON parses"),
			              FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ModernJson), Root)))
				return false;
			Root->GetObjectField(TEXT("terrain"))->SetNumberField(Field, Invalid);
			FString Mutated;
			FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Mutated));
			TestFalse(TEXT("unknown or negative persisted rule version is rejected"),
			          FCityPersistence::Deserialize(Mutated, Read, Error));
		}
	}
	Doc.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	Doc.Terrain.WaterRulesVersion = 2;
	TestFalse(TEXT("unsupported water rule cannot serialize"), FCityPersistence::Serialize(Doc, Json, Error));
	Doc.Terrain.WaterRulesVersion = 0;
	Doc.Terrain.SceneryVersion = 2;
	TestFalse(TEXT("unsupported scenery rule cannot serialize"),
	          FCityPersistence::Serialize(Doc, Json, Error));
	return true;
}
#endif
