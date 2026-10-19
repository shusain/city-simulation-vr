#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityScenarioSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Engine/Font.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLoanSettlementTest, "CitySim.Loans.SettlementAndArrears",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLoanSettlementTest::RunTest(const FString& Parameters)
{
	FString Error;
	FCityPlacementModel City(0);
	FCityEconomySimulation Economy;
	FCityEconomyTuning Tuning;
	Tuning.FixedStepSeconds = .125;
	Tuning.FixedStepsPerMonth = 4;
	TestTrue(TEXT("Loan issues principal"), Economy.Borrow(City, 5000, Error));
	TestEqual(TEXT("Lender credits treasury"), City.GetFunds(), int64(5000));
	TestFalse(TEXT("One active contract"), Economy.Borrow(City, 10000, Error));
	TestEqual(TEXT("Loan is not tax"), Economy.GetSnapshot().LastIncome, int64(0));
	TestEqual(TEXT("First due"), Economy.GetProjectedBudget(City, Tuning).DebtService, int64(234));
	Economy.Advance(.125, City, Tuning);
	TestEqual(TEXT("No off-month charge"), City.GetFunds(), int64(5000));
	Economy.SetPaused(true);
	Economy.Advance(10, City, Tuning);
	TestEqual(TEXT("Paused clock does not charge"), City.GetFunds(), int64(5000));
	Economy.SetPaused(false);
	Economy.Advance(.375, City, Tuning);
	TestEqual(TEXT("First principal installment"), Economy.GetSnapshot().Loan.Principal, int64(4791));
	TestEqual(TEXT("First interest"), Economy.GetSnapshot().Loan.LastInterestPaid, int64(25));
	TestEqual(TEXT("First source flow"), Economy.GetSnapshot().Loan.LastBorrowed, int64(5000));
	TestEqual(TEXT("Exact cash debit"), City.GetFunds(), int64(4766));
	TestTrue(TEXT("Spend remaining cash"), City.TrySpendFunds(City.GetFunds()));
	Economy.Advance(.5, City, Tuning);
	const auto& Missed = Economy.GetSnapshot().Loan;
	TestEqual(TEXT("Missed principal remains"), Missed.Principal, int64(4791));
	TestEqual(TEXT("Missed interest remains"), Missed.InterestDue, int64(24));
	TestEqual(TEXT("Missed dues retained"), Missed.Arrears(), int64(232));
	City.ApplyMonthlyEconomy(10, 0);
	Economy.Advance(.5, City, Tuning);
	TestEqual(TEXT("Partial cash pays interest first"), Economy.GetSnapshot().Loan.InterestDue, int64(38));
	TestEqual(TEXT("No principal forgiveness"), Economy.GetSnapshot().Loan.Principal, int64(4791));
	TestTrue(TEXT("Arrears clock validates"), Economy.GetSnapshot().Loan.Validate(3, Error));
	City.ApplyMonthlyEconomy(20000, 0);
	const int64 Before = City.GetFunds(), Payoff = Economy.GetSnapshot().Loan.Payoff();
	TestTrue(TEXT("Full early payoff"), Economy.RepayLoan(City, Error));
	TestEqual(TEXT("Only current debt debited"), City.GetFunds(), Before - Payoff);
	TestEqual(TEXT("Future debt service zero"), Economy.GetProjectedBudget(City, Tuning).DebtService,
	          int64(0));
	TestFalse(TEXT("Repeated payoff rejected"), Economy.RepayLoan(City, Error));
	Economy.Advance(.5, City, Tuning);
	TestEqual(TEXT("Early principal recorded next month"), Economy.GetSnapshot().Loan.LastPrincipalPaid,
	          int64(4791));
	TestEqual(TEXT("Early interest recorded next month"), Economy.GetSnapshot().Loan.LastInterestPaid,
	          int64(38));
	TestEqual(TEXT("No post-payoff interest"), Economy.GetSnapshot().Loan.LastInterestAccrued, int64(0));
	for (int64 Amount : {5000LL, 10000LL, 20000LL})
	{
		FCityPlacementModel Funded(10000);
		FCityLoanState Loan;
		TestTrue(TEXT("Offer issues"), Loan.Borrow(Funded, Amount, 0, Error));
		int64 Interest = 0, Principal = 0;
		for (int32 Month = 1; Month <= 24; ++Month)
		{
			Loan.SettleMonth(Funded);
			Interest += Loan.LastInterestPaid;
			Principal += Loan.LastPrincipalPaid;
			TestTrue(TEXT("Each installment validates"), Loan.Validate(Month, Error));
		}
		TestEqual(TEXT("All principal returns to lender"), Principal, Amount);
		TestEqual(TEXT("Contract closes at maturity"), Loan.Payoff(), int64(0));
		TestEqual(TEXT("Interest exact cash loss"), Funded.GetFunds(), 10000 - Interest);
		TestTrue(TEXT("Quoted total agrees with actual payments"),
		         FCityLoanState::Terms(Amount).Contains(FString::Printf(TEXT("interest $%lld"), Interest)));
	}
	// An operating bill consumes cash before the lender, without contaminating upkeep/payroll.
	FCityPlacementModel Priority(0);
	FCityEconomySimulation PriorityEconomy;
	TestTrue(TEXT("Priority loan"), PriorityEconomy.Borrow(Priority, 5000, Error));
	auto Road = Priority.BeginRoadPreview(Priority.GetEntranceCell());
	Priority.UpdateRoadPreviewEnd(Road, {2, 8});
	TestTrue(TEXT("Operating road built"), Priority.CommitRoad(Road).IsSuccess());
	Priority.TrySpendFunds(Priority.GetFunds() - 1);
	PriorityEconomy.Advance(.5, Priority, Tuning);
	TestTrue(TEXT("Operating shortfall separate from debt"),
	         PriorityEconomy.GetSnapshot().UpkeepShortfall > 0);
	TestEqual(TEXT("Lender gets no priority over operations"),
	          PriorityEconomy.GetSnapshot().Loan.LastInterestPaid, int64(0));
	TestEqual(TEXT("Loan arrears not erased by city shortfall"),
	          PriorityEconomy.GetSnapshot().Loan.InterestDue, int64(25));
	FCityScenarioSimulation Scenario;
	Scenario.Start(0);
	FCityEconomySnapshot Candidate;
	Candidate.LastBilledMonthId=1;Candidate.Residents=100;Candidate.LastIncome=300;Candidate.LastUpkeep=100;
	Candidate.Loan.LastPrincipalPaid=209;Candidate.Loan.LastInterestPaid=25;
	Scenario.Advance(Candidate);
	TestEqual(TEXT("Debt payments count against profitable objective"),Scenario.GetSnapshot().ProfitableMonthStreak,0);
	Candidate.LastBilledMonthId=2;Candidate.LastIncome=500;Candidate.Loan.Principal=5000;Candidate.Loan.InterestDue=25;
	Scenario.Advance(Candidate);
	TestEqual(TEXT("Arrears cannot count as healthy profitability"),Scenario.GetSnapshot().ProfitableMonthStreak,0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLoanSaveTest, "CitySim.Loans.PersistenceAndReports",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLoanSaveTest::RunTest(const FString& Parameters)
{
	FString Error, Json;
	FCitySaveDocument D;
	FCityPlacementModel City(0);
	TestTrue(TEXT("Borrow"), D.Economy.Loan.Borrow(City, 10000, 0, Error));
	City.TrySpendFunds(10000);
	D.Economy.Loan.SettleMonth(City);
	D.Economy.LastBilledMonthId = D.Economy.MonthId = 1;
	D.Economy.FixedStepId = 25;
	D.Placement = City.ExportState();
	D.Traffic.FixedStepId = 25;
	D.Population.LastSettledMonthId = 1;
	TestTrue(
	    TEXT("Capture debt after settlement"),
	    D.Reports.Capture(D.Economy, D.Population, FCityServiceSnapshot(), D.Environment, City.GetFunds()));
	TestEqual(TEXT("Debt report principal"), D.Reports.Months[0].Get(ECityReportMetric::LoanPrincipal),
	          int64(10000));
	TestEqual(TEXT("Borrowing separate recorded flow"),
	          D.Reports.Months[0].Get(ECityReportMetric::LoanBorrowed), int64(10000));
	TestTrue(TEXT("Loan report reconciles"), D.Reports.Validate(1, Error));
	TestTrue(TEXT("Write current save"), FCityPersistence::Serialize(D, Json, Error));
	FCitySaveDocument Loaded;
	TestTrue(TEXT("Read current save"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("Interest arrears round trip"), Loaded.Economy.Loan.InterestDue, int64(50));
	TestEqual(TEXT("Loan source flow round trip"), Loaded.Economy.Loan.LastBorrowed, int64(10000));
	TestTrue(TEXT("Loan coverage round trip"), Loaded.Reports.Months[0].bHasLoans);
	const auto Paper = CityNewspaperEdition(Loaded.Reports, 0);
	TestTrue(TEXT("Newspaper reports borrowing"),
	         Paper.Articles.Last().Body.Contains(TEXT("borrowed $10000")));
	const auto Annual = Loaded.Reports.Annual();
	TestTrue(TEXT("Annual coverage recorded"), Annual[0].bHasLoans);
	D.Reports.Months[0].bHasLoans = false;
	TestFalse(TEXT("Unknown history cannot assert debt"), D.Reports.Validate(1, Error));
	auto Invalid = Loaded.Economy.Loan;
	Invalid.MonthsRemaining = 24;
	TestFalse(TEXT("Loan clock cannot reset"), Invalid.Validate(1, Error));
	Invalid = Loaded.Economy.Loan;
	Invalid.Principal = 20000;
	TestFalse(TEXT("Balance above issued principal rejected"), Invalid.Validate(1, Error));
	FCityReportHistory Populated;
	for (int32 Month=1;Month<=3;++Month)
	{
		FCityReportSample S;
		S.MonthId=Month;S.bHasLoans=S.bHasSentiment=S.bHasMigration=true;
		auto Set=[&S](ECityReportMetric M,int64 V){S.Values[static_cast<int32>(M)]=V;};
		Set(ECityReportMetric::Population,10);Set(ECityReportMetric::ResidentNone,10);
		Set(ECityReportMetric::Unemployed,10);Set(ECityReportMetric::HappinessTotal,500);
		Set(ECityReportMetric::Discontented,4);Set(ECityReportMetric::ConcernTaxes,8);
		Set(ECityReportMetric::ConcernServices,6);Set(ECityReportMetric::Income,100);
		Set(ECityReportMetric::Upkeep,70);Set(ECityReportMetric::Treasury,30000);
		Set(ECityReportMetric::LoanPrincipal,17500);Set(ECityReportMetric::LoanBorrowed,Month==1 ? 20000 : 0);
		Set(ECityReportMetric::LoanPrincipalPaid,834);Set(ECityReportMetric::LoanInterestPaid,100);
		Populated.Months.Add(S);
	}
	const auto Edition=CityNewspaperEdition(Populated,0);
	auto Content=MakeShared<FCityUiNewspaper>();
	Content->Masthead=FText::FromString(Edition.Masthead);Content->Dateline=FText::FromString(Edition.Dateline);
	Content->Headline=FText::FromString(Edition.Headline);Content->Lead=FText::FromString(Edition.Lead);
	Content->Footer=FText::FromString(Edition.Footer);
	for(const auto& Article:Edition.Articles)
	{
		FCityUiNewsArticle A;A.Section=FText::FromString(Article.Section);A.Headline=FText::FromString(Article.Headline);
		A.Body=FText::FromString(Article.Body);Content->Articles.Add(A);
	}
	auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/CitySim/UI/Fonts/F_CityNewsSerif.F_CityNewsSerif"));
	TestNotNull(TEXT("Newspaper typeface"),Font);
	auto Sheet=CityNewspaper::MakeSheet(Content,Font,1000,.82f);Sheet->SlatePrepass();
	AddInfo(FString::Printf(TEXT("Populated loan newspaper size: %s"),*Sheet->GetDesiredSize().ToString()));
	TestTrue(TEXT("Debt and citizen articles fit the VR newspaper"),Sheet->GetDesiredSize().Y<=760);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLoanUiTest, "CitySim.Loans.SharedReviewAndBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLoanUiTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LoanReviewTest"), nullptr, true,
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
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenLoans);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewLoanLarge);
	UI->RefreshReports();
	TestEqual(TEXT("Review leaves cash alone"), Sim->GetPlacementModel().GetFunds(), Cash);
	TestTrue(TEXT("Exact terms visible"), UI->SharedReportText.ToString().Contains(TEXT("0.5% / month")));
	const auto Bounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	AddInfo(Bounds.ToString());
	TestTrue(TEXT("Terms inside VR sheet above controls"),
	         Bounds.Min.Z > 30 && Bounds.Max.Z < 82 && Bounds.Min.Y >= -47 && Bounds.Max.Y <= 47);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::Back);
	TestEqual(TEXT("Cancel returns to loans"), UI->PalettePage, ECityPalettePage::Loans);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewLoanSmall);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ConfirmLoan);
	TestEqual(TEXT("Confirm credits treasury"), Sim->GetPlacementModel().GetFunds(), Cash + 5000);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ConfirmLoan);
	TestEqual(TEXT("Stale confirmation cannot duplicate credit"), Sim->GetPlacementModel().GetFunds(),
	          Cash + 5000);
	TestTrue(TEXT("Further loan disabled"), UI->IsPaletteActionDisabled(ECityPaletteAction::ReviewLoanSmall));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewLoanPayoff);
	UI->RefreshReports();
	UI->bDesktopUi = true;
	TestTrue(TEXT("Desktop gets same payoff quote"),
	         UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Full payoff now $5000")));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ConfirmLoan);
	TestEqual(TEXT("Payoff returns exact cash"), Sim->GetPlacementModel().GetFunds(), Cash);
	for (auto Page : {ECityPalettePage::City, ECityPalettePage::Loans, ECityPalettePage::LoanReview})
		for (bool Hand : {false, true})
		{
			const auto Entries = FCityPaletteModel::SurfaceEntries(Page, Hand);
			TestTrue(TEXT("Fits existing pool"), Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
			for (int32 I = 0; I < Entries.Num(); ++I)
				for (int32 J = 0; J < I; ++J)
				{
					const auto A = FCityPaletteModel::ButtonRect(Page, I, Entries[I].Action, Hand);
					const auto B = FCityPaletteModel::ButtonRect(Page, J, Entries[J].Action, Hand);
					TestFalse(TEXT("Controls do not overlap"),
					          FMath::Abs(A.Center.Y - B.Center.Y) < A.HalfSize.X + B.HalfSize.X &&
					              FMath::Abs(A.Center.Z - B.Center.Z) < A.HalfSize.Y + B.HalfSize.Y);
				}
		}
	W->DestroyWorld(false);
	return true;
}
#endif
