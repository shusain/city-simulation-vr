#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityPolicies.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Persistence/CityPersistence.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityScenarioSimulation.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Font.h"

namespace
{
FCityPopulationSnapshot PolicyPeople()
{
	FCityPopulationSnapshot P;
	for (int32 I = 1; I <= 3; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I;
		C.ConsumptionNeed = 1;
		C.Education = static_cast<ECityEducationLevel>(I - 1);
		P.Citizens.Add(C);
	}
	return P;
}
FCityServiceSnapshot PolicySchools()
{
	FCityServiceSnapshot S;
	S.EducationCitizenIds.Add(1);
	S.HigherEducationCitizenIds.Add(3);
	return S;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPolicyLedgerTest, "CitySim.Policies.FundingAndTransfers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPolicyLedgerTest::RunTest(const FString& Parameters)
{
	FCityPolicyState Policy;
	auto P = PolicyPeople();
	auto S = PolicySchools();
	FCityEconomyTuning T;
	Policy.SetEnabled(ECityPolicy::ReserveAssistance, true);
	Policy.SetEnabled(ECityPolicy::SchoolBursaries, true);
	TestEqual(TEXT("Funding quote includes two administrators"), Policy.Forecast(P, S, T), int64(69));
	FCityPlacementModel City(69);
	FString Error;
	TestTrue(TEXT("Monthly funding settles"), Policy.SettleMonth(1, City, P, S, T, true));
	TestEqual(TEXT("Exact treasury debit"), City.GetFunds(), int64(0));
	TestEqual(TEXT("Resident grants are real money"),
	          P.Citizens[0].AvailableMoney + P.Citizens[1].AvailableMoney + P.Citizens[2].AvailableMoney,
	          int64(19));
	TestEqual(TEXT("Paid plus administration conserves treasury"), Policy.Paid(), int64(69));
	TestEqual(TEXT("Served learner combines both policies"), P.Citizens[0].LastPolicyGrant, int64(7));
	TestEqual(TEXT("Unserved learner receives no bursary"), P.Citizens[1].LastPolicyGrant, int64(5));
	TestFalse(TEXT("Same month cannot pay twice"), Policy.SettleMonth(1, City, P, S, T, true));
	TestTrue(TEXT("Funded records validate"), Policy.Validate(1, Error));
	P = PolicyPeople();
	FCityPlacementModel Short(39);
	Policy.SettleMonth(2, Short, P, S, T, true);
	TestEqual(TEXT("Insufficient reserve program skipped entirely"), Policy.Programs[0].GrantsPaid, int64(0));
	TestEqual(TEXT("Later affordable program can run"), Policy.Programs[1].GrantsPaid, int64(4));
	TestEqual(TEXT("No skipped administration charge"), Short.GetFunds(), int64(10));
	TestEqual(TEXT("Warning counts skipped program"), Policy.Unfunded(), int64(1));
	TestEqual(TEXT("Skipped resident gets no fictional support"), P.Citizens[1].LastPolicyGrant, int64(0));
	TestTrue(TEXT("Unfunded record validates"), Policy.Validate(2, Error));
	FCityPlacementModel Empty(100);
	auto None = FCityPopulationSnapshot();
	Policy.SettleMonth(3, Empty, None, S, T, true);
	TestEqual(TEXT("No recipients no admin"), Empty.GetFunds(), int64(100));
	P = PolicyPeople();
	P.Citizens[0].JailStationId = 99;
	P.Citizens[2].bOnMedicalLeave = true;
	TestEqual(TEXT("Unavailable learners excluded"),
	          FCityPolicyState::Quote(ECityPolicy::SchoolBursaries, P, S, T).Eligible, int64(0));
	TestEqual(TEXT("Operating funding outage suspends bursaries"),
	          FCityPolicyState::Quote(ECityPolicy::SchoolBursaries, PolicyPeople(), S, T, false).Total(),
	          int64(0));
	TestEqual(TEXT("No school means no cash award"),
	          FCityPolicyState::Quote(ECityPolicy::SchoolBursaries, PolicyPeople(), {}, T).Total(), int64(0));
	P = PolicyPeople();
	P.Citizens[0].AvailableMoney = 4;
	TestEqual(TEXT("Assistance tops up gap, not universal wage"),
	          FCityPolicyState::Quote(ECityPolicy::ReserveAssistance, P, S, T).Grants, int64(11));
	Policy.SetEnabled(ECityPolicy::ReserveAssistance, false);
	Policy.SetEnabled(ECityPolicy::SchoolBursaries, false);
	Policy.SettleMonth(4, Empty, P, S, T, true);
	TestEqual(TEXT("Disabling stops next monthly cost"), Policy.Paid(), int64(0));
	TestEqual(TEXT("Next month clears old personal grant"), P.Citizens[0].LastPolicyGrant, int64(0));
	FCityScenarioSimulation Scenario;
	Scenario.Start(0);
	FCityEconomySnapshot E;
	E.LastBilledMonthId = 1;
	E.Residents = 100;
	E.LastIncome = 40;
	E.Policies.Programs[0].GrantsPaid = 15;
	E.Policies.Programs[0].AdministrationPaid = 25;
	Scenario.Advance(E);
	TestEqual(TEXT("Policy bill counts against profitable objective"),
	          Scenario.GetSnapshot().ProfitableMonthStreak, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPolicySaveTest, "CitySim.Policies.PersistenceAndNews",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPolicySaveTest::RunTest(const FString& Parameters)
{
	FString Error, Json;
	FCitySaveDocument D;
	FCityPlacementModel City(100);
	auto P = PolicyPeople();
	const auto Schools = PolicySchools();
	D.Economy.Policies.SetEnabled(ECityPolicy::ReserveAssistance, true);
	D.Economy.Policies.SetEnabled(ECityPolicy::SchoolBursaries, true);
	D.Economy.Policies.SettleMonth(1, City, P, Schools, D.DifficultyRules, true);
	D.Economy.LastBilledMonthId = D.Economy.MonthId = 1;
	D.Placement = City.ExportState();
	// Closing empty census can differ from grant recipients after migration.
	D.Population.LastSettledMonthId = 1;
	TestTrue(TEXT("Monthly report captures named transfers"),
	         D.Reports.Capture(D.Economy, D.Population, {}, D.Environment, City.GetFunds()));
	TestEqual(TEXT("Administration report"),
	          D.Reports.Months[0].Get(ECityReportMetric::PolicyAdministrationPaid), int64(50));
	TestEqual(TEXT("Recipient-payments can overlap"),
	          D.Reports.Months[0].Get(ECityReportMetric::PolicyRecipientPayments), int64(5));
	TestTrue(TEXT("Policy report validates after departures"), D.Reports.Validate(1, Error));
	if (!TestTrue(TEXT("Write policies"), FCityPersistence::Serialize(D, Json, Error)))
		return false;
	FCitySaveDocument L;
	if (!TestTrue(TEXT("Read policies"), FCityPersistence::Deserialize(Json, L, Error)))
		return false;
	TestTrue(TEXT("Enabled state survives reload"), L.Economy.Policies.Programs[1].bEnabled);
	TestEqual(TEXT("Paid cost survives reload"), L.Economy.Policies.Paid(), int64(69));
	TestTrue(TEXT("Policy news uses real amounts"),
	         CityNewspaperEdition(L.Reports, 0).Articles.Last().Body.Contains(TEXT("reserve aid $15")));
	TestTrue(TEXT("Annual coverage present"), L.Reports.Annual()[0].bHasPolicies);
	auto Bad = L.Economy.Policies;
	Bad.Programs[0].GrantsPaid = 1;
	TestFalse(TEXT("Partial fictional benefit ledger rejected"), Bad.Validate(1, Error));
	Bad = L.Economy.Policies;
	Bad.LastSettledMonth = 2;
	TestFalse(TEXT("Future policy clock rejected"), Bad.Validate(1, Error));
	L.Reports.Months[0].bHasPolicies = false;
	TestFalse(TEXT("Unknown history cannot contain benefits"), L.Reports.Validate(1, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityPolicyUiTest, "CitySim.Policies.SharedReviewAndPaperBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityPolicyUiTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PolicyReviewTest"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* UI = W->SpawnActor<ACityVRToolController>();
	UI->Board = W->SpawnActor<ACityBoardActor>();
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bPaletteOpen = true;
	const int64 Cash = Sim->GetPlacementModel().GetFunds();
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenPolicies);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewReservePolicy);
	UI->RefreshReports();
	TestEqual(TEXT("Review moves no money"), Sim->GetPlacementModel().GetFunds(), Cash);
	TestFalse(TEXT("Review not enable"), Sim->GetEconomySnapshot().Policies.Programs[0].bEnabled);
	const auto B = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	AddInfo(B.ToString());
	TestTrue(TEXT("Review fits VR sheet"), B.Min.Z > 30 && B.Max.Z < 82 && B.Min.Y >= -47 && B.Max.Y <= 47);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::Back);
	TestEqual(TEXT("Cancel returns to policies"), UI->PalettePage, ECityPalettePage::Policies);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewBursaryPolicy);
	UI->RefreshReports();
	UI->bDesktopUi = true;
	TestTrue(TEXT("Desktop shares real eligibility quote"),
	         UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Eligible now 0")));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ConfirmPolicy);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ConfirmPolicy);
	TestTrue(TEXT("Confirm idempotently enables"), Sim->GetEconomySnapshot().Policies.Programs[1].bEnabled);
	TestEqual(TEXT("Confirm has no immediate bill"), Sim->GetPlacementModel().GetFunds(), Cash);
	for (auto Page : {ECityPalettePage::City, ECityPalettePage::Policies, ECityPalettePage::PolicyReview})
		for (bool Hand : {false, true})
		{
			const auto Entries = FCityPaletteModel::SurfaceEntries(Page, Hand);
			TestTrue(TEXT("Fits shared pool"), Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
			for (int32 I = 0; I < Entries.Num(); ++I)
				for (int32 J = 0; J < I; ++J)
				{
					const auto A = FCityPaletteModel::ButtonRect(Page, I, Entries[I].Action, Hand),
					           C = FCityPaletteModel::ButtonRect(Page, J, Entries[J].Action, Hand);
					TestFalse(TEXT("Policy/City buttons do not overlap"),
					          FMath::Abs(A.Center.Y - C.Center.Y) < A.HalfSize.X + C.HalfSize.X &&
					              FMath::Abs(A.Center.Z - C.Center.Z) < A.HalfSize.Y + C.HalfSize.Y);
				}
		}
	FCityReportHistory H;
	for (int32 M = 1; M <= 3; ++M)
	{
		FCityReportSample S;
		S.MonthId = M;
		S.bHasPolicies = S.bHasLoans = S.bHasSentiment = S.bHasMigration = true;
		auto Set = [&](ECityReportMetric Metric, int64 V) { S.Values[static_cast<int32>(Metric)] = V; };
		Set(ECityReportMetric::Population, 10);
		Set(ECityReportMetric::HappinessTotal, 500);
		Set(ECityReportMetric::Discontented, 4);
		Set(ECityReportMetric::ConcernTaxes, 8);
		Set(ECityReportMetric::ConcernServices, 6);
		Set(ECityReportMetric::Income, 1000);
		Set(ECityReportMetric::Upkeep, 700);
		Set(ECityReportMetric::LoanBorrowed, M == 1 ? 20000 : 0);
		Set(ECityReportMetric::LoanPrincipal, 17500);
		Set(ECityReportMetric::LoanPrincipalPaid, 834);
		Set(ECityReportMetric::LoanInterestPaid, 100);
		Set(ECityReportMetric::ReserveAssistancePaid, 50);
		Set(ECityReportMetric::SchoolBursariesPaid, 20);
		Set(ECityReportMetric::PolicyAdministrationPaid, 50);
		Set(ECityReportMetric::UnfundedPolicies, 1);
		H.Months.Add(S);
	}
	const auto Paper = CityNewspaperEdition(H, 0);
	TestTrue(TEXT("Unfunded policies make factual headline"),
	         Paper.Articles.Last().Headline.Contains(TEXT("falls short")));
	auto Content = MakeShared<FCityUiNewspaper>();
	Content->Masthead = FText::FromString(Paper.Masthead);
	Content->Dateline = FText::FromString(Paper.Dateline);
	Content->Headline = FText::FromString(Paper.Headline);
	Content->Lead = FText::FromString(Paper.Lead);
	Content->Footer = FText::FromString(Paper.Footer);
	for (const auto& Article : Paper.Articles)
	{
		FCityUiNewsArticle A;
		A.Section = FText::FromString(Article.Section);
		A.Headline = FText::FromString(Article.Headline);
		A.Body = FText::FromString(Article.Body);
		Content->Articles.Add(A);
	}
	auto* Font = LoadObject<UFont>(nullptr, TEXT("/Game/CitySim/UI/Fonts/F_CityNewsSerif.F_CityNewsSerif"));
	auto Sheet = CityNewspaper::MakeSheet(Content, Font, 1000, .82f);
	Sheet->SlatePrepass();
	AddInfo(FString::Printf(TEXT("Policy newspaper height %f"), Sheet->GetDesiredSize().Y));
	TestTrue(TEXT("Full loan/policy/concern paper fits"), Sheet->GetDesiredSize().Y <= 760);
	W->DestroyWorld(false);
	return true;
}
#endif
