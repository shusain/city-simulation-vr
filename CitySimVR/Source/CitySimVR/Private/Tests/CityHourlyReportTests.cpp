#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityReportHistory.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Persistence/CityPersistence.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHourlyReportsTest, "CitySim.Reports.HourlyObservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityHourlyReportsTest::RunTest(const FString& Parameters)
{
    FCitySaveDocument D;
    D.Placement.Zones.Init(ECityZoneType::None, FCityGrid::CellCount);
    FCityServiceSnapshot S;
    FCityEnvironmentSnapshot Env;
    auto Advance = [&](FCitySaveDocument& Doc, int64 Step) {
        Doc.Economy.FixedStepId = Doc.Disasters.LastFixedStepId = Step;
        Doc.Economy.MonthId = Doc.Economy.LastBilledMonthId = Step / 150;
        Doc.Population.LastSettledMonthId = Step / 150;
        Doc.Economy.LastIncome = 40;
        Doc.Population.TotalArrivalPurchases = Step; // one real event per fixed step in this fixture
        Doc.Population.ExportsThisMonth = Step % 150;
        Doc.Population.LastMonthExports = Step >= 150 ? 150 : 0;
        Doc.Reports.ObserveHourly(Doc.Economy, Doc.Population, S, Env, 1000 - Step, 150);
    };
    Advance(D, 0);
    for (int64 Step = 1; Step <= 6; ++Step) Advance(D, Step);
    TestEqual(TEXT("No observation before the first completed hour"), D.Reports.Hours.Num(), 0);
    Advance(D, 7);
    TestEqual(TEXT("First real hour at quantized step7"), D.Reports.Hours.Num(), 1);
    if (D.Reports.Hours.IsEmpty()) return false;
    TestEqual(TEXT("Actual purchases in hour1"), D.Reports.Hours[0].Get(ECityReportMetric::GoodsPurchased), int64(7));
    TestEqual(TEXT("Actual closing treasury"), D.Reports.Hours[0].Get(ECityReportMetric::Treasury), int64(993));
    TestEqual(TEXT("Daily income never apportioned across hours"), D.Reports.Hours[0].Get(ECityReportMetric::Income), int64(0));
    for (int64 Step = 8; Step <= 10; ++Step) Advance(D, Step);
    FString Json, Error;
    if (!TestTrue(TEXT("Mid-hour save"), FCityPersistence::Serialize(D, Json, Error))) { AddError(Error); return false; }
    FCitySaveDocument Reload;
    if (!TestTrue(TEXT("Mid-hour load"), FCityPersistence::Deserialize(Json, Reload, Error))) { AddError(Error); return false; }
    for (int64 Step = 11; Step <= 150; ++Step) { Advance(D, Step); Advance(Reload, Step); }
    TestEqual(TEXT("24 actual hours per day"), D.Reports.Hours.Num(), 24);
    int64 Purchases = 0, Exports = 0, Income = 0;
    for (int32 I = 0; I < D.Reports.Hours.Num(); ++I)
    {
        const auto& H = D.Reports.Hours[I];
        Purchases += H.Get(ECityReportMetric::GoodsPurchased);
        Exports += H.Get(ECityReportMetric::GoodsExported);
        Income += H.Get(ECityReportMetric::Income);
        for (int32 M = 0; M < static_cast<int32>(ECityReportMetric::Count); ++M)
            TestEqual(TEXT("Reload preserves every hourly metric"), H.Values[M], Reload.Reports.Hours[I].Values[M]);
        if (I < 23) TestEqual(TEXT("No bill before midnight"), H.Get(ECityReportMetric::Income), int64(0));
    }
    TestEqual(TEXT("Actual purchase event conservation across save/load"), Purchases, int64(150));
    TestEqual(TEXT("Export conservation across daily reset"), Exports, int64(150));
    TestEqual(TEXT("Daily income appears exactly once at midnight"), Income, int64(40));
    TestEqual(TEXT("Midnight bill in hour24"), D.Reports.Hours.Last().Get(ECityReportMetric::Income), int64(40));
    TestFalse(TEXT("Same step cannot duplicate hour"), D.Reports.ObserveHourly(D.Economy, D.Population, S, Env, 850, 150));

    TSharedPtr<FJsonObject> Root;
    TestTrue(TEXT("Parse saved fixture"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root));
    if (!Root.IsValid()) return false;
    Root->RemoveField(TEXT("hourlyReports"));
    FString LegacyJson;
    FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&LegacyJson));
    FCitySaveDocument Legacy;
    TestTrue(TEXT("Old current-schema save without hourly detail loads"), FCityPersistence::Deserialize(LegacyJson, Legacy, Error));
    TestFalse(TEXT("No invented legacy hourly baseline"), Legacy.Reports.bHourlyPrimed);
    Advance(Legacy, 10);
    for (int64 Step = 11; Step <= 13; ++Step) Advance(Legacy, Step);
    TestEqual(TEXT("Only new partial hour recorded"), Legacy.Reports.Hours.Num(), 1);
    if (!Legacy.Reports.Hours.IsEmpty())
    {
        TestEqual(TEXT("Partial hour begins at load"), Legacy.Reports.Hours[0].HourStartStep, int64(10));
        TestEqual(TEXT("Partial hour counts only new events"), Legacy.Reports.Hours[0].Get(ECityReportMetric::GoodsPurchased), int64(3));
    }
    for (int64 Step = 151; Step <= 900; ++Step) Advance(D, Step);
    TestEqual(TEXT("Latest five days retained"), D.Reports.Hours.Num(), FCityReportHistory::MaximumHours);
    TestTrue(TEXT("Retained hours validate"), D.Reports.ValidateHourly(900, 150, Error));
    D.Reports.Hours.Last().HourEndStep--;
    TestFalse(TEXT("Malformed hourly window rejected"), D.Reports.ValidateHourly(900, 150, Error));

    for (const int32 Speed : {1, 3, 5})
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
        Sim->SetSimulationSpeed(Speed);
        Sim->SetSimulationPaused(true);
        Sim->Tick(24.0f);
        TestTrue(TEXT("Pause records no hour"), Sim->GetReportHistory().Hours.IsEmpty());
        Sim->SetSimulationPaused(false);
        for (int32 Tick = 0; Tick < 150 / Speed; ++Tick) Sim->Tick(2.40001f);
        const auto& H = Sim->GetReportHistory();
        TestEqual(TEXT("Subsystem clock captures every hour at each speed"), H.Hours.Num(), 24);
        TestEqual(TEXT("Financial settlement remains daily at each speed"), H.Months.Num(), 1);
        if (!H.Hours.IsEmpty()) TestEqual(TEXT("Last hourly boundary equals actual fixed step"), H.Hours.Last().HourEndStep, int64(150));
        AddInfo(FString::Printf(TEXT("HOURLY281 Speed=%d Hours=%d DailySettlements=%d SaveLoad=continuous"), Speed, H.Hours.Num(), H.Months.Num()));
        World->DestroyWorld(false);
    }
    return true;
}
#endif
