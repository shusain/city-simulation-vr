#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Presentation/CityValueControl.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityValueControlLifecycleTest, "CitySim.Presentation.BoundedValueControl",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityValueControlLifecycleTest::RunTest(const FString& Parameters)
{
	FCityBoundedValueControl Control(0, 2500, 100);
	Control.Begin(1000);
	Control.StepBy(2);
	TestEqual(TEXT("step changes only the pending value"), Control.GetPendingValue(), 1200);
	TestEqual(TEXT("original value remains available before commit"), Control.GetOriginalValue(), 1000);
	Control.Cancel();
	TestEqual(TEXT("cancel restores the original value"), Control.GetPendingValue(), 1000);
	TestFalse(TEXT("cancel closes the edit session"), Control.IsEditing());

	Control.Begin(1000);
	Control.StepBy(-100);
	TestEqual(TEXT("steps clamp at the lower bound"), Control.GetPendingValue(), 0);
	Control.SetFromTrack(2.0);
	TestEqual(TEXT("track positions clamp at the upper bound"), Control.GetPendingValue(), 2500);
	Control.SetFromTrack(0.5);
	TestEqual(TEXT("track input snaps to a coarse step"), Control.GetPendingValue(), 1300);
	TestEqual(TEXT("commit returns the pending value"), Control.Commit(), 1300);
	TestEqual(TEXT("commit promotes pending to original"), Control.GetOriginalValue(), 1300);

	TestEqual(TEXT("viewer-left edge maps to the minimum"),
	          FCityHorizontalSliderPresentation::NormalizedFromLocalY(15.0, 8.0, 7.0), 0.0);
	TestEqual(TEXT("viewer-right edge maps to the maximum"),
	          FCityHorizontalSliderPresentation::NormalizedFromLocalY(1.0, 8.0, 7.0), 1.0);
	TestEqual(TEXT("minimum thumb is drawn at viewer-left"),
	          FCityHorizontalSliderPresentation::LocalYFromNormalized(0.0, 8.0, 7.0), 15.0);
	TestEqual(TEXT("maximum thumb is drawn at viewer-right"),
	          FCityHorizontalSliderPresentation::LocalYFromNormalized(1.0, 8.0, 7.0), 1.0);
	return true;
}

#endif
