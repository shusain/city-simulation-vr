#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
	FCityBuildingDefinition MakeGrowthDefinition(const FName Id, const int32 Width = 2, const int32 Height = 2,
	                                             const int64 Cost = 0)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(Width, Height);
		Definition.Cost = Cost;
		Definition.LogicalHeightCm = 100.0;
		return Definition;
	}

	uint64 PlaceGrowthFixture(FCityPlacementModel& City, const FCityBuildingDefinition& Definition,
	                          const FCityCellCoord& Anchor)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Anchor);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}

	void AddRoad(FCityPlacementModel& City, const int32 EndX)
	{
		FCityRoadPreview Preview = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Preview, FCityCellCoord(EndX, FCityPlacementModel::EntranceCell.Y));
		City.CommitRoad(Preview);
	}

	void ZoneFootprint(FCityPlacementModel& City, const FCityCellCoord& Anchor, const ECityZoneType Zone)
	{
		for (int32 Y = Anchor.Y; Y < Anchor.Y + 2; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + 2; ++X)
			{
				City.SetZone(FCityCellCoord(X, Y), Zone);
			}
		}
	}

	void SetupGrowthCity(FCityPlacementModel& City, const FCityBuildingDefinition& Power,
	                     const FCityBuildingDefinition& Service)
	{
		AddRoad(City, 63);
		PlaceGrowthFixture(City, Power, FCityCellCoord(0, 9));
		PlaceGrowthFixture(City, Service, FCityCellCoord(2, 9));
		for (int32 Index = 0; Index < 10; ++Index)
		{
			ZoneFootprint(City, FCityCellCoord(4 + Index * 3, 9), ECityZoneType::Residential);
		}
		for (int32 Index = 0; Index < 9; ++Index)
		{
			ZoneFootprint(City, FCityCellCoord(34 + Index * 3, 9), ECityZoneType::Commercial);
		}
	}

	FCityNeedsSnapshot BuildGrowthNeeds(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                                    const FCityEconomyTuning& Tuning, const FCityBuildingDefinition* Home,
	                                    const FCityBuildingDefinition* Shop, const bool bShortfall = false)
	{
		return FCityNeedsEvaluator::BuildSnapshot(City, Services, Tuning, Home, Shop, bShortfall);
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityZoningAndUtilitiesTest, "CitySim.Simulation.ZoningAndUtilities",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityZoningAndUtilitiesTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 100;
	Tuning.PowerCapacityPerBuilding = 200;
	Tuning.UtilityDemandPerBuilding = 10;
	Tuning.WaterCapacityPerTreatment = 1000;
	FCityPlacementModel City(100000);
	AddRoad(City, 30);
	const FCityBuildingDefinition Power = MakeGrowthDefinition(TEXT("Power"), 1, 1);
	const FCityBuildingDefinition Service = MakeGrowthDefinition(TEXT("Service"), 1, 1);
	const FCityBuildingDefinition Home = MakeGrowthDefinition(TEXT("Home"), 1, 1);
	const uint64 PowerId = PlaceGrowthFixture(City, Power, FCityCellCoord(0, 9));
	PlaceGrowthFixture(City, Service, FCityCellCoord(1, 9));

	TArray<uint64> HomeIds;
	for (int32 X = 2; X <= 22; ++X)
	{
		HomeIds.Add(PlaceGrowthFixture(City, Home, FCityCellCoord(X, 9)));
	}
	const FCityServiceSnapshot AtCapacity = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	TestEqual(TEXT("one power building supplies exactly twenty ten-unit homes"), AtCapacity.OperationalHomes, 20);
	TestEqual(TEXT("power use stops at two hundred"), AtCapacity.PowerUsed, 200);
	TestEqual(TEXT("stable entity-ID allocation disables the final home"), AtCapacity.GetFailure(HomeIds.Last()),
	          ECityOperationFailure::PowerCapacity);

	City.DemolishEntity(PowerId);
	const FCityServiceSnapshot WithoutPower = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
	TestEqual(TEXT("utility demolition removes resident eligibility"), WithoutPower.Residents, 0);
	TestEqual(TEXT("existing structures remain and report no power"), WithoutPower.GetFailure(HomeIds[0]),
	          ECityOperationFailure::NoPower);
	ZoneFootprint(City, FCityCellCoord(24, 9), ECityZoneType::Residential);
	FCityGrowthSimulation DisabledGrowth(42);
	const FCityBuildingDefinition GrowthHome = MakeGrowthDefinition(TEXT("Home"), 2, 2, 200);
	const FCityBuildingDefinition GrowthShop = MakeGrowthDefinition(TEXT("Shop"), 2, 2, 300);
	const FCityGrowthResult DisabledResult = DisabledGrowth.AdvanceToStep(
	    25, City, WithoutPower, BuildGrowthNeeds(City, WithoutPower, Tuning, &GrowthHome, &GrowthShop), Tuning,
	    &GrowthHome, &GrowthShop, false);
	TestEqual(TEXT("zoning without an operational power source cannot grow"), DisabledResult.Failure,
	          ECityGrowthFailure::NoEligibleZone);
	TestEqual(TEXT("growth reports the candidate lot's exact power failure"), DisabledResult.BlockingOperationFailure,
	          ECityOperationFailure::NoPower);
	TestTrue(TEXT("growth explanation gives a concrete power action"),
	         FCityGrowthSimulation::DescribeResult(DisabledResult).Contains(TEXT("Power beside connected road")));

	FCityPlacementModel ZoneCity(1000);
	const FCityCellCoord ZonedCell(10, 10);
	TestTrue(TEXT("empty cell accepts free zoning"), ZoneCity.SetZone(ZonedCell, ECityZoneType::Residential));
	const uint64 ZonedHome = PlaceGrowthFixture(ZoneCity, Home, ZonedCell);
	TestEqual(TEXT("building footprint retains zone metadata"), ZoneCity.GetZone(ZonedCell),
	          ECityZoneType::Residential);
	TestFalse(TEXT("occupied cell cannot be repainted"), ZoneCity.SetZone(ZonedCell, ECityZoneType::Commercial));
	ZoneCity.DemolishEntity(ZonedHome);
	TestEqual(TEXT("demolition preserves zone metadata"), ZoneCity.GetZone(ZonedCell), ECityZoneType::Residential);

	FCityPlacementModel RangeCity(1000);
	AddRoad(RangeCity, 20);
	PlaceGrowthFixture(RangeCity, Power, FCityCellCoord(0, 9));
	PlaceGrowthFixture(RangeCity, Service, FCityCellCoord(1, 9));
	Tuning.WaterRangeRoadHops = 2;
	const FCityServiceSnapshot RangeServices = FCityServiceEvaluator::BuildSnapshot(RangeCity, Tuning);
	TestEqual(TEXT("preview rule rejects anchors outside service road-hop reach"),
	          FCityServiceEvaluator::EvaluateCandidate(RangeCity, RangeServices, Tuning, TEXT("Home"),
	                                                   FCityCellCoord(10, 9), FIntPoint(2, 2)),
	          ECityOperationFailure::NoService);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLocalGrowthBlockerTest, "CitySim.Simulation.LocalGrowthBlockers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityLocalGrowthBlockerTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 100;
	const auto Home = MakeGrowthDefinition(TEXT("Home"), 2, 2, 200);
	const auto Shop = MakeGrowthDefinition(TEXT("Shop"), 2, 2, 300);
	const auto Power = MakeGrowthDefinition(TEXT("Power"), 2, 2);
	const auto Service = MakeGrowthDefinition(TEXT("Service"), 2, 2);
	for (const auto Failure :
	     {ECityOperationFailure::FireDamage, ECityOperationFailure::OnFire, ECityOperationFailure::Repairing,
	      ECityOperationFailure::AwaitingFreight, ECityOperationFailure::NoRoad, ECityOperationFailure::NoStaff})
	{
		FCityPlacementModel City(100000);
		SetupGrowthCity(City, Power, Service);
		const uint64 DisabledId = PlaceGrowthFixture(City, Home, {60, 60});
		auto Services = FCityServiceEvaluator::BuildSnapshot(City, Tuning);
		TestTrue(TEXT("retained remote lot is disabled"), Services.bHasDisabledEntities);
		Services.EntityFailures.Add(DisabledId, Failure);
		const auto Needs = BuildGrowthNeeds(City, Services, Tuning, &Home, &Shop);
		TestEqual(TEXT("serviced residential land reports ready"), Needs.ResidentialConstraint,
		          ECityDemandConstraint::None);
		const int64 Funds = City.GetFunds();
		const int32 Count = City.GetEntities().Num();
		FCityGrowthSimulation Growth(42);
		const auto Result =
		    Growth.AdvanceToStep(Tuning.GrowthIntervalSteps, City, Services, Needs, Tuning, &Home, &Shop, false);
		TestTrue(TEXT("local failure cannot veto unrelated ready zoning"), Result.IsSuccess());
		TestEqual(TEXT("only one lot grows"), City.GetEntities().Num(), Count + 1);
		TestEqual(TEXT("private growth does not charge the city"), City.GetFunds(), Funds);
		TestTrue(TEXT("disabled original remains"), City.GetEntities().Contains(DisabledId));
		TestEqual(TEXT("growth does not repair or clear local failure"), Services.GetFailure(DisabledId), Failure);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDeterministicGrowthTest, "CitySim.Simulation.DeterministicGrowth",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityDeterministicGrowthTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.WaterRangeRoadHops = 100;
	Tuning.GrowthIntervalSteps = 25;
	const FCityBuildingDefinition Home = MakeGrowthDefinition(TEXT("Home"), 2, 2, 200);
	const FCityBuildingDefinition Shop = MakeGrowthDefinition(TEXT("Shop"), 2, 2, 300);
	const FCityBuildingDefinition Power = MakeGrowthDefinition(TEXT("Power"), 2, 2);
	const FCityBuildingDefinition Service = MakeGrowthDefinition(TEXT("Service"), 2, 2);
	FCityPlacementModel FirstCity(100000);
	FCityPlacementModel SecondCity(100000);
	SetupGrowthCity(FirstCity, Power, Service);
	SetupGrowthCity(SecondCity, Power, Service);
	FCityGrowthSimulation FirstGrowth(42);
	FCityGrowthSimulation SecondGrowth(42);

	for (int32 Attempt = 1; Attempt <= 19; ++Attempt)
	{
		const int32 FirstCountBefore = FirstCity.GetEntities().Num();
		const FCityServiceSnapshot FirstServices = FCityServiceEvaluator::BuildSnapshot(FirstCity, Tuning);
		const FCityServiceSnapshot SecondServices = FCityServiceEvaluator::BuildSnapshot(SecondCity, Tuning);
		const FCityGrowthResult FirstResult = FirstGrowth.AdvanceToStep(
		    Attempt * Tuning.GrowthIntervalSteps, FirstCity, FirstServices,
		    BuildGrowthNeeds(FirstCity, FirstServices, Tuning, &Home, &Shop), Tuning, &Home, &Shop, false);
		const FCityGrowthResult SecondResult = SecondGrowth.AdvanceToStep(
		    Attempt * Tuning.GrowthIntervalSteps, SecondCity, SecondServices,
		    BuildGrowthNeeds(SecondCity, SecondServices, Tuning, &Home, &Shop), Tuning, &Home, &Shop, false);
		TestTrue(TEXT("adequately serviced zoning grows"), FirstResult.IsSuccess());
		TestEqual(TEXT("growth adds at most one building per interval"), FirstCity.GetEntities().Num(),
		          FirstCountBefore + 1);
		TestEqual(TEXT("same seed repeats the selected X anchor"), FirstResult.Anchor.X, SecondResult.Anchor.X);
		TestEqual(TEXT("same seed repeats the selected Y anchor"), FirstResult.Anchor.Y, SecondResult.Anchor.Y);
		TestEqual(TEXT("same seed repeats the zone sequence"), FirstResult.Zone, SecondResult.Zone);
	}
	const FCityServiceSnapshot MatureCity = FCityServiceEvaluator::BuildSnapshot(FirstCity, Tuning);
	TestEqual(TEXT("valid fixtures reach one hundred residents"), MatureCity.Residents, 100);
	TestEqual(TEXT("growth balances ninety jobs before the tenth home"), MatureCity.Jobs, 90);

	FCityPlacementModel DisconnectedCity(1000);
	ZoneFootprint(DisconnectedCity, FCityCellCoord(10, 10), ECityZoneType::Residential);
	FCityGrowthSimulation DisconnectedGrowth(42);
	const FCityGrowthResult DisconnectedResult = DisconnectedGrowth.AdvanceToStep(
	    25, DisconnectedCity, FCityServiceEvaluator::BuildSnapshot(DisconnectedCity, Tuning),
	    BuildGrowthNeeds(DisconnectedCity, FCityServiceEvaluator::BuildSnapshot(DisconnectedCity, Tuning), Tuning,
	                     &Home, &Shop),
	    Tuning, &Home, &Shop, false);
	TestEqual(TEXT("disconnected zoning does not grow"), DisconnectedResult.Failure,
	          ECityGrowthFailure::NoEligibleZone);
	TestEqual(TEXT("disconnected zoning retains its road blocker"), DisconnectedResult.BlockingOperationFailure,
	          ECityOperationFailure::NoRoad);
	TestTrue(TEXT("disconnected zoning explanation names the white entrance"),
	         FCityGrowthSimulation::DescribeResult(DisconnectedResult).Contains(TEXT("white entrance")));

	FCityPlacementModel MissingServiceCity(1000);
	AddRoad(MissingServiceCity, 20);
	ZoneFootprint(MissingServiceCity, FCityCellCoord(10, 9), ECityZoneType::Residential);
	FCityGrowthSimulation MissingServiceGrowth(42);
	const FCityGrowthResult MissingServiceResult = MissingServiceGrowth.AdvanceToStep(
	    25, MissingServiceCity, FCityServiceEvaluator::BuildSnapshot(MissingServiceCity, Tuning),
	    BuildGrowthNeeds(MissingServiceCity, FCityServiceEvaluator::BuildSnapshot(MissingServiceCity, Tuning), Tuning,
	                     &Home, &Shop),
	    Tuning, &Home, &Shop, false);
	TestEqual(TEXT("road-connected zoning identifies missing service"), MissingServiceResult.BlockingOperationFailure,
	          ECityOperationFailure::NoService);
	TestTrue(TEXT("missing service explanation gives road-hop placement action"),
	         FCityGrowthSimulation::DescribeResult(MissingServiceResult).Contains(TEXT("within its road range")));

	FCityPlacementModel ShortfallCity(100000);
	SetupGrowthCity(ShortfallCity, Power, Service);
	FCityGrowthSimulation ShortfallGrowth(42);
	const FCityGrowthResult ShortfallResult = ShortfallGrowth.AdvanceToStep(
	    25, ShortfallCity, FCityServiceEvaluator::BuildSnapshot(ShortfallCity, Tuning),
	    BuildGrowthNeeds(ShortfallCity, FCityServiceEvaluator::BuildSnapshot(ShortfallCity, Tuning), Tuning, &Home,
	                     &Shop, true),
	    Tuning, &Home, &Shop, true);
	TestEqual(TEXT("unpaid upkeep blocks the next growth attempt"), ShortfallResult.Failure,
	          ECityGrowthFailure::UpkeepShortfall);
	return true;
}

#endif
