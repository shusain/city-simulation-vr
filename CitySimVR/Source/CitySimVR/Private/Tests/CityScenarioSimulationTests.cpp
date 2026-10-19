#include "Simulation/CityScenarioSimulation.h"

#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityScenarioObjectiveTest, "CitySim.Scenario.ObjectiveProgress",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityScenarioObjectiveTest::RunTest(const FString& Parameters)
{
	FCityScenarioSimulation Scenario;
	Scenario.Start(4);
	FCityEconomySnapshot Economy;
	Economy.LastBilledMonthId = 5;
	Economy.Residents = 90;
	Economy.LastIncome = 200;
	Economy.LastUpkeep = 100;
	Scenario.Advance(Economy);
	TestEqual(TEXT("Profit before population goal does not count"), Scenario.GetSnapshot().ProfitableMonthStreak, 0);

	Economy.Residents = 100;
	Economy.LastBilledMonthId = 6;
	Scenario.Advance(Economy);
	TestEqual(TEXT("First eligible profitable month counts"), Scenario.GetSnapshot().ProfitableMonthStreak, 1);
	Scenario.Advance(Economy);
	TestEqual(TEXT("A billed month is observed once"), Scenario.GetSnapshot().ProfitableMonthStreak, 1);

	Economy.LastBilledMonthId = 7;
	Economy.LastIncome = 50;
	Economy.LastUpkeep = 50;
	Scenario.Advance(Economy);
	TestEqual(TEXT("Non-profitable month resets streak"), Scenario.GetSnapshot().ProfitableMonthStreak, 0);

	Economy.LastIncome = 101;
	Economy.LastUpkeep = 100;
	for (int32 Month = 8; Month <= 10; ++Month)
	{
		Economy.LastBilledMonthId = Month;
		Scenario.Advance(Economy);
	}
	TestEqual(TEXT("Three consecutive months complete scenario"), Scenario.GetSnapshot().Phase,
	          ECityScenarioPhase::Succeeded);
	TestEqual(TEXT("Streak is capped at goal"), Scenario.GetSnapshot().ProfitableMonthStreak, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityScenarioRestoreTest, "CitySim.Scenario.RestoreMidObjective",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityScenarioRestoreTest::RunTest(const FString& Parameters)
{
	FCityScenarioSimulation Scenario;
	FCityScenarioSnapshot Saved;
	Saved.Phase = ECityScenarioPhase::Running;
	Saved.ProfitableMonthStreak = 2;
	Saved.LastObservedBilledMonthId = 12;
	FString Error;
	TestTrue(TEXT("Valid mid-objective state restores"), Scenario.Restore(Saved, Error));

	FCityEconomySnapshot Economy;
	Economy.Residents = 120;
	Economy.LastBilledMonthId = 13;
	Economy.LastIncome = 300;
	Economy.LastUpkeep = 200;
	Scenario.Advance(Economy);
	TestEqual(TEXT("Restored streak can complete"), Scenario.GetSnapshot().Phase, ECityScenarioPhase::Succeeded);

	Saved.ProfitableMonthStreak = 4;
	TestFalse(TEXT("Out-of-range progress is rejected"), Scenario.Restore(Saved, Error));
	return true;
}

#endif
