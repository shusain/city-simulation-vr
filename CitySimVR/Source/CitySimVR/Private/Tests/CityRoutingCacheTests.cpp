#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Performance/CityBenchmark.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityRoutingCacheTest, "CitySim.Roads.PreparedRoutingEquivalence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityRoutingCacheTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("Dry flat terrain generates"), Terrain.Generate(FCityBenchmark::CapacityTerrainParameters()));
	City.SetTerrain(&Terrain);
	auto Road = City.BeginRoadPreview({0, 8});
	Road.End = {24, 8};
	TestTrue(TEXT("Main road commits"), City.CommitRoad(Road).IsSuccess());
	Road = City.BeginRoadPreview({8, 2});
	Road.End = {8, 20};
	TestTrue(TEXT("Ground branch commits"), City.CommitRoad(Road).IsSuccess());
	Road = City.BeginRoadPreview({2, 14}, TEXT("Highway"), 60);
	Road.End = {20, 14};
	Road.Profile = ECityRoadProfile::Elevated;
	const auto Raised = City.CommitRoad(Road);
	if (!TestTrue(TEXT("Raised highway commits"), Raised.IsSuccess()))
		AddError(Raised.Detail);

	auto Compare = [&](const FCityPlacementModel& Model, bool bExpectSegments = true)
	{
		FCityRoadNetworkSnapshot Cached;
		FCityRoadNetworkBuilder::Build(Model, FCityRoadDefinition::Local(), Cached);
		auto Reference = Cached;
		Reference.bUsePreparedRouting = false;
		FCityRoadRoutingTiming Timing;
		Cached.RoutingTiming = &Timing;
		const TArray<FCityCellCoord> Points = {{0, 8},     {8, 2},   {24, 8},     {8, 20},
		                                       {8, 14, 1}, {60, 60}, {14, 14, 1}, {2, 14}};
		for (int32 Mask = 0; Mask < 16; ++Mask)
			for (const auto& A : Points)
				for (const auto& B : Points)
				{
					TArray<FCityCellCoord> Actual, Expected;
					TestEqual(TEXT("Cached route reachability matches access-filtered reference"),
					          Cached.FindRoute(A, B, static_cast<ECityRoadAccess>(Mask), Actual),
					          Reference.FindRoute(A, B, static_cast<ECityRoadAccess>(Mask), Expected));
					TestTrue(TEXT("Weighted routes and equal-cost ties match exactly"), Actual == Expected);
				}
		TestEqual(TEXT("Repeated requests do not rebuild adjacency"), Timing.ReferenceBuilds, uint64(0));
		TestEqual(TEXT("All requests use the prepared graph"), Timing.PreparedSearches, uint64(1024));
		for (const auto& A : Points)
			for (const auto& B : Points)
				for (const auto Type :
				     {ECityTripVehicleType::Car, ECityTripVehicleType::Walk, ECityTripVehicleType::FreightTruck})
				{
					FCityTrafficVehicleState Actual, Expected;
					Actual.VehicleType = Expected.VehicleType = Type;
					TestEqual(TEXT("Vehicle routing succeeds identically"),
					          FCityTrafficSimulation::BuildRoute(Cached, A, B, Actual),
					          FCityTrafficSimulation::BuildRoute(Reference, A, B, Expected));
					TestEqual(TEXT("Route leg count matches"), Actual.Route.Num(), Expected.Route.Num());
					for (int32 Index = 0; Index < FMath::Min(Actual.Route.Num(), Expected.Route.Num()); ++Index)
					{
						TestEqual(TEXT("Edge matches"), Actual.Route[Index].EdgeId, Expected.Route[Index].EdgeId);
						TestEqual(TEXT("Direction matches"), Actual.Route[Index].bReverse,
						          Expected.Route[Index].bReverse);
						TestEqual(TEXT("Lane matches"), Actual.Route[Index].LaneIndex, Expected.Route[Index].LaneIndex);
					}
					TestEqual(TEXT("Origin distance matches"), Actual.StartDistanceUnits, Expected.StartDistanceUnits);
					TestEqual(TEXT("Destination distance matches"), Actual.EndDistanceUnits, Expected.EndDistanceUnits);
				}
		if (bExpectSegments)
			TestTrue(TEXT("Route conversion uses indexed segments"), Timing.IndexedSegments > 0);
	};
	Compare(City);
	FString Error;
	TestTrue(TEXT("Upgrade changes speed and access"),
	         City.UpgradeRoadCells({{22, 8}, {23, 8}, {24, 8}}, TEXT("Highway"), 60, Error));
	Compare(City);
	TestTrue(TEXT("Road disconnection commits"), City.RemoveRoad({8, 8}));
	Compare(City);
	auto Reloaded = FCityPlacementModel::CreateValidatedFromState(City.ExportState(), {}, Error);
	if (TestNotNull(TEXT("Reload preserves road state"), Reloaded.Get()))
		Compare(*Reloaded);
	FCityPlacementModel Empty(100000);
	Compare(Empty, false);
	return true;
}
#endif
