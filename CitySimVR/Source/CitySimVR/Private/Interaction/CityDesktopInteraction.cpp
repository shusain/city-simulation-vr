#include "Interaction/CityVRToolController.h"
#include "Presentation/CityTutorialSettings.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputMappingContext.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityFirstPlayableGuide.h"
#include "UnrealClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

namespace
{
bool IsCompletedDesktopSelection(const ECityPaletteAction Action)
{
	if (!FCityPaletteModel::ConstructionDefinition(Action).IsNone() ||
	    (Action >= ECityPaletteAction::SelectInspect && Action <= ECityPaletteAction::SelectIndustrialZone))
		return true;
	// Closing is a desktop presentation policy. Shared commands still own the
	// selection, including choosing an already-active value.
	switch (Action)
	{
	case ECityPaletteAction::LightingAutomatic:
	case ECityPaletteAction::LightingDay:
	case ECityPaletteAction::LightingDusk:
	case ECityPaletteAction::LightingNight:
	case ECityPaletteAction::OverlayService:
	case ECityPaletteAction::OverlayPower:
	case ECityPaletteAction::OverlayResidential:
	case ECityPaletteAction::OverlayCommercial:
	case ECityPaletteAction::OverlayIndustrial:
	case ECityPaletteAction::OverlayPollution:
	case ECityPaletteAction::OverlayTraffic:
	case ECityPaletteAction::OverlayCrime:
	case ECityPaletteAction::OverlayFire:
	case ECityPaletteAction::OverlayClear:
	case ECityPaletteAction::OverlayCommercialAi:
	case ECityPaletteAction::OverlayEducation:
	case ECityPaletteAction::OverlayParks:
	case ECityPaletteAction::OverlayHealth:
	case ECityPaletteAction::OverlayWaste:
	case ECityPaletteAction::SelectTestFire:
	case ECityPaletteAction::SelectTestCrime:
	case ECityPaletteAction::SelectTestArson:
	case ECityPaletteAction::SelectTestSickness:
	case ECityPaletteAction::SelectTestSevereSickness:
	case ECityPaletteAction::SelectTestWaste:
	case ECityPaletteAction::DisastersOff:
	case ECityPaletteAction::DisastersLow:
	case ECityPaletteAction::DisastersMedium:
	case ECityPaletteAction::DisastersHigh:
	case ECityPaletteAction::AlarmMute:
	case ECityPaletteAction::Alarm25:
	case ECityPaletteAction::Alarm50:
	case ECityPaletteAction::Alarm75:
	case ECityPaletteAction::Alarm100:
	case ECityPaletteAction::AmbienceMute:
	case ECityPaletteAction::Ambience25:
	case ECityPaletteAction::Ambience50:
	case ECityPaletteAction::Ambience75:
	case ECityPaletteAction::Ambience100:
	case ECityPaletteAction::MusicMute:
	case ECityPaletteAction::Music25:
	case ECityPaletteAction::Music50:
	case ECityPaletteAction::Music75:
	case ECityPaletteAction::Music100:
	case ECityPaletteAction::ToggleConditionIndicators:
	case ECityPaletteAction::Save:
	case ECityPaletteAction::PreviewAlarm:
	case ECityPaletteAction::TestEarthquake:
	case ECityPaletteAction::TestTornado:
	case ECityPaletteAction::ApplyTaxes:
	case ECityPaletteAction::CancelTaxes:
		return true;
	default:
		return false;
	}
}
} // namespace

void ACityVRToolController::InitializeDesktopVideo()
{
	if (!GEngine || !GEngine->GetGameUserSettings())
		return;
	auto* Settings = GEngine->GetGameUserSettings();
	const bool bOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen"));
	const bool bOverride = bOffscreen || FParse::Param(FCommandLine::Get(), TEXT("windowed")) ||
	                       FParse::Param(FCommandLine::Get(), TEXT("fullscreen"));
	bool bInitialized = false;
	GConfig->GetBool(TEXT("CitySim.DesktopVideo"), TEXT("Initialized"), bInitialized, GGameUserSettingsIni);
	FIntPoint Native = Settings->GetDesktopResolution();
	if (Native.X < 1024 || Native.Y < 720)
		Native = FIntPoint(1280, 800);
	if (!bInitialized && !bOverride)
	{
		Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		Settings->SetScreenResolution(Native);
		Settings->ConfirmVideoMode();
		Settings->ApplyResolutionSettings(false);
		Settings->SaveSettings();
		GConfig->SetBool(TEXT("CitySim.DesktopVideo"), TEXT("Initialized"), true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	else if (bOverride)
	{
		// Recovery/QA options are session-only and do not erase saved preferences.
		Settings->SetFullscreenMode(GSystemResolution.WindowMode);
		Settings->SetScreenResolution(FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY));
	}
	TArray<FIntPoint> Supported;
	if (!bOverride && Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen &&
	    Settings->GetScreenResolution() != Native)
	{
		// Update old DPI-virtualized borderless sizes after DPI-aware desktop startup.
		Settings->SetScreenResolution(Native);
		Settings->ConfirmVideoMode();
		Settings->ApplyResolutionSettings(false);
		Settings->SaveSettings();
	}
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	DesktopVideo.Initialize(Settings, Supported, Native, !bOffscreen);
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_VIDEO Mode=%s Resolution=%dx%d Override=%d"),
	       *DesktopVideo.ModeLabel(), DesktopVideo.GetResolution().X, DesktopVideo.GetResolution().Y,
	       bOverride);
}

FCityUiActionView ACityVRToolController::MakeActionView(const FCityPaletteEntry& Entry,
                                                        const int32 Index) const
{
	FCityUiActionView View;
	View.Index = Index;
	View.bStartNewRow = PalettePage == ECityPalettePage::Reports &&
	    (Entry.Action == ECityPaletteAction::ReportPeople || Entry.Action == ECityPaletteAction::ReportMetricPrevious ||
	     Entry.Action == ECityPaletteAction::Back);
	View.ActionId = static_cast<int32>(Entry.Action);
	View.Label = FText::FromString(GetPaletteEntryLabel(Entry));
	if (bDesktopUi && Entry.Action == ECityPaletteAction::OpenZone)
		View.Label = FText::FromString(View.Label.ToString() + TEXT(" [Z]"));
	View.Help = FText::FromString(GetPaletteEntryHelp(Entry));
	View.bEnabled = !IsPaletteActionDisabled(Entry.Action);
	View.bSelected = IsPaletteActionSelected(Entry.Action);
	View.bTutorialAttention = View.bEnabled && IsTutorialTarget(Entry.Action);
	return View;
}
FCityUiActionView ACityVRToolController::GetPaletteEntryView(const int32 Index) const
{
	return VisiblePaletteEntries.IsValidIndex(Index) ? MakeActionView(VisiblePaletteEntries[Index], Index)
	                                                 : FCityUiActionView();
}
const FCityTerrainGenerationParameters* ACityVRToolController::GetDesktopTerrainPreview() const
{
	if (!bDesktopUi || !bPaletteOpen)
		return nullptr;
	if (PalettePage == ECityPalettePage::NewCitySetup || PalettePage == ECityPalettePage::NewCityReview)
		return &NewCityTerrain;
	if (PalettePage == ECityPalettePage::ArchiveReview && ArchiveReviewSlot.bSupported)
		return &ArchiveReviewSlot.Terrain;
	if (PalettePage == ECityPalettePage::LoadReview && ReviewedCitySlots.IsValidIndex(ReviewedCityIndex) &&
	    ReviewedCitySlots[ReviewedCityIndex].bSupported)
		return &ReviewedCitySlots[ReviewedCityIndex].Terrain;
	return nullptr;
}
FCityUiSnapshot ACityVRToolController::GetUiSnapshot() const
{
	FCityUiSnapshot Data;
	Data.bEntryScene = bEntrySceneActive;
	Data.bMenuOpen = bPaletteOpen;
	Data.bZoneMenu = bPaletteOpen && !bEntrySceneActive && PalettePage == ECityPalettePage::Zone;
	Data.bTaxMenu = bDesktopUi && bPaletteOpen && PalettePage == ECityPalettePage::Taxes;
	if (Data.bTaxMenu)
	{
		const FCityBoundedValueControl* Controls[] = {&ResidentialTaxControl, &CommercialTaxControl,
		                                              &IndustrialTaxControl};
		const TCHAR* Labels[] = {TEXT("Residential"), TEXT("Commercial"), TEXT("Industrial")};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			FCityUiGauge Rate;
			Rate.Label = FText::FromString(Labels[Index]);
			Rate.Value = FText::FromString(
			    FString::Printf(TEXT("%.0f%%"), Controls[Index]->GetPendingValue() / 100.0));
			Data.TaxRates.Add(Rate);
		}
	}
	Data.bCityInfoOpen = bNeedsOpen && InspectedEntityId == FCityGrid::EmptyEntityId;
	Data.bObjectivesOpen = bObjectivesOpen;
	Data.bDetailedMenu =
	    (bEntrySceneActive && PalettePage != ECityPalettePage::MainMenu) ||
	    PalettePage == ECityPalettePage::Reports || PalettePage == ECityPalettePage::DisasterHistory ||
	    PalettePage == ECityPalettePage::Taxes ||
	    (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	     PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	     PalettePage == ECityPalettePage::CommercialAi || PalettePage == ECityPalettePage::TechnologyReview || PalettePage == ECityPalettePage::Policies ||
	     PalettePage == ECityPalettePage::PolicyReview);
	Data.MenuTitle = FText::FromString(FCityPaletteModel::PageTitle(PalettePage));
	Data.bNewspaper = PalettePage == ECityPalettePage::QuarterlyNews;
	if (Data.bNewspaper)
		Data.Newspaper = SharedNewspaper;
	Data.MenuBody = FText::FromString(GetPaletteHelp());
	if (PalettePage == ECityPalettePage::Audio)
		Data.MenuBody = FText::FromString(TEXT("Separate FX, ambience and music volumes."));
	if (PalettePage == ECityPalettePage::Reports || PalettePage == ECityPalettePage::DisasterHistory ||
	    (PalettePage == ECityPalettePage::HousingHistory || PalettePage == ECityPalettePage::QuarterlyNews ||
	     PalettePage == ECityPalettePage::Loans || PalettePage == ECityPalettePage::LoanReview ||
	     PalettePage == ECityPalettePage::CommercialAi || PalettePage == ECityPalettePage::TechnologyReview || PalettePage == ECityPalettePage::Policies ||
	     PalettePage == ECityPalettePage::PolicyReview))
		Data.MenuBody =
		    FText::FromString(SharedReportText.ToString() + TEXT("\n") + SharedReportRange.ToString());
	if (bPaletteOpen && PalettePage == ECityPalettePage::Reports)
		Data.ReportChart = SharedReportChart;
	const auto Toolbar = FCityPaletteModel::SurfaceEntries(ECityPalettePage::Root, false);
	for (int32 Index = 0; Index < VisiblePaletteEntries.Num(); ++Index)
	{
		// Dedicated Desktop rows present rates as text and +/- as controls.
		const auto Action = VisiblePaletteEntries[Index].Action;
		if (Data.bTaxMenu && ((Action >= ECityPaletteAction::TaxResidentialDown &&
		                       Action <= ECityPaletteAction::TaxCommercialTrack) ||
		                      (Action >= ECityPaletteAction::TaxIndustrialDown &&
		                       Action <= ECityPaletteAction::TaxIndustrialTrack)))
			continue;
		// Dock-level Desktop pages dismiss with Esc, their close control or another dock choice.
		// Nested categories and all VR surfaces retain shared Back navigation.
		const bool bDockPage =
		    PalettePage == ECityPalettePage::Build || PalettePage == ECityPalettePage::Zone ||
		    PalettePage == ECityPalettePage::City || PalettePage == ECityPalettePage::Overlays ||
		    PalettePage == ECityPalettePage::Settings;
		if (bDesktopUi && !bEntrySceneActive && bDockPage &&
		    VisiblePaletteEntries[Index].Action == ECityPaletteAction::Back)
			continue;
		// VR and desktop render these same views; desktop moves persistent navigation into its toolbar.
		const bool bToolbarAction = Toolbar.ContainsByPredicate([&](const FCityPaletteEntry& Entry) {
			return Entry.Action == VisiblePaletteEntries[Index].Action;
		});
		if (!bDesktopUi || bEntrySceneActive || !bToolbarAction)
			Data.MenuEntries.Add(GetPaletteEntryView(Index));
	}
	if (bDesktopUi) PopulateDesktopMenu(Data);
	if (Board)
	{
		Data.bRoadTool = Board->GetToolMode() == ECityToolMode::Road;
		const auto Tool = Board->GetToolMode();
		Data.bCanCancelTool = Tool != ECityToolMode::Inspect || bPaletteOpen;
		Data.bPlacementTool = Tool != ECityToolMode::Inspect && Tool != ECityToolMode::Zone &&
		                      Tool != ECityToolMode::Demolish && Tool != ECityToolMode::Repair &&
		                      Tool != ECityToolMode::TestFire && Tool != ECityToolMode::TestCrime &&
		                      Tool != ECityToolMode::TestArson && Tool != ECityToolMode::TestSickness &&
		                      Tool != ECityToolMode::TestSevereSickness && Tool != ECityToolMode::TestWaste;
		Data.Calendar = FText::FromString(Board->DescribeCalendar(!bDesktopUi));
		Data.Totals = FText::FromString(Board->DescribeCityTotals());
		int64 MonthlyMoney = 0, PopulationChange = 0;
		bool bPopulationHistory = false;
		Board->GetCityHudTrends(MonthlyMoney, PopulationChange, bPopulationHistory);
		Data.Trends = FText::FromString(CityUiContent::Trend(0, MonthlyMoney, true) + TEXT("  |  ") +
		                                CityUiContent::Trend(1, PopulationChange, bPopulationHistory));
		Data.Tool = FText::FromString(Board->GetToolDisplayName());
		Data.Construction = FText::FromString(CityUiContent::Construction(*Board));
		if (bDesktopUi && !bEntrySceneActive && !bPaletteOpen)
			Data.Construction = FText::FromString(Board->DescribeDesktopToolContext());
		else if (bDesktopUi)
			Data.Construction = FText::GetEmpty();
		if (bDesktopUi && !bEntrySceneActive && !bPaletteOpen && Tool == ECityToolMode::Inspect)
		{
			if (Board->GetDiagnosticOverlay() == ECityDiagnosticOverlay::Happiness)
				Data.Tool = FText::FromString(TEXT("Happiness | red <50 | amber50-69 | green70+"));
			if (bConditionIndicatorsVisible)
			{
				const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
				int32 Searching = 0;
				if (Sim)
					for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
						Searching += FCityHousingHistory::IsSearching(static_cast<uint8>(C.HousingStatus));
				if (Searching)
					Data.Construction = FText::FromString(FString::Printf(
					    TEXT("HOME SEARCH: %d residents | City > Housing events"), Searching));
			}
		}
		Data.bUnaffordable = Board->IsConstructionPriceBlocked();
		if (bNeedsOpen)
		{
			Data.Growth = FText::FromString(CityUiContent::Growth(*Board));
			for (const auto& Source : Board->GetCityInfoCards())
			{
				FCityUiInfoCard Card;
				Card.Title = FText::FromString(Source.Title);
				Card.Labels = FText::FromString(Source.Labels);
				Card.Values = FText::FromString(Source.Values);
				Data.CityCards.Add(Card);
			}
		}
	}
	for (int32 Index = 0; Index < Toolbar.Num(); ++Index)
	{
		const auto& Entry = Toolbar[Index];
		auto View = MakeActionView(Entry, Index);
		if (bDesktopUi)
		{
			// A closed workspace's remembered page is not an active Desktop selection.
			if (FCityPaletteModel::IsNavigationAction(Entry.Action))
			{
				View.bSelected = bPaletteOpen && View.bSelected;
				if (!bPaletteOpen && Board)
				{
					View.bSelected = (Entry.Action == ECityPaletteAction::OpenBuild && Data.bPlacementTool) ||
					                 (Entry.Action == ECityPaletteAction::OpenZone &&
					                  Board->GetToolMode() == ECityToolMode::Zone);
				}
			}
			else if (bPaletteOpen)
				View.bSelected = false;
		}
		Data.ToolbarEntries.Add(View);
	}
	if (const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
	{
		Data.bPaused = Sim->IsSimulationPaused();
		Data.SpeedMultiplier = Sim->GetSimulationSpeed();
		Data.Budget = FText::FromString(CityUiContent::Budget(*Sim));
		Data.LastNet = FText::FromString(CityUiContent::LastNet(*Sim));
		Data.Objectives = FText::FromString(CityUiContent::Objective(GetObjectiveCard()));
		const auto Objective = GetObjectiveCard();
		Data.ObjectiveTitle = FText::FromString(Objective.Title);
		Data.bTutorialGuidance = GetDefault<UCityTutorialSettings>()->bEnabled && !bObjectiveReview &&
		    (CityTutorialAttention::IsOpeningStep(Objective.Id) || Objective.Id == TEXT("tutorial_complete"));
		Data.ObjectiveProgress = FText::FromString(Objective.Progress +
		    (Data.bTutorialGuidance ? TEXT("\n\n") + Objective.Hint : TEXT("")));
		Data.bObjectiveHistoryAvailable = !Sim->GetGuidedProgress().CompletedIds.IsEmpty();
		Data.bObjectiveReview = bObjectiveReview && Data.bObjectiveHistoryAvailable;
		Data.bObjectiveNextAvailable = Data.bObjectiveReview && Sim->GetGuidedProgress().CompletedIds.Num() > 1;
		Data.Funds =
		    FText::FromString(TEXT("$") + FText::AsNumber(Sim->GetPlacementModel().GetFunds()).ToString());
		Data.Population = FText::AsNumber(Sim->GetEconomySnapshot().Residents);
		Data.Weather = FText::FromString(Sim->GetEnvironmentSnapshot().DescribeWeather());
		if (bNeedsOpen)
			CityUiContent::NeedsGauges(Sim->GetNeedsSnapshot(), Data);
	}
	Data.Inspection =
	    FText::FromString(bDesktopUi ? SharedInspectionDetail : LastInspectionText + SharedInspectionMetrics);
	Data.InspectionTitle = SharedInspectionTitle;
	Data.InspectionSummary = SharedInspectionSummary;
	Data.InspectionGauges = SharedInspectionGauges;
	Data.InspectionSections = SharedInspectionSections;
	if (const auto* InspectionSim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	    bDesktopUi && InspectionSim && !Data.Inspection.IsEmpty())
	{
		CityUiContent::InspectionDetails(*InspectionSim, InspectedEntityId, Data);
		CityUiContent::InspectionOverview(*InspectionSim, InspectedEntityId, Data);
		if (!InspectionSim->GetPopulationSnapshot().Households.ContainsByPredicate(
		        [&](const auto& H) { return H.HomeEntityId == InspectedEntityId; }))
		{
			const FString Cause = FCityConditionIndicators::Explain(*InspectionSim, InspectedEntityId);
			if (!Cause.IsEmpty()) Data.InspectionSummary = FText::FromString(Cause);
		}
	}
	Data.Upgrade = SharedUpgradeLabel;
	Data.bUpgradeAvailable = bInspectorUpgradeAvailable;
	if (!bEntrySceneActive)
	{
		const auto Candidates = FCityPaletteModel::SurfaceEntries(PalettePage, false);
		for (const auto& Entry : Candidates)
			if (IsTutorialTarget(Entry.Action)) { Data.TutorialActionId = static_cast<int32>(Entry.Action); break; }
	}
	if (IsTutorialTarget(ECityPaletteAction::Pause)) Data.TutorialActionId = -20;
	return Data;
}

bool ACityVRToolController::GetDesktopModelSource(FCityDesktopModelSource& Out) const
{
	Out = {};
	const auto* Sim = GetWorld() ? GetWorld()->GetSubsystem<UCitySimulationSubsystem>() : nullptr;
	return bDesktopUi && Board && !bEntrySceneActive && !bPaletteOpen &&
	       Board->GetToolMode() == ECityToolMode::Inspect && Sim &&
	       InspectionGeneration == Sim->GetCitySessionGeneration() &&
	       Board->GetDesktopModelSource(InspectedEntityId, Out);
}
void ACityVRToolController::InitializeDesktopInput(UEnhancedInputComponent* Input,
                                                   UEnhancedInputLocalPlayerSubsystem* Subsystem)
{
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("CityDesktopMappingContext"));
	auto Bind = [this, Input](const TCHAR* Name, const FKey& Key, const int32 Action) {
		auto* InputAction = MakeAction(Name);
		MapAction(InputAction, Key);
		Input->BindAction(InputAction, ETriggerEvent::Started, this,
		                  &ACityVRToolController::ExecuteDesktopUiAction, Action);
	};
	auto* Confirm = MakeAction(TEXT("CityDesktopConfirm"));
	MapAction(Confirm, EKeys::LeftMouseButton);
	Input->BindAction(Confirm, ETriggerEvent::Started, this, &ACityVRToolController::ConfirmDesktop);
	Bind(TEXT("CityDesktopCancel"), EKeys::Escape, -7);
	Bind(TEXT("CityDesktopRotate"), EKeys::R, -6);
	Bind(TEXT("CityDesktopPause"), EKeys::SpaceBar, -20);
	Bind(TEXT("CityDesktopSave"), EKeys::F5, -4);
	Bind(TEXT("CityDesktopLoad"), EKeys::F9, -5);
	Bind(TEXT("CityDesktopSpeed1"), EKeys::One, -21);
	Bind(TEXT("CityDesktopSpeed3"), EKeys::Two, -22);
	Bind(TEXT("CityDesktopSpeed5"), EKeys::Three, -23);
	Bind(TEXT("CityDesktopRoadDown"), EKeys::LeftBracket, -11);
	Bind(TEXT("CityDesktopRoadUp"), EKeys::RightBracket, -12);
	Bind(TEXT("CityDesktopZoneCycle"), EKeys::Z, -24);
	Subsystem->AddMappingContext(MappingContext, 50);
	Board->SetCommandInputBound(true);
	bInputInitialized = true;
	UE_LOG(
	    LogTemp, Display,
	    TEXT(
	        "CITYSIM_DESKTOP_INPUT_READY Mouse=Confirm MMB=Pan RMB=Orbit Wheel=Zoom Keyboard=SharedActions"));
}
bool ACityVRToolController::IsDesktopZoningPaletteActive() const
{
	return bDesktopUi && !bEntrySceneActive && bPaletteOpen && PalettePage == ECityPalettePage::Zone &&
	       Board && Board->GetToolMode() == ECityToolMode::Zone;
}
bool ACityVRToolController::IsDesktopPointerBlocked() const
{
	const auto* Player = GetWorld()->GetFirstPlayerController();
	const auto* Pawn = Player ? Cast<ACitySandboxDesktopPawn>(Player->GetPawn()) : nullptr;
	return bEntrySceneActive || (bPaletteOpen && !IsDesktopZoningPaletteActive()) || !bDesktopFocused ||
	       !LifecycleInputGate.CanAcceptInput() || (DesktopHud && DesktopHud->IsPointerOverUi()) ||
	       (Pawn && Pawn->IsCameraGestureActive());
}
bool ACityVRToolController::IsDesktopCameraInputAllowed() const
{
	return bDesktopUi && !bEntrySceneActive && (!bPaletteOpen || IsDesktopZoningPaletteActive()) &&
	       bDesktopFocused && LifecycleInputGate.CanAcceptInput() &&
	       !(DesktopHud && DesktopHud->IsPointerOverUi());
}
void ACityVRToolController::UpdateDesktopFocus(const bool bFocused)
{
	if (bDesktopFocused == bFocused)
		return;
	bDesktopFocused = bFocused;
	// Window activation does not necessarily emit an application foreground event
	// (for example after a Windows permission prompt or Alt-Tab). Pair the polled
	// transitions so the entry menu dismissed on focus loss is restored on return.
	if (bFocused)
		HandleApplicationForegrounded();
	else
		HandleApplicationBackgrounded();
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_DESKTOP_FOCUS Focused=%d EntryScene=%d MenuOpen=%d"),
	       bFocused, bEntrySceneActive, bPaletteOpen);
}
void ACityVRToolController::UpdateDesktopInteraction()
{
	if (!Board)
		return;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const bool bFocused =
		    GEngine->GameViewport->Viewport->HasFocus() && FSlateApplication::Get().IsActive();
		UpdateDesktopFocus(bFocused);
	}
	FVector Origin, Direction;
	auto* Player = GetWorld()->GetFirstPlayerController();
	const bool bRay =
	    !IsDesktopPointerBlocked() && Player && Player->DeprojectMousePositionToWorld(Origin, Direction);
	Board->SetControllerRay(bRay ? Origin : FVector::ZeroVector, bRay ? Direction : FVector::ForwardVector,
	                        bRay, true);
}
void ACityVRToolController::ExecuteDesktopUiAction(const int32 Action)
{
	if (!bDesktopUi || !Board || !LifecycleInputGate.CanAcceptInput() || !bDesktopFocused)
		return;
	if (Action >= 0)
	{
		const auto Toolbar = FCityPaletteModel::SurfaceEntries(ECityPalettePage::Root, false);
		const auto Matches = [Action](const FCityPaletteEntry& Entry) {
			return static_cast<int32>(Entry.Action) == Action;
		};
		if (!VisiblePaletteEntries.ContainsByPredicate(Matches) && !Toolbar.ContainsByPredicate(Matches))
			return;
		if (bEntrySceneActive && !VisiblePaletteEntries.ContainsByPredicate(Matches))
			return;
		const auto PaletteAction = static_cast<ECityPaletteAction>(Action);
		if (IsPaletteActionDisabled(PaletteAction))
			return;
		const auto PreviousTool = Board->GetToolMode();
		const auto PreviousOverlay = Board->GetDiagnosticOverlay();
		const auto PreviousPage = PalettePage;
		const bool bRepeatToolbar = bPaletteOpen && Toolbar.ContainsByPredicate(Matches) &&
		                            FCityPaletteModel::IsNavigationAction(PaletteAction);
		if (!bPaletteOpen && FCityPaletteModel::IsNavigationAction(PaletteAction))
			SetPaletteOpen(true);
		ExecuteSharedPaletteAction(PaletteAction);
		const bool bZoneSelection = PaletteAction == ECityPaletteAction::SelectResidentialZone ||
		                            PaletteAction == ECityPaletteAction::SelectCommercialZone ||
		                            PaletteAction == ECityPaletteAction::SelectIndustrialZone;

		if (!bEntrySceneActive && !bZoneSelection &&
		    (PalettePage == ECityPalettePage::Root || IsCompletedDesktopSelection(PaletteAction) ||
		     Board->GetToolMode() != PreviousTool || Board->GetDiagnosticOverlay() != PreviousOverlay ||
		     (bRepeatToolbar && PalettePage == PreviousPage)))
			SetPaletteOpen(false);
		return;
	}
	if (bEntrySceneActive && Action != -1 && Action != -5 && Action != -7 && Action != -13)
		return;
	switch (Action)
	{
	case -1:
		ShowMainMenu();
		break;
	case -2:
		SetCitySummaryVisible(!bNeedsOpen);
		break;
	case -3:
		SetObjectivesVisible(!bObjectivesOpen);
		break;
	case -4:
		SaveCity();
		break;
	case -5:
		LoadCity();
		break;
	case -6:
		if (!bPaletteOpen)
			Rotate();
		break;
	case -7:
		// Unwind an unfinished gesture before dismissing its controls or disarming the tool.
		if (!bEntrySceneActive && Board->HasActiveToolAction())
			Board->RequestCancel();
		else if (bPaletteOpen && !bEntrySceneActive)
			SetPaletteOpen(false);
		else if (!bEntrySceneActive && Board->GetToolMode() != ECityToolMode::Inspect)
		{
			ClearInspection();
			SelectInspect();
		}
		else
			Cancel();
		break;
	case -8:
		ActivateInspectedUpgrade();
		break;
	case -9:
		ExecuteObjectiveFooter(0);
		break;
	case -10:
		ExecuteObjectiveFooter(1);
		break;
	case -11:
		if (!bPaletteOpen)
			Board->AdjustRoadElevation(-1);
		break;
	case -12:
		if (!bPaletteOpen)
			Board->AdjustRoadElevation(1);
		break;
	case -13:
		if (bEntrySceneActive)
			Cancel();
		else
			SetPaletteOpen(false);
		break;
	case -14:
		ClearInspection();
		SetCitySummaryVisible(false);
		break;
	case -30:
		ClearInspection();
		break;
	case -61:
		if (!bPaletteOpen && Board->GetToolMode() == ECityToolMode::Inspect && !Board->HasActiveToolAction())
			if (const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
			{
				FString Label;
				const auto Id = FCityConditionIndicators::RelatedLot(*Sim, InspectedEntityId, Label);
				if (Id)
					InspectBuilding(static_cast<int64>(Id));
			}
		break;
	case -33:
		SetCitySummaryVisible(false);
		break;
	case -20:
		TogglePause();
		break;
	case -21:
		SetSpeed1();
		break;
	case -22:
		SetSpeed3();
		break;
	case -23:
		SetSpeed5();
		break;
	case -24: {
		// Do not steal a shortcut from an unrelated dialog or unfinished edit.
		if (bPaletteOpen && PalettePage != ECityPalettePage::Zone)
			break;
		const bool bCycle = Board->GetToolMode() == ECityToolMode::Zone;
		const auto Zone = Board->GetSelectedZone();
		const auto Choice = !bCycle || Zone == ECityZoneType::Industrial
		                        ? ECityPaletteAction::SelectResidentialZone
		                    : Zone == ECityZoneType::Residential ? ECityPaletteAction::SelectCommercialZone
		                                                         : ECityPaletteAction::SelectIndustrialZone;
		if (!bPaletteOpen)
			SetPaletteOpen(true);
		SetPalettePage(ECityPalettePage::Zone);
		ExecuteDesktopUiAction(static_cast<int32>(Choice));
		break;
	}
	default:
		break;
	}
}
