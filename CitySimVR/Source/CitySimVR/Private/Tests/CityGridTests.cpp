#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/CityGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGridBoundsTest, "CitySim.Grid.BoundsAndOccupancy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityGridBoundsTest::RunTest(const FString& Parameters)
{
	FCityGrid Grid;
	TestTrue(TEXT("lower-left is valid"), Grid.IsValidCell(FCityCellCoord(0, 0)));
	TestTrue(TEXT("upper-right is valid"), Grid.IsValidCell(FCityCellCoord(63, 63)));
	TestFalse(TEXT("negative X is invalid"), Grid.IsValidCell(FCityCellCoord(-1, 0)));
	TestFalse(TEXT("X 64 is invalid"), Grid.IsValidCell(FCityCellCoord(64, 0)));
	TestFalse(TEXT("Y 64 is invalid"), Grid.IsValidCell(FCityCellCoord(0, 64)));

	const FCityCellFootprint NonSquare(2, 3);
	TestEqual(TEXT("unrotated footprint size"), NonSquare.GetRotatedSize(0), FIntPoint(2, 3));
	TestEqual(TEXT("rotated footprint size"), NonSquare.GetRotatedSize(1), FIntPoint(3, 2));
	TestTrue(TEXT("rotated footprint fits at 61,62"), Grid.IsFootprintInBounds(FCityCellCoord(61, 62), NonSquare, 1));
	TestFalse(TEXT("rotated footprint exceeds X at 62,62"),
	          Grid.IsFootprintInBounds(FCityCellCoord(62, 62), NonSquare, 1));

	TestTrue(TEXT("empty footprint can be occupied"), Grid.CanOccupy(FCityCellCoord(10, 10), NonSquare));
	TestTrue(TEXT("valid entity occupies atomically"), Grid.TryOccupy(FCityCellCoord(10, 10), NonSquare, 42));
	TestEqual(TEXT("occupied cell stores entity"), Grid.GetOccupant(FCityCellCoord(11, 12)), uint64(42));
	TestFalse(TEXT("overlap is rejected"), Grid.TryOccupy(FCityCellCoord(11, 12), FCityCellFootprint(1, 1), 43));
	TestEqual(TEXT("failed overlap did not modify another cell"), Grid.GetOccupant(FCityCellCoord(12, 12)),
	          FCityGrid::EmptyEntityId);
	Grid.ClearEntity(42);
	TestEqual(TEXT("clear removes all entity cells"), Grid.GetOccupant(FCityCellCoord(11, 12)),
	          FCityGrid::EmptyEntityId);
	return true;
}

#endif
