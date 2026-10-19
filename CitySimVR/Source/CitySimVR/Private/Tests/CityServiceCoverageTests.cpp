#include "Misc/AutomationTest.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FCityBuildingDefinition MakeServiceReachDefinition(const FName Id, const int32 Width = 1, const int32 Height = 1)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(Width, Height);
		Definition.LogicalHeightCm = 100.0;
		return Definition;
	}

	uint64 PlaceServiceReachFixture(FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
	                                const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Anchor);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}

	void AddServiceReachRoadSegment(FCityPlacementModel& City, const FCityCellCoord& Start, const FCityCellCoord& End)
	{
		FCityRoadPreview Preview = City.BeginRoadPreview(Start);
		City.UpdateRoadPreviewEnd(Preview, End);
		City.CommitRoad(Preview);
	}

	void AddServiceReachEntranceRoad(FCityPlacementModel& City, const int32 EndX)
	{
		AddServiceReachRoadSegment(City, FCityPlacementModel::EntranceCell,
		                           FCityCellCoord(EndX, FCityPlacementModel::EntranceCell.Y));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityServiceCoverageTest, "CitySim.Simulation.ServiceRoadReach",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityServiceCoverageTest::RunTest(const FString& Parameters)
{
	const FCityBuildingDefinition Service = MakeServiceReachDefinition(TEXT("Service"));
	FCityPlacementModel StraightCity(100000);
	AddServiceReachEntranceRoad(StraightCity, 20);
	const uint64 StraightServiceId = PlaceServiceReachFixture(StraightCity, Service, FCityCellCoord(0, 9));
	FCityServiceReach StraightReach;
	FCityServiceEvaluator::BuildProviderReach(StraightCity, StraightServiceId, 12, true, StraightReach);
	TestEqual(TEXT("provider-adjacent road is hop zero"), StraightReach.GetRoadHopDistance(FCityCellCoord(0, 8)), 0);
	TestEqual(TEXT("inclusive boundary road is exactly twelve hops"),
	          StraightReach.GetRoadHopDistance(FCityCellCoord(12, 8)), 12);
	TestFalse(TEXT("road beyond inclusive hop budget is not reached"),
	          StraightReach.IsRoadReached(FCityCellCoord(13, 8)));
	TestTrue(
	    TEXT("candidate beside exact hop boundary is served"),
	    FCityServiceEvaluator::IsFootprintReached(StraightCity, StraightReach, FCityCellCoord(12, 9), FIntPoint(1, 1)));
	TestFalse(
	    TEXT("candidate one hop beyond boundary is unserved"),
	    FCityServiceEvaluator::IsFootprintReached(StraightCity, StraightReach, FCityCellCoord(13, 9), FIntPoint(1, 1)));

	FCityPlacementModel BranchCity(100000);
	AddServiceReachEntranceRoad(BranchCity, 20);
	AddServiceReachRoadSegment(BranchCity, FCityCellCoord(5, 8), FCityCellCoord(5, 12));
	AddServiceReachRoadSegment(BranchCity, FCityCellCoord(5, 12), FCityCellCoord(10, 12));
	AddServiceReachRoadSegment(BranchCity, FCityCellCoord(10, 8), FCityCellCoord(10, 12));
	const uint64 BranchServiceId = PlaceServiceReachFixture(BranchCity, Service, FCityCellCoord(0, 9));
	FCityServiceReach BranchReach;
	FCityServiceEvaluator::BuildProviderReach(BranchCity, BranchServiceId, 20, true, BranchReach);
	TestEqual(TEXT("corner and branch use shortest road path"), BranchReach.GetRoadHopDistance(FCityCellCoord(5, 12)),
	          9);
	TestEqual(TEXT("loop retains deterministic shortest distance"),
	          BranchReach.GetRoadHopDistance(FCityCellCoord(10, 12)), 14);

	FCityPlacementModel MultiEntranceCity(100000);
	AddServiceReachEntranceRoad(MultiEntranceCity, 20);
	AddServiceReachRoadSegment(MultiEntranceCity, FCityCellCoord(3, 8), FCityCellCoord(3, 10));
	const FCityBuildingDefinition WideService = MakeServiceReachDefinition(TEXT("Service"), 2, 2);
	const uint64 WideServiceId = PlaceServiceReachFixture(MultiEntranceCity, WideService, FCityCellCoord(1, 9));
	FCityServiceReach MultiEntranceReach;
	FCityServiceEvaluator::BuildProviderReach(MultiEntranceCity, WideServiceId, 12, true, MultiEntranceReach);
	TestEqual(TEXT("first provider entrance seeds hop zero"),
	          MultiEntranceReach.GetRoadHopDistance(FCityCellCoord(1, 8)), 0);
	TestEqual(TEXT("second provider entrance seeds hop zero"),
	          MultiEntranceReach.GetRoadHopDistance(FCityCellCoord(2, 8)), 0);
	TestEqual(TEXT("side provider entrance also seeds hop zero"),
	          MultiEntranceReach.GetRoadHopDistance(FCityCellCoord(3, 9)), 0);
	TestTrue(TEXT("parcel farther by anchor distance is served through nearer footprint entrance"),
	         FCityServiceEvaluator::IsFootprintReached(MultiEntranceCity, MultiEntranceReach, FCityCellCoord(14, 9),
	                                                   FIntPoint(1, 1)));

	const uint64 FarServiceId = PlaceServiceReachFixture(MultiEntranceCity, Service, FCityCellCoord(20, 9));
	FCityServiceReach MultiProviderReach;
	const TArray<uint64> MultiProviderIds{WideServiceId, FarServiceId};
	FCityServiceEvaluator::BuildServiceReach(MultiEntranceCity, MultiProviderIds, 12, true, MultiProviderReach);
	TestEqual(TEXT("multiple providers select nearest source"),
	          MultiProviderReach.GetRoadHopDistance(FCityCellCoord(12, 8)), 8);

	FCityPlacementModel DetourCity(100000);
	AddServiceReachEntranceRoad(DetourCity, 10);
	AddServiceReachRoadSegment(DetourCity, FCityCellCoord(10, 8), FCityCellCoord(10, 10));
	AddServiceReachRoadSegment(DetourCity, FCityCellCoord(10, 10), FCityCellCoord(2, 10));
	const uint64 DetourServiceId = PlaceServiceReachFixture(DetourCity, Service, FCityCellCoord(0, 9));
	FCityServiceReach DetourReach;
	FCityServiceEvaluator::BuildProviderReach(DetourCity, DetourServiceId, 12, true, DetourReach);
	TestFalse(
	    TEXT("geometrically near parcel beyond road-hop budget is unserved"),
	    FCityServiceEvaluator::IsFootprintReached(DetourCity, DetourReach, FCityCellCoord(2, 11), FIntPoint(1, 1)));

	FCityPlacementModel IslandCity(100000);
	AddServiceReachRoadSegment(IslandCity, FCityCellCoord(30, 30), FCityCellCoord(35, 30));
	const uint64 IslandServiceId = PlaceServiceReachFixture(IslandCity, Service, FCityCellCoord(30, 31));
	FCityServiceReach ActiveIslandReach;
	FCityServiceEvaluator::BuildProviderReach(IslandCity, IslandServiceId, 12, true, ActiveIslandReach);
	TestTrue(TEXT("disconnected provider contributes no active reach"), ActiveIslandReach.RoadHopDistances.IsEmpty());
	FCityServiceReach PotentialIslandReach;
	FCityServiceEvaluator::BuildProviderReach(IslandCity, IslandServiceId, 12, false, PotentialIslandReach);
	TestEqual(TEXT("disconnected provider still exposes potential island reach"),
	          PotentialIslandReach.RoadHopDistances.Num(), 6);

	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 12;
	FCityServiceSnapshot Snapshot = FCityServiceEvaluator::BuildSnapshot(StraightCity, Tuning);
	TestTrue(TEXT("operational snapshot reaches boundary"), Snapshot.ServiceReach.IsRoadReached(FCityCellCoord(12, 8)));
	StraightCity.RemoveRoad(FCityCellCoord(6, 8));
	Snapshot = FCityServiceEvaluator::BuildSnapshot(StraightCity, Tuning);
	TestFalse(TEXT("bridge deletion removes reach beyond break"),
	          Snapshot.ServiceReach.IsRoadReached(FCityCellCoord(7, 8)));
	TestEqual(TEXT("candidate on disconnected side reports road disconnection first"),
	          FCityServiceEvaluator::EvaluateCandidate(StraightCity, Snapshot, Tuning, TEXT("Home"),
	                                                   FCityCellCoord(7, 9), FIntPoint(1, 1)),
	          ECityOperationFailure::NoRoad);

	FCityPlacementModel DemolitionCity(100000);
	AddServiceReachEntranceRoad(DemolitionCity, 63);
	const uint64 DemolishedServiceId = PlaceServiceReachFixture(DemolitionCity, Service, FCityCellCoord(0, 9));
	DemolitionCity.DemolishEntity(DemolishedServiceId);
	const FCityServiceSnapshot AfterDemolition = FCityServiceEvaluator::BuildSnapshot(DemolitionCity, Tuning);
	TestEqual(TEXT("provider demolition immediately removes service reach"),
	          FCityServiceEvaluator::EvaluateCandidate(DemolitionCity, AfterDemolition, Tuning, TEXT("Home"),
	                                                   FCityCellCoord(12, 9), FIntPoint(1, 1)),
	          ECityOperationFailure::NoService);
	TArray<FCityCellCoord> EdgeRoads;
	FCityServiceEvaluator::GetAdjacentRoadCells(DemolitionCity, FCityCellCoord(62, 9), FIntPoint(2, 2), true,
	                                            EdgeRoads);
	TestTrue(TEXT("board-edge footprint finds in-bounds adjacent road"), EdgeRoads.Contains(FCityCellCoord(62, 8)));
	TestTrue(TEXT("board-edge footprint also finds final-column road"), EdgeRoads.Contains(FCityCellCoord(63, 8)));
	return true;
}

#endif
