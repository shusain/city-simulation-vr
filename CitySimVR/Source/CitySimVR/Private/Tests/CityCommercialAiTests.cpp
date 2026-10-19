#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Interaction/CityPaletteModel.h"
#include "CityTechnologyFixture.h"
#include "Interaction/CityVRToolController.h"
#include "Components/TextRenderComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace
{
FCityServiceSnapshot AiServices()
{
	FCityServiceSnapshot S;
	S.OperatingFacilities.Add(TEXT("AIInstitute"), 1);
	S.OperatingFacilities.Add(TEXT("EnterpriseAICenter"), 1);
    S.OperatingAiCenters.Add(100);
	for (const uint64 Id : {uint64(20), uint64(30), uint64(40)})
	{
		S.EntityJobs.Add(Id, 10);
		S.EntitySkilledJobs.Add(Id, 2);
		S.EntityOutput.Add(Id, 12);
	}
	S.Jobs = S.CommercialJobs = 30;
	S.CommercialOutput = 36;
	return S;
}
FCityPopulationSnapshot AiPeople()
{
	FCityPopulationSnapshot P;
	P.bCommercialAiPilot = true;
	P.LastSettledMonthId = 1;
	for (const uint64 Id : {uint64(40), uint64(20), uint64(30)})
	{
		FCityBusinessLedgerRecord B;
		B.EntityId = Id;
		B.bInitialFreightReceived = true;
		B.JobCapacity = 10;
		B.SkilledJobCapacity = 2;
		B.Inventory = 7;
		B.Balance = 15;
		B.PayrollDue = B.PayrollPaid = 10;
		P.Businesses.Add(B);
	}
	return P;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAiFundingTest, "CitySim.AI.FundingAndBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAiFundingTest::RunTest(const FString& Parameters)
{
	auto S = AiServices();
	auto P = AiPeople();
	TestEqual(TEXT("Capacity bounded to two"), FCityCommercialAi::Eligible(P, S).Num(), 2);
	TestEqual(TEXT("Stable selection independent of ledger order"), FCityCommercialAi::Eligible(P, S)[0],
	          uint64(20));
	FCityCommercialAi::Apply(&P, S);
	TestTrue(TEXT("Unpaid construction/selection has no adoption effect"), S.CommercialAiShops.IsEmpty());
	FCityCommercialAi::Settle(P, &S, 1);
	int64 Fees = 0, Balances = 0, Inventories = 0;
	for (const auto& B : P.Businesses)
	{
		Fees += B.AiFeePaid;
		Balances += B.Balance;
		Inventories += B.Inventory;
	}
	TestEqual(TEXT("Actual external fees"), Fees, int64(10));
	TestEqual(TEXT("Fee is debited once from firms"), Balances, int64(35));
	TestEqual(TEXT("Software creates no stock"), Inventories, int64(21));
	S = AiServices();
	FCityCommercialAi::Apply(&P, S);
	TestEqual(TEXT("Two paid contracts change two shops"), S.CommercialAiShops.Num(), 2);
	TestEqual(TEXT("One position removed per participating shop"), S.Jobs, 28);
	TestEqual(TEXT("Industry/output not fabricated"), S.IndustrialOutput, 0);
	TestEqual(TEXT("Unqualified/unfilled roles give no throughput bonus"), S.CommercialOutput, 36);
	TestEqual(TEXT("Specialists require Higher"), S.EntitySkilledMinimumEducation.FindRef(20), 3);
	P = AiPeople();
	P.Businesses[1].Balance = 4;
	P.Businesses[2].PayrollPaid = 0;
	FCityCommercialAi::Settle(P, &S, 25);
	TestEqual(TEXT("Insufficient balance is not a partial fee"), P.Businesses[1].AiFeePaid, int64(0));
	TestEqual(TEXT("Unpaid payroll has priority over subscription"), P.Businesses[2].AiFeePaid, int64(0));
	TestEqual(TEXT("No treasury money or arrears invented"), P.Businesses[1].Balance, int64(4));
	P.bCommercialAiPilot = false;
	FCityCommercialAi::Settle(P, &S, 50);
	for (const auto& B : P.Businesses)
        TestTrue(TEXT("Pause retains history, no renewal"), B.AiPaidMonth < 3);
    P = AiPeople(); S = AiServices();
    FCityCommercialAi::Settle(P, &S, 1);
    const auto Once = P.Businesses[1].Balance;
    for (int64 Day = 2; Day < 25; ++Day) FCityCommercialAi::Settle(P, &S, Day);
    TestEqual(TEXT("Daily settlements do not repeat calendar-month fees"), P.Businesses[1].Balance, Once);
    FCityCommercialAi::Settle(P, &S, 25);
    TestEqual(TEXT("Next calendar month renews once"), P.Businesses[1].Balance, Once - 5);
    FCityCommercialAi::Settle(P, &S, 25);
    TestEqual(TEXT("Repeated boundary is idempotent"), P.Businesses[1].Balance, Once - 5);
    TestEqual(TEXT("Named center at payment is recorded"), P.Businesses[1].AiCenterAtPayment, uint64(100));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAiTransitionsTest, "CitySim.AI.StaffingAndOutages",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAiTransitionsTest::RunTest(const FString& Parameters)
{
	auto P = AiPeople();
	auto S = AiServices();
	FCityCommercialAi::Settle(P, &S, 1);
	for (int32 I = 0; I < 9; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I + 1;
		C.WorkplaceEntityId = 20;
		C.Education = ECityEducationLevel::Higher;
		C.bSkilledJob = I < 2;
		P.Citizens.Add(C);
	}
	FCityCommercialAi::Apply(&P, S);
	TestEqual(TEXT("Qualified paid staffed shop gets retail capacity"), S.EntityOutput.FindRef(20), 15);
	TestEqual(TEXT("Unstaffed paid shop gets no output bonus"), S.EntityOutput.FindRef(30), 12);
	TestEqual(TEXT("Bonus reconciles aggregate retail"), S.CommercialOutput, 39);
	for (auto& C : P.Citizens)
		C.bOnMedicalLeave = true;
	S = AiServices();
	FCityCommercialAi::Apply(&P, S);
	TestEqual(TEXT("Medical leave disables throughput"), S.EntityOutput.FindRef(20), 12);
	S = AiServices();
	S.OperatingFacilities.Add(TEXT("AIInstitute"), 0);
	FCityCommercialAi::Apply(&P, S);
	TestTrue(TEXT("AI outage immediately suspends adoption"), S.CommercialAiShops.IsEmpty());
	TestEqual(TEXT("Outage restores ordinary jobs"), S.Jobs, 30);
	FCityCommercialAi::Settle(P, &S, 25);
	for (const auto& B : P.Businesses)
		TestTrue(TEXT("Outage does not renew software fees"), B.AiPaidMonth <= 1);
	P = AiPeople();
	S = AiServices();
	FCityCommercialAi::Settle(P, &S, 1);
	P.bCommercialAiPilot = false;
	FCityCommercialAi::Apply(&P, S);
	TestEqual(TEXT("Pause restores normal roles"), S.EntityJobs.FindRef(20), 10);
	TestEqual(TEXT("Pause does not refund consumed provider money"), P.Businesses[1].Balance, int64(10));
	P.bCommercialAiPilot = true;
	P.LastSettledMonthId = 25;
	FCityCommercialAi::Apply(&P, S);
	TestTrue(TEXT("Expired receipts cannot grant free adoption"), S.CommercialAiShops.IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAiQualificationTest, "CitySim.AI.QualifiedHiringAndSaveValidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAiQualificationTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	FCityBuildingDefinition D;
	D.DefinitionId = TEXT("Shop");
	D.DisplayName = TEXT("Shop");
	D.Footprint.Width = D.Footprint.Height = 1;
	auto PreviewA = City.BeginPreview(D, {2, 9});
	auto PreviewB = City.BeginPreview(D, {4, 9});
	const auto A = City.CommitPlacement(PreviewA, D);
	PreviewB = City.BeginPreview(D, {4, 9});
	const auto B = City.CommitPlacement(PreviewB, D);
	if (!TestTrue(TEXT("Two firm entities"), A.IsSuccess() && B.IsSuccess()))
		return false;
	FCityPopulationSnapshot P;
	P.NextCitizenId = 3;
	for (int32 I = 0; I < 2; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I + 1;
		C.Education = I == 0 ? ECityEducationLevel::Secondary : ECityEducationLevel::Higher;
		P.Citizens.Add(C);
		FCityBusinessLedgerRecord Firm;
		Firm.EntityId = I == 0 ? A.EntityId : B.EntityId;
		Firm.OpeningPayrollReserve = 1;
		P.Businesses.Add(Firm);
	}
	FCityPopulationSimulation Pop;
	FString Error;
	if (!TestTrue(TEXT("Current valid records restore"), Pop.RestoreState(P, Error)))
		return false;
	FCityServiceSnapshot S;
	S.EntityJobs.Add(A.EntityId, 1);
	S.EntityJobs.Add(B.EntityId, 1);
	S.EntitySkilledJobs.Add(A.EntityId, 1);
	S.EntitySkilledJobs.Add(B.EntityId, 1);
	S.EntitySkilledMinimumEducation.Add(A.EntityId, 3);
	FCityEconomySnapshot E;
	E.Residents = 2;
	E.Jobs = 2;
	FCityEconomyTuning T;
	Pop.Reconcile(City, E, T, &S);
	const auto& People = Pop.GetSnapshot();
	TestEqual(TEXT("Secondary fills ordinary specialist slot"), People.Citizens[0].WorkplaceEntityId,
	          B.EntityId);
	TestEqual(TEXT("Skipped Higher vacancy remains available for Higher resident"),
	          People.Citizens[1].WorkplaceEntityId, A.EntityId);
	auto Invalid = People;
	Invalid.Citizens[1].Education = ECityEducationLevel::Secondary;
	TestFalse(TEXT("Unqualified saved AI specialist rejected"), Pop.RestoreState(Invalid, Error));
	Invalid = People;
	Invalid.Businesses[0].AiFeePaid = 5;
	Invalid.Businesses[0].AiPaidMonth = 1;
	TestFalse(TEXT("Future prepaid contract rejected"), Pop.RestoreState(Invalid, Error));
	Invalid = People;
	Invalid.Businesses[0].AiFeePaid = 4;
	TestFalse(TEXT("Partial fabricated fee rejected"), Pop.RestoreState(Invalid, Error));
	TestEqual(TEXT("Combined AI and maintained outpatient schema is distinct from the old pilot"),
	          FCitySaveDocument::CurrentSchemaVersion, 72);
	TestEqual(TEXT("Pilot returns to technology choices"),
	          FCityPaletteModel::ParentPage(ECityPalettePage::CommercialAi),
	          ECityPalettePage::TechnologyReview);
	for (bool Hand : {false, true})
		TestTrue(TEXT("Shared controls stay bounded"),
		         FCityPaletteModel::SurfaceEntries(ECityPalettePage::CommercialAi, Hand).Num() <=
		             FCityPaletteModel::MaximumEntryCount);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAiLiveTest, "CitySim.AI.LiveSettlementReloadAndReview",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAiLiveTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AiLiveTest"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	if (!TestTrue(TEXT("Real education/technology/AI construction and hiring"),
	              FCityTechnologyFixture::SeedAi(Sim)))
	{
		W->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Operating AI enables enterprise center"),
	         Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("EnterpriseAICenter")) > 0);
	TestFalse(TEXT("Commercial adoption is initially optional/off"),
	          Sim->GetPopulationSnapshot().bCommercialAiPilot);
	TestTrue(TEXT("Funded scenario shops"), FCityTechnologyFixture::FundAiScenario(Sim));
	TestTrue(TEXT("Command enables selected pilot"), Sim->SetCommercialAiPilot(true));
    // Regression: the chain was operating yesterday, but today's treasury cannot pay it.
    Sim->SetTaxRates(0,0,0);
    Sim->GetPlacementModel().ApplyMonthlyEconomy(0,Sim->GetPlacementModel().GetFunds());
    FCityTechnologyFixture::Month(Sim);
    TestEqual(TEXT("Current public payroll cannot be funded"),Sim->GetEconomySnapshot().LastPublicPayrollPaid,int64(0));
    for (const auto& B : Sim->GetPopulationSnapshot().Businesses)
        TestEqual(TEXT("No software fee on newly unpaid provider day"),B.AiFeePaid,int64(0));
    TestTrue(TEXT("Newly unpaid providers cannot grant adoption"),Sim->GetServiceSnapshot().CommercialAiShops.IsEmpty());
    Sim->GetPlacementModel().ApplyMonthlyEconomy(100000,0);
    for (int32 Day=0; Day<4; ++Day) FCityTechnologyFixture::Month(Sim);
    TestTrue(TEXT("Live qualified payroll enables actual retail bonus"),!Sim->GetServiceSnapshot().CommercialAiStaffedShops.IsEmpty());
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/Technology-AI-Native.json");
	FString Error;
	TestTrue(TEXT("Current city saves selection and actual firm receipts"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Current city reloads without historical migration"),
	         Sim->LoadCityFromPath(Path, Error, false));
	TestTrue(TEXT("Selected rollout survives reload"), Sim->GetPopulationSnapshot().bCommercialAiPilot);
	int64 Fees = 0;
	for (const auto& B : Sim->GetPopulationSnapshot().Businesses)
		Fees += B.AiFeePaid;
	TestTrue(TEXT("Firm fees remain bounded by capacity and funding"),
	         Fees > 0 && Fees <= FCityCommercialAi::MonthlyFee * 2);
	FCityDiagnosticOverlayResult View;
    FCityDiagnosticOverlayEvaluator::Build(Sim->GetPlacementModel(), Sim->GetServiceSnapshot(), Sim->GetNeedsSnapshot(),
        Sim->GetEconomyTuning(), nullptr,nullptr,nullptr, Sim->GetEnvironmentSnapshot(), ECityDiagnosticOverlay::CommercialAi,0,View,
        nullptr,nullptr,nullptr,nullptr,&Sim->GetPopulationSnapshot());
    TestTrue(TEXT("Pilot view identifies actual shop cells"),!View.CellDetails.IsEmpty());
    TestTrue(TEXT("Pilot view explains paid and waiting states"),View.Legend.Contains(TEXT("paid")));
	auto* UI = W->SpawnActor<ACityVRToolController>();
	UI->Board = W->SpawnActor<ACityBoardActor>();
	UI->Board->BuildingCatalog = const_cast<UCityBuildingCatalog*>(Sim->GetBuildingCatalog());
	UI->Board->DispatchBeginPlay();
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bPaletteOpen = true;
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenTechnology);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::ReviewAIInstitute);
	UI->RefreshReports();
	TestTrue(TEXT("AI requirement visible"),
	         UI->ReportText->Text.ToString().Contains(TEXT("Research campus operating")));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenCommercialAi);
	UI->RefreshReports();
	TestTrue(TEXT("Pilot fees and jobs are reviewable"),
	         UI->ReportText->Text.ToString().Contains(TEXT("external software provider")));
	const auto Bounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	AddInfo(Bounds.ToString());
	TestTrue(TEXT("Pilot fits shared world sheet"),
	         Bounds.Min.Z > 28 && Bounds.Max.Z < 82 && Bounds.Min.Y >= -47 && Bounds.Max.Y <= 47);
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::DisableCommercialAi);
	TestFalse(TEXT("Shared PAUSE command applies"), Sim->GetPopulationSnapshot().bCommercialAiPilot);
	TestTrue(TEXT("Pause removes current adoption immediately"),
	         Sim->GetServiceSnapshot().CommercialAiShops.IsEmpty());
	const uint64 AI = FCityTechnologyFixture::Find(Sim, TEXT("AIInstitute"));
	Sim->GetPlacementModel().DemolishEntity(AI);
	FCityTechnologyFixture::Month(Sim);
	TestEqual(TEXT("Demolition suspends enterprise capability"),
	          Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("EnterpriseAICenter")), 0);
	TestTrue(TEXT("AI can rebuild after demolition"),
	         FCityTechnologyFixture::Place(Sim, TEXT("AIInstitute"), 42));
	TestEqual(TEXT("Rebuilt chain resumes without banked unlock"),
	          Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("EnterpriseAICenter")), 1);
	W->DestroyWorld(false);
	return true;
}
#endif
