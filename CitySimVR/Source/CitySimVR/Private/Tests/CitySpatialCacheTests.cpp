#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTerrain.h"

namespace
{
	bool OldRoadAccess(const FCityPlacementModel& City, uint64 Id)
	{
		if (!City.GetEntities().Contains(Id))
			return false;
		for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
			for (int32 X = 0; X < FCityGrid::Width; ++X)
				if (City.GetGrid().GetOccupant({X, Y}) == Id)
					for (const auto& Offset : TArray<FIntPoint>{{0, 1}, {1, 0}, {0, -1}, {-1, 0}})
					{
						const FCityCellCoord Road(X + Offset.X, Y + Offset.Y);
						if (City.IsRoadConnected(Road) && City.SupportsBuildingFrontage(Road))
							return true;
					}
		return false;
	}
	bool SameSet(const TSet<FCityCellCoord>& A, const TSet<FCityCellCoord>& B)
	{
		if (A.Num() != B.Num())
			return false;
		for (const auto& Cell : A)
			if (!B.Contains(Cell))
				return false;
		return true;
	}
	FCityBuildingDefinition Definition(FName Id, FCityCellFootprint Size = {1, 1})
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Id;
		D.DisplayName = Id.ToString();
		D.Footprint = Size;
		D.LogicalHeightCm = 100;
		return D;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySpatialInvalidationTest, "CitySim.Performance.SpatialMutationInvalidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySpatialInvalidationTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview({0, 8});
	Road.End = {28, 8};
	TestTrue(TEXT("Connected main road commits"), City.CommitRoad(Road).IsSuccess());
	const auto D = Definition(TEXT("Home"), {2, 3});
	auto Preview = City.BeginPreview(D, {3, 9}, 1);
	const uint64 Id = City.CommitPlacement(Preview, D).EntityId;
	TestTrue(TEXT("Rotated footprint commits"), Id != 0);
	TestEqual(TEXT("Footprint road access matches full-grid oracle"), City.HasRoadAccess(Id), OldRoadAccess(City, Id));
	TestFalse(TEXT("Unknown entity has no access"), City.HasRoadAccess(987654));
	FCityServiceReach Reach;
	FCityServiceEvaluator::BuildProviderReach(City, Id, 30, true, Reach);
	const auto Before = City.GetSpatialCacheStats();
	City.TrySpendFunds(1);
	City.ApplyMonthlyEconomy(100, 50);
	City.SetZone({20, 9}, ECityZoneType::Residential);
	FCityServiceEvaluator::BuildProviderReach(City, Id, 30, true, Reach);
	TestEqual(TEXT("Funds and zoning reuse stable reach"), City.GetSpatialCacheStats().ReachBuilds, Before.ReachBuilds);
	TestTrue(TEXT("Stable reach cache actually hit"), City.GetSpatialCacheStats().ReachHits > Before.ReachHits);
	TestTrue(TEXT("Disconnect road"), City.RemoveRoad({1, 8}));
	TestFalse(TEXT("Disconnected access refreshes immediately"), City.HasRoadAccess(Id));
	TestEqual(TEXT("Disconnected result matches old scan"), City.HasRoadAccess(Id), OldRoadAccess(City, Id));
	FCityServiceEvaluator::BuildProviderReach(City, Id, 30, true, Reach);
	TestTrue(TEXT("Disconnected reach is empty"), Reach.RoadHopDistances.IsEmpty());
	Road = City.BeginRoadPreview({0, 8});
	Road.End = {2, 8};
	TestTrue(TEXT("Reconnect road"), City.CommitRoad(Road).IsSuccess());
	TestTrue(TEXT("Reconnected building regains access"), City.HasRoadAccess(Id));
	FString Error;
	TestTrue(TEXT("Upgrade adjacent cells to highway"),
	         City.UpgradeRoadCells({{3, 8}, {4, 8}, {5, 8}}, TEXT("Highway"), 0, Error));
	TestFalse(TEXT("Highway removes building frontage"), City.HasRoadAccess(Id));
	TestEqual(TEXT("Changed road class matches old scan"), City.HasRoadAccess(Id), OldRoadAccess(City, Id));
	const uint64 BeforeGrowth = City.GetSpatialCacheStats().Invalidations;
	auto NewPreview = City.BeginPreview(Definition(TEXT("Service")), {10, 9});
	const uint64 NewId = City.CommitPlacement(NewPreview, Definition(TEXT("Service"))).EntityId;
	TestTrue(TEXT("New building commits"), NewId != 0);
	TestTrue(TEXT("Creation invalidates geometry"), City.GetSpatialCacheStats().Invalidations > BeforeGrowth);
	TestEqual(TEXT("New lot access matches oracle"), City.HasRoadAccess(NewId), OldRoadAccess(City, NewId));
	FCityServiceEvaluator::BuildProviderReach(City, NewId, 30, true, Reach);
	City.DemolishEntity(NewId);
	FCityServiceEvaluator::BuildProviderReach(City, NewId, 30, true, Reach);
	TestTrue(TEXT("Demolished provider cannot leave stale reach"), Reach.RoadHopDistances.IsEmpty());
	FCityTerrainHeightfield Terrain;
	TestTrue(TEXT("Terrain generates"), Terrain.Generate(FCityTerrainGenerationParameters(), &Error));
	const uint64 BeforeTerrain = City.GetSpatialCacheStats().Invalidations;
	City.SetTerrain(&Terrain);
	TestTrue(TEXT("Terrain replacement invalidates"), City.GetSpatialCacheStats().Invalidations > BeforeTerrain);
	TestEqual(TEXT("Terrain access matches oracle"), City.HasRoadAccess(Id), OldRoadAccess(City, Id));
	City.SetTerrain(nullptr);
	TMap<FName, FCityBuildingDefinition> Definitions;
	Definitions.Add(D.DefinitionId, D);
	auto Loaded = FCityPlacementModel::CreateValidatedFromState(City.ExportState(), Definitions, Error);
	if (TestNotNull(TEXT("City reload validates"), Loaded.Get()))
	{
		TestEqual(TEXT("Loaded city owns a fresh cache"), Loaded->GetSpatialCacheStats().FrontageHits, uint64(0));
		TestEqual(TEXT("Loaded access matches oracle"), Loaded->HasRoadAccess(Id), OldRoadAccess(*Loaded, Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWaterCacheEquivalenceTest, "CitySim.Water.CachedGeometryLiveAllocation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWaterCacheEquivalenceTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview({0, 8});
	Road.End = {60, 8};
	TestTrue(TEXT("Water main commits"), City.CommitRoad(Road).IsSuccess());
	const auto D = Definition(TEXT("Service"));
	auto P = City.BeginPreview(D, {2, 9});
	const uint64 A = City.CommitPlacement(P, D).EntityId;
	P = City.BeginPreview(D, {35, 9});
	const uint64 B = City.CommitPlacement(P, D).EntityId;
	TestTrue(TEXT("Both sources commit"), A != 0 && B != 0);
	auto Compare = [&](int32 Rules, int32 Capacity, bool bOperational, int32 Pressure, int32 Budget)
	{
		FCityWaterSnapshot Cached;
		Cached.RulesVersion = Rules;
		FCityWaterSource Source;
		Source.EntityId = A;
		Source.Capacity = Capacity;
		Source.bOperational = bOperational;
		Source.SourcePressurePercent = Pressure;
		Cached.Sources.Add(Source);
		Source.EntityId = B;
		Source.Capacity = 7;
		Source.bOperational = true;
		Source.SourcePressurePercent = 120;
		Cached.Sources.Add(Source);
		auto Reference = Cached;
		auto UncachedCity = City;
		UncachedCity.SetSpatialCacheEnabled(false);
		FCityWaterEvaluator::BuildNetwork(City, Cached, Budget);
		FCityWaterEvaluator::BuildNetwork(UncachedCity, Reference, Budget);
		TestTrue(TEXT("Physical pressure equals uncached traversal"),
		         Cached.RoadPressurePercent.OrderIndependentCompareEqual(Reference.RoadPressurePercent));
		TestTrue(TEXT("Active union equals uncached traversal"),
		         SameSet(Cached.ActiveNetworkRoads, Reference.ActiveNetworkRoads));
		TestTrue(TEXT("Potential union equals uncached traversal"),
		         SameSet(Cached.PotentialNetworkRoads, Reference.PotentialNetworkRoads));
		for (const auto& Anchor : TArray<FCityCellCoord>{{3, 9}, {20, 9}, {59, 9}, {0, 20}})
		{
			const uint64 Id = 100 + Anchor.X;
			TestEqual(TEXT("Live allocation equals uncached traversal"),
			          FCityWaterEvaluator::Allocate(City, Cached, Id, Anchor, {1, 1}, 15),
			          FCityWaterEvaluator::Allocate(UncachedCity, Reference, Id, Anchor, {1, 1}, 15));
			TestEqual(TEXT("Shortage cause remains live"), Cached.EntityCauses.FindRef(Id),
			          Reference.EntityCauses.FindRef(Id));
			TestEqual(TEXT("Display pressure remains live"), Cached.EntityPressurePercent.FindRef(Id),
			          Reference.EntityPressurePercent.FindRef(Id));
		}
		for (int32 I = 0; I < Cached.Sources.Num(); ++I)
			TestEqual(TEXT("Finite source budgets conserved"), Cached.Sources[I].Used, Reference.Sources[I].Used);
	};
	Compare(1, 200, true, 100, 32);
	const auto Warm = City.GetSpatialCacheStats();
	Compare(1, 10, true, 100, 32);
	TestEqual(TEXT("Changed capacity reuses geometry"), City.GetSpatialCacheStats().ReachBuilds, Warm.ReachBuilds);
	Compare(1, 0, false, 100, 32);
	Compare(1, 200, true, 120, 32);
	Compare(0, 200, true, 100, 8);
	Compare(0, 200, true, 100, 32);
	City.RemoveRoad({25, 8});
	Compare(1, 200, true, 100, 32);
	City.DemolishEntity(A);
	Compare(1, 200, true, 100, 32);
	return true;
}
#endif
