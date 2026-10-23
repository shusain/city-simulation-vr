#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityReportHistory.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Persistence/CityPersistence.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMetricTimeTest, "CitySim.Reports.TimeScaleAggregation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityMetricTimeTest::RunTest(const FString&)
{
    FCityReportHistory History;
    FCityEconomySnapshot E;
    FCityPopulationSnapshot P;
    FCityServiceSnapshot Services;
    FCityEnvironmentSnapshot Environment;
    TestTrue(TEXT("Empty history has no fabricated periods"), History.Monthly().IsEmpty() && History.Annual().IsEmpty());
    for (int64 Day = 1; Day <= 301; ++Day)
    {
        E.LastBilledMonthId = P.LastSettledMonthId = Day;
        E.LastIncome = Day;
        E.LastUpkeep = 2;
        P.LastMonthSales = 3;
        TestTrue(TEXT("One daily event record"), History.Capture(E, P, Services, Environment, 1000 + Day));
        TestFalse(TEXT("Duplicate settlement rejected"), History.Capture(E, P, Services, Environment, 0));
    }
    const auto Daily = History.Samples(ECityReportPeriod::Daily);
    const auto Weekly = History.Samples(ECityReportPeriod::Weekly);
    TestEqual(TEXT("Daily view exposes every settled day"), Daily.Num(), 301);
    TestEqual(TEXT("Daily amounts remain unchanged"), Daily[4].Get(ECityReportMetric::Income), int64(5));
    TestEqual(TEXT("Five-day weeks split301days into61weeks"), Weekly.Num(), 61);
    TestEqual(TEXT("First week sums days1through5"), Weekly[0].Get(ECityReportMetric::Income), int64(15));
    TestEqual(TEXT("Second week sums days6through10"), Weekly[1].Get(ECityReportMetric::Income), int64(40));
    TestEqual(TEXT("Weekly snapshots use latest closing balance"), Weekly[0].Get(ECityReportMetric::Treasury), int64(1005));
    TestEqual(TEXT("Week61 has one observed day"), Weekly.Last().ObservedDays, 1);
    TestEqual(TEXT("Partial week is never extrapolated"), Weekly.Last().Get(ECityReportMetric::Income), int64(301));
    int64 FiveWeeks = 0;
    for (int32 I = 0; I < 5; ++I) FiveWeeks += Weekly[I].Get(ECityReportMetric::Income);
    TestEqual(TEXT("Five weeks reconcile to calendar month"), FiveWeeks, History.Monthly()[0].Get(ECityReportMetric::Income));
    auto Partial = History;
    Partial.Months.RemoveAt(0, 2);
    TestEqual(TEXT("Week containing first retained day stays partial"), Partial.Weekly()[0].ObservedDays, 3);
    TestEqual(TEXT("Missing opening days are not invented"), Partial.Weekly()[0].Get(ECityReportMetric::Income), int64(12));
    TestEqual(TEXT("Empty weekly view is empty"), FCityReportHistory().Weekly().Num(), 0);
    const auto Months = History.Monthly();
    const auto Years = History.Annual();
    TestEqual(TEXT("301 days form13 calendar months"), Months.Num(), 13);
    TestEqual(TEXT("First month sums daily event dollars"), Months[0].Get(ECityReportMetric::Income), int64(325));
    TestEqual(TEXT("Second month does not repeat first month"), Months[1].Get(ECityReportMetric::Income), int64(950));
    TestEqual(TEXT("Goods are event quantities, not multiplied by fine steps"), Months[0].Get(ECityReportMetric::GoodsPurchased), int64(75));
    TestEqual(TEXT("Closing balance is never summed"), Months[0].Get(ECityReportMetric::Treasury), int64(1025));
    TestEqual(TEXT("First year covers300 days"), Years[0].ObservedDays, 300);
    TestEqual(TEXT("Full year has12 months"), Years[0].ObservedMonths, 12);
    TestEqual(TEXT("Annual dollars reconcile with all source events"), Years[0].Get(ECityReportMetric::Income), int64(45150));
    TestEqual(TEXT("Next year contains only its one observed day"), Years[1].ObservedDays, 1);
    TestEqual(TEXT("Partial year never extrapolated"), Years[1].Get(ECityReportMetric::Income), int64(301));
    TestEqual(TEXT("Flow unit is dollars, not a monthly rate"), FString(FCityReportHistory::Unit(ECityReportMetric::Income)), FString(TEXT("dollars")));

    // Daily resident observations must survive rollup without replacing all days with month-end mood.
    FCityReportHistory Mood;
    for (int64 Day = 1; Day <= 26; ++Day)
    {
        auto& D = Mood.Months.Emplace_GetRef(); D.MonthId = Day; D.bHasSentiment = D.bHasMigration = true;
        D.Values[static_cast<int32>(ECityReportMetric::Population)] = Day == 26 ? 20 : 10;
        D.Values[static_cast<int32>(ECityReportMetric::HappinessTotal)] = Day == 26 ? 200 : 1000;
    }
    auto Q = FCityQuarterlyNews::Editions(Mood);
    TestEqual(TEXT("Daily resident denominator includes partial month exactly once"), Q[0].ResidentObservations, int64(270));
    TestEqual(TEXT("Resident-day weighted mood uses every observed day"), Q[0].AverageHappiness(), int64(93));
    TestFalse(TEXT("26 days is a partial quarter"), Q[0].IsComplete());
    Mood.Months[0].bHasSentiment = false;
    TestFalse(TEXT("Later month cannot hide missing daily sentiment"), Mood.Annual()[0].bHasSentiment);
    TestFalse(TEXT("Quarter preserves missing daily sentiment"), FCityQuarterlyNews::Editions(Mood)[0].bHasSentiment);
    Mood.Months.RemoveAt(0);
    TestFalse(TEXT("Missing first-day baseline cannot invent quarterly migration"), FCityQuarterlyNews::Editions(Mood)[0].bHasMovement);
    // A retention window starting in the middle of a month counts only retained days.
    History.Months.RemoveAt(0, 5);
    TestEqual(TEXT("Retained partial opening month"), History.Monthly()[0].ObservedDays, 20);
    TestEqual(TEXT("Retained month sums available observations only"), History.Monthly()[0].Get(ECityReportMetric::Income), int64(310));

    TArray<FCityReportSample> Reference;
    for (int32 Speed : {1, 3, 5})
    {
        FCityPlacementModel City(10000);
        FCityEconomySimulation Clock;
        FCityEconomyTuning Tuning;
        FCityPopulationSimulation Population;
        FCityReportHistory Reports;
        Clock.SetSpeedMultiplier(Speed);
        bool Saved = false;
        const int32 Steps = 26 * Tuning.FixedStepsPerMonth;
        for (int32 Frame = 0; Frame < Steps / Speed; ++Frame)
        {
            Clock.Advance(Tuning.FixedStepSeconds, City, Tuning, nullptr, nullptr, &Population.GetSnapshot(), [&]() {
                const auto& State = Clock.GetSnapshot();
                const int64 Previous = Population.GetSnapshot().LastSettledMonthId;
                Population.AdvanceToMonth(City, State, Tuning, &Services, true, true);
                if (Previous != State.LastBilledMonthId)
                    Reports.Capture(State, Population.GetSnapshot(), Services, Environment, City.GetFunds());
            });
            if (!Saved && Clock.GetSnapshot().FixedStepId >= 1949)
            {
                FCitySaveDocument D, Loaded;
                D.Placement = City.ExportState(); D.Economy = Clock.GetSnapshot();
                D.Population = Population.GetSnapshot(); D.Reports = Reports;
                D.Disasters.LastFixedStepId = D.Economy.FixedStepId;
                FString Json, Error;
                const bool Valid = FCityPersistence::Serialize(D, Json, Error) && FCityPersistence::Deserialize(Json, Loaded, Error);
                if (!TestTrue(TEXT("Mid-month metrics serialize/reload"), Valid)) { AddError(Error); return false; }
                if (!TestTrue(TEXT("Restored population"), Population.RestoreState(Loaded.Population, Error))) return false;
                if (!TestTrue(TEXT("Restored clock"), Clock.RestoreState(Loaded.Economy, 0, false, Speed, Tuning, Error))) return false;
                Reports = Loaded.Reports;
                TestFalse(TEXT("Reload does not duplicate last closed day"), Reports.Capture(Loaded.Economy, Loaded.Population, Services, Environment, City.GetFunds()));
                Saved = true;
            }
        }
        TestTrue(TEXT("Save/reload exercised at each speed"), Saved);
        TestEqual(TEXT("Equal simulated duration at1x3x5x"), Clock.GetSnapshot().FixedStepId, int64(Steps));
        TestEqual(TEXT("Every closed day captured after reload"), Reports.Months.Num(), 26);
        TestEqual(TEXT("Daily-to-month coverage"), Reports.Monthly()[1].ObservedDays, 1);
        if (Reference.IsEmpty()) Reference = Reports.Months;
        else for (int32 I = 0; I < Reference.Num() && I < Reports.Months.Num(); ++I)
            for (int32 M = 0; M < static_cast<int32>(ECityReportMetric::Count); ++M)
                TestEqual(TEXT("Exact metrics independent of speed/reload"), Reports.Months[I].Values[M], Reference[I].Values[M]);
        AddInfo(FString::Printf(TEXT("METRICS281 Speed=%d Steps=%lld Days=%d Months=%d Reload=1"), Speed, Clock.GetSnapshot().FixedStepId, Reports.Months.Num(), Reports.Monthly().Num()));
    }
    return true;
}
#endif
