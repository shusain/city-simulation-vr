#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityInspectionTrends.h"
#include "Presentation/CityUiContent.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Commands/CityPlacementModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityInspectionTrendsTest, "CitySim.Inspection.ObservedResidentTrends",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityInspectionTrendsTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(City.GetEntranceCell()); City.UpdateRoadPreviewEnd(Road, {50, 8}); City.CommitRoad(Road);
	FCityBuildingDefinition D; D.DefinitionId = TEXT("Home"); D.DisplayName = TEXT("Home"); D.Footprint = {1, 1}; D.Cost = 1;
	auto Preview = City.BeginPreview(D, {6, 9}); const auto Placed = City.CommitPlacement(Preview, D);
	if (!TestTrue(TEXT("A real home identity exists"), Placed.IsSuccess())) return false;
	FCityPopulationSnapshot P; FCityCitizenRecord C; C.Id = 1; C.HomeEntityId = Placed.EntityId; C.Happiness = 60; C.HappinessTarget = 90;
	P.Citizens.Add(C);
	FCityInspectionTrends Trends; FCityUiSnapshot View;
	auto Present = [&]() { View.InspectionOverview.SetNum(3); Trends.Present(Placed.EntityId, View); };
	Trends.Observe(City, P, 0, 0, 1); Present();
	TestEqual(TEXT("First sample is unknown, not fake steady"), View.InspectionOverviewIcons[0], FName(TEXT("TrendUnknown")));
	Trends.Observe(City, P, 1, 0, 1); Present();
	TestEqual(TEXT("Unchanged actual happiness stays steady despite higher target"), View.InspectionOverviewIcons[1], FName(TEXT("TrendSteady")));
	C.Id = 2; C.Happiness = 70; P.Citizens.Add(C); P.Citizens[0].Happiness = 70;
	Trends.Observe(City, P, 2, 0, 1); Present();
	TestEqual(TEXT("Population growth is observed"), View.InspectionOverviewIcons[0], FName(TEXT("TrendUp")));
	TestEqual(TEXT("Measured happiness rises"), View.InspectionOverviewIcons[1], FName(TEXT("TrendUp")));
	TestTrue(TEXT("Comparison window is explicit"), View.InspectionOverviewHelp[1].ToString().Contains(TEXT("since game day 1")));
	P.Citizens.RemoveAt(1); P.Citizens[0].Happiness = 50;
	Trends.Observe(City, P, 25, 1, 1); Present();
	TestEqual(TEXT("New-month comparison retains last observed population"), View.InspectionOverviewIcons[0], FName(TEXT("TrendDown")));
	TestEqual(TEXT("Measured happiness declines"), View.InspectionOverviewIcons[1], FName(TEXT("TrendDown")));
	TestTrue(TEXT("Monthly window advances to the previous observation"), View.InspectionOverviewHelp[1].ToString().Contains(TEXT("since game day 3")));
	P.Citizens.Reset(); Trends.Observe(City, P, 26, 1, 1); Present();
	TestEqual(TEXT("An empty home can still show residents departing"), View.InspectionOverviewIcons[0], FName(TEXT("TrendDown")));
	TestTrue(TEXT("An empty home does not fabricate average happiness"), View.InspectionOverview[1].IsEmpty());
	P.Citizens.Add(C);
	Trends.Observe(City, P, 26, 1, 2); Present();
	TestEqual(TEXT("Loading another city resets history even at the same clock"), View.InspectionOverviewIcons[1], FName(TEXT("TrendUnknown")));
	View.InspectionOverview.Reset(); Trends.Present(Placed.EntityId, View);
	TestTrue(TEXT("No resident overview is invented for a hidden panel"), View.InspectionOverview.IsEmpty());
	FCityInspectionTrends Fractional;
	P.Citizens.SetNum(2); P.Citizens[0] = C; P.Citizens[1] = C;
	P.Citizens[1].Id = C.Id + 1;
	P.Citizens[0].Happiness = P.Citizens[1].Happiness = 60;
	Fractional.Observe(City, P, 0, 0, 3); P.Citizens[1].Happiness = 61;
	Fractional.Observe(City, P, 1, 0, 3); View.InspectionOverview.SetNum(3); Fractional.Present(Placed.EntityId, View);
	TestEqual(TEXT("A half-point improvement does not falsely show steady"), View.InspectionOverviewIcons[1], FName(TEXT("TrendUp")));
	return true;
}
#endif
