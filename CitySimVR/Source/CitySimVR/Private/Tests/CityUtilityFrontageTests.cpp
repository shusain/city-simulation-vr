#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUtilityAvenueFrontageTest, "CitySim.Utilities.GroundAvenueEntranceFrontage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityUtilityAvenueFrontageTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Actual utility catalog is available"), Catalog))
		return false;
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& Definition : Catalog->Definitions)
		Definitions.Add(Definition.DefinitionId, Definition);
	TSet<int32> Edges;
	int32 Cases = 0;
	for (int32 Seed = 7429; Seed < 7461; ++Seed)
	{
		FCityTerrainGenerationParameters Params;
		Params.GeneratorVersion = 2;
		Params.Seed = Seed;
		Params.WaterRulesVersion = Params.SceneryVersion = 1;
		FCityTerrainHeightfield Terrain;
		if (!TestTrue(TEXT("Generated terrain is valid"), Terrain.Generate(Params)))
			return false;
		const auto Entry = Params.GetEntranceCell();
		const FIntPoint Inward = Entry.X == 0    ? FIntPoint(1, 0)
		                         : Entry.X == 63 ? FIntPoint(-1, 0)
		                         : Entry.Y == 0  ? FIntPoint(0, 1)
		                                         : FIntPoint(0, -1);
		Edges.Add(Params.GetMapRotation());
		for (const FName Name : {FName(TEXT("Power")), FName(TEXT("Service")), FName(TEXT("WaterTower"))})
			for (const int32 Side : {-1, 1})
			{
				const FString Context = FString::Printf(TEXT("seed%d %s side%d"), Seed, *Name.ToString(), Side);
				const auto* Definition = Catalog->FindDefinition(Name);
				if (!TestNotNull(Context + TEXT(" catalog definition"), Definition))
					return false;
				FCityPlacementModel City(100000);
				City.SetTerrain(&Terrain);
				auto Road = City.BeginRoadPreview(Entry, TEXT("Avenue"), 30);
				City.UpdateRoadPreviewEnd(Road, {Entry.X + Inward.X * 8, Entry.Y + Inward.Y * 8});
				Road.Profile = ECityRoadProfile::Ground;
				if (!TestTrue(Context + TEXT(" ground avenue commits"), City.CommitRoad(Road).IsSuccess()))
					return false;
				const FCityCellCoord Frontage(Entry.X + Inward.X * 4, Entry.Y + Inward.Y * 4);
				const FIntPoint Offset(-Inward.Y * Side, Inward.X * Side);
				const int32 Turns = Offset.Y > 0 ? 0 : Offset.X < 0 ? 1 : Offset.Y < 0 ? 2 : 3;
				const FIntPoint Size = Definition->Footprint.GetRotatedSize(Turns);
				// Centre the current plot along the nine-tile avenue, including six-tile water frontage.
				const FCityCellCoord Anchor(Frontage.X + (Offset.X > 0   ? 1
				                                          : Offset.X < 0 ? -Size.X
				                                                         : -Size.X / 2),
				                            Frontage.Y + (Offset.Y > 0   ? 1
				                                          : Offset.Y < 0 ? -Size.Y
				                                                         : -Size.Y / 2));
				auto Preview = City.BeginRoadFacingPreview(*Definition, Anchor);
				const auto Placed = City.CommitPlacement(Preview, *Definition);
				if (!TestTrue(Context + TEXT(" adjacent utility commits"), Placed.IsSuccess()))
					return false;
				++Cases;
				TestEqual(Context + TEXT(" entrance faces avenue"), Preview.QuarterTurnsClockwise, Turns);
				auto CheckAccess = [&](const FCityPlacementModel& Model)
				{
					TestTrue(Context + TEXT(" avenue reaches incoming city connection"),
					         Model.IsRoadConnected(Frontage));
					TestTrue(Context + TEXT(" ground avenue permits frontage"),
					         Model.SupportsBuildingFrontage(Frontage));
					TestTrue(Context + TEXT(" utility has road access"), Model.HasRoadAccess(Placed.EntityId));
					FCityCellCoord Portal;
					TestTrue(Context + TEXT(" entrance supports actual freight portal"),
					         FCityTrafficSimulation::FindPortal(Model, Catalog, Placed.EntityId, Portal));
					const auto Services = FCityServiceEvaluator::BuildSnapshot(Model, FCityEconomyTuning(), Catalog);
					TestTrue(Context + TEXT(" utility does not report NoRoad"),
					         Services.GetFailure(Placed.EntityId) != ECityOperationFailure::NoRoad);
				};
				CheckAccess(City);
				FString Error;
				auto Reloaded = FCityPlacementModel::CreateValidatedFromState(City.ExportState(), Definitions, Error);
				if (!TestTrue(Context + TEXT(" placement state reloads: ") + Error, Reloaded.IsValid()))
					return false;
				Reloaded->SetTerrain(&Terrain);
				CheckAccess(*Reloaded);
			}
	}
	TestEqual(TEXT("Every entrance edge is covered"), Edges.Num(), 4);
	TestEqual(TEXT("Both avenue sides and all three reported utilities are covered"), Cases, 192);
	AddInfo(FString::Printf(TEXT("Ground avenue utility cases=%d, before and after reload; entrance edges=%d"), Cases,
	                        Edges.Num()));
	return true;
}
#endif
