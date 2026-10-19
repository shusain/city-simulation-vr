#if WITH_DEV_AUTOMATION_TESTS

#include "Interaction/CityBoardPicking.h"
#include "Misc/AutomationTest.h"

namespace
{
	bool PickLocalPoint(const FVector& LocalPoint, FCityCellCoord& OutCell)
	{
		return CityBoardPicking::PickCellFromWorldRay(FTransform::Identity, LocalPoint + FVector(0.0, 0.0, 1000.0),
		                                              FVector(0.0, 0.0, -1.0), OutCell);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityBoardPickingTest, "CitySim.Board.Picking",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityBoardPickingTest::RunTest(const FString& Parameters)
{
	FCityCellCoord Cell;
	TestTrue(TEXT("lower-left corner picks"), PickLocalPoint(FVector(0.0, 0.0, 0.0), Cell));
	TestEqual(TEXT("lower-left is cell 0,0"), Cell, FCityCellCoord(0, 0));

	const double LastPoint = CityBoardPicking::LogicalBoardSizeCm - 0.01;
	TestTrue(TEXT("upper-right interior picks"), PickLocalPoint(FVector(LastPoint, LastPoint, 0.0), Cell));
	TestEqual(TEXT("upper-right interior is cell 63,63"), Cell, FCityCellCoord(63, 63));

	TestFalse(TEXT("negative edge is outside"), PickLocalPoint(FVector(-0.01, 0.0, 0.0), Cell));
	TestFalse(TEXT("far edge is half-open and outside"),
	          PickLocalPoint(FVector(CityBoardPicking::LogicalBoardSizeCm, 0.0, 0.0), Cell));
	TestFalse(TEXT("parallel ray is rejected"),
	          CityBoardPicking::PickCellFromWorldRay(FTransform::Identity, FVector(100.0, 100.0, 100.0),
	                                                 FVector(1.0, 0.0, 0.0), Cell));

	const FTransform BoardTransform(FRotator(0.0, 37.0, 0.0), FVector(120.0, -75.0, 90.0), FVector(0.01));
	const FCityCellCoord ExpectedCell(17, 42);
	const FVector WorldCellCenter =
	    BoardTransform.TransformPosition(CityBoardPicking::GetCellCenterLocal(ExpectedCell));
	const FVector WorldNormal = BoardTransform.TransformVectorNoScale(FVector::UpVector);
	FVector WorldHit;
	TestTrue(TEXT("translated rotated scaled board picks"),
	         CityBoardPicking::PickCellFromWorldRay(BoardTransform, WorldCellCenter + WorldNormal * 100.0, -WorldNormal,
	                                                Cell, &WorldHit));
	TestEqual(TEXT("transform does not change logical cell"), Cell, ExpectedCell);
	TestTrue(TEXT("reported world hit is board cell center"), WorldHit.Equals(WorldCellCenter, 0.01));

	const FTransform SandboxBoardTransform(FRotator::ZeroRotator, FVector(-100.0, 0.0, 100.4), FVector(0.00390625));
	const FVector DesktopViewOrigin(0.0, -250.0, 250.0);
	const FVector DesktopViewDirection = FRotator(-25.0, 90.0, 0.0).Vector();
	TestTrue(
	    TEXT("desktop center ray reaches the visible board"),
	    CityBoardPicking::PickCellFromWorldRay(SandboxBoardTransform, DesktopViewOrigin, DesktopViewDirection, Cell));
	TestEqual(TEXT("desktop center ray resolves expected cell"), Cell, FCityCellCoord(32, 22));
	return true;
}

#endif
