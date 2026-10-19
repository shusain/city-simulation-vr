#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityConditionIndicators.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityHappinessSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionPriorityTest, "CitySim.Indicators.PriorityOverflowAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionPriorityTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	FCityBuildingDefinition Def;
	Def.DefinitionId = TEXT("Home");
	Def.DisplayName = TEXT("Home");
	Def.Footprint = {1, 1};
	Def.Cost = 1;
	TArray<uint64> Ids;
	for (int32 I = 0; I < 24; ++I)
	{
		auto P = City.BeginPreview(Def, {(I % 6) * 10, 2 + (I / 6) * 10});
		Ids.Add(City.CommitPlacement(P, Def).EntityId);
	}
	FCityFireSnapshot Fire;
	FCityCrimeSnapshot Crime;
	FCityPopulationSnapshot People;
	FCityServiceSnapshot Services;
	Fire.LastFixedStepId = Crime.LastFixedStepId = 10;
	for (const auto Id : Ids)
		Fire.DamagedBuildings.Add(Id, {20, -1});
	Fire.RecentIncidents.Add({1, Ids[23], 0, 9, -1});
	Crime.RecentIncidents.Add({1, Ids[22], 9, 14, true});
	Services.EntityFailures.Add(Ids[21], ECityOperationFailure::PowerCapacity);
	int32 Total = 0;
	const auto Rows = FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People, 0, nullptr,
	                                                  &Services, 16, 0, &Total);
	TestEqual(TEXT("Total is preserved before the render cap"), Total, 24);
	TestEqual(TEXT("Pool remains bounded"), Rows.Num(), 16);
	if (Rows.Num() < 2)
		return false;
	TestTrue(TEXT("Critical incident leads without dwell"), Rows[0].bFire);
	TestEqual(TEXT("Fire has its own symbol"), Rows[0].Symbol(), FName(TEXT("Fire")));
	TestTrue(TEXT("Crime leads repair and utility warnings"), Rows[1].bCrime);
	TestTrue(TEXT("Secondary problems remain discoverable"), Rows[1].CauseCount() > 1);
	const auto All = FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People, 0, nullptr,
	                                                 &Services, MAX_int32, 0);
	TestEqual(TEXT("Focus list retains all affected identities"), All.Num(), 24);
	Fire = {};
	Crime = {};
	Services = {};
	auto& C = People.Citizens.Emplace_GetRef();
	C.Id = 1;
	C.HomeEntityId = Ids[0];
	C.WorkplaceEntityId = 0;
	C.LastHappinessMonth = 1;
	TestTrue(TEXT("Joblessness alone is quiet"),
	         FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People).IsEmpty());
	C.DiscontentMonths = FCityHappinessEvaluator::SustainedMonths - 1;
	TestTrue(TEXT("Discontent warning uses the existing sustained threshold"),
	         FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People).IsEmpty());
	++C.DiscontentMonths;
	TestEqual(TEXT("Sustained state appears at native monthly threshold"),
	          FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People)[0].Symbol(),
	          FName(TEXT("Happiness")));
	C.DiscontentMonths = 0;
	Services.EntityFailures.Add(Ids[0], ECityOperationFailure::NoPower);
	TestEqual(
	    TEXT("Current outage is immediate"),
	    FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People, 0, nullptr, &Services)[0]
	        .Symbol(),
	    FName(TEXT("Power")));
	Services.EntityFailures.Reset();
	TestTrue(TEXT("Restored allocation clears without presentation ticking authority"),
	         FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People, 0, nullptr, &Services)
	             .IsEmpty());
	C.HappinessConcerns = static_cast<int32>(ECityHappinessConcern::Purchases);
	TestEqual(TEXT("Settled purchases have a separate marker"),
	          FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People)[0].Symbol(),
	          FName(TEXT("Shop")));
	People.Citizens.Reset();
	People.LastSettledMonthId = 1;
	FCityHousingEvent Event;
	Event.Id = 1;
	Event.SourceHome = Ids[0];
	Event.Month = 1;
	Event.Kind = ECityHousingEventKind::Departed;
	Event.Residents = 2;
	People.HousingEvents.Add(Event);
	const auto Departure = FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People);
	if (!TestEqual(TEXT("Recorded departure survives absent citizens"), Departure.Num(), 1))
		return false;
	TestEqual(TEXT("Departure identifies the source home"), Departure[0].EntityId, Ids[0]);
	TestEqual(TEXT("Departure remains a recorded outcome"), Departure[0].Title(0),
	          FString(TEXT("RESIDENTS LEFT")));
	People.LastSettledMonthId = 2;
	TestTrue(TEXT("Brief housing cue expires at next settled month"),
	         FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, &People).IsEmpty());
	return true;
}
#endif
