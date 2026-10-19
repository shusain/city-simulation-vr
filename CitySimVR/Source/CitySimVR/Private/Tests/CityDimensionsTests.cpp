#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CityRoadNetwork.h"

namespace
{
	bool AddDimensionRoad(FCityPlacementModel& City, FCityCellCoord Start, FCityCellCoord End)
	{
		auto Preview = City.BeginRoadPreview(Start);
		City.UpdateRoadPreviewEnd(Preview, End);
		return City.CommitRoad(Preview).IsSuccess();
	}
	FCityRoadNetworkSnapshot DimensionGraph(const FCityPlacementModel& City)
	{
		FCityRoadNetworkSnapshot Graph;
		FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Graph);
		return Graph;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsOccupancyTest, "CitySim.Dimensions.CoexistingOccupancy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDimensionsOccupancyTest::RunTest(const FString& Parameters)
{
	FCityGrid Legacy(FCityGridDimensions(64, 64)), Expanded(FCityGridDimensions(80, 80));
	TestFalse(TEXT("legacy boundary is unchanged"), Legacy.IsValidCell({64, 0}));
	TestTrue(TEXT("expanded upper corner exists"), Expanded.IsValidCell({79, 79, 1}));
	TestFalse(TEXT("expanded boundary is exclusive"), Expanded.IsValidCell({80, 79}));
	TestFalse(TEXT("unknown road level rejected"), Expanded.IsValidCell({79, 79, 2}));
	TestTrue(TEXT("expanded rotated footprint fits"), Expanded.TryOccupy({77, 78}, {2, 3}, 42, 1));
	TestFalse(TEXT("expanded overhang rejected atomically"), Expanded.TryOccupy({79, 79}, {2, 2}, 43));
	TestEqual(TEXT("edge occupancy retained"), Expanded.GetOccupant({79, 79}), uint64(42));
	TestTrue(TEXT("raised layer independently occupied"), Expanded.TryOccupy({79, 79, 1}, {1, 1}, 44));
	TestEqual(TEXT("raised layer keeps its ID"), Expanded.GetOccupant({79, 79, 1}), uint64(44));
	TestTrue(TEXT("legacy grid remains independent"), Legacy.TryOccupy({63, 63}, {1, 1}, 45));
	Expanded.ClearEntity(42);
	TestEqual(TEXT("expanded clear preserves raised layer"), Expanded.GetOccupant({79, 79, 1}), uint64(44));
	TestEqual(TEXT("expanded clear preserves other city"), Legacy.GetOccupant({63, 63}), uint64(45));
	for (const auto Dimensions : {FCityGridDimensions(0, 0), FCityGridDimensions(-80, -80), FCityGridDimensions(80, 64),
	                              FCityGridDimensions(MAX_int32, MAX_int32)})
	{
		TestFalse(TEXT("malformed dimensions rejected"), Dimensions.IsValid());
		TestEqual(TEXT("invalid dimensions cannot overflow cell allocation"), Dimensions.GetCellCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsPlacementTest, "CitySim.Dimensions.PlacementAndRestore",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDimensionsPlacementTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel Legacy(100000, FCityGridDimensions(64, 64));
	FCityPlacementModel Expanded(100000, FCityGridDimensions(80, 80));
	TestTrue(TEXT("expanded zone fits"), Expanded.SetZoneArea({78, 78}, {2, 2}, ECityZoneType::Residential));
	TestTrue(TEXT("former row-alias first zone"), Expanded.SetZoneArea({79, 1}, {1, 1}, ECityZoneType::Commercial));
	TestTrue(TEXT("former row-alias second zone"), Expanded.SetZoneArea({15, 2}, {1, 1}, ECityZoneType::Industrial));
	TestTrue(TEXT("zones have distinct indexes"), Expanded.GetZone({79, 1}) == ECityZoneType::Commercial &&
	                                                  Expanded.GetZone({15, 2}) == ECityZoneType::Industrial);
	TestFalse(TEXT("legacy zoning cannot enter expanded area"),
	          Legacy.SetZoneArea({78, 78}, {2, 2}, ECityZoneType::Residential));
	TestTrue(TEXT("entrance road reaches expanded edge"), AddDimensionRoad(Expanded, {0, 8}, {79, 8}));
	FCityBuildingDefinition Definition;
	Definition.DefinitionId = TEXT("DimensionFixture");
	Definition.DisplayName = TEXT("Dimension fixture");
	Definition.Footprint = {2, 2};
	auto Preview = Expanded.BeginPreview(Definition, {77, 9});
	const auto Placed = Expanded.CommitPlacement(Preview, Definition);
	TestTrue(TEXT("building beyond legacy edge placed"), Placed.IsSuccess());
	TestTrue(TEXT("expanded frontage is connected"), Expanded.HasConnectedRoadAccess({77, 9}, {2, 2}));
	TMap<FCityCellCoord, int32> Reach;
	Expanded.GetRoadReach({{0, 8}}, 100, true, Reach);
	TestEqual(TEXT("expanded edge road is 79 hops away"), Reach.FindRef({79, 8}), 79);
	TestTrue(TEXT("cut edge road"), Expanded.RemoveRoad({76, 8}));
	TestFalse(TEXT("frontage cache invalidated after road edit"), Expanded.HasConnectedRoadAccess({77, 9}, {2, 2}));
	Expanded.GetRoadReach({{0, 8}}, 100, true, Reach);
	TestFalse(TEXT("reach cache invalidated after road edit"), Reach.Contains({79, 8}));
	TestTrue(TEXT("restore connection"), AddDimensionRoad(Expanded, {76, 8}, {76, 8}));
	TestTrue(TEXT("frontage reconnects"), Expanded.HasConnectedRoadAccess({77, 9}, {2, 2}));
	const auto State = Expanded.ExportState();
	TestEqual(TEXT("expanded zone state has 6400 cells"), State.Zones.Num(), 6400);
	TMap<FName, FCityBuildingDefinition> Definitions;
	Definitions.Add(Definition.DefinitionId, Definition);
	FString Error;
	auto Restored = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	if (!TestTrue(TEXT("expanded placement state restores"), Restored.IsValid()))
		return false;
	TestTrue(TEXT("restored dimensions retained"), Restored->GetGrid().GetDimensions() == State.Dimensions);
	TestEqual(TEXT("restored building retains entity ID"), Restored->GetGrid().GetOccupant({78, 10}), Placed.EntityId);
	TestEqual(TEXT("restored funds unchanged"), Restored->GetFunds(), Expanded.GetFunds());
	TestEqual(TEXT("restored revision unchanged"), Restored->GetRevision(), Expanded.GetRevision());
	TestTrue(TEXT("restored frontage rebuilt"), Restored->HasConnectedRoadAccess({77, 9}, {2, 2}));
	TestTrue(TEXT("legacy model still has 4096 zones"), Legacy.ExportState().Zones.Num() == 4096);
	for (int32 Case = 0; Case < 3; ++Case)
	{
		auto Corrupt = State;
		if (Case == 0)
			Corrupt.Dimensions = FCityGridDimensions(MAX_int32, MAX_int32);
		if (Case == 1)
			Corrupt.Zones.Pop();
		if (Case == 2)
			Corrupt.RoadCells.Add({80, 8});
		TestFalse(TEXT("corrupt restore rejected before use"),
		          FCityPlacementModel::CreateValidatedFromState(Corrupt, Definitions, Error).IsValid());
		TestEqual(TEXT("failed restore preserves live city"), Expanded.GetGrid().GetOccupant({78, 10}),
		          Placed.EntityId);
	}
	FCitySaveDocument Document;
	Document.Placement = State;
	FString Json = TEXT("sentinel");
	TestFalse(TEXT("expanded placement cannot masquerade as a legacy save"),
	          FCityPersistence::Serialize(Document, Json, Error));
	TestEqual(TEXT("rejected serialization preserves caller output"), Json, FString(TEXT("sentinel")));
	Document.GridWidth = Document.GridHeight = 80;
	TestFalse(TEXT("foreign terrain/environment cannot masquerade as expanded save"),
	          FCityPersistence::Serialize(Document, Json, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDimensionsRoutingTest, "CitySim.Dimensions.RoadKeysAndRouting",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDimensionsRoutingTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000, FCityGridDimensions(80, 80));
	// These two paths collide if links still encode their cells using width64.
	TestTrue(TEXT("first collision path built"), AddDimensionRoad(City, {64, 1}, {66, 1}));
	TestTrue(TEXT("second collision path built"), AddDimensionRoad(City, {0, 2}, {2, 2}));
	auto Graph = DimensionGraph(City);
	TestTrue(TEXT("expanded graph validates"), Graph.IsValid());
	TestEqual(TEXT("distinct paths both become edges"), Graph.Edges.Num(), 2);
	if (!TestEqual(TEXT("distinct paths retain four nodes"), Graph.Nodes.Num(), 4))
		return false;
	TestEqual(TEXT("expanded node order starts in row1"), Graph.Nodes[0].Cell, FCityCellCoord(64, 1));
	TestEqual(TEXT("row2 node has stable ID2"), Graph.Nodes[2].Cell, FCityCellCoord(0, 2));
	for (bool Prepared : {false, true})
	{
		Graph.bUsePreparedRouting = Prepared;
		TArray<FCityCellCoord> Route;
		TestTrue(TEXT("expanded edge routes in both paths"),
		         Graph.FindRoute({64, 1}, {66, 1}, ECityRoadAccess::Car, Route));
		TestEqual(TEXT("route length"), Route.Num(), 3);
		TestTrue(TEXT("second path routes independently"),
		         Graph.FindRoute({0, 2}, {2, 2}, ECityRoadAccess::Car, Route));
		TestFalse(TEXT("disconnected paths never join through key alias"),
		          Graph.FindRoute({64, 1}, {0, 2}, ECityRoadAccess::Car, Route));
		TestTrue(TEXT("failed route clears output"), Route.IsEmpty());
	}
	FCityPlacementModel Legacy(100000, FCityGridDimensions(64, 64)), Expanded(100000, FCityGridDimensions(80, 80));
	for (auto* Model : {&Legacy, &Expanded})
	{
		TestTrue(TEXT("square first edge"), AddDimensionRoad(*Model, {2, 2}, {6, 2}));
		TestTrue(TEXT("square second edge"), AddDimensionRoad(*Model, {6, 2}, {6, 6}));
		TestTrue(TEXT("square third edge"), AddDimensionRoad(*Model, {6, 6}, {2, 6}));
		TestTrue(TEXT("square fourth edge"), AddDimensionRoad(*Model, {2, 6}, {2, 2}));
	}
	const auto LegacyGraph = DimensionGraph(Legacy), ExpandedGraph = DimensionGraph(Expanded);
	const TArray<FCityCellCoord> ExpectedNodes = {{2, 2}, {6, 2}, {2, 6}, {6, 6}};
	if (!TestEqual(TEXT("legacy square has four nodes"), LegacyGraph.Nodes.Num(), ExpectedNodes.Num()))
		return false;
	for (int32 Index = 0; Index < ExpectedNodes.Num(); ++Index)
		TestEqual(TEXT("legacy packed node ID retains its original coordinate"), LegacyGraph.Nodes[Index].Cell,
		          ExpectedNodes[Index]);
	TestEqual(TEXT("same geometry retains edge count"), LegacyGraph.Edges.Num(), ExpandedGraph.Edges.Num());
	for (int32 Index = 0; Index < LegacyGraph.Edges.Num() && Index < ExpandedGraph.Edges.Num(); ++Index)
	{
		TestEqual(TEXT("existing start-node IDs unchanged"), LegacyGraph.Edges[Index].StartNodeId,
		          ExpandedGraph.Edges[Index].StartNodeId);
		TestEqual(TEXT("existing end-node IDs unchanged"), LegacyGraph.Edges[Index].EndNodeId,
		          ExpandedGraph.Edges[Index].EndNodeId);
		TestTrue(TEXT("existing edge cell ordering unchanged"),
		         LegacyGraph.Edges[Index].Cells == ExpandedGraph.Edges[Index].Cells);
	}
	TArray<FCityCellCoord> LegacyRoute, ExpandedRoute;
	TestTrue(TEXT("legacy tied route works"), LegacyGraph.FindRoute({2, 2}, {6, 6}, ECityRoadAccess::Car, LegacyRoute));
	TestTrue(TEXT("expanded tied route works"),
	         ExpandedGraph.FindRoute({2, 2}, {6, 6}, ECityRoadAccess::Car, ExpandedRoute));
	TestTrue(TEXT("route tie-break remains unchanged"), LegacyRoute == ExpandedRoute);
	const TArray<FCityCellCoord> ExpectedRoute = {{2, 2}, {3, 2}, {4, 2}, {5, 2}, {6, 2},
	                                              {6, 3}, {6, 4}, {6, 5}, {6, 6}};
	TestTrue(TEXT("legacy equal-cost route retains original cells"), LegacyRoute == ExpectedRoute);
	Graph.Dimensions = FCityGridDimensions(64, 64);
	TestFalse(TEXT("mislabeled graph rejected"), Graph.IsValid());
	TestFalse(TEXT("legacy graph bounds reject expanded request"),
	          Graph.FindRoute({64, 1}, {66, 1}, ECityRoadAccess::Car, ExpandedRoute));
	return true;
}

#endif
