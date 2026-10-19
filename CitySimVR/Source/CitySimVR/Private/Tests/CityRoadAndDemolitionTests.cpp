#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityTerrain.h"

namespace
{
	FCityBuildingDefinition MakeRoadTestBuilding(const FName DefinitionId, const int32 Width, const int32 Height,
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

	FCityRoadResult BuildRoad(FCityPlacementModel& Model, const FCityCellCoord& Start, const FCityCellCoord& End)
	{
		FCityRoadPreview Preview = Model.BeginRoadPreview(Start);
		Model.UpdateRoadPreviewEnd(Preview, End);
		return Model.CommitRoad(Preview);
	}

	FCityPlacementResult BuildBuilding(FCityPlacementModel& Model, const FCityBuildingDefinition& Definition,
	                                   const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = Model.BeginPreview(Definition, Anchor);
		return Model.CommitPlacement(Preview, Definition);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityTerrainAwarePlacementTest, "CitySim.Roads.TerrainAwarePlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityTerrainAwarePlacementTest::RunTest(const FString& Parameters)
{
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("terrain fixture generates"), Terrain.Generate(FCityTerrainGenerationParameters()));
	FCityCellCoord FirstWater(-1, -1);
	FCityCellCoord SecondWater(-1, -1);
	FCityCellCoord Supported(-1, -1);
	FCityCellCoord Invalid(-1, -1);
	FCityPlacementModel Probe(100000);
	Probe.SetTerrain(&Terrain);
	for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
	{
		for (int32 X = 0; X < FCityGrid::Width; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			if (Terrain.IsWaterCell(Cell))
			{
				if (FirstWater.X < 0)
					FirstWater = Cell;
				else if (SecondWater.X < 0)
					SecondWater = Cell;
			}
			const ECityRoadTerrainClass Class = Probe.EvaluateRoadTerrain(Cell).TerrainClass;
			if (Class == ECityRoadTerrainClass::Supported && Supported.X < 0)
				Supported = Cell;
			if (Class == ECityRoadTerrainClass::Invalid && Invalid.X < 0)
				Invalid = Cell;
		}
	}
	TestTrue(TEXT("water fixture exists"), FirstWater.X >= 0 && SecondWater.X >= 0);
	const FCityBuildingDefinition Building = MakeRoadTestBuilding(TEXT("TerrainBuilding"), 1, 1, 100);
	FCityPlacementPreview WaterBuilding = Probe.BeginPreview(Building, FirstWater);
	TestEqual(TEXT("ordinary building preview exposes water blocker"),
	          Probe.ValidatePlacement(WaterBuilding, Building).Failure, ECityPlacementFailure::WaterOrShore);
	TestFalse(TEXT("water cannot be zoned"), Probe.CanSetZoneArea(FirstWater, FIntPoint(1, 1)));

	FCityRoadPreview BridgePreview = Probe.BeginRoadPreview(FirstWater);
	Probe.UpdateRoadPreviewEnd(BridgePreview, FirstWater);
	const FCityRoadResult BridgeValidation = Probe.ValidateRoad(BridgePreview);
	TestTrue(TEXT("water road preview is bridge-capable"), BridgeValidation.IsSuccess());
	TestEqual(TEXT("bridge preview reports one bridge cell"), BridgeValidation.BridgeCells.Num(), 1);
	TestEqual(TEXT("bridge preview includes named surcharge"), BridgeValidation.Cost,
	          FCityPlacementModel::RoadCellCost + FCityPlacementModel::BridgeSurcharge);
	const int64 FundsBeforeBridge = Probe.GetFunds();
	const FCityRoadResult BridgeCommit = Probe.CommitRoad(BridgePreview);
	TestEqual(TEXT("bridge transaction agrees with preview cost"), BridgeCommit.Cost, BridgeValidation.Cost);
	TestEqual(TEXT("bridge transaction deducts exact preview cost"), Probe.GetFunds(),
	          FundsBeforeBridge - BridgeValidation.Cost);

	TestTrue(TEXT("default terrain contains a supported-grade fixture"), Supported.X >= 0);
	if (Supported.X >= 0)
	{
		FCityRoadPreview SupportedPreview = Probe.BeginRoadPreview(Supported);
		Probe.UpdateRoadPreviewEnd(SupportedPreview, Supported);
		const FCityRoadResult SupportedValidation = Probe.ValidateRoad(SupportedPreview);
		TestTrue(TEXT("supported grade is accepted"), SupportedValidation.IsSuccess());
		TestEqual(TEXT("supported preview reports treatment"), SupportedValidation.SupportedCells.Num(), 1);
		TestEqual(TEXT("supported grade includes named surcharge"), SupportedValidation.Cost,
		          FCityPlacementModel::RoadCellCost + FCityPlacementModel::SupportedGradeSurcharge);
	}
	if (Invalid.X >= 0)
	{
		FCityRoadPreview InvalidPreview = Probe.BeginRoadPreview(Invalid);
		Probe.UpdateRoadPreviewEnd(InvalidPreview, Invalid);
		TestEqual(TEXT("excessive grade is rejected"), Probe.ValidateRoad(InvalidPreview).Failure,
		          ECityRoadFailure::UnsupportedGrade);
	}

	FCityPlacementModelState State = Probe.ExportState();
	TMap<FName, FCityBuildingDefinition> Definitions;
	FString Error;
	TUniquePtr<FCityPlacementModel> Restored = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	TestTrue(TEXT("bridge road state restores"), Restored.IsValid());
	if (Restored)
	{
		Restored->SetTerrain(&Terrain);
		TestTrue(TEXT("restored bridge retains road connectivity state"), Restored->IsRoad(FirstWater));
		TestEqual(TEXT("restored water road derives bridge treatment"),
		          Restored->EvaluateRoadTerrain(FirstWater).TerrainClass, ECityRoadTerrainClass::Bridge);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadAndDemolitionTest, "CitySim.Roads.ConnectivityAndDemolition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityRoadAndDemolitionTest::RunTest(const FString& Parameters)
{
	TArray<FCityCellCoord> Path;
	FCityPlacementModel::BuildOrthogonalRoadPath(FCityCellCoord(1, 1), FCityCellCoord(3, 3), Path);
	TestEqual(TEXT("X-then-Y path has no duplicate corner"), Path.Num(), 5);
	TestEqual(TEXT("path starts at gesture start"), Path[0], FCityCellCoord(1, 1));
	TestEqual(TEXT("X segment completes before Y"), Path[2], FCityCellCoord(3, 1));
	TestEqual(TEXT("path ends at gesture end"), Path[4], FCityCellCoord(3, 3));

	FCityPlacementModel Model(1000);
	FCityRoadPreview CanceledRoad = Model.BeginRoadPreview(FCityCellCoord(2, 2));
	Model.UpdateRoadPreviewEnd(CanceledRoad, FCityCellCoord(6, 4));
	Model.CancelRoadPreview(CanceledRoad);
	TestFalse(TEXT("canceling a mid-road gesture deactivates it"), CanceledRoad.bActive);
	TestEqual(TEXT("canceling a mid-road gesture costs nothing"), Model.GetFunds(), int64(1000));
	TestEqual(TEXT("a canceled road cannot commit"), Model.CommitRoad(CanceledRoad).Failure,
	          ECityRoadFailure::NoActivePreview);

	const FCityRoadResult EntranceRoad = BuildRoad(Model, FCityCellCoord(1, 8), FCityCellCoord(4, 8));
	TestTrue(TEXT("straight entrance road commits"), EntranceRoad.IsSuccess());
	TestEqual(TEXT("new road cells are charged once"), EntranceRoad.Cost, int64(40));
	TestEqual(TEXT("treasury reflects road price"), Model.GetFunds(), int64(960));
	TestTrue(TEXT("road touching entrance is connected"), Model.IsRoadConnected(FCityCellCoord(4, 8)));

	const FCityBuildingDefinition FacingBuilding = MakeRoadTestBuilding(TEXT("Facing"), 2, 2, 0);
	TestEqual(TEXT("south road keeps authored -Y frontage"),
	          Model.GetDefaultRoadFacingQuarterTurns(FacingBuilding, FCityCellCoord(2, 9)), 0);
	TestEqual(TEXT("north road turns frontage toward +Y"),
	          Model.GetDefaultRoadFacingQuarterTurns(FacingBuilding, FCityCellCoord(2, 6)), 2);
	FCityPlacementPreview FacingPreview = Model.BeginRoadFacingPreview(FacingBuilding, FCityCellCoord(2, 6));
	TestEqual(TEXT("road-facing preview applies default rotation"), FacingPreview.QuarterTurnsClockwise, 2);
	Model.RotatePreviewClockwise(FacingPreview);
	Model.UpdateRoadFacingPreviewAnchor(FacingPreview, FacingBuilding, FCityCellCoord(1, 6));
	TestEqual(TEXT("manual rotation remains authoritative while moving preview"), FacingPreview.QuarterTurnsClockwise,
	          3);
	FCityPlacementModel EastWestFacingModel(1000);
	TestTrue(TEXT("east/west frontage entrance road commits"),
	         BuildRoad(EastWestFacingModel, FCityCellCoord(1, 8), FCityCellCoord(4, 8)).IsSuccess());
	TestTrue(TEXT("connected east/west frontage branch commits"),
	         BuildRoad(EastWestFacingModel, FCityCellCoord(4, 8), FCityCellCoord(4, 12)).IsSuccess());
	TestEqual(TEXT("east road turns frontage toward +X"),
	          EastWestFacingModel.GetDefaultRoadFacingQuarterTurns(FacingBuilding, FCityCellCoord(2, 10)), 1);
	TestEqual(TEXT("west road turns frontage toward -X"),
	          EastWestFacingModel.GetDefaultRoadFacingQuarterTurns(FacingBuilding, FCityCellCoord(5, 10)), 3);
	FCityBuildingDefinition RearEntranceBuilding = FacingBuilding;
	RearEntranceBuilding.EntranceSides = static_cast<int32>(ECityBuildingEntranceSide::PositiveY);
	TestEqual(TEXT("asset-declared +Y entrance rotates toward a south road"),
	          Model.GetDefaultRoadFacingQuarterTurns(RearEntranceBuilding, FCityCellCoord(2, 9)), 2);
	RearEntranceBuilding.EntranceSides |= static_cast<int32>(ECityBuildingEntranceSide::NegativeY);
	TestEqual(TEXT("multi-entrance metadata permits the best matching side"),
	          Model.GetDefaultRoadFacingQuarterTurns(RearEntranceBuilding, FCityCellCoord(2, 9)), 0);

	const FCityRoadResult CrossRoad = BuildRoad(Model, FCityCellCoord(2, 7), FCityCellCoord(2, 9));
	TestTrue(TEXT("road can cross and reuse existing road"), CrossRoad.IsSuccess());
	TestEqual(TEXT("only two unique crossing cells are charged"), CrossRoad.Cost, int64(20));
	TestEqual(TEXT("T/cross topology uses four-neighbor mask"), Model.GetRoadNeighborMask(FCityCellCoord(2, 8)),
	          uint8(15));

	const FCityBuildingDefinition Home = MakeRoadTestBuilding(TEXT("Home"), 2, 2, 200);
	const FCityPlacementResult AccessibleHome = BuildBuilding(Model, Home, FCityCellCoord(3, 9));
	TestTrue(TEXT("building beside connected road commits"), AccessibleHome.IsSuccess());
	TestTrue(TEXT("orthogonally adjacent building has road access"), Model.HasRoadAccess(AccessibleHome.EntityId));

	const FCityPlacementResult DiagonalHome = BuildBuilding(Model, Home, FCityCellCoord(5, 9));
	TestTrue(TEXT("diagonal test building commits"), DiagonalHome.IsSuccess());
	TestFalse(TEXT("diagonal-only contact is not road access"), Model.HasRoadAccess(DiagonalHome.EntityId));

	const int64 FundsBeforeBlockedRoad = Model.GetFunds();
	const FCityRoadResult BlockedRoad = BuildRoad(Model, FCityCellCoord(0, 10), FCityCellCoord(6, 10));
	TestEqual(TEXT("road crossing a building is rejected"), BlockedRoad.Failure, ECityRoadFailure::Blocked);
	TestEqual(TEXT("blocked road does not charge"), Model.GetFunds(), FundsBeforeBlockedRoad);
	TestFalse(TEXT("blocked path does not partially occupy"), Model.IsRoad(FCityCellCoord(0, 10)));

	FCityPlacementModel PoorModel(15);
	const FCityRoadResult TooExpensive = BuildRoad(PoorModel, FCityCellCoord(1, 1), FCityCellCoord(2, 1));
	TestEqual(TEXT("whole road price is checked before commit"), TooExpensive.Failure,
	          ECityRoadFailure::InsufficientFunds);
	TestFalse(TEXT("funds failure creates no first cell"), PoorModel.IsRoad(FCityCellCoord(1, 1)));
	TestFalse(TEXT("funds failure creates no last cell"), PoorModel.IsRoad(FCityCellCoord(2, 1)));

	FCityPlacementModel ConnectivityModel(1000);
	TestTrue(TEXT("entrance bridge commits"),
	         BuildRoad(ConnectivityModel, FCityCellCoord(1, 8), FCityCellCoord(4, 8)).IsSuccess());
	TestTrue(TEXT("far side begins connected"), ConnectivityModel.IsRoadConnected(FCityCellCoord(4, 8)));
	TestTrue(TEXT("bridge road removes cell-by-cell"), ConnectivityModel.RemoveRoad(FCityCellCoord(2, 8)));
	TestEqual(TEXT("road demolition refunds half one cell cost"), ConnectivityModel.GetFunds(), int64(965));
	TestFalse(TEXT("bridge deletion disconnects far side"), ConnectivityModel.IsRoadConnected(FCityCellCoord(4, 8)));
	TestTrue(TEXT("entrance side remains connected"), ConnectivityModel.IsRoadConnected(FCityCellCoord(1, 8)));
	TestTrue(TEXT("disconnected island commits"),
	         BuildRoad(ConnectivityModel, FCityCellCoord(10, 10), FCityCellCoord(12, 10)).IsSuccess());
	TestFalse(TEXT("island is not connected to entrance"), ConnectivityModel.IsRoadConnected(FCityCellCoord(11, 10)));

	FCityPlacementModel DemolitionModel(1000);
	const FCityPlacementResult BuiltForDemolition = BuildBuilding(DemolitionModel, Home, FCityCellCoord(20, 20));
	TestTrue(TEXT("demolition fixture commits"), BuiltForDemolition.IsSuccess());
	const FCityDemolitionResult Demolition = DemolitionModel.DemolishEntity(BuiltForDemolition.EntityId);
	TestTrue(TEXT("demolition succeeds"), Demolition.IsSuccess());
	TestEqual(TEXT("demolition clears full footprint"), Demolition.ChangedCells.Num(), 4);
	TestEqual(TEXT("demolition refunds half recorded cost once"), Demolition.Refund, int64(100));
	TestEqual(TEXT("treasury contains one build charge and one refund"), DemolitionModel.GetFunds(), int64(900));
	TestEqual(TEXT("demolition clears occupancy"), DemolitionModel.GetGrid().GetOccupant(FCityCellCoord(21, 21)),
	          FCityGrid::EmptyEntityId);
	TestEqual(TEXT("second demolition is rejected"),
	          DemolitionModel.DemolishEntity(BuiltForDemolition.EntityId).Failure,
	          ECityDemolitionFailure::EntityNotFound);
	TestEqual(TEXT("second demolition pays no refund"), DemolitionModel.GetFunds(), int64(900));
	return true;
}

#endif
