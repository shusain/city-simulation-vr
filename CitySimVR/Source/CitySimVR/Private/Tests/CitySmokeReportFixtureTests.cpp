#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CitySmokeReportFixture.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySmokeReportFixtureTest,"CitySim.Desktop.SmokeReportFixture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCitySmokeReportFixtureTest::RunTest(const FString& Parameters)
{
    FCityUiReportChart Chart; int64 Period = -123;
    TestFalse(TEXT("Empty chart fails safely"),CitySmokeReports::SelectedPeriod(Chart,Period));
    TestEqual(TEXT("Failed read does not invent a period"),Period,int64(-123));
    Chart.Periods.Add(1); Chart.Values.Add(0); Chart.Bars.Add(0); Chart.Selected=1;
    TestFalse(TEXT("Out-of-bounds selected index rejected"),CitySmokeReports::SelectedPeriod(Chart,Period));
    Chart.Selected=0; Chart.Values.Reset();
    TestFalse(TEXT("Mismatched value array rejected"),CitySmokeReports::SelectedPeriod(Chart,Period));
    Chart.Values.Add(0);
    TestTrue(TEXT("Real selected zero-valued observation remains valid"),CitySmokeReports::SelectedPeriod(Chart,Period));
    const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    auto* Sim=World->GetSubsystem<UCitySimulationSubsystem>(); FString Error;
    Sim->SetSimulationSpeed(3);
    TestTrue(TEXT("Fresh city seeds through real daily settlement hooks"),CitySmokeReports::Seed(*Sim,Error));
    TestEqual(TEXT("Fixture preserves caller speed"),Sim->GetSimulationSpeed(),3);
    TestTrue(TEXT("Fixture preserves caller pause"),Sim->IsSimulationPaused());
    TestEqual(TEXT("Three calendar months of real steps"),Sim->GetEconomySnapshot().FixedStepId,
        int64(Sim->GetEconomyTuning().FixedStepsPerMonth)*FCityCalendar::DaysPerMonth*3);
    const auto& History=Sim->GetReportHistory(); const auto Months=History.Monthly();
    TestEqual(TEXT("75 real daily observations"),History.Months.Num(),FCityCalendar::DaysPerMonth*3);
    TestEqual(TEXT("Three monthly observations"),Months.Num(),3);
    for (const auto& M:Months) TestEqual(TEXT("Every opening fixture month is complete"),M.ObservedDays,FCityCalendar::DaysPerMonth);
    TestEqual(TEXT("Hourly recorder remains bounded"),History.Hours.Num(),FCityReportHistory::MaximumHours);
    Sim->SetEntryMenuSuspended(true); Error.Reset();
    TestFalse(TEXT("Suspended clock fails instead of unbounded wait"),CitySmokeReports::Seed(*Sim,Error));
    TestTrue(TEXT("Fixture failure identifies prerequisite"),Error.Contains(TEXT("did not advance")));
    AddInfo(TEXT("SMOKE279 fixture=75 daily settlements/3 complete months; empty/invalid selection and suspended-clock guards passed"));
    World->DestroyWorld(false); return true;
}
#endif
