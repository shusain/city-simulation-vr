#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Performance/CityFrameTiming.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFrameTimingTest, "CitySim.Performance.FrameTimingDistribution",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFrameTimingTest::RunTest(const FString& Parameters)
{
	FCityFrameTiming Timing;
	TestEqual(TEXT("Empty timing is unavailable"), Timing.Percentile(95), -1.0);
	Timing.Add(0);
	Timing.Add(-1);
	for (int32 Frame = 1; Frame <= 100; ++Frame)
		Timing.Add(Frame);
	TestEqual(TEXT("Invalid samples do not enter timing"), Timing.Count, uint64(100));
	TestEqual(TEXT("Median retains the main frame distribution"), Timing.Percentile(50), 50.0);
	TestEqual(TEXT("P95 retains tail latency"), Timing.Percentile(95), 95.0);
	TestEqual(TEXT("P99 retains rare hitches"), Timing.Percentile(99), 99.0);
	Timing.Add(2500);
	TestEqual(TEXT("Long hitch retains its actual peak without unbounded allocation"), Timing.PeakMs, 2500.0);
	TestEqual(TEXT("Overflow percentile reports a conservative actual peak"), Timing.Percentile(100), 2500.0);
	TestEqual(TEXT("Histogram storage is fixed"), Timing.Buckets.Num(), FCityFrameTiming::LastBucket + 1);
	return true;
}
#endif
