#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityUiContent.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityReportHistory.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUiChartTest, "CitySim.Desktop.SharedCharts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUiChartTest::RunTest(const FString& Parameters)
{
	FCityNeedsSnapshot Needs;
	Needs.ResidentialDemandBalance = 20;
	Needs.CommercialDemandBalance = -80;
	Needs.OccupiedResidents = 20;
	Needs.HousingCapacity = 40;
	Needs.PowerUsage = 120;
	Needs.PowerCapacity = 100;
	Needs.bHasDensityDemand = true;
	Needs.ResidentialDensity.Desired[0] = 30;
	Needs.ResidentialDensity.Reserved[2] = 40;
	FCityUiSnapshot Data;
	CityUiContent::NeedsGauges(Needs, Data);
	TestEqual(TEXT("same three VR demand categories"), Data.DemandGauges.Num(), 3);
	TestEqual(TEXT("same five VR capacity categories"), Data.CapacityGauges.Num(), 5);
	TestEqual(TEXT("all family density preferences shown"), Data.DensityGauges.Num(), 9);
	TestEqual(TEXT("demand uses the VR forty-place bound"), Data.DemandGauges[0].Fill, .5);
	TestEqual(TEXT("negative demand is bounded without losing numeric value"), Data.DemandGauges[1].Fill, -1.0);
	TestEqual(TEXT("operational housing utilization"), Data.CapacityGauges[0].Fill, .5);
	TestEqual(TEXT("capacity fill cannot overflow its rail"), Data.CapacityGauges[2].Fill, 1.0);
	TestEqual(TEXT("no capacity is a finite empty bar"), Data.CapacityGauges[3].Fill, 0.0);
	TestEqual(TEXT("high housing surplus is signed"), Data.DensityGauges[2].Fill, -1.0);
	TestEqual(TEXT("empty history cannot invent a chart"),
	          CityUiContent::ReportChart({}, ECityReportMetric::Treasury, 0, false).Bars.Num(), 0);
	TArray<FCityReportSample> Samples;
	for (int32 Index = 0; Index < 30; ++Index)
	{
		FCityReportSample Sample;
		Sample.MonthId = Index + 1;
		Sample.Values[static_cast<int32>(ECityReportMetric::Treasury)] = Index % 2 == 0 ? -100 : 200;
		Samples.Add(Sample);
	}
	const auto Chart = CityUiContent::ReportChart(Samples, ECityReportMetric::Treasury, 29, false);
	TestEqual(TEXT("report window is bounded to twenty-four observations"), Chart.Bars.Num(), 24);
	TestEqual(TEXT("latest selected bar is in its window"), Chart.Selected, 23);
	TestEqual(TEXT("range starts at the real saved period"), Chart.Periods[0], int64(7));
	TestEqual(TEXT("zero line accounts for negative values"), Chart.Zero, 1.0 / 3.0);
	TestEqual(TEXT("negative treasury bar is below zero"), Chart.Bars[0], 0.0);
	TestEqual(TEXT("positive treasury bar reaches its actual scale"), Chart.Bars[1], 1.0);
	const auto Annual = CityUiContent::ReportChart(Samples, ECityReportMetric::Treasury, 0, true);
	TestTrue(TEXT("annual chart identifies years"), Annual.FirstPeriod.ToString().StartsWith(TEXT("Years")));
	for (auto& Sample : Samples)
		Sample.Values[static_cast<int32>(ECityReportMetric::Treasury)] = 0;
	const auto Zero = CityUiContent::ReportChart(Samples, ECityReportMetric::Treasury, 29, false);
	TestEqual(TEXT("all-zero history retains a finite zero line"), Zero.Zero, 0.0);
	TestEqual(TEXT("all-zero history remains visible as zero observations"), Zero.Bars.Num(), 24);
	return true;
}
#endif
