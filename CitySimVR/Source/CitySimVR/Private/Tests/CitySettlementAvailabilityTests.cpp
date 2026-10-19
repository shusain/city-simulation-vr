#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHealthSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySettlementAvailabilityTest, "CitySim.Population.SettlementAvailability",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySettlementAvailabilityTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	const auto Place = [&](FName Id, int32 X)
	{
		FCityBuildingDefinition D;
		D.DefinitionId = Id;
		D.DisplayName = Id.ToString();
		D.Footprint = {1, 1};
		auto Preview = City.BeginPreview(D, {X, 1});
		return City.CommitPlacement(Preview, D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), 1), Factory = Place(TEXT("Factory"), 3);
	FCityServiceSnapshot Services;
	Services.EntityResidents.Add(Home, 3);
	Services.EntityJobs.Add(Factory, 2);
	Services.EntityOutput.Add(Factory, 40);
	FCityEconomyTuning Tuning;
	FCityEconomySnapshot Economy;
	Economy.Residents = 3;
	Economy.Jobs = 2;
	Economy.HouseholdGoodsDemand = 2;
	FCityPopulationSimulation Population;
	Population.Reconcile(City, Economy, Tuning, &Services);
	FCityPopulationSimulation Healthy = Population;
	FCityPopulationSimulation::ForecastGoodsSales(City, Economy, Tuning, Services, Population.GetSnapshot(),
	                                              &Economy.HouseholdIncome, &Economy.GoodsProduced, nullptr,
	                                              &Economy.EmployedResidents);
	Economy.MonthId = Economy.LastBilledMonthId = 1;
	Economy.FixedStepId = 25;
	Healthy.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	TestTrue(TEXT("Identical availability retains the full legacy comparison"),
	         Healthy.MatchesEconomy(Economy, Tuning));
	const auto* Worker = Population.GetSnapshot().Citizens.FindByPredicate([&](const auto& C)
	                                                                       { return C.WorkplaceEntityId == Factory; });
	if (!TestNotNull(TEXT("Factory has a real worker"), Worker))
		return false;
	FCityHealthSimulation Health;
	TestTrue(TEXT("Boundary illness starts"), Health.StartIllness(Worker->Id, ECityIllness::Severe));
	Population.SyncHealthAvailability(Health.GetSnapshot());
	Population.AdvanceToMonth(City, Economy, Tuning, &Services, true, true);
	TestEqual(TEXT("One unavailable worker halves actual finite production"),
	          Population.BuildAggregate(Tuning).GoodsProduced, 20);
	TestEqual(TEXT("Pre-care forecast remains independent"), Economy.GoodsProduced, 40);
	TestFalse(TEXT("Different availability cannot satisfy the legacy full comparison"),
	          Population.MatchesEconomy(Economy, Tuning));
	TestTrue(TEXT("Settlement compares actual staffed production and all unchanged finances"),
	         Population.MatchesSettledEconomy(Economy, Tuning));
	for (const int32 Field : {0, 1, 2, 3, 4, 5, 6, 7})
	{
		auto Corrupt = Economy;
		switch (Field)
		{
		case 0:
			++Corrupt.Residents;
			break;
		case 1:
			++Corrupt.Jobs;
			break;
		case 2:
			++Corrupt.EmployedResidents;
			break;
		case 3:
			++Corrupt.HouseholdGoodsDemand;
			break;
		case 4:
			++Corrupt.RetailCapacity;
			break;
		case 5:
			++Corrupt.GoodsSold;
			break;
		case 6:
			++Corrupt.HouseholdIncome;
			break;
		case 7:
			++Corrupt.HouseholdSpending;
			break;
		}
		TestFalse(TEXT("Every non-production inconsistency is still rejected"),
		          Population.MatchesSettledEconomy(Corrupt, Tuning));
	}
	auto Corrupt = Population.GetSnapshot();
	Corrupt.Businesses[0].ProducedGoods++;
	FCityPopulationSimulation Restored;
	FString Error;
	TestTrue(TEXT("Corrupted production remains structurally loadable"), Restored.RestoreState(Corrupt, Error));
	TestFalse(TEXT("Production ledger must match actual finite workforce output"),
	          Restored.MatchesSettledEconomy(Economy, Tuning));
	TestTrue(TEXT("Exact production and wallets resume"), Restored.RestoreState(Population.GetSnapshot(), Error));
	TestTrue(TEXT("Resumed settlement reconciles"), Restored.MatchesSettledEconomy(Economy, Tuning));
	return true;
}
#endif
