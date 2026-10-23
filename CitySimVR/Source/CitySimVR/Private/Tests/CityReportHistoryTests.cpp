#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Simulation/CityReportHistory.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Persistence/CityPersistence.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityReportHistoryTest, "CitySim.Reports.SettledHistory",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityReportHistoryTest::RunTest(const FString& Parameters)
{
	FCityPopulationSnapshot ExportState;
	ExportState.NextTransitTripId = 2;
	auto& Business = ExportState.Businesses.Emplace_GetRef();
	Business.EntityId = 1;
	Business.bIndustrial = true;
	auto& Trip = ExportState.TransitTrips.Emplace_GetRef();
	Trip.Id = 1;
	Trip.OriginEntityId = 1;
	Trip.Purpose = ECityTripPurpose::FreightExport;
	Trip.VehicleType = ECityTripVehicleType::FreightTruck;
	Trip.Quantity = 5;
	FCityPopulationSimulation ExportPopulation;
	FString ExportError;
	TestTrue(TEXT("Export fixture restores"), ExportPopulation.RestoreState(ExportState, ExportError));
	ExportPopulation.CompleteTransitTrip(1, 1);
	ExportPopulation.CompleteTransitTrip(1, 1);
	TestEqual(TEXT("Only completed exports counted once"), ExportPopulation.GetSnapshot().ExportsThisMonth,
	          int64(5));
	FCityReportHistory History;
	FCityEconomySnapshot E;
	FCityPopulationSnapshot P;
	FCityServiceSnapshot Services;
	FCityEnvironmentSnapshot Environment;
	TestEqual(TEXT("Empty annual"), History.Annual().Num(), 0);
	TestFalse(TEXT("No unsettled sample"), History.Capture(E, P, Services, Environment, 0));
	E.LastBilledMonthId = P.LastSettledMonthId = 1;
	E.LastIncome = 40;
	P.LastMonthSales = 3;
	P.LastMonthExports = 7;
	auto& C = P.Citizens.Emplace_GetRef();
	C.Education = ECityEducationLevel::Secondary;
	C.WorkplaceEntityId = 2;
	TestTrue(TEXT("Settled sample"), History.Capture(E, P, Services, Environment, -10));
	TestFalse(TEXT("Duplicate sample"), History.Capture(E, P, Services, Environment, 0));
	TestEqual(TEXT("Filled assignments"), History.Months[0].Get(ECityReportMetric::FilledJobs), int64(1));
	TestEqual(TEXT("Staff secondary"), History.Months[0].Get(ECityReportMetric::StaffSecondary), int64(1));
	TestEqual(TEXT("Completed exports"), History.Months[0].Get(ECityReportMetric::GoodsExported), int64(7));
	E.LastBilledMonthId = P.LastSettledMonthId = 2;
	History.Capture(E, P, Services, Environment, 5);
	auto Annual = History.Annual();
	TestEqual(TEXT("Partial year observed months"), Annual[0].ObservedMonths, 1);
	TestEqual(TEXT("Annual income sums flows"), Annual[0].Get(ECityReportMetric::Income), int64(80));
	TestEqual(TEXT("Annual population final snapshot"), Annual[0].Get(ECityReportMetric::Population),
	          int64(1));
	TestEqual(TEXT("Annual treasury final balance"), Annual[0].Get(ECityReportMetric::Treasury), int64(5));
	FCitySaveDocument D;
	D.Placement.Zones.Init(ECityZoneType::None, FCityGrid::CellCount);
	D.Economy.LastBilledMonthId = D.Economy.MonthId = 2;
	D.Economy.FixedStepId = D.Disasters.LastFixedStepId = 2 * FCityCalendar::DefaultStepsPerDay;
	D.Population.LastSettledMonthId = 2;
	D.Reports = History;
	D.Population.ExportsThisMonth = 4;
	FString Json, Error;
	TestTrue(TEXT("Serialize history"), FCityPersistence::Serialize(D, Json, Error));
	FCitySaveDocument Loaded;
	TestTrue(TEXT("Deserialize history"), FCityPersistence::Deserialize(Json, Loaded, Error));
	if (!Error.IsEmpty())
		AddInfo(Error);
	TestEqual(TEXT("History survives load"), Loaded.Reports.Months.Num(), 2);
	TestEqual(TEXT("Pending exports survive load"), Loaded.Population.ExportsThisMonth, int64(4));
	TestFalse(TEXT("Load cannot repeat settled month"),
	          Loaded.Reports.Capture(E, P, Services, Environment, 5));
	for (int64 Month = 3; Month <= FCityReportHistory::MaximumMonths + 5; ++Month)
	{
		E.LastBilledMonthId = P.LastSettledMonthId = Month;
		History.Capture(E, P, Services, Environment, 0);
	}
	TestEqual(TEXT("Bounded months"), History.Months.Num(), FCityReportHistory::MaximumMonths);
	TestEqual(TEXT("Oldest retained"), History.Months[0].MonthId, int64(6));
	TestTrue(TEXT("Valid bounded history"), History.Validate(FCityReportHistory::MaximumMonths + 5, Error));
	History.Months[1].MonthId = History.Months[0].MonthId;
	TestFalse(TEXT("Duplicate persisted month rejected"), History.Validate(FCityReportHistory::MaximumMonths + 5, Error));
	// The actual clock executes a callback for every fixed step even during catch-up.
	auto RunClock = [](double Delta, int32 Count) {
		FCityPlacementModel City(10000);
		FCityEconomySimulation Economy;
		FCityEconomyTuning Tuning;
		FCityPopulationSimulation Population;
		FCityReportHistory Reports;
		FCityServiceSnapshot S;
		FCityEnvironmentSnapshot Env;
		for (int32 I = 0; I < Count; ++I)
			Economy.Advance(Delta, City, Tuning, nullptr, nullptr, &Population.GetSnapshot(), [&]() {
				const auto& Snapshot = Economy.GetSnapshot();
				const int64 Previous = Population.GetSnapshot().LastSettledMonthId;
				Population.AdvanceToMonth(City, Snapshot, Tuning, &S, true, true);
				if (Previous != Snapshot.LastBilledMonthId)
					Reports.Capture(Snapshot, Population.GetSnapshot(), S, Env, City.GetFunds());
			});
		return Reports;
	};
	auto Fine = RunClock(2.4, 600);
	auto Coarse = RunClock(24.0, 60);
	TestEqual(TEXT("Fine monthly sample count"), Fine.Months.Num(), 4);
	TestEqual(TEXT("Catchup monthly count"), Coarse.Months.Num(), Fine.Months.Num());
	if (Fine.Months.Num() == Coarse.Months.Num())
		for (int32 I = 0; I < Fine.Months.Num(); ++I)
		{
			TestEqual(TEXT("Catchup month identity"), Fine.Months[I].MonthId, Coarse.Months[I].MonthId);
			for (int32 M = 0; M < static_cast<int32>(ECityReportMetric::Count); ++M)
				TestEqual(TEXT("Catchup exact metric"), Fine.Months[I].Values[M], Coarse.Months[I].Values[M]);
		}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityReportsPanelTest, "CitySim.Reports.Panel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityReportsPanelTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ReportsPanelTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* CameraOwner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	UI->ShowReports();
	TestEqual(TEXT("Monthly is the default view"), UI->ReportPeriod, ECityReportPeriod::Monthly);
	TestTrue(TEXT("Report drawer opens"), UI->ReportText->IsVisible());
	TestTrue(TEXT("No fabricated chart in empty city"),
	         UI->ReportText->Text.ToString().Contains(TEXT("No settled days")) &&
	         UI->ReportText->Text.ToString().Contains(TEXT("Charts appear after the first daily settlement")) &&
	         UI->ReportText->Text.ToString().Contains(TEXT("at midnight")));
	UI->ShowReports(ECityReportPeriod::Hourly);
	TestTrue(TEXT("Hourly empty state explains its own boundary"), UI->ReportText->Text.ToString().Contains(TEXT("next completed game hour")));
	int32 VisiblePeriods = 0;
	for (const auto& Entry : UI->VisiblePaletteEntries)
	{
		TestFalse(TEXT("Annual is absent from selector"), Entry.Action == ECityPaletteAction::ReportAnnual);
		TestFalse(TEXT("Housing stays outside chart reports"), Entry.Action == ECityPaletteAction::OpenHousingHistory);
		if (Entry.Action == ECityPaletteAction::ReportHourly || Entry.Action == ECityPaletteAction::ReportDaily ||
		    Entry.Action == ECityPaletteAction::ReportWeekly || Entry.Action == ECityPaletteAction::ReportMonthly) ++VisiblePeriods;
	}
	TestEqual(TEXT("Four visible report period choices"), VisiblePeriods, 4);
	UI->ShowReports();
	FCityEconomySnapshot E;
	FCityPopulationSnapshot P;
	FCityServiceSnapshot S;
	FCityEnvironmentSnapshot Env;
	for (int64 M = 1; M <= 750; ++M)
	{
		E.LastBilledMonthId = P.LastSettledMonthId = M;
		E.LastIncome = M;
		Sim->Reports.Capture(E, P, S, Env, M % 2 ? -100 : 100);
	}
	for (int32 Category = 0; Category < 5; ++Category)
	{
		UI->ReportCategory = Category;
		UI->RefreshPaletteText();
		TestTrue(TEXT("Exact report text is visible"), UI->ReportText->IsVisible());
		TestTrue(TEXT("Report text stays inside background"),
		         UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox().Max.Y < 37);
		TestEqual(TEXT("Chart component budget"), UI->ReportBars.Num(), 24);
	}
	UI->ReportCategory = 2;
	UI->ReportMetricIndex = 0;
	UI->RefreshPaletteText();
	TestTrue(TEXT("Negative treasury range is visible"),
	         UI->ReportRangeText->Text.ToString().Contains(TEXT("-100 to 100")));
	UI->ReportPeriodIndex = 0;
	UI->RefreshPaletteText();
	TestTrue(TEXT("Oldest period is accessible"), UI->ReportText->Text.ToString().Contains(TEXT("Month 1")));
	for (int32 Index = 0; Index < UI->VisiblePaletteEntries.Num(); ++Index)
	{
		const auto Rect = UI->GetPaletteButtonRect(Index, UI->VisiblePaletteEntries[Index].Action);
		const auto Normal = UI->PaletteRoot->GetForwardVector();
		const auto Point = UI->PaletteRoot->GetComponentTransform().TransformPosition(Rect.Center);
		TestEqual(TEXT("Report controls resolve independently"),
		          UI->FindPaletteButton(Point + Normal * 40, -Normal), Index);
	}
	UI->ShowReports(ECityReportPeriod::Annual);
	TestTrue(TEXT("Annual partial year count"),
	         UI->ReportText->Text.ToString().Contains(TEXT("150/300 days observed")));
	TestTrue(TEXT("Annual coverage includes actual daily observations"), UI->ReportText->Text.ToString().Contains(TEXT("150/300 days")));
	UI->ReportMetricIndex = 1; // Tax income: a sum, not a monthly rate.
	UI->RefreshPaletteText();
	TestTrue(TEXT("Annual flow displays observed total with correct units"),
	         UI->ReportText->Text.ToString().Contains(TEXT("dollars")) &&
	         UI->ReportText->Text.ToString().Contains(TEXT("observed total")) &&
	         !UI->ReportText->Text.ToString().Contains(TEXT("/month")));

	// Real quantized hour boundaries, using a separate synthetic presentation source.
	FCityEconomySnapshot HourE; FCityPopulationSnapshot HourP;
	Sim->Reports.ObserveHourly(HourE, HourP, S, Env, 100, 150);
	for (int64 Step = 1; Step <= 19; ++Step)
	{ HourE.FixedStepId = Step; Sim->Reports.ObserveHourly(HourE, HourP, S, Env, 100 + Step, 150); }
	const auto InvokePeriod = [&](ECityPaletteAction Action) { UI->ExecuteSharedPaletteAction(Action); UI->RefreshPaletteText(); };
	const ECityPaletteAction PeriodActions[] = {ECityPaletteAction::ReportHourly, ECityPaletteAction::ReportDaily,
	    ECityPaletteAction::ReportWeekly, ECityPaletteAction::ReportMonthly};
	const ECityReportPeriod Periods[] = {ECityReportPeriod::Hourly, ECityReportPeriod::Daily,
	    ECityReportPeriod::Weekly, ECityReportPeriod::Monthly};
	for (int32 I = 0; I < 4; ++I)
	{
		InvokePeriod(PeriodActions[I]);
		TestEqual(TEXT("Direct selector chooses requested period"), UI->ReportPeriod, Periods[I]);
		TestEqual(TEXT("Changing period retains metric"), UI->ReportMetricIndex, 1);
		int32 SelectedCount = 0;
		for (auto Action : PeriodActions) SelectedCount += UI->IsPaletteActionSelected(Action);
		TestEqual(TEXT("Exactly one period option selected"), SelectedCount, 1);
		const auto Samples = Sim->Reports.Samples(Periods[I]);
		TestEqual(TEXT("Period switch selects latest observation"), UI->ReportPeriodIndex, Samples.Num() - 1);
		TestTrue(TEXT("Report names the selected period"), UI->ReportText->Text.ToString().StartsWith(FCityReportHistory::PeriodName(Periods[I])));
		InvokePeriod(ECityPaletteAction::ReportPeriodPrevious);
		TestEqual(TEXT("Previous navigates selected period"), UI->ReportPeriodIndex, Samples.Num() - 2);
		InvokePeriod(ECityPaletteAction::ReportPeriodNext);
		InvokePeriod(ECityPaletteAction::ReportPeriodNext);
		TestEqual(TEXT("Next clamps to latest recorded period"), UI->ReportPeriodIndex, Samples.Num() - 1);
	}
	// Exercise the actual subsystem settlement hook and generate an isolated PIE fixture.
	Sim->Reports = FCityReportHistory();
	Sim->Economy.SetPaused(false);
	for (int32 I = 0; I < 4500; ++I)
		Sim->Tick(2.4f);
	TestEqual(TEXT("Subsystem records every settled month"), Sim->Reports.Months.Num(), 30);
	Sim->Economy.SetPaused(true);
	Sim->Tick(360);
	TestEqual(TEXT("Paused subsystem never samples"), Sim->Reports.Months.Num(), 30);
	auto Document = Sim->BuildSaveDocument();
	// Explicit synthetic chart values are only for visual layout QA, never player history.
	for (auto& M : Document.Reports.Months)
	{
		const int64 People = M.MonthId * 4;
		M.Values[static_cast<int32>(ECityReportMetric::Population)] = People;
		M.Values[static_cast<int32>(ECityReportMetric::ResidentBasic)] = People;
		M.Values[static_cast<int32>(ECityReportMetric::ResidentEducated)] = People;
		M.Values[static_cast<int32>(ECityReportMetric::Unemployed)] = People;
		M.Values[static_cast<int32>(ECityReportMetric::Treasury)] = (M.MonthId - 10) * 250;
		M.Values[static_cast<int32>(ECityReportMetric::Income)] = M.MonthId * 20;
	}
	FString FixtureJson, FixtureError;
	TestTrue(TEXT("Visual fixture serializes"),
	         FCityPersistence::Serialize(Document, FixtureJson, FixtureError));
	const FString FixturePath = FPaths::ConvertRelativePathToFull(
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/ReportHistory-ReportsFixture.json")));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FixturePath), true);
	TestTrue(TEXT("Isolated visual fixture saved"), FFileHelper::SaveStringToFile(FixtureJson, *FixturePath));
	UI->SetPaletteOpen(false);
	TestFalse(TEXT("Dismissed report text hidden"), UI->ReportText->IsVisible());
	TestFalse(TEXT("Dismissed chart hidden"), UI->ReportBars[0]->IsVisible());
	World->DestroyWorld(false);
	return true;
}
#endif
