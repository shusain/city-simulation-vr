#include "Interaction/CityAreaSelection.h"

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAreaSelectionTest, "CitySim.Input.AreaSelectionAndZoneRectangle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityAreaSelectionTest::RunTest(const FString& Parameters)
{
	const FCityAreaSelection Forward =
	    CityAreaSelection::MakeInclusiveRectangle(FCityCellCoord(2, 3), FCityCellCoord(5, 7));
	const FCityAreaSelection Reverse =
	    CityAreaSelection::MakeInclusiveRectangle(FCityCellCoord(5, 7), FCityCellCoord(2, 3));
	TestEqual(TEXT("inclusive anchor X"), Forward.Anchor.X, 2);
	TestEqual(TEXT("inclusive anchor Y"), Forward.Anchor.Y, 3);
	TestEqual(TEXT("inclusive width"), Forward.Size.X, 4);
	TestEqual(TEXT("inclusive height"), Forward.Size.Y, 5);
	TestEqual(TEXT("inclusive cell count"), Forward.GetCellCount(), 20);
	TestEqual(TEXT("reverse drag keeps anchor X"), Reverse.Anchor.X, Forward.Anchor.X);
	TestEqual(TEXT("reverse drag keeps anchor Y"), Reverse.Anchor.Y, Forward.Anchor.Y);
	TestEqual(TEXT("reverse drag keeps width"), Reverse.Size.X, Forward.Size.X);
	TestEqual(TEXT("reverse drag keeps height"), Reverse.Size.Y, Forward.Size.Y);

	TArray<FCityCellCoord> Cells;
	CityAreaSelection::BuildCells(Forward, Cells);
	TestEqual(TEXT("preview produces every rectangle cell"), Cells.Num(), 20);
	TestTrue(TEXT("preview contains first corner"), Cells.Contains(FCityCellCoord(2, 3)));
	TestTrue(TEXT("preview contains opposite corner"), Cells.Contains(FCityCellCoord(5, 7)));

	FCityPlacementModel City(1000);
	TestTrue(TEXT("empty rectangle validates"), City.CanSetZoneArea(Forward.Anchor, Forward.Size));
	TestTrue(TEXT("valid rectangle applies atomically"),
	         City.SetZoneArea(Forward.Anchor, Forward.Size, ECityZoneType::Residential));
	for (const FCityCellCoord& Cell : Cells)
	{
		TestEqual(TEXT("every selected cell is zoned"), City.GetZone(Cell), ECityZoneType::Residential);
	}

	FCityBuildingDefinition Building;
	Building.DefinitionId = TEXT("Blocker");
	Building.DisplayName = TEXT("Blocker");
	Building.Footprint.Width = 1;
	Building.Footprint.Height = 1;
	Building.Cost = 1;
	FCityPlacementPreview Preview = City.BeginPreview(Building, FCityCellCoord(10, 10));
	TestTrue(TEXT("blocking building commits"), City.CommitPlacement(Preview, Building).IsSuccess());
	FCityRoadPreview RoadPreview = City.BeginRoadPreview(FCityCellCoord(11, 10));
	City.UpdateRoadPreviewEnd(RoadPreview, FCityCellCoord(11, 10));
	TestTrue(TEXT("blocking road commits"), City.CommitRoad(RoadPreview).IsSuccess());
	const FCityCellCoord BlockedAnchor(9, 9);
	const FIntPoint BlockedSize(3, 3);
	TestTrue(TEXT("rectangle containing an occupied cell keeps eligible cells"),
	         City.CanSetZoneArea(BlockedAnchor, BlockedSize));
	TestTrue(TEXT("rectangle applies around occupied cells"),
	         City.SetZoneArea(BlockedAnchor, BlockedSize, ECityZoneType::Commercial));
	TestEqual(TEXT("cell before blocker is zoned"), City.GetZone(FCityCellCoord(9, 9)), ECityZoneType::Commercial);
	TestEqual(TEXT("cell after blocker is zoned"), City.GetZone(FCityCellCoord(11, 11)), ECityZoneType::Commercial);
	TestEqual(TEXT("occupied cell is not repainted"), City.GetZone(FCityCellCoord(10, 10)), ECityZoneType::None);
	TestEqual(TEXT("road cell is not zoned"), City.GetZone(FCityCellCoord(11, 10)), ECityZoneType::None);
	TestFalse(TEXT("single occupied cell has no zoning target"),
	          City.CanSetZoneArea(FCityCellCoord(10, 10), FIntPoint(1, 1)));
	TestFalse(TEXT("single occupied cell remains unchanged"),
	          City.SetZone(FCityCellCoord(10, 10), ECityZoneType::Residential));
	return true;
}

#endif
