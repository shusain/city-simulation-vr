#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"

namespace
{
	FCityBuildingDefinition MakeDefinition(const FName DefinitionId, const int32 Width, const int32 Height,
	                                       const int64 Cost)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = DefinitionId;
		Definition.DisplayName = DefinitionId.ToString();
		Definition.Footprint = FCityCellFootprint(Width, Height);
		Definition.Cost = Cost;
		Definition.LogicalHeightCm = 800.0;
		return Definition;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPlacementTransactionsTest, "CitySim.Placement.Transactions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityPlacementTransactionsTest::RunTest(const FString& Parameters)
{
	const FCityBuildingDefinition Home = MakeDefinition(TEXT("Home"), 2, 2, 200);
	FCityPlacementModel Model(1000);

	FCityPlacementPreview Preview = Model.BeginPreview(Home, FCityCellCoord(10, 10));
	const FCityPlacementResult ValidCommit = Model.CommitPlacement(Preview, Home);
	TestTrue(TEXT("valid placement commits"), ValidCommit.IsSuccess());
	TestEqual(TEXT("first entity ID is stable"), ValidCommit.EntityId, uint64(1));
	TestEqual(TEXT("all footprint cells are reported"), ValidCommit.ChangedCells.Num(), 4);
	TestEqual(TEXT("cost is deducted exactly once"), Model.GetFunds(), int64(800));
	for (int32 Y = 10; Y < 12; ++Y)
	{
		for (int32 X = 10; X < 12; ++X)
		{
			TestEqual(TEXT("every committed cell has the entity"), Model.GetGrid().GetOccupant(FCityCellCoord(X, Y)),
			          uint64(1));
		}
	}

	const FCityPlacementResult DoubleCommit = Model.CommitPlacement(Preview, Home);
	TestEqual(TEXT("double commit is rejected"), DoubleCommit.Failure, ECityPlacementFailure::NoActivePreview);
	TestEqual(TEXT("double commit does not charge twice"), Model.GetFunds(), int64(800));

	FCityPlacementPreview Overlap = Model.BeginPreview(Home, FCityCellCoord(11, 11));
	const FCityPlacementResult OverlapResult = Model.CommitPlacement(Overlap, Home);
	TestEqual(TEXT("overlap is rejected"), OverlapResult.Failure, ECityPlacementFailure::Occupied);
	TestEqual(TEXT("overlap failure leaves unrelated cell empty"), Model.GetGrid().GetOccupant(FCityCellCoord(12, 12)),
	          FCityGrid::EmptyEntityId);
	TestEqual(TEXT("overlap failure preserves funds"), Model.GetFunds(), int64(800));

	FCityPlacementPreview Outside = Model.BeginPreview(Home, FCityCellCoord(63, 63));
	const FCityPlacementResult OutsideResult = Model.CommitPlacement(Outside, Home);
	TestEqual(TEXT("out-of-bounds placement is rejected"), OutsideResult.Failure, ECityPlacementFailure::OutOfBounds);
	TestEqual(TEXT("out-of-bounds failure preserves funds"), Model.GetFunds(), int64(800));

	FCityPlacementModel PoorModel(100);
	FCityPlacementPreview TooExpensive = PoorModel.BeginPreview(Home, FCityCellCoord(1, 1));
	const FCityPlacementResult FundsResult = PoorModel.CommitPlacement(TooExpensive, Home);
	TestEqual(TEXT("insufficient funds are rejected"), FundsResult.Failure, ECityPlacementFailure::InsufficientFunds);
	TestEqual(TEXT("funds failure changes no occupancy"), PoorModel.GetGrid().GetOccupant(FCityCellCoord(1, 1)),
	          FCityGrid::EmptyEntityId);
	TestEqual(TEXT("funds failure preserves treasury"), PoorModel.GetFunds(), int64(100));

	FCityPlacementModel StaleModel(1000);
	FCityPlacementPreview Stale = StaleModel.BeginPreview(Home, FCityCellCoord(2, 2));
	FCityPlacementPreview Fresh = StaleModel.BeginPreview(Home, FCityCellCoord(20, 20));
	TestTrue(TEXT("fresh transaction commits"), StaleModel.CommitPlacement(Fresh, Home).IsSuccess());
	const FCityPlacementResult StaleResult = StaleModel.CommitPlacement(Stale, Home);
	TestEqual(TEXT("old preview revision is rejected"), StaleResult.Failure, ECityPlacementFailure::StalePreview);
	TestEqual(TEXT("stale failure leaves footprint empty"), StaleModel.GetGrid().GetOccupant(FCityCellCoord(2, 2)),
	          FCityGrid::EmptyEntityId);
	TestEqual(TEXT("only fresh transaction is charged"), StaleModel.GetFunds(), int64(800));

	FCityPlacementPreview Canceled = Model.BeginPreview(Home, FCityCellCoord(30, 30));
	Model.CancelPreview(Canceled);
	const FCityPlacementResult CanceledResult = Model.CommitPlacement(Canceled, Home);
	TestEqual(TEXT("canceled preview cannot commit"), CanceledResult.Failure, ECityPlacementFailure::NoActivePreview);
	TestEqual(TEXT("cancel has zero economic effect"), Model.GetFunds(), int64(800));
	TestEqual(TEXT("cancel has zero grid effect"), Model.GetGrid().GetOccupant(FCityCellCoord(30, 30)),
	          FCityGrid::EmptyEntityId);

	const FCityBuildingDefinition Depot = MakeDefinition(TEXT("Depot"), 2, 3, 100);
	FCityPlacementPreview Rotated = Model.BeginPreview(Depot, FCityCellCoord(5, 5), 1);
	const FCityPlacementResult RotatedResult = Model.CommitPlacement(Rotated, Depot);
	TestTrue(TEXT("rotated 2x3 placement commits"), RotatedResult.IsSuccess());
	TestEqual(TEXT("rotated footprint reports six cells"), RotatedResult.ChangedCells.Num(), 6);
	TestEqual(TEXT("rotated footprint spans three X cells"), Model.GetGrid().GetOccupant(FCityCellCoord(7, 5)),
	          RotatedResult.EntityId);
	TestEqual(TEXT("rotated footprint spans two Y cells"), Model.GetGrid().GetOccupant(FCityCellCoord(5, 6)),
	          RotatedResult.EntityId);
	TestEqual(TEXT("cell outside rotated footprint remains empty"), Model.GetGrid().GetOccupant(FCityCellCoord(5, 7)),
	          FCityGrid::EmptyEntityId);
	return true;
}

#endif
