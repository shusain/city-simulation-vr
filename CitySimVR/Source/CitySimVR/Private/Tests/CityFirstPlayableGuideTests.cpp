#include "Simulation/CityFirstPlayableGuide.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFirstPlayableGuideTest, "CitySim.Acceptance.FirstPlayableGuide",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityFirstPlayableGuideTest::RunTest(const FString& Parameters)
{
	FCityFirstPlayableGuideState State;
	TestTrue(TEXT("clean start points to START"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("START")));

	State.ScenarioPhase = ECityScenarioPhase::Running;
	TestTrue(TEXT("running city starts at entrance road"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("white entrance")));

	State.bHasConnectedEntranceRoad = true;
	TestTrue(TEXT("road points to power"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("place POWER")));
	State.BuiltPowerBuildings = 1;
	TestTrue(TEXT("disconnected power explains connection"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("connect POWER")));

	State.OperationalPowerBuildings = 1;
	TestTrue(TEXT("power points to water treatment"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("place WATER TREATMENT")));
	State.BuiltServiceBuildings = 1;
	TestTrue(TEXT("disconnected water explains connection"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("connect WATER TREATMENT")));

	State.OperationalServiceBuildings = 1;
	TestTrue(TEXT("utilities point to residential zoning"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("residential")));
	State.bHasResidentialZone = true;
	State.Residents = 10;
	TestTrue(TEXT("residents without jobs point to commercial zoning"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("commercial")));

	State.bHasCommercialZone = true;
	State.Jobs = 10;
	TestTrue(TEXT("balanced starter city points to population goal"),
	         FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("reach 100")));

	State.Residents = 100;
	State.ProfitableMonthStreak = 2;
	const FString ProfitStep = FCityFirstPlayableGuide::DescribeNextStep(State);
	TestTrue(TEXT("population goal points to profit streak"), ProfitStep.Contains(TEXT("1 more month")));

	State.ScenarioPhase = ECityScenarioPhase::Succeeded;
	TestTrue(TEXT("success points to save"), FCityFirstPlayableGuide::DescribeNextStep(State).Contains(TEXT("SAVE")));
	State.CompletedPurchases = 1;
	State.ScenarioPhase = ECityScenarioPhase::Running;
	TestEqual(TEXT("Established city skips to live stability"), FCityFirstPlayableGuide::BuildObjective(State).Id,
	          FName(TEXT("stability")));
	TestTrue(TEXT("Streak bar uses exact current count"),
	         FMath::IsNearlyEqual(FCityFirstPlayableGuide::BuildObjective(State).Fraction, 2.0f / 3));
	State.OperationalPowerBuildings = 0;
	TestEqual(TEXT("Utility regression takes priority over growth"), FCityFirstPlayableGuide::BuildObjective(State).Id,
	          FName(TEXT("power")));
	TestTrue(TEXT("Built failed plant gets recovery hint"),
	         FCityFirstPlayableGuide::BuildObjective(State).Hint.Contains(TEXT("Inspect")));
	State.OperationalPowerBuildings = 1;
	State.Residents = 0;
	TestEqual(TEXT("Zoning alone does not complete occupancy"), FCityFirstPlayableGuide::BuildObjective(State).Id,
	          FName(TEXT("first_residents")));
	State.Residents = 1;
	State.Jobs = 0;
	TestEqual(TEXT("Commercial zoning alone does not complete jobs"), FCityFirstPlayableGuide::BuildObjective(State).Id,
	          FName(TEXT("first_jobs")));
	State.Jobs = 1;
	State.Residents = 57;
	TestEqual(TEXT("Population bar measures authoritative residents"),
	          FCityFirstPlayableGuide::BuildObjective(State).Fraction, .57f);
	State.bHasResidentialZone = false;
	TestEqual(TEXT("Existing occupied city can bypass zone teaching"),
	          FCityFirstPlayableGuide::BuildObjective(State).Id, FName(TEXT("population")));
	State = {};
	TestEqual(TEXT("Reset returns to start"), FCityFirstPlayableGuide::BuildObjective(State).Id, FName(TEXT("start")));
	return true;
}

#endif
