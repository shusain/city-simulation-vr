#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopNewspaper.h"
#include "Components/WidgetComponent.h"
#include "Components/ScrollBox.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityQuarterlyNews.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

namespace
{
FCityReportHistory NewsHistory()
{
	FCityReportHistory H;
	for (int64 Day = 1; Day <= 150; ++Day)
	{
		const int64 Month = (Day - 1) / 25 + 1;
		FCityReportSample S;
		S.MonthId = Day;
		S.bHasSentiment = S.bHasMigration = true;
		S.Values[static_cast<int32>(ECityReportMetric::Population)] = 10;
		S.Values[static_cast<int32>(ECityReportMetric::ResidentNone)] = 10;
		S.Values[static_cast<int32>(ECityReportMetric::Unemployed)] = 10;
		S.Values[static_cast<int32>(ECityReportMetric::HappinessTotal)] = Month <= 3 ? 600 : 500;
		S.Values[static_cast<int32>(ECityReportMetric::Discontented)] = 4;
		S.Values[static_cast<int32>(ECityReportMetric::SearchingResidents)] = 2;
		S.Values[static_cast<int32>(ECityReportMetric::ConcernTaxes)] = 8;
		S.Values[static_cast<int32>(ECityReportMetric::ConcernServices)] = 6;
		S.Values[static_cast<int32>(ECityReportMetric::Income)] = 4;
		S.Values[static_cast<int32>(ECityReportMetric::Upkeep)] = Day % 25 == 0 ? 22 : 2;
		S.Values[static_cast<int32>(ECityReportMetric::Treasury)] = -20;
		S.Values[static_cast<int32>(ECityReportMetric::RelocatedResidents)] = Month * 2;
		S.Values[static_cast<int32>(ECityReportMetric::DepartedResidents)] = Month;
		H.Months.Add(S);
	}
	return H;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNewsFactsTest, "CitySim.News.QuarterCoverageAndFacts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNewsFactsTest::RunTest(const FString& Parameters)
{
	auto H = NewsHistory();
	FString Error;
	TestTrue(TEXT("Census fixture reconciles"), H.Validate(150, Error));
	auto Q = FCityQuarterlyNews::Editions(H);
	TestEqual(TEXT("Three months per edition"), Q.Num(), 2);
	TestTrue(TEXT("Both quarters complete"), Q[0].IsComplete() && Q[1].IsComplete());
	TestEqual(TEXT("Observed tax total"), Q[1].TaxIncome, int64(300));
	TestEqual(TEXT("Upkeep summed; cash remains snapshot"), Q[1].Upkeep, int64(210));
	TestEqual(TEXT("Movement is quarterly, not lifetime"), Q[1].Relocated, int64(6));
	TestEqual(TEXT("Departures subtract preceding month"), Q[1].Departed, int64(3));
	const FString Body = FCityQuarterlyNews::Body(H, 0);
	TestTrue(TEXT("Factual trend compares consecutive full quarters"),
	         Body.Contains(TEXT("-10 vs prior quarter")));
	TestTrue(TEXT("Top concern and remedy"), Body.Contains(TEXT("taxes: 8 people | review taxes")));
	TestTrue(TEXT("Overlapping concerns described"),
	         FCityQuarterlyNews::Footer(H, 0).Contains(TEXT("concerns overlap")));
	H.Months.RemoveAt(125, 25);
	Q = FCityQuarterlyNews::Editions(H);
	TestFalse(TEXT("Two months never called complete"), Q[1].IsComplete());
	TestEqual(TEXT("Partial income is not extrapolated"), Q[1].TaxIncome, int64(200));
	TestTrue(TEXT("Partial trend unavailable"),
	         FCityQuarterlyNews::Body(H, 0).Contains(TEXT("comparison unavailable")));
	H.Months.RemoveAt(0, 75);
	TestFalse(TEXT("Without preceding record migration unknown"),
	          FCityQuarterlyNews::Editions(H)[0].bHasMovement);
	H.Months[0].bHasSentiment = false;
	for (int32 I = static_cast<int32>(ECityReportMetric::HappinessTotal);
	     I < static_cast<int32>(ECityReportMetric::Count); ++I)
		H.Months[0].Values[I] = 0;
	TestTrue(TEXT("Historical sentiment not invented"),
	         FCityQuarterlyNews::Body(H, 0).Contains(TEXT("not recorded for every")));
	H = NewsHistory();
	H.Months[24].Values[static_cast<int32>(ECityReportMetric::HappinessTotal)] = 900;
	H.Months[24].Values[static_cast<int32>(ECityReportMetric::Population)] = 30;
	TestEqual(TEXT("Mood weighted by resident observations, not rounded month means"),
	          FCityQuarterlyNews::Editions(H)[0].AverageHappiness(), int64(42));
	H = NewsHistory();
	for (auto& M : H.Months)
		M.MonthId += 300;
	TestTrue(TEXT("Calendar rolls to Year2 Q2"), FCityQuarterlyNews::Body(H, 0).Contains(TEXT("Year 2 Q2")));
	TestTrue(TEXT("Out-of-range page clamps"), FCityQuarterlyNews::Body(H, 999).Contains(TEXT("Q1")));
	TestTrue(TEXT("Empty history explained"),
	         FCityQuarterlyNews::Body({}, 0).Contains(TEXT("No settled months")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNewsCaptureTest, "CitySim.News.SentimentCaptureAndPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNewsCaptureTest::RunTest(const FString& Parameters)
{
	FCityEconomySnapshot E;
	FCityPopulationSnapshot P;
	E.LastBilledMonthId = P.LastSettledMonthId = 1;
	P.Citizens.SetNum(2);
	for (auto& C : P.Citizens)
	{
		C.LastHappinessMonth = 1;
		C.Happiness = 40;
		C.DiscontentMonths = 3;
		C.HappinessConcerns = static_cast<int32>(ECityHappinessConcern::Taxes) |
		                      static_cast<int32>(ECityHappinessConcern::Purchases);
		C.HousingStatus = ECityHousingStatus::Searching;
	}
	FCityReportHistory H;
	TestTrue(TEXT("Settled capture"), H.Capture(E, P, {}, {}, 123));
	TestFalse(TEXT("Same settlement cannot duplicate news"), H.Capture(E, P, {}, {}, 123));
	TestEqual(TEXT("Actual mood captured"), H.Months[0].Get(ECityReportMetric::HappinessTotal), int64(80));
	TestEqual(TEXT("Tax concerns count residents"), H.Months[0].Get(ECityReportMetric::ConcernTaxes),
	          int64(2));
	TestEqual(TEXT("Sustained concern count"), H.Months[0].Get(ECityReportMetric::SustainedDiscontent),
	          int64(2));
	FCitySaveDocument D;
	D.Placement.Zones.Init(ECityZoneType::None, FCityGrid::CellCount);
	D.Economy.LastBilledMonthId = D.Economy.MonthId = D.Population.LastSettledMonthId = 1;
	D.Reports = H;
	FString Json, Error;
	TestTrue(TEXT("Current schema serializes"), FCityPersistence::Serialize(D, Json, Error));
	FCitySaveDocument Loaded;
	TestTrue(TEXT("Current schema loads"), FCityPersistence::Deserialize(Json, Loaded, Error));
	TestEqual(TEXT("News stable over reload"), FCityQuarterlyNews::Body(Loaded.Reports, 0),
	          FCityQuarterlyNews::Body(H, 0));
	auto Bad = H;
	Bad.Months[0].Values[static_cast<int32>(ECityReportMetric::ConcernTaxes)] = 3;
	TestFalse(TEXT("Corrupt resident count rejected"), Bad.Validate(1, Error));
	Bad = H;
	Bad.Months[0].Values[static_cast<int32>(ECityReportMetric::HappinessTotal)] = 201;
	TestFalse(TEXT("Impossible mood rejected"), Bad.Validate(1, Error));
	E.LastBilledMonthId = P.LastSettledMonthId = 2;
	TestTrue(TEXT("Unobserved month captures other facts"), H.Capture(E, P, {}, {}, 123));
	TestFalse(TEXT("Stale citizen mood is not a fresh observation"), H.Months.Last().bHasSentiment);
	for (int64 Month = 3; Month <= FCityReportHistory::MaximumMonths + 10; ++Month)
	{
		E.LastBilledMonthId = P.LastSettledMonthId = Month;
		for (auto& C : P.Citizens)
			C.LastHappinessMonth = Month;
		H.Capture(E, P, {}, {}, 123);
	}
	TestEqual(TEXT("Existing bounded history retained"), H.Months.Num(), FCityReportHistory::MaximumMonths);
	TestTrue(TEXT("Retained partial starting quarter distinguished"),
	         !FCityQuarterlyNews::Editions(H)[0].IsComplete());
	TestTrue(TEXT("Retained reports valid"), H.Validate(FCityReportHistory::MaximumMonths + 10, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNewspaperEditorialTest, "CitySim.News.NewspaperEditorialFacts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNewspaperEditorialTest::RunTest(const FString& Parameters)
{
	auto H = NewsHistory();
	auto Paper = CityNewspaperEdition(H, 0);
	TestEqual(TEXT("Actual departures lead the edition"), Paper.Headline,
	          FString(TEXT("3 residents leave the city")));
	TestEqual(TEXT("Four factual subarticles"), Paper.Articles.Num(), 4);
	TestTrue(TEXT("Largest concern is selected"), Paper.Articles[0].Headline.Contains(TEXT("Tax burden")));
	TestTrue(TEXT("Closing concern count is explicit"),
	         Paper.Articles[0].Body.Contains(TEXT("8 residents")) &&
	             Paper.Articles[0].Body.Contains(TEXT("month 6")));
	TestTrue(TEXT("Fiscal flows are observed totals"),
	         Paper.Articles.Last().Body.Contains(TEXT("$300")) &&
	             Paper.Articles.Last().Body.Contains(TEXT("$210")) &&
	             Paper.Articles.Last().Body.Contains(TEXT("$-20")));
	TestTrue(TEXT("Mood comparison is factual"), Paper.Lead.Contains(TEXT("-10 points")));
	H.Months.RemoveAt(125, 25);
	TestTrue(TEXT("Partial coverage appears in dateline"),
	         CityNewspaperEdition(H, 0).Dateline.Contains(TEXT("PARTIAL EDITION")));
	TestFalse(TEXT("Partial edition never claims full-quarter comparison"),
	          CityNewspaperEdition(H, 0).Lead.Contains(TEXT("previous complete quarter")));
	H.Months.RemoveAt(0, 75);
	TestTrue(TEXT("Missing movement baseline not invented"),
	         CityNewspaperEdition(H, 0).Articles[2].Body.Contains(TEXT("unavailable")));
	H.Months[0].bHasSentiment = false;
	TestTrue(TEXT("Unknown mood is explicit"),
	         CityNewspaperEdition(H, 0).Lead.Contains(TEXT("no recorded citizen sentiment")));
	TestEqual(TEXT("Inaugural paper has no fake issues"), CityNewspaperEdition({}, 0).Articles.Num(), 0);
	return true;
}

class FCityQuarterlyNewsFixture
{
  public:
	static void SetHistory(UCitySimulationSubsystem* Sim)
	{
		Sim->Reports = NewsHistory();
	}
	static bool Seed(UCitySimulationSubsystem* Sim, ACityBoardActor* Board)
	{
		auto Parameters = Sim->Terrain.GetParameters();
		Parameters.GeneratorVersion = 0;
		Parameters.BroadElevationAmplitudeCm = Parameters.HillAmplitudeCm = Parameters.RidgeAmplitudeCm = 0;
		Sim->Terrain.Generate(Parameters);
		Sim->ResetCity(false);
		auto* Original = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!Original)
			return false;
		auto* Catalog = NewObject<UCityBuildingCatalog>();
		Catalog->Definitions = Original->Definitions;
		for (auto& D : Catalog->Definitions)
			if (D.DefinitionId == TEXT("Home") && D.DensityLevels.Num() > 1)
				D.DensityLevels.SetNum(1);
		Board->BuildingCatalog = Catalog;
		Sim->RegisterBuildingCatalog(Catalog);
		Board->PrepareCityView();
		auto& City = Sim->GetPlacementModel();
		auto Road = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Road, {30, 8});
		if (!City.CommitRoad(Road).IsSuccess())
			return false;
		const TCHAR* Names[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Home"), TEXT("Home")};
		const int32 Xs[] = {1, 4, 7, 11, 15};
		for (int32 I = 0; I < 5; ++I)
		{
			const auto* D = Catalog->FindDefinition(Names[I]);
			if (!D)
				return false;
			auto Preview = City.BeginPreview(*D, {Xs[I], 9});
			if (!City.CommitPlacement(Preview, *D).IsSuccess())
				return false;
		}
		Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
		Sim->EconomyTuning.FixedStepSeconds = .125;
		Sim->EconomyTuning.FixedStepsPerMonth = 4;
		Sim->EconomyTuning.MaxCatchUpSteps = 32;
		Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
		Sim->StartScenario();
		Sim->SetTaxRates(2500, 1000, 1000);
		for (int32 I = 0; I < 17; ++I)
			Sim->Tick(.125f);
		Sim->SetSimulationPaused(true);
		++Sim->PresentationGeneration;
		Board->PrepareCityView();
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNewsUiTest, "CitySim.News.SharedNavigationAndBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNewsUiTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("QuarterlyNewsTest"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	FCityQuarterlyNewsFixture::SetHistory(Sim);
	auto* UI = W->SpawnActor<ACityVRToolController>();
	UI->Board = W->SpawnActor<ACityBoardActor>();
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bPaletteOpen = true;
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenQuarterlyNews);
	UI->RefreshReports();
	TestEqual(TEXT("Shared page opens"), UI->PalettePage, ECityPalettePage::QuarterlyNews);
	TestTrue(TEXT("Latest edition selected"), UI->SharedReportText.ToString().Contains(TEXT("Q2")));
	TestTrue(TEXT("World newspaper surface exists"), UI->NewspaperSheet != nullptr);
	if (UI->NewspaperSheet)
	{
		UI->NewspaperSheet->GetSlateWidget()->SlatePrepass();
		AddInfo(FString::Printf(TEXT("World newspaper desired size: %s"),
		                        *UI->NewspaperSheet->GetSlateWidget()->GetDesiredSize().ToString()));
		TestTrue(TEXT("Whole world newspaper fits fixed sheet"),
		         UI->NewspaperSheet->GetSlateWidget()->GetDesiredSize().Y <= 760);
		const auto PageBounds =
		    UI->NewspaperSheet->CalcBounds(UI->NewspaperSheet->GetRelativeTransform()).GetBox();
		AddInfo(FString::Printf(TEXT("World newspaper sheet bounds: %s"), *PageBounds.ToString()));
		TestTrue(TEXT("World sheet stays above buttons and within paper panel"),
		         PageBounds.Min.Z > 24.2 && PageBounds.Max.Z < 83 && PageBounds.Min.Y >= -37 &&
		             PageBounds.Max.Y <= 37);
	}
	const auto Bounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	const auto Footer = UI->ReportRangeText->CalcBounds(UI->ReportRangeText->GetRelativeTransform()).GetBox();
	AddInfo(FString::Printf(TEXT("News bounds %s | footer %s"), *Bounds.ToString(), *Footer.ToString()));
	TestTrue(TEXT("News stays within shared VR panel"),
	         Bounds.Min.Y >= -37 && Bounds.Max.Y <= 37 && Bounds.Max.Z <= 55 && Bounds.Min.Z > Footer.Max.Z);
	TestTrue(TEXT("Footer above buttons"), Footer.Min.Z >= 24.2);
	TestTrue(TEXT("Newest cannot advance"), UI->IsPaletteActionDisabled(ECityPaletteAction::NewsNewer));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::NewsOlder);
	UI->RefreshReports();
	TestTrue(TEXT("Older quarter browses"), UI->SharedReportText.ToString().Contains(TEXT("Q1")));
	TestTrue(TEXT("Oldest cannot go back"), UI->IsPaletteActionDisabled(ECityPaletteAction::NewsOlder));
	UI->bDesktopUi = true;
	TestTrue(TEXT("Desktop uses same article"),
	         UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("taxes: 8")));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::NewsMoney);
	TestEqual(TEXT("Money remedy opens actual reports"), UI->PalettePage, ECityPalettePage::Reports);
	TestEqual(TEXT("Money category selected"), UI->ReportCategory, 2);
	for (bool Hand : {false, true})
	{
		const auto Entries = FCityPaletteModel::SurfaceEntries(ECityPalettePage::QuarterlyNews, Hand);
		TestTrue(TEXT("News fits existing entry pool"),
		         Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
		for (int32 I = 0; I < Entries.Num(); ++I)
		{
			const auto A =
			    FCityPaletteModel::ButtonRect(ECityPalettePage::QuarterlyNews, I, Entries[I].Action, Hand);
			for (int32 J = 0; J < I; ++J)
			{
				const auto B = FCityPaletteModel::ButtonRect(ECityPalettePage::QuarterlyNews, J,
				                                             Entries[J].Action, Hand);
				TestFalse(TEXT("News buttons do not overlap"),
				          FMath::Abs(A.Center.Y - B.Center.Y) < A.HalfSize.X + B.HalfSize.X &&
				              FMath::Abs(A.Center.Z - B.Center.Z) < A.HalfSize.Y + B.HalfSize.Y);
			}
		}
	}
	W->DestroyWorld(false);
	return true;
}

class FCityDesktopNewsCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopNewsCommand(FAutomationTestBase* T) : Test(T)
	{
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (W)
			for (TActorIterator<ACityVRToolController> It(W); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->DesktopHud || !UI->Board)
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Quarterly news HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		auto Click = [&](ECityPaletteAction Action) {
			const auto Data = UI->GetUiSnapshot();
			for (int32 I = 0; I < Data.MenuEntries.Num(); ++I)
				if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
				{
					const FString ButtonName = Data.bNewspaper ? FString::Printf(TEXT("NewsAction%d"), I)
					                                           : FString::Printf(TEXT("Menu%d"), I);
					auto* Button = Cast<UCityDesktopActionButton>(
					    UI->DesktopHud->WidgetTree->FindWidget(FName(ButtonName)));
					if (Button && Button->GetIsEnabled())
						Button->OnClicked.Broadcast();
					else
						Test->AddError(TEXT("News action unavailable"));
					return;
				}
			Test->AddError(TEXT("News action not found"));
		};
		auto Capture = [](const TCHAR* Name) {
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/QuarterlyNews");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / Name, true, false);
		};
		FString Error;
		switch (Phase)
		{
		case 0:
			UI->ReturnToCity();
			if (!FCityQuarterlyNewsFixture::Seed(Sim, UI->Board))
			{
				Test->AddError(TEXT("Native news fixture failed"));
				return true;
			}
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			Test->TestEqual(TEXT("Four real monthly settlements"), Sim->GetReportHistory().Months.Num(), 4);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 1:
			Click(ECityPaletteAction::OpenQuarterlyNews);
			break;
		case 2:
			Test->TestTrue(TEXT("Partial quarter shows actual observations"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("PARTIAL | 1/3")));
			Capture(TEXT("PartialQuarter.png"));
			break;
		case 3:
			Click(ECityPaletteAction::NewsOlder);
			break;
		case 4:
			Test->TestTrue(TEXT("Older complete quarter reachable"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Q1 | COMPLETE")));
			Capture(TEXT("FirstQuarter.png"));
			break;
		case 5:
			Click(ECityPaletteAction::NewsNewer);
			Sim->SetSimulationPaused(false);
			for (int32 I = 0; I < 8; ++I)
				Sim->Tick(.125f);
			Sim->SetSimulationPaused(true);
			break;
		case 6:
			Test->TestTrue(TEXT("Completed second quarter rendered"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Q2 | COMPLETE")));
			Test->TestTrue(TEXT("No departures claimed before the discontent threshold"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("0 left")));
			Capture(TEXT("SecondQuarter.png"));
			break;
		case 7:
			Sim->SetSimulationPaused(false);
			for (int32 I = 0; I < 8; ++I)
				Sim->Tick(.125f);
			Sim->SetSimulationPaused(true);
			UI->RefreshReports();
			break;
		case 8:
			Test->TestTrue(TEXT("Actual later departures summarized"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("30 left")));
			BeforeReload = UI->GetUiSnapshot().MenuBody.ToString();
			Capture(TEXT("DepartureQuarter.png"));
			break;
		case 9: {
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/Technology-News-City.json");
			Test->TestTrue(TEXT("Real news city saves"), Sim->SaveCityToPath(Path, Error));
			Test->TestTrue(TEXT("Real news city reloads"), Sim->LoadCityFromPath(Path, Error, false));
			UI->RefreshReports();
			Test->TestEqual(TEXT("Article unchanged after reload"), UI->GetUiSnapshot().MenuBody.ToString(),
			                BeforeReload);
			Click(ECityPaletteAction::NewsMoney);
			break;
		}
		case 10:
			Test->TestEqual(TEXT("Money link opens report category"), UI->ReportCategory, 2);
			Capture(TEXT("FinancialRemedy.png"));
			break;
		case 11:
			UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenQuarterlyNews);
			break;
		case 12:
			Click(ECityPaletteAction::NewsOlder);
			break;
		case 13:
			if (auto* Paper =
			        Cast<UCityDesktopNewspaper>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Newspaper"))))
				if (auto* Content = Paper->GetParent())
					if (auto* Scroll = Cast<UScrollBox>(Content->GetParent()))
						Scroll->ScrollToEnd();
			break;
		case 14:
			Capture(TEXT("LowerArticles.png"));
			break;
		case 15: {
			UI->DesktopHud->SetVisibility(ESlateVisibility::Collapsed);
			UI->bDesktopUi = false;
			UI->SetActorHiddenInGame(false);
			UI->SetPaletteOpen(false);
			UI->SetPaletteOpen(true);
			UI->ExecuteSharedPaletteAction(ECityPaletteAction::OpenQuarterlyNews);
			UI->RefreshPaletteText();
			UI->SetActorTickEnabled(false);
			const auto* Camera = UI->HeadsetCamera.Get();
			UI->PaletteRoot->SetWorldTransform(
			    FTransform(FRotationMatrix::MakeFromX(-Camera->GetForwardVector()).Rotator(),
			               Camera->GetComponentLocation() + Camera->GetForwardVector() * 95 -
			                   Camera->GetUpVector() * 53.5,
			               FVector(1)));
			UI->NewspaperSheet->RequestRedraw();
			break;
		}
		case 16:
			Test->TestTrue(TEXT("World paper has a rendered texture"),
			               UI->NewspaperSheet->GetRenderTarget() != nullptr);
			Capture(TEXT("WorldNewspaper.png"));
			break;
		case 17:
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_DESKTOP_QUARTERLY_NEWS_COMPLETE Captures=7 SyntheticButtons=1 "
			            "PhysicalInputPending=1"));
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + .8;
		return false;
	}

  private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0;
	double Next = 0;
	FString BeforeReload;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopNewsTest, "CitySim.Desktop.QuarterlyNews",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopNewsTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Technology")))
	{
		AddError(TEXT("News smoke requires CityDesktop and isolated Technology UserDir"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopNewsCommand(this));
	return true;
}
#endif
