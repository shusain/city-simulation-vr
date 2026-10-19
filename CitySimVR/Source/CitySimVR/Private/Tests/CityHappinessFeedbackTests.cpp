#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingHistory.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityConditionIndicators.h"
#include "Interaction/CityVRToolController.h"
#include "Persistence/CityPersistence.h"
#include "Components/TextRenderComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Presentation/CityDesktopHud.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// A deterministic native city with genuinely recorded monthly search/departure.
// This helper is test-only and uses an isolated automation save root.
class FCityHousingFeedbackFixture
{
  public:
	static uint64 Seed(UCitySimulationSubsystem* Sim, ACityBoardActor* Board)
	{
		auto Parameters = Sim->Terrain.GetParameters();
		Parameters.GeneratorVersion = 0;
		Parameters.BroadElevationAmplitudeCm = Parameters.HillAmplitudeCm = Parameters.RidgeAmplitudeCm = 0;
		Sim->Terrain.Generate(Parameters);
		Sim->ResetCity(false);
		auto* Original = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
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
		City.CommitRoad(Road);
		uint64 Home = 0;
		const TCHAR* Names[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Home"), TEXT("Home")};
		const int32 Xs[] = {1, 4, 7, 11, 15};
		for (int32 I = 0; I < 5; ++I)
		{
			const auto* D = Catalog->FindDefinition(Names[I]);
			auto Preview = City.BeginPreview(*D, {Xs[I], 9});
			const auto R = City.CommitPlacement(Preview, *D);
			if (!R.IsSuccess())
				return 0;
			if (I == 2)
				Home = R.EntityId;
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
		// Direct fixture commands bypass the board's ordinary placement renderer.
		++Sim->PresentationGeneration;
		Board->PrepareCityView();
		return Home;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHappinessFeedbackUiTest,
                                 "CitySim.HappinessFeedback.SharedOverlayWarningsAndList",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHappinessFeedbackUiTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HappinessFeedback"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = W->SpawnActor<ACityBoardActor>();
	const uint64 Home = FCityHousingFeedbackFixture::Seed(Sim, Board);
	Board->DispatchBeginPlay();
	if (!TestTrue(TEXT("Native feedback fixture places homes"), Home != 0))
	{
		W->DestroyWorld(false);
		return false;
	}
	const auto P = Sim->GetPopulationSnapshot();
	TestEqual(TEXT("Normal native admission creates actual residents"), P.Citizens.Num(), 30);
	TestEqual(TEXT("Each household records one real search transition"), P.HousingEvents.Num(), 3);
	TestTrue(TEXT("Search explains actual unmet purchases"),
	         P.HousingEvents.Num() &&
	             (P.HousingEvents[0].Concerns & static_cast<int32>(ECityHappinessConcern::Purchases)) != 0);
	FCityDiagnosticOverlayResult Overlay;
	FCityDiagnosticOverlayEvaluator::Build(
	    Sim->GetPlacementModel(), Sim->GetServiceSnapshot(), Sim->GetNeedsSnapshot(), Sim->GetEconomyTuning(),
	    nullptr, nullptr, nullptr, Sim->GetEnvironmentSnapshot(), ECityDiagnosticOverlay::Happiness, 0,
	    Overlay, nullptr, nullptr, nullptr, nullptr, &P);
	const auto Cell = Sim->GetPlacementModel().GetEntities().FindChecked(Home).Anchor;
	const int32 Index = Cell.Y * Overlay.Dimensions.GetWidth() + Cell.X;
	TestEqual(TEXT("Overlay uses real resident mean"), Overlay.Values[Index],
	          FCityHappinessEvaluator::Summarize(P, Home).Average);
	TestTrue(TEXT("Exact cell detail explains search and concerns"),
	         Overlay.DescribeCell(Cell).Contains(TEXT("HOME SEARCH")) &&
	             Overlay.DescribeCell(Cell).Contains(TEXT("unmet purchases")));
	TestTrue(TEXT("Low happiness has a distinct red map color"),
	         Overlay.ColorAt(Index).R > Overlay.ColorAt(Index).G);
	auto Badges = FCityConditionIndicators::Build(Sim->GetPlacementModel(), Sim->GetFireSnapshot(),
	                                              Sim->GetCrimeSnapshot(), 10, nullptr, &P);
	int32 Warned = 0;
	for (const auto& B : Badges)
	{
		Warned += B.SearchingResidents;
		TestTrue(TEXT("Badge describes household search"), B.Title(0).Contains(TEXT("HOME SEARCH")));
	}
	TestEqual(TEXT("Clustering preserves every searching resident"), Warned, 30);
	TestTrue(TEXT("Existing bounded badge pool preserved"),
	         Badges.Num() <= FCityConditionIndicators::MaximumBadges);
	auto* UI = W->SpawnActor<ACityVRToolController>();
	UI->Board = Board;
	auto* CameraOwner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bPaletteOpen = true;
	UI->SetPalettePage(ECityPalettePage::HousingHistory);
	UI->RefreshReports();
	TestTrue(TEXT("Shared history list identifies home and citizens"),
	         UI->SharedReportText.ToString().Contains(TEXT("Home #")) &&
	             UI->SharedReportText.ToString().Contains(TEXT("Citizens #")));
	const auto Bounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	AddInfo(FString::Printf(TEXT("Housing list native bounds: %s"), *Bounds.ToString()));
	TestTrue(TEXT("World report content stays above controls/below panel top"),
	         Bounds.Min.Z >= 28 && Bounds.Max.Z <= 55);
	TestTrue(TEXT("World report content stays inside panel width"),
	         Bounds.Min.Y >= -37 && Bounds.Max.Y <= 37);
	auto LongEvent = P.HousingEvents.Last();
	LongEvent.Kind = ECityHousingEventKind::Departed;
	LongEvent.Concerns = FCityHappinessEvaluator::ValidConcerns;
	LongEvent.Money = MAX_int64;
	UI->ReportText->SetText(FText::FromString(FCityHousingHistory::Describe(LongEvent)));
	const auto LongBounds = UI->ReportText->CalcBounds(UI->ReportText->GetRelativeTransform()).GetBox();
	const auto FooterBounds =
	    UI->ReportRangeText->CalcBounds(UI->ReportRangeText->GetRelativeTransform()).GetBox();
	AddInfo(FString::Printf(TEXT("Long event bounds: %s | footer: %s"), *LongBounds.ToString(),
	                        *FooterBounds.ToString()));
	TestTrue(TEXT("All recorded concern types and large money fit above footer"),
	         LongBounds.Min.Z > FooterBounds.Max.Z && LongBounds.Max.Z <= 55 && LongBounds.Min.Y >= -37 &&
	             LongBounds.Max.Y <= 37);
	TestTrue(TEXT("Warning footer stays above navigation buttons"), FooterBounds.Min.Z >= 24.2);
	UI->RefreshReports();
	TestFalse(TEXT("Older recorded searches reachable"),
	          UI->IsPaletteActionDisabled(ECityPaletteAction::HousingOlder));
	UI->ExecuteSharedPaletteAction(ECityPaletteAction::HousingOlder);
	UI->RefreshReports();
	TestTrue(TEXT("Older page shows another household"), UI->HousingHistoryPage == 1);
	UI->bDesktopUi = true;
	TestTrue(TEXT("Desktop receives the same list"),
	         UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("HOME SEARCH")));
	UI->bDesktopUi = false;
	UI->HousingHistoryPage = 0;
	UI->RefreshReports();
	const uint64 NextId = Sim->GetPopulationSnapshot().NextHousingEventId;
	for (int32 I = 0; I < 5; ++I)
		UI->RefreshReports();
	Sim->Tick(5);
	TestEqual(TEXT("Display and pause never duplicate history"),
	          Sim->GetPopulationSnapshot().NextHousingEventId, NextId);
	FString RoofDetail;
	FVector Roof;
	TArray<FCityInspectionMetric> Metrics;
	Board->GetInspectionBuilding(Home, Roof, RoofDetail, &Metrics);
	TestTrue(TEXT("Home inspection leads with household warning"),
	         RoofDetail.StartsWith(TEXT("HOUSEHOLD SEARCH")));
	Sim->SetSimulationPaused(false);
	for (int32 I = 0; I < 16; ++I)
		Sim->Tick(.125f);
	Sim->SetSimulationPaused(true);
	UI->RefreshReports();
	TestTrue(TEXT("Genuine departures appear in the recent list"),
	         UI->SharedReportText.ToString().Contains(TEXT("LEFT CITY")) &&
	             UI->SharedReportText.ToString().Contains(TEXT("cash carried out")));
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/Housing-Feedback-Departed.json");
	FString Error;
	TestTrue(TEXT("Factual history city saves"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Factual history city loads"), Sim->LoadCityFromPath(Path, Error, false));
	UI->RefreshReports();
	TestTrue(TEXT("Reload retains departed citizen identities/reasons"),
	         UI->SharedReportText.ToString().Contains(TEXT("Citizens #")) &&
	             UI->SharedReportText.ToString().Contains(TEXT("unmet purchases")));
	W->DestroyWorld(false);
	return true;
}
class FCityDesktopHousingFeedbackCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopHousingFeedbackCommand(FAutomationTestBase* T) : Test(T)
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
			Test->AddError(TEXT("Desktop feedback HUD missing"));
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
					auto* B = Cast<UCityDesktopActionButton>(
					    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Menu%d"), I))));
					if (!B || !B->GetIsEnabled())
						Test->AddError(TEXT("Housing menu button unavailable"));
					else
						B->OnClicked.Broadcast();
					return;
				}
			Test->AddError(TEXT("Housing menu action missing"));
		};
		auto Capture = [](const TCHAR* Name) {
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/HousingFeedback");
			IFileManager::Get().MakeDirectory(*Directory, true);
			FScreenshotRequest::RequestScreenshot(Directory / Name, true, false);
		};
		FString Error;
		switch (Phase)
		{
		case 0:
			UI->ReturnToCity();
			Home = FCityHousingFeedbackFixture::Seed(Sim, UI->Board);
			if (!Home)
			{
				Test->AddError(TEXT("Housing feedback city failed to place"));
				return true;
			}
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			Test->TestEqual(TEXT("Fixture contains real search events"),
			                Sim->GetPopulationSnapshot().HousingEvents.Num(), 3);
			Test->TestTrue(
			    TEXT("Search review city saves"),
			    Sim->SaveCityToPath(FPaths::ProjectSavedDir() / TEXT("Automation/Housing-Feedback-Search.json"),
			                        Error));
			break;
		case 1:
			UI->UpdateConditionIndicators();
			Test->TestTrue(TEXT("Desktop warning is visible in its context strip"),
			               UI->GetUiSnapshot().Construction.ToString().Contains(TEXT("HOME SEARCH: 30")));
			Test->TestTrue(TEXT("World-attached HOME SEARCH warnings are visible"),
			               !UI->ConditionIndicators.IsEmpty() &&
			                   UI->ConditionIndicators[0].SearchingResidents > 0);
			Capture(TEXT("SearchWarnings.png"));
			break;
		case 2:
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 3:
			Click(ECityPaletteAction::OverlayHappiness);
			break;
		case 4:
			Test->TestEqual(TEXT("Real happiness button selects overlay"), UI->Board->GetDiagnosticOverlay(),
			                ECityDiagnosticOverlay::Happiness);
			Test->TestTrue(TEXT("Desktop happiness legend is visible"),
			               UI->GetUiSnapshot().Tool.ToString().Contains(TEXT("red <50")));
			Capture(TEXT("HappinessOverlay.png"));
			break;
		case 5:
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 6:
			Click(ECityPaletteAction::OpenHousingHistory);
			break;
		case 7:
			Test->TestTrue(TEXT("Rendered list identifies real people and causes"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Citizens #")) &&
			                   UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("unmet purchases")));
			Capture(TEXT("HousingSearchEvents.png"));
			break;
		case 8:
			Click(ECityPaletteAction::HousingOlder);
			break;
		case 9:
			Test->TestEqual(TEXT("Older button browses real history"), UI->HousingHistoryPage, 1);
			Capture(TEXT("HousingOlderEvents.png"));
			break;
		case 10:
			UI->ReturnToCity();
			UI->Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
			UI->InspectBuilding(Home);
			break;
		case 11: {
			auto* B = Cast<UCityDesktopActionButton>(
			    UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectionDetails")));
			if (B)
				B->OnClicked.Broadcast();
			else
				Test->AddError(TEXT("Home details control missing"));
			Capture(TEXT("HouseholdWarningDetails.png"));
			break;
		}
		case 12:
			UI->ReturnToCity();
			Sim->SetSimulationPaused(false);
			for (int32 I = 0; I < 16; ++I)
				Sim->Tick(.125f);
			Sim->SetSimulationPaused(true);
			Test->TestEqual(TEXT("Native departures recorded"),
			                Sim->GetPopulationSnapshot().DepartedResidents, int64(30));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 13:
			Click(ECityPaletteAction::OpenHousingHistory);
			break;
		case 14:
			Test->TestTrue(TEXT("Departure list exposes outward money and reasons"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("LEFT CITY")) &&
			                   UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("cash carried out")));
			Capture(TEXT("HousingDepartureEvents.png"));
			break;
		case 15:
			Test->TestTrue(
			    TEXT("Departed review city saves"),
			    Sim->SaveCityToPath(FPaths::ProjectSavedDir() / TEXT("Automation/Housing-Feedback-Departed.json"),
			                        Error));
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_DESKTOP_HOUSING_FEEDBACK_COMPLETE SyntheticButtons=1 Captures=6 "
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
	uint64 Home = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopHousingFeedbackTest, "CitySim.Desktop.HousingFeedback",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopHousingFeedbackTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) ||
	    !UserDir.Contains(TEXT("Housing-Feedback")))
	{
		AddError(
		    TEXT("Housing feedback render check requires isolated Housing-Feedback UserDir and CityDesktop"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopHousingFeedbackCommand(this));
	return true;
}

#endif
