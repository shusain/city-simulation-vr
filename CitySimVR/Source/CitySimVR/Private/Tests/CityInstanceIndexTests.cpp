#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Presentation/CityInstanceIndex.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityInstanceIndexTest, "CitySim.Presentation.InstanceIndex",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCityInstanceIndexTest::RunTest(const FString& Parameters)
{
	FCityInstanceIndex Index;
	TestEqual(TEXT("first home uses instance zero"), Index.Add(TEXT("Home"), 10), 0);
	TestEqual(TEXT("second home uses instance one"), Index.Add(TEXT("Home"), 20), 1);
	TestEqual(TEXT("another definition owns its own instance list"), Index.Add(TEXT("Shop"), 30), 0);

	TestTrue(TEXT("first home can be removed"), Index.Remove(10));
	const FCityInstanceHandle* ShiftedHome = Index.Find(20);
	TestNotNull(TEXT("remaining home still resolves"), ShiftedHome);
	if (ShiftedHome)
	{
		TestEqual(TEXT("shifted home index is repaired"), ShiftedHome->InstanceIndex, 0);
	}
	TestEqual(TEXT("reverse home lookup is repaired"), Index.GetEntityAt(TEXT("Home"), 0), uint64(20));
	TestEqual(TEXT("other definition lookup is unaffected"), Index.GetEntityAt(TEXT("Shop"), 0), uint64(30));
	TestFalse(TEXT("removed entity no longer resolves"), Index.Find(10) != nullptr);
	TestFalse(TEXT("missing entity cannot be removed"), Index.Remove(999));
	return true;
}

#endif
