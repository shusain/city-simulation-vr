#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/CityPlacementModel.h"
#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"

namespace
{
	FCityBuildingDefinition MakeEconomyBuilding(const FName Id)
	{
		FCityBuildingDefinition Definition;
		Definition.DefinitionId = Id;
		Definition.DisplayName = Id.ToString();
		Definition.Footprint = FCityCellFootprint(1, 1);
		Definition.Cost = 0;
		Definition.LogicalHeightCm = 100.0;
		return Definition;
	}

	uint64 Place(FCityPlacementModel& City, const FCityBuildingDefinition& Definition, const FCityCellCoord& Cell)
	{
		FCityPlacementPreview Preview = City.BeginPreview(Definition, Cell);
		return City.CommitPlacement(Preview, Definition).EntityId;
	}

	void AddConnectedRoad(FCityPlacementModel& City)
	{
		FCityRoadPreview Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, FCityCellCoord(6, FCityPlacementModel::EntranceCell.Y));
		City.CommitRoad(Road);
	}

	void AddStandardFixtures(FCityPlacementModel& City)
	{
		AddConnectedRoad(City);
		Place(City, MakeEconomyBuilding(TEXT("Power")), FCityCellCoord(1, 9));
		Place(City, MakeEconomyBuilding(TEXT("Service")), FCityCellCoord(4, 9));
		Place(City, MakeEconomyBuilding(TEXT("Home")), FCityCellCoord(3, 9));
		Place(City, MakeEconomyBuilding(TEXT("Shop")), FCityCellCoord(2, 9));
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEconomyFixedStepTest, "CitySim.Simulation.FixedStepEconomy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityEconomyFixedStepTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.FixedStepSeconds = 0.2;
	Tuning.FixedStepsPerMonth = 5;
	Tuning.MaxCatchUpSteps = 25;

	FCityPlacementModel FineCity(10000);
	FCityPlacementModel CoarseCity(10000);
	AddStandardFixtures(FineCity);
	AddStandardFixtures(CoarseCity);
	FCityEconomySimulation FineSimulation;
	FCityEconomySimulation CoarseSimulation;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		FineSimulation.Advance(0.02, FineCity, Tuning);
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		CoarseSimulation.Advance(0.2, CoarseCity, Tuning);
	}

	const FCityEconomySnapshot& Fine = FineSimulation.GetSnapshot();
	const FCityEconomySnapshot& Coarse = CoarseSimulation.GetSnapshot();
	TestEqual(TEXT("render-rate variants execute the same fixed steps"), Fine.FixedStepId, Coarse.FixedStepId);
	TestEqual(TEXT("render-rate variants reach the same month"), Fine.MonthId, Coarse.MonthId);
	TestEqual(TEXT("render-rate variants produce the same treasury"), FineCity.GetFunds(), CoarseCity.GetFunds());
	TestEqual(TEXT("one home supplies ten fixture residents"), Fine.Residents, 10);
	TestEqual(TEXT("one road-connected shop supplies ten jobs"), Fine.Jobs, 10);
	TestEqual(TEXT("employed residents earn household income"), Fine.HouseholdIncome, int64(50));
	TestEqual(TEXT("shops cannot sell without industrial goods"), Fine.GoodsSold, 0);
	TestEqual(TEXT("monthly income follows realized household wages and sales"), Fine.LastIncome, int64(50));
	TestEqual(TEXT("residential tax is billed separately"), Fine.LastResidentialTaxRevenue, int64(50));
	TestEqual(TEXT("commercial tax requires realized sales"), Fine.LastCommercialTaxRevenue, int64(0));
	TestEqual(TEXT("monthly upkeep includes roads, home, shop, power, and service"), Fine.LastUpkeep, int64(102));

	const int64 FundsBeforePause = FineCity.GetFunds();
	const int64 StepsBeforePause = Fine.FixedStepId;
	FineSimulation.SetPaused(true);
	FineSimulation.Advance(50.0, FineCity, Tuning);
	TestEqual(TEXT("pause does not advance the clock"), FineSimulation.GetSnapshot().FixedStepId, StepsBeforePause);
	TestEqual(TEXT("pause does not alter the treasury"), FineCity.GetFunds(), FundsBeforePause);

	FineSimulation.SetPaused(false);
	FineSimulation.SetSpeedMultiplier(3);
	FineSimulation.Advance(1.0, FineCity, Tuning);
	TestEqual(TEXT("accelerated time crosses exactly three additional months"),
	          FineSimulation.GetSnapshot().LastBilledMonthId, int64(5));
	TestEqual(TEXT("each accelerated month bills once"), FineCity.GetFunds(), FundsBeforePause - 3 * 52);

	FCityPlacementModel TaxCity(10000);
	AddStandardFixtures(TaxCity);
	FCityEconomySimulation TaxSimulation;
	TestTrue(TEXT("bounded tax policy accepts valid rates"), TaxSimulation.SetTaxRates(500, 1500));
	TestFalse(TEXT("bounded tax policy rejects invalid rates"), TaxSimulation.SetTaxRates(-100, 1000));
	TaxSimulation.Advance(1.0, TaxCity, Tuning);
	TestEqual(TEXT("residential rate deterministically scales integer billing"),
	          TaxSimulation.GetSnapshot().LastResidentialTaxRevenue, int64(25));
	TestEqual(TEXT("commercial rate cannot tax unavailable goods"),
	          TaxSimulation.GetSnapshot().LastCommercialTaxRevenue, int64(0));
	TestEqual(TEXT("separate rates produce deterministic realized income"), TaxSimulation.GetSnapshot().LastIncome,
	          int64(25));

	FCityPlacementModel GoodsCity(10000);
	AddStandardFixtures(GoodsCity);
	Place(GoodsCity, MakeEconomyBuilding(TEXT("Factory")), FCityCellCoord(5, 9));
	FCityEconomySimulation GoodsSimulation;
	GoodsSimulation.Advance(1.0, GoodsCity, Tuning);
	const FCityEconomySnapshot& Goods = GoodsSimulation.GetSnapshot();
	TestEqual(TEXT("resident consumption creates five goods orders"), Goods.HouseholdGoodsDemand, 5);
	TestEqual(TEXT("factory supplies authored aggregate goods"), Goods.GoodsProduced, 20);
	TestEqual(TEXT("shop supplies retail capacity"), Goods.RetailCapacity, 10);
	TestEqual(TEXT("five goods flow from industry through shops to households"), Goods.GoodsSold, 5);
	TestEqual(TEXT("household spending follows completed purchases"), Goods.HouseholdSpending, int64(25));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEconomyBoundsTest, "CitySim.Simulation.EconomyBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityEconomyBoundsTest::RunTest(const FString& Parameters)
{
	FCityEconomyTuning Tuning;
	Tuning.FixedStepSeconds = 0.2;
	Tuning.FixedStepsPerMonth = 5;
	Tuning.MaxCatchUpSteps = 4;

	FCityPlacementModel HitchCity(1000);
	FCityEconomySimulation HitchSimulation;
	HitchSimulation.Advance(1000.0, HitchCity, Tuning);
	TestEqual(TEXT("one hitch executes at most the configured steps"), HitchSimulation.GetSnapshot().FixedStepId,
	          int64(4));
	TestTrue(TEXT("hitch backlog is capped below one additional fixed step"),
	         HitchSimulation.GetAccumulatorSeconds() < Tuning.FixedStepSeconds);
	Tuning.MaxCatchUpSteps = 5;

	FCityPlacementModel PoorCity(0);
	Place(PoorCity, MakeEconomyBuilding(TEXT("Power")), FCityCellCoord(10, 10));
	FCityEconomySimulation PoorSimulation;
	PoorSimulation.Advance(1.0, PoorCity, Tuning);
	TestEqual(TEXT("unpaid upkeep never makes treasury negative"), PoorCity.GetFunds(), int64(0));
	TestEqual(TEXT("unpaid upkeep is recorded"), PoorSimulation.GetSnapshot().UpkeepShortfall, int64(50));
	TestTrue(TEXT("shortfall blocks growth for the following month"),
	         PoorSimulation.GetSnapshot().bGrowthBlockedByShortfall);

	FCityPlacementModel RichCity(MAX_int64 - 10);
	RichCity.ApplyMonthlyEconomy(50, Tuning.HomeUpkeep);
	TestEqual(TEXT("income saturates instead of overflowing"), RichCity.GetFunds(), MAX_int64 - Tuning.HomeUpkeep);
	TestEqual(TEXT("default-rate tax scaling preserves a saturated base"),
	          FCityEconomySimulation::CalculateTaxRevenue(MAX_int64, 1, FCityEconomySnapshot::DefaultTaxBasisPoints),
	          MAX_int64);
	return true;
}

#endif
