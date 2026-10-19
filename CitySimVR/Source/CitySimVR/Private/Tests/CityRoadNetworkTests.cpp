#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityRoadNetwork.h"

namespace
{
	void AddRoadPath(FCityPlacementModel& City, const FCityCellCoord& Start, const FCityCellCoord& End)
	{
		FCityRoadPreview Preview = City.BeginRoadPreview(Start);
		City.UpdateRoadPreviewEnd(Preview, End);
		check(City.CommitRoad(Preview).IsSuccess());
	}

	FCityRoadNetworkSnapshot BuildNetwork(const FCityPlacementModel& City)
	{
		FCityRoadNetworkSnapshot Network;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
		return Network;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadDefinitionTest, "CitySim.Roads.Definitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityRoadDefinitionTest::RunTest(const FString& Parameters)
{
	const FCityRoadDefinition Dirt = FCityRoadDefinition::Dirt();
	const FCityRoadDefinition Local = FCityRoadDefinition::Local();
	const FCityRoadDefinition Avenue = FCityRoadDefinition::Avenue();
	const FCityRoadDefinition Highway = FCityRoadDefinition::Highway();
	TestTrue(TEXT("all road definitions validate"),
	         Dirt.IsValid() && Local.IsValid() && Avenue.IsValid() && Highway.IsValid());
	TestTrue(TEXT("road speeds increase by hierarchy"), Dirt.SpeedUnitsPerStep < Local.SpeedUnitsPerStep &&
	                                                        Local.SpeedUnitsPerStep < Avenue.SpeedUnitsPerStep &&
	                                                        Avenue.SpeedUnitsPerStep < Highway.SpeedUnitsPerStep);
	TestTrue(TEXT("avenue has more lanes than local"),
	         Avenue.ForwardLanes + Avenue.ReverseLanes > Local.ForwardLanes + Local.ReverseLanes);
	TestFalse(TEXT("highway forbids direct building frontage"),
	          EnumHasAnyFlags(Highway.Access, ECityRoadAccess::BuildingFrontage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoadGraphTopologyTest, "CitySim.Roads.DeterministicGraphTopology",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityRoadGraphTopologyTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel Straight(100000);
	AddRoadPath(Straight, FCityCellCoord(1, 1), FCityCellCoord(5, 1));
	const FCityRoadNetworkSnapshot StraightGraph = BuildNetwork(Straight);
	TestTrue(TEXT("straight graph validates"), StraightGraph.IsValid());
	TestEqual(TEXT("straight has endpoint nodes"), StraightGraph.Nodes.Num(), 2);
	TestEqual(TEXT("straight collapses to one maximal edge"), StraightGraph.Edges.Num(), 1);
	TestEqual(TEXT("straight edge preserves all cells"), StraightGraph.Edges[0].Cells.Num(), 5);

	FCityPlacementModel Corner(100000);
	AddRoadPath(Corner, FCityCellCoord(1, 1), FCityCellCoord(4, 1));
	AddRoadPath(Corner, FCityCellCoord(4, 1), FCityCellCoord(4, 4));
	const FCityRoadNetworkSnapshot CornerGraph = BuildNetwork(Corner);
	TestEqual(TEXT("corner is an explicit graph node"), CornerGraph.Nodes.Num(), 3);
	TestEqual(TEXT("corner creates two maximal edges"), CornerGraph.Edges.Num(), 2);
	TestEqual(TEXT("corner has two directed lane connectors"), CornerGraph.Connectors.Num(), 2);

	FCityPlacementModel Tee(100000);
	AddRoadPath(Tee, FCityCellCoord(1, 3), FCityCellCoord(7, 3));
	AddRoadPath(Tee, FCityCellCoord(4, 3), FCityCellCoord(4, 7));
	const FCityRoadNetworkSnapshot TeeGraph = BuildNetwork(Tee);
	TestTrue(TEXT("T graph validates"), TeeGraph.IsValid());
	TestEqual(TEXT("T has junction plus three endpoints"), TeeGraph.Nodes.Num(), 4);
	TestEqual(TEXT("T has three maximal edges"), TeeGraph.Edges.Num(), 3);
	TestEqual(TEXT("T has six directed lane connectors"), TeeGraph.Connectors.Num(), 6);

	FCityPlacementModel Cross(100000);
	AddRoadPath(Cross, FCityCellCoord(1, 4), FCityCellCoord(7, 4));
	AddRoadPath(Cross, FCityCellCoord(4, 1), FCityCellCoord(4, 7));
	const FCityRoadNetworkSnapshot CrossGraph = BuildNetwork(Cross);
	TestTrue(TEXT("cross graph validates"), CrossGraph.IsValid());
	TestEqual(TEXT("cross has junction plus four endpoints"), CrossGraph.Nodes.Num(), 5);
	TestEqual(TEXT("cross has four maximal edges"), CrossGraph.Edges.Num(), 4);
	TestEqual(TEXT("cross has twelve directed lane connectors"), CrossGraph.Connectors.Num(), 12);
	TArray<FCityCellCoord> CrossRoute;
	TestTrue(TEXT("shared network routes cars through a junction"),
	         CrossGraph.FindRoute(FCityCellCoord(1, 4), FCityCellCoord(4, 7), ECityRoadAccess::Car, CrossRoute));
	TestEqual(TEXT("junction route starts at requested cell"), CrossRoute[0], FCityCellCoord(1, 4));
	TestEqual(TEXT("junction route ends at requested cell"), CrossRoute.Last(), FCityCellCoord(4, 7));

	const FCityRoadNetworkSnapshot Repeated = BuildNetwork(Cross);
	TestEqual(TEXT("repeated graph has stable node count"), Repeated.Nodes.Num(), CrossGraph.Nodes.Num());
	TestEqual(TEXT("repeated graph has stable edge count"), Repeated.Edges.Num(), CrossGraph.Edges.Num());
	for (int32 Index = 0; Index < CrossGraph.Edges.Num(); ++Index)
	{
		TestEqual(TEXT("edge start is deterministic"), Repeated.Edges[Index].StartNodeId,
		          CrossGraph.Edges[Index].StartNodeId);
		TestEqual(TEXT("edge end is deterministic"), Repeated.Edges[Index].EndNodeId,
		          CrossGraph.Edges[Index].EndNodeId);
		TestTrue(TEXT("edge cells are deterministic"), Repeated.Edges[Index].Cells == CrossGraph.Edges[Index].Cells);
	}

	FCityPlacementModel UpgradeCity(1000);
	AddRoadPath(UpgradeCity, FCityCellCoord(1, 10), FCityCellCoord(7, 10));
	FString UpgradeError;
	const uint64 BeforeRevision = UpgradeCity.GetRevision();
	const int64 FundsBeforeUpgrade = UpgradeCity.GetFunds();
	TestTrue(TEXT("road cells upgrade atomically"),
	         UpgradeCity.UpgradeRoadCells({FCityCellCoord(3, 10), FCityCellCoord(4, 10), FCityCellCoord(5, 10)},
	                                      TEXT("Avenue"), 20, UpgradeError));
	TestEqual(TEXT("upgrade charges each changed cell once"), UpgradeCity.GetFunds(), FundsBeforeUpgrade - 60);
	TestEqual(TEXT("upgrade advances revision once"), UpgradeCity.GetRevision(), BeforeRevision + 1);
	TestEqual(TEXT("upgraded cell retains definition"), UpgradeCity.GetRoadDefinitionId(FCityCellCoord(4, 10)),
	          FName(TEXT("Avenue")));
	const FCityRoadNetworkSnapshot UpgradeGraph = BuildNetwork(UpgradeCity);
	TestTrue(
	    TEXT("upgraded graph contains avenue edge"),
	    UpgradeGraph.Edges.ContainsByPredicate(
	        [](const FCityRoadEdge& Edge)
	        { return Edge.RoadClass == ECityRoadClass::Avenue && Edge.ForwardLanes == 2 && Edge.ReverseLanes == 2; }));
	const int64 FundsBeforeFailure = UpgradeCity.GetFunds();
	TestFalse(TEXT("upgrade rejects non-road cells"),
	          UpgradeCity.UpgradeRoadCells({FCityCellCoord(4, 10), FCityCellCoord(4, 11)}, TEXT("Highway"), 40,
	                                       UpgradeError));
	TestEqual(TEXT("failed upgrade does not charge"), UpgradeCity.GetFunds(), FundsBeforeFailure);
	FCityPlacementModel HighwayCity(10000);
	AddRoadPath(HighwayCity, FCityCellCoord(1, 12), FCityCellCoord(6, 12));
	TArray<FCityCellCoord> HighwayCells;
	for (int32 X = 1; X <= 6; ++X)
		HighwayCells.Add(FCityCellCoord(X, 12));
	TestTrue(TEXT("highway upgrade succeeds"),
	         HighwayCity.UpgradeRoadCells(HighwayCells, TEXT("Highway"), 0, UpgradeError));
	const FCityRoadNetworkSnapshot HighwayGraph = BuildNetwork(HighwayCity);
	TArray<FCityCellCoord> HighwayRoute;
	TestTrue(TEXT("cars can route on highway"),
	         HighwayGraph.FindRoute(HighwayCells[0], HighwayCells.Last(), ECityRoadAccess::Car, HighwayRoute));
	TestFalse(TEXT("pedestrians cannot route on highway"),
	          HighwayGraph.FindRoute(HighwayCells[0], HighwayCells.Last(), ECityRoadAccess::Pedestrian, HighwayRoute));
	FCityPlacementModel FrontageCity(10000);
	AddRoadPath(FrontageCity, FCityPlacementModel::EntranceCell,
	            FCityCellCoord(5, FCityPlacementModel::EntranceCell.Y));
	TestTrue(TEXT("local road provides building frontage"),
	         FrontageCity.HasConnectedRoadAccess(FCityCellCoord(2, 9), FIntPoint(1, 1)));
	TArray<FCityCellCoord> FrontageRoadCells;
	for (int32 X = 0; X <= 5; ++X)
		FrontageRoadCells.Add(FCityCellCoord(X, FCityPlacementModel::EntranceCell.Y));
	TestTrue(TEXT("connected road upgrades to limited access"),
	         FrontageCity.UpgradeRoadCells(FrontageRoadCells, TEXT("Highway"), 0, UpgradeError));
	TestFalse(TEXT("highway does not provide direct building frontage"),
	          FrontageCity.HasConnectedRoadAccess(FCityCellCoord(2, 9), FIntPoint(1, 1)));

	FCityPlacementModel GestureCity(1000);
	AddRoadPath(GestureCity, FCityCellCoord(2, 20), FCityCellCoord(4, 20));
	const int64 FundsBeforeGesture = GestureCity.GetFunds();
	FCityRoadPreview AvenueGesture = GestureCity.BeginRoadPreview(FCityCellCoord(3, 20), TEXT("Avenue"),
	                                                              FCityRoadDefinition::Avenue().PlacementCostPerCell);
	GestureCity.UpdateRoadPreviewEnd(AvenueGesture, FCityCellCoord(6, 20));
	const FCityRoadResult AvenueValidation = GestureCity.ValidateRoad(AvenueGesture);
	TestEqual(TEXT("mixed gesture finds two existing upgrades"), AvenueValidation.UpgradedCells.Num(), 2);
	TestEqual(TEXT("mixed gesture finds two new cells"), AvenueValidation.ChangedCells.Num(), 2);
	TestEqual(TEXT("mixed gesture charges upgrade deltas plus new avenue cells"), AvenueValidation.Cost,
	          static_cast<int64>(100));
	const FCityRoadResult AvenueResult = GestureCity.CommitRoad(AvenueGesture);
	TestTrue(TEXT("mixed build and upgrade commits atomically"), AvenueResult.IsSuccess());
	TestEqual(TEXT("mixed road gesture charges once"), GestureCity.GetFunds(), FundsBeforeGesture - 100);
	TestEqual(TEXT("existing road upgraded to avenue"), GestureCity.GetRoadDefinitionId(FCityCellCoord(3, 20)),
	          FName(TEXT("Avenue")));
	TestEqual(TEXT("new road authored as avenue"), GestureCity.GetRoadDefinitionId(FCityCellCoord(6, 20)),
	          FName(TEXT("Avenue")));
	return true;
}

#endif
