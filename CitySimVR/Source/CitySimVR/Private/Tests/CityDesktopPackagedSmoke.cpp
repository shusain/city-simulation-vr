#include "Misc/AutomationTest.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopChart.h"
#include "Presentation/CityDesktopModelPreview.h"
#include "Presentation/CityDesktopIcon.h"
#include "Presentation/CityDesktopTerrainPreview.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityAudioSettings.h"
#include "Presentation/CityMusicComponent.h"
#include "Sound/SoundWave.h"
#include "Misc/CoreDelegates.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

// Run only in an isolated packaged test launch. Button delegates are synthetic;
// physical mouse/camera/focus behavior remains a manual acceptance check.
class FCityDesktopPackagedSmokeCommand : public IAutomationLatentCommand
{
public:
	explicit FCityDesktopPackagedSmokeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (World)
			for (TActorIterator<ACityVRToolController> It(World); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->DesktopHud)
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Packaged desktop HUD did not initialize"));
			return true;
		}
		// Offscreen windows have no OS focus. Exercise UI dispatch independently.
		const bool bObservedDesktopFocus = UI->bDesktopFocused;
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto Click = [this, UI](const ECityPaletteAction Action)
		{
			const auto Data = UI->GetUiSnapshot();
			for (int32 Index = 0; Index < Data.MenuEntries.Num(); ++Index)
				if (Data.MenuEntries[Index].ActionId == static_cast<int32>(Action))
				{
					auto* Button = Cast<UCityDesktopActionButton>(
					    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Menu%d"), Index))));
					if (!Button || !Button->GetIsEnabled())
					{
						Test->AddError(TEXT("Expected enabled menu button"));
						return;
					}
					Button->OnClicked.Broadcast();
					return;
				}
			Test->AddError(TEXT("Expected shared menu action was not presented"));
		};
		auto Capture = [](const TCHAR* Name)
		{
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/Desktop");
			IFileManager::Get().MakeDirectory(*Directory, true);
			FScreenshotRequest::RequestScreenshot(Directory / Name, true, false);
		};
		auto Control = [this, UI](const TCHAR* Name)
		{
			auto* Button = Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(FName(Name)));
			if (Button && Button->GetIsEnabled())
				Button->OnClicked.Broadcast();
			else
				Test->AddError(FString::Printf(TEXT("Expected enabled control: %s"), Name));
		};
		auto PanelVisible = [UI](const TCHAR* Name)
		{
			const auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(FName(Name));
			return Panel && Panel->IsVisible();
		};
		auto CheckIdleInspect = [this, UI, &PanelVisible]()
		{
			Test->TestFalse(TEXT("Idle Inspect Cancel is collapsed"), PanelVisible(TEXT("Cancel")));
			for (int32 Index = 0; Index < FCityPaletteModel::BarEntryCount; ++Index)
			{
				const auto* Button = Cast<UCityDesktopActionButton>(
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Toolbar%d"), Index))));
				Test->TestTrue(TEXT("Only Inspect keeps a selected toolbar style"),
				               Button && Button->IsSelected() == (Index == 0) &&
				                   Button->GetStyle().Normal.OutlineSettings.Width == (Index == 0 ? 2.f : 1.f));
			}
		};
		auto CheckPreviewPixels = [this, UI]()
		{
			const auto* Preview =
			    Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			Test->TestTrue(TEXT("Inspector keeps its live render surface while visible"),
			               Preview && Preview->HasModel() && Preview->GetPreviewTarget());
			if (!Preview || !Preview->GetPreviewTarget())
				return;
			const auto& Geometry = Preview->GetCachedGeometry();
			const FVector2D Display =
			    Geometry.LocalToAbsolute(Geometry.GetLocalSize()) - Geometry.GetAbsolutePosition();
			Test->TestTrue(TEXT("Displayed model preserves render aspect ratio without stretching"),
			               Display.Y > 1 && FMath::IsNearlyEqual(Display.X / Display.Y, 512. / 320., .01));
			TArray<FColor> Pixels;
			auto* Resource = Preview->GetPreviewTarget()->GameThread_GetRenderTargetResource();
			Test->TestTrue(TEXT("Live render target readback succeeds"), Resource && Resource->ReadPixels(Pixels));
			TSet<FColor> Colors;
			int32 BrightPixels = 0;
			for (const auto& Pixel : Pixels)
			{
				Colors.Add(Pixel);
				if (Pixel.R > 100 && Pixel.G > 100)
					++BrightPixels;
			}
			Test->TestTrue(TEXT("The building remains visibly rendered across inspector states"),
			               Colors.Num() > 32 && BrightPixels > 256);
		};
		auto CheckTopControls = [this, UI]()
		{
			const auto* Top = UI->DesktopHud->WidgetTree->FindWidget(TEXT("TopPanel"));
			if (!Top)
			{
				Test->AddError(TEXT("Top panel missing"));
				return;
			}
			if (!Top->IsVisible())
				return;
			const FVector2D TopEnd = Top->GetCachedGeometry().LocalToAbsolute(Top->GetCachedGeometry().GetLocalSize());
			for (const TCHAR* Name : {TEXT("CityToggle"), TEXT("ObjectivesToggle")})
			{
				const auto* Button =
				    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(FName(Name)));
				const auto* Label =
				    Cast<UTextBlock>(UI->DesktopHud->WidgetTree->FindWidget(FName(FString(Name) + TEXT("Label"))));
				if (!Button || !Label)
				{
					Test->AddError(TEXT("Side-panel control missing"));
					continue;
				}
				const auto& Box = Button->GetCachedGeometry();
				const auto& Text = Label->GetCachedGeometry();
				const FVector2D End = Box.LocalToAbsolute(Box.GetLocalSize());
				const FVector2D TextEnd = Text.LocalToAbsolute(Text.GetLocalSize());
				Test->TestTrue(TEXT("Updated top controls remain inside the header"),
				               End.X <= TopEnd.X + 1 && End.Y <= TopEnd.Y + 1);
				Test->TestTrue(TEXT("Toggle labels remain inside their clickable controls"),
				               Text.GetAbsolutePosition().X >= Box.GetAbsolutePosition().X - 1 &&
				                   Text.GetAbsolutePosition().Y >= Box.GetAbsolutePosition().Y - 1 &&
				                   TextEnd.X <= End.X + 1 && TextEnd.Y <= End.Y + 1);
			}
		};
		auto CheckMenuLabels = [this, UI]()
		{
			for (int32 Index = 0; Index < UI->GetUiSnapshot().MenuEntries.Num(); ++Index)
			{
				const FString Name = FString::Printf(TEXT("Menu%d"), Index);
				const auto* Button = UI->DesktopHud->WidgetTree->FindWidget(FName(Name));
				const auto* Label = UI->DesktopHud->WidgetTree->FindWidget(FName(Name + TEXT("Label")));
				if (!Button || !Label)
				{
					Test->AddError(TEXT("Menu label or button missing"));
					continue;
				}
				const auto& Box = Button->GetCachedGeometry();
				const auto& Text = Label->GetCachedGeometry();
				const FVector2D End = Box.LocalToAbsolute(Box.GetLocalSize());
				const FVector2D TextEnd = Text.LocalToAbsolute(Text.GetLocalSize());
				Test->TestTrue(TEXT("Menu labels fit their own clickable rectangles"),
				               Text.GetAbsolutePosition().X >= Box.GetAbsolutePosition().X - 1 &&
				                   Text.GetAbsolutePosition().Y >= Box.GetAbsolutePosition().Y - 1 &&
				                   TextEnd.X <= End.X + 1 && TextEnd.Y <= End.Y + 1);
			}
		};
		auto CheckMenuBody = [this, UI]()
		{
			const auto* Body = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuBody"));
			const auto* Choices = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuButtons"));
			const auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuPanel"));
			if (!Body || !Choices || !Panel)
			{
				Test->AddError(TEXT("Menu layout widgets missing"));
				return;
			}
			const auto& BodyGeometry = Body->GetCachedGeometry();
			const double Bottom = BodyGeometry.LocalToAbsolute(BodyGeometry.GetLocalSize()).Y;
			const auto& PanelGeometry = Panel->GetCachedGeometry();
			Test->TestTrue(TEXT("Size guidance stays above action buttons"),
			               Bottom <= Choices->GetCachedGeometry().GetAbsolutePosition().Y + 1);
			Test->TestTrue(TEXT("Size guidance stays within menu panel"),
			               Bottom <= PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()).Y - 4);
		};
		auto CheckToolLayout = [this, UI]()
		{
			const auto* Context = UI->DesktopHud->WidgetTree->FindWidget(TEXT("ToolContextPanel"));
			const auto* Toolbar = UI->DesktopHud->WidgetTree->FindWidget(TEXT("ToolbarPanel"));
			if (!Context || !Toolbar)
			{
				Test->AddError(TEXT("Tool controls and contextual information require separate panels"));
				return;
			}
			const auto& ContextBox = Context->GetCachedGeometry();
			Test->TestTrue(TEXT("Contextual information stays above the persistent controls"),
			               ContextBox.LocalToAbsolute(ContextBox.GetLocalSize()).Y <=
			                   Toolbar->GetCachedGeometry().GetAbsolutePosition().Y);
			for (const TCHAR* Name : {TEXT("Tool"), TEXT("Construction")})
			{
				const auto* Node = UI->DesktopHud->WidgetTree->FindWidget(Name);
				while (Node && Node != Context && Node != Toolbar)
					Node = Node->GetParent();
				Test->TestTrue(TEXT("Tool details and price warnings belong to their own information panel"),
				               Node == Context);
			}
			for (int32 Index = 0; Index < FCityPaletteModel::BarEntryCount; ++Index)
			{
				const auto* Button =
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Toolbar%d"), Index)));
				const auto* Label = Cast<UTextBlock>(
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Toolbar%dLabel"), Index))));
				if (!Button || !Label)
				{
					Test->AddError(TEXT("Persistent tool button or label missing"));
					return;
				}
				const auto& Box = Button->GetCachedGeometry();
				const FVector2D Position = Box.GetAbsolutePosition(), Size = Box.GetLocalSize();
				Test->TestTrue(TEXT("Persistent tool buttons have rendered geometry"), Size.X > 1 && Size.Y > 1);
				// Centered text may extend into the button's style padding. Check
				// its full desired extent against the clickable bounds.
				const auto& TextBox = Label->GetCachedGeometry();
				const FVector2D Desired = Label->GetDesiredSize();
				const FVector2D TextOffset((TextBox.GetLocalSize().X - Desired.X) * .5, 0);
				const FVector2D TextStart = Box.AbsoluteToLocal(TextBox.LocalToAbsolute(TextOffset));
				const FVector2D TextEnd =
				    Box.AbsoluteToLocal(TextBox.LocalToAbsolute(TextOffset + Desired));
				Test->TestTrue(FString::Printf(TEXT("Toolbar%d full label fits clickable bounds (%.1f..%.1f of %.1f)"),
				                             Index, TextStart.X, TextEnd.X, Size.X),
				               TextStart.X >= -1.f && TextStart.Y >= -1.f &&
				                   TextEnd.X <= Size.X + 1.f && TextEnd.Y <= Size.Y + 1.f);
				if (ToolPositions.Num() < FCityPaletteModel::BarEntryCount)
				{
					ToolPositions.Add(Position);
					ToolSizes.Add(Size);
				}
				else
				{
					Test->TestTrue(TEXT("Tool changes preserve each button's position and size"),
					               Position.Equals(ToolPositions[Index], .5) && Size.Equals(ToolSizes[Index], .5));
				}
			}
		};
		switch (Phase++)
		{
		case -3:
			// Offscreen automation has no real window activation; establish the
			// entry state before exercising a synthetic loss/return below.
			UI->ShowMainMenu();
			break;
		case -2:
			Test->TestTrue(TEXT("Entry menu is visible before focus interruption"), PanelVisible(TEXT("MenuPanel")));
			Capture(TEXT("EntryBeforeFocus.png"));
			break;
		case -1:
			UI->UpdateDesktopFocus(false);
			Test->TestFalse(TEXT("Focus loss dismisses the entry menu"), UI->GetUiSnapshot().bMenuOpen);
			UI->UpdateDesktopFocus(true);
			Test->TestTrue(TEXT("Polled focus return restores packaged entry without an OS foreground delegate"),
			               UI->GetUiSnapshot().bMenuOpen && UI->GetUiSnapshot().bEntryScene);
			UI->LifecycleInputGate.Update(false);
			// Give Slate a rendered frame after rebuilding the menu before its
			// screenshot/layout checks; same-frame captures can still be empty.
			break;
		case 0:
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopMusicOnly")))
			{
				auto* Music = UI->Board->FindComponentByClass<UCityMusicComponent>();
				Test->TestTrue(TEXT("Entry menu starts one native music player"),
				               Music && Music->IsReady() && Music->GetPlayingVoiceCount() == 1);
			}
			CheckTopControls();
			Test->TestTrue(TEXT("Packaged desktop entry is active"),
			               UI->bEntrySceneActive && UI->bDesktopUi && UI->bInputInitialized);
			Test->TestTrue(TEXT("Native HUD is on the viewport"), UI->DesktopHud->IsInViewport());
			if (const auto* Title = Cast<UTextBlock>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuTitle"))))
			{
				Test->TestNotNull(TEXT("Cooked HUD has a serializable font asset"), Title->GetFont().FontObject.Get());
				Test->TestTrue(TEXT("Cooked text has a nonzero layout extent"), Title->GetDesiredSize().Y > 1);
			}
			else
				Test->AddError(TEXT("Menu title widget missing"));
			Capture(TEXT("Entry.png"));
			break;
		case 1:
			Test->TestTrue(TEXT("Restored entry menu is rendered on the viewport"),
			               PanelVisible(TEXT("MenuPanel")) && UI->DesktopHud->IsInViewport());
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopStartupOnly")))
			{
				UE_LOG(LogTemp, Display,
				       TEXT("CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE StartupOnly=1 PhysicalInputPending=1"));
				return true;
			}
			Click(ECityPaletteAction::OpenNewCitySetup);
			break;
		case 2:
			Test->TestEqual(TEXT("New-city button opens shared setup"), UI->PalettePage,
			                ECityPalettePage::NewCitySetup);
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopMenusOnly")))
			{
				Phase = 400;
				break;
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopDimensionsOnly")))
			{
				Click(ECityPaletteAction::CycleMapSize);
				Phase = 200;
				break;
			}
			Click(ECityPaletteAction::ReviewCreate);
			break;
		case 3:
			Test->TestEqual(TEXT("Review is required before creating a city"), UI->PalettePage,
			                ECityPalettePage::NewCityReview);
			Click(ECityPaletteAction::ConfirmCreate);
			break;
		case 4:
			Test->TestFalse(TEXT("Shared create command enters the city"), UI->bEntrySceneActive);
			Sim->SetSimulationPaused(true);
			UI->SetObjectivesVisible(false);
			CheckToolLayout();
			Capture(TEXT("Overview.png"));
			// Preview-only follow-ups do not repeat unrelated menu/graphics/report validation.
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopInspectorOnly")))
				Phase = 78;
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopPaletteOnly")))
				Phase = 100;
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopDimensionsOnly")))
				Phase = 202;
			if (FParse::Param(FCommandLine::Get(), TEXT("CityDesktopMusicOnly")))
				Phase = 300;
			break;
		case 5:
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			break;
		case 6:
			CheckMenuLabels();
			Test->TestEqual(TEXT("Toolbar opens shared build menu"), UI->PalettePage, ECityPalettePage::Build);
			{
				const auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuPanel"));
				const FVector2D View = UI->DesktopHud->GetCachedGeometry().GetLocalSize();
				Test->TestTrue(TEXT("Build dialog occupies less than half the viewport width and height"),
				               Panel && Panel->GetCachedGeometry().GetLocalSize().X < View.X * .5 &&
				                   Panel->GetCachedGeometry().GetLocalSize().Y < View.Y * .5);
				for (const auto& Entry : UI->GetUiSnapshot().MenuEntries)
					Test->TestTrue(TEXT("Normal build excludes debug construction"),
					               Entry.ActionId != static_cast<int32>(ECityPaletteAction::SelectHome) &&
					                   Entry.ActionId != static_cast<int32>(ECityPaletteAction::SelectShop));
			}
			Capture(TEXT("Build.png"));
			break;
		case 7:
			Click(ECityPaletteAction::SelectPolice);
			break;
		case 8:
			Test->TestEqual(TEXT("Button delegate selects authoritative building tool"), UI->Board->GetToolMode(),
			                ECityToolMode::Police);
			Test->TestFalse(TEXT("Tool selection closes the menu"), UI->bPaletteOpen);
			UI->SetCitySummaryVisible(true);
			UI->SetObjectivesVisible(true);
			break;
		case 9:
		{
			CheckTopControls();
			CheckToolLayout();
			const auto Data = UI->GetUiSnapshot();
			auto* Budget = Cast<UTextBlock>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Budget")));
			Test->TestEqual(TEXT("Rendered desktop budget matches shared full-bills content"),
			                Budget->GetText().ToString(), Data.Budget.ToString());
			const auto* Objectives = UI->DesktopHud->WidgetTree->FindWidget(TEXT("ObjectivesPanel"));
			const auto* Info = UI->DesktopHud->WidgetTree->FindWidget(TEXT("InfoPanel"));
			Test->TestTrue(TEXT("Objectives and stats are both visible"),
			               PanelVisible(TEXT("ObjectivesPanel")) && PanelVisible(TEXT("InfoPanel")));
			if (Objectives && Info)
			{
				const auto& Left = Objectives->GetCachedGeometry();
				const auto& Right = Info->GetCachedGeometry();
				Test->TestTrue(TEXT("Objectives sit left of stats with an unobstructed gap"),
				               Left.LocalToAbsolute(Left.GetLocalSize()).X < Right.GetAbsolutePosition().X);
			}
			Capture(TEXT("CityInfo.png"));
			break;
		}
		case 10:
			Control(TEXT("CityToggle"));
			break;
		case 11:
			CheckTopControls();
			Test->TestTrue(TEXT("Stats toggle leaves only objectives visible"),
			               PanelVisible(TEXT("ObjectivesPanel")) && !PanelVisible(TEXT("InfoPanel")));
			Capture(TEXT("ObjectivesOnly.png"));
			break;
		case 12:
			Control(TEXT("ObjectivesToggle"));
			break;
		case 13:
			Test->TestTrue(TEXT("Both side panels can be hidden"),
			               !PanelVisible(TEXT("ObjectivesPanel")) && !PanelVisible(TEXT("InfoPanel")));
			Control(TEXT("CityToggle"));
			break;
		case 14:
			CheckTopControls();
			Test->TestTrue(TEXT("Stats toggle is independent of objectives"),
			               !PanelVisible(TEXT("ObjectivesPanel")) && PanelVisible(TEXT("InfoPanel")));
			Capture(TEXT("StatsOnly.png"));
			break;
		case 15:
			Control(TEXT("ObjectivesToggle"));
			break;
		case 16:
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenOverlays));
			break;
		case 17:
			Test->TestTrue(TEXT("Menu temporarily covers objectives without losing preference"),
			               UI->bPaletteOpen && UI->bObjectivesOpen && !PanelVisible(TEXT("ObjectivesPanel")));
			Click(ECityPaletteAction::OverlayPower);
			break;
		case 18:
			Test->TestTrue(TEXT("Completed selection restores objectives and retains stats"),
			               !UI->bPaletteOpen && PanelVisible(TEXT("ObjectivesPanel")) &&
			                   PanelVisible(TEXT("InfoPanel")));
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Audio);
			break;
		case 19:
			Click(ECityPaletteAction::Alarm50);
			break;
		case 20:
			Test->TestFalse(TEXT("Audio selection closes menu"), UI->bPaletteOpen);
			Test->TestEqual(TEXT("Audio choice reaches shared saved preference in isolated test data"),
			                GetDefault<UCityAudioSettings>()->GetAlarmVolume(), .5f);
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Audio);
			Click(ECityPaletteAction::Ambience25);
			Test->TestFalse(TEXT("Ambience selection closes the desktop menu"), UI->bPaletteOpen);
			Test->TestEqual(TEXT("Independent ambience choice reaches the shared preference"),
			                GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), .25f);
			Test->TestEqual(TEXT("Ambience selection preserves effects volume"),
			                GetDefault<UCityAudioSettings>()->GetAlarmVolume(), .5f);
			GetMutableDefault<UCityAudioSettings>()->SetAmbienceVolume(.5f);
			UI->ExecuteDesktopUiAction(-14);
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Reports);
			break;
		case 21:
			CheckMenuLabels();
			Test->TestFalse(TEXT("Info close hides the panel"), PanelVisible(TEXT("InfoPanel")));
			Test->TestFalse(TEXT("No settled history hides the chart instead of inventing bars"),
			                PanelVisible(TEXT("ReportChart")));
			Capture(TEXT("Reports.png"));
			break;
		case 22:
			Control(TEXT("MenuClose"));
			break;
		case 23:
			Test->TestFalse(TEXT("Close button dismisses a nested report dialog"), UI->bPaletteOpen);
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Settings);
			break;
		case 24:
			CheckMenuLabels();
			Capture(TEXT("Settings.png"));
			break;
		case 25:
			UI->SetDebugModeEnabled(true);
			UI->ShowDebugTools();
			break;
		case 26:
			CheckMenuLabels();
			Capture(TEXT("Debug.png"));
			break;
		case 27:
			Click(ECityPaletteAction::SelectHome);
			break;
		case 28:
			Test->TestTrue(TEXT("Debug home action selects and dismisses"),
			               UI->Board->GetToolMode() == ECityToolMode::Home && !UI->bPaletteOpen);
			UI->SetDebugModeEnabled(false);
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Taxes);
			UI->BeginTaxEdit();
			break;
		case 29:
			CheckMenuLabels();
			Capture(TEXT("Taxes.png"));
			break;
		case 30:
			Control(TEXT("MenuClose"));
			break;
		case 31:
			Test->TestFalse(TEXT("Close discards unfinished tax edit"), UI->ResidentialTaxControl.IsEditing());
			Test->TestFalse(TEXT("Road-only controls stay hidden for Inspect"), PanelVisible(TEXT("RoadUp")));
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Zone);
			break;
		case 32:
			CheckMenuLabels();
			Capture(TEXT("Zone.png"));
			break;
		case 33:
			Click(ECityPaletteAction::SelectResidentialZone);
			break;
		case 34:
			Test->TestTrue(TEXT("Zoning selection retains its palette while painting"),
			               UI->IsDesktopZoningPaletteActive());
			UI->ExecuteDesktopUiAction(-24);
			break;
		case 35:
			CheckToolLayout();
			Test->TestEqual(TEXT("Z cycles to commercial"), UI->Board->GetSelectedZone(), ECityZoneType::Commercial);
			Test->TestTrue(
			    TEXT("Chosen zone has a rendered selection marker"),
			    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Menu1")))->IsSelected());
			Capture(TEXT("ZoneCommercial.png"));
			break;
		case 36:
			UI->ExecuteDesktopUiAction(-24);
			break;
		case 37:
			Test->TestEqual(TEXT("Z cycles to industrial"), UI->Board->GetSelectedZone(), ECityZoneType::Industrial);
			Capture(TEXT("ZoneIndustrial.png"));
			break;
		case 38:
			Control(TEXT("MenuClose"));
			break;
		case 39:
			Test->TestFalse(TEXT("Zoning close dismisses its palette"), UI->bPaletteOpen);
			Control(TEXT("MenuToggle"));
			break;
		case 40:
			Test->TestTrue(TEXT("Main menu button opens real city management choices"),
			               UI->bEntrySceneActive && UI->PalettePage == ECityPalettePage::MainMenu &&
			                   UI->GetUiSnapshot().MenuEntries.Num() >= 4);
			Capture(TEXT("MainMenu.png"));
			break;
		case 41:
			Click(ECityPaletteAction::ContinueCity);
			break;
		case 42:
			Control(TEXT("Speed3"));
			break;
		case 43:
			Test->TestEqual(TEXT("3x button selects the simulation speed"), Sim->GetSimulationSpeed(), 3);
			Test->TestTrue(
			    TEXT("Selected speed is marked on its button"),
			    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Speed3")))->IsSelected());
			Test->TestFalse(TEXT("Desktop calendar has no redundant time status"),
			                Cast<UTextBlock>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Calendar")))
			                    ->GetText()
			                    .ToString()
			                    .Contains(TEXT("Time:")));
			Capture(TEXT("Speed3.png"));
			break;
		case 44:
			Control(TEXT("Pause"));
			break;
		case 45:
			Test->TestTrue(TEXT("Pause is marked and remembered speed is unmarked"),
			               Sim->IsSimulationPaused() &&
			                   Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Pause")))
			                       ->IsSelected() &&
			                   !Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Speed3")))
			                        ->IsSelected());
			Capture(TEXT("Paused.png"));
			break;
		case 46:
			Control(TEXT("Speed5"));
			break;
		case 47:
			Test->TestTrue(TEXT("5x button resumes and selects its speed"),
			               !Sim->IsSimulationPaused() && Sim->GetSimulationSpeed() == 5);
			Capture(TEXT("Speed5.png"));
			break;
		case 48:
			Control(TEXT("Speed1"));
			break;
		case 49:
			Test->TestEqual(TEXT("1x button restores normal speed"), Sim->GetSimulationSpeed(), 1);
			Control(TEXT("Pause"));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenPowerFacilities));
			break;
		case 50:
			Click(ECityPaletteAction::SelectWindPower);
			Test->TestFalse(TEXT("Building choices still close automatically"), UI->bPaletteOpen);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenPowerFacilities));
			break;
		case 51:
			CheckMenuLabels();
			CheckToolLayout();
			Test->TestTrue(
			    TEXT("Reopened power choices mark the last selected model"),
			    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Menu3")))->IsSelected());
			Capture(TEXT("SelectedPower.png"));
			break;
		case 52:
			Control(TEXT("MenuClose"));
			break;
		case 53:
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenSettings));
			break;
		case 54:
			Click(ECityPaletteAction::OpenGraphics);
			break;
		case 55:
			CheckMenuLabels();
			CheckToolLayout();
			Test->TestEqual(TEXT("Explicit windowed QA launch is respected"), UI->DesktopVideo.GetMode(),
			                EWindowMode::Windowed);
			BeforeVSync = UI->DesktopVideo.IsVSyncEnabled();
			Capture(TEXT("Graphics.png"));
			break;
		case 56:
			Click(ECityPaletteAction::GraphicsMode);
			Test->TestEqual(TEXT("Graphics button selects fullscreen"), UI->DesktopVideo.GetMode(),
			                EWindowMode::Fullscreen);
			Control(TEXT("MenuClose"));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenSettings));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenGraphics));
			break;
		case 57:
			Test->TestEqual(TEXT("Closing discards unapplied video edits"), UI->DesktopVideo.GetMode(),
			                EWindowMode::Windowed);
			Click(ECityPaletteAction::GraphicsVSync);
			Click(ECityPaletteAction::GraphicsRenderScale);
			Click(ECityPaletteAction::GraphicsAntiAliasing);
			break;
		case 58:
			Click(ECityPaletteAction::GraphicsApply);
			break;
		case 59:
			Test->TestEqual(TEXT("Preview applies native scene quality to the renderer"),
			                IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))->GetInt(),
			                UI->DesktopVideo.GetRenderScale());
			Test->TestEqual(TEXT("Preview applies MSAA to the renderer"),
			                IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"))->GetInt(),
			                UI->DesktopVideo.GetAntiAliasingSamples());
			Test->TestTrue(TEXT("Applied video changes require explicit confirmation"),
			               UI->DesktopVideo.IsConfirming());
			CheckMenuLabels();
			Capture(TEXT("GraphicsPreview.png"));
			break;
		case 60:
		{
			Click(ECityPaletteAction::GraphicsKeep);
			bool SavedVSync = BeforeVSync;
			Test->TestTrue(TEXT("Keep writes the confirmed preference into isolated game settings"),
			               GConfig->GetBool(TEXT("/Script/Engine.GameUserSettings"), TEXT("bUseVSync"), SavedVSync,
			                                GGameUserSettingsIni) &&
			                   SavedVSync != BeforeVSync);
			int32 SavedScale = 0, SavedAA = 0;
			Test->TestTrue(
			    TEXT("Keep serializes scene scale and MSAA"),
			    GConfig->GetInt(TEXT("CitySim.DesktopVideo"), TEXT("RenderScale"), SavedScale, GGameUserSettingsIni) &&
			        GConfig->GetInt(TEXT("CitySim.DesktopVideo"), TEXT("MSAASamples"), SavedAA, GGameUserSettingsIni) &&
			        SavedScale == UI->DesktopVideo.GetRenderScale() &&
			        SavedAA == UI->DesktopVideo.GetAntiAliasingSamples());
			break;
		}
		case 61:
			Click(ECityPaletteAction::OpenGraphics);
			break;
		case 62:
			Click(ECityPaletteAction::GraphicsVSync);
			Click(ECityPaletteAction::GraphicsAntiAliasing);
			break;
		case 63:
			Click(ECityPaletteAction::GraphicsApply);
			break;
		case 64:
			Test->TestTrue(TEXT("Unconfirmed display changes expire on wall time"),
			               UI->DesktopVideo.Tick(FPlatformTime::Seconds() + 16));
			Test->TestTrue(TEXT("Timeout restores the confirmed preference"),
			               UI->DesktopVideo.IsVSyncEnabled() != BeforeVSync);
			Test->TestEqual(TEXT("Timeout restores the confirmed renderer AA"),
			                IConsoleManager::Get().FindConsoleVariable(TEXT("r.MSAACount"))->GetInt(),
			                UI->DesktopVideo.GetAntiAliasingSamples());
			UI->SetPalettePage(ECityPalettePage::Graphics);
			break;
		case 65:
			Control(TEXT("MenuClose"));
			break;
		case 66:
			UI->SetCitySummaryVisible(true);
			UI->SetObjectivesVisible(false);
			Control(TEXT("CityDetails"));
			if (auto* Growth = UI->DesktopHud->WidgetTree->FindWidget(TEXT("Growth")))
			{
				UWidget* Parent = Growth->GetParent();
				while (Parent && !Cast<UScrollBox>(Parent))
					Parent = Parent->GetParent();
				if (auto* Scroll = Cast<UScrollBox>(Parent))
					Scroll->ScrollToEnd();
				else
					Test->AddError(TEXT("City growth guidance requires a scrollable panel"));
			}
			break;
		case 67:
			if (const auto* Growth = Cast<UTextBlock>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Growth"))))
			{
				Test->TestTrue(TEXT("Cooked growth guidance explains all density tiers"),
				               Growth->GetText().ToString().Contains(TEXT("Low / Medium / High")));
				const UWidget* Parent = Growth->GetParent();
				while (Parent && !Cast<UScrollBox>(Parent))
					Parent = Parent->GetParent();
				if (const auto* Scroll = Cast<UScrollBox>(Parent))
				{
					const auto& View = Scroll->GetCachedGeometry();
					const auto& Text = Growth->GetCachedGeometry();
					const FVector2D ViewEnd = View.LocalToAbsolute(View.GetLocalSize());
					const FVector2D TextEnd = Text.LocalToAbsolute(Text.GetLocalSize());
					Test->TestTrue(TEXT("Scrolling exposes the complete wrapped growth guidance"),
					               Text.GetAbsolutePosition().X >= View.GetAbsolutePosition().X - 1 &&
					                   TextEnd.X <= ViewEnd.X + 1 && TextEnd.Y <= ViewEnd.Y + 1 &&
					                   TextEnd.Y > View.GetAbsolutePosition().Y);
				}
			}
			else
				Test->AddError(TEXT("Cooked growth guidance text is missing"));
			Capture(TEXT("Growth.png"));
			break;
		case 68:
		{
			UI->SetCitySummaryVisible(false);
			FString CityPath, Error;
			if (FParse::Value(FCommandLine::Get(), TEXT("CityUiCity="), CityPath))
				Test->TestTrue(TEXT("Chart QA loads a copied city without writing to it"),
				               Sim->LoadCityFromPath(CityPath, Error, false));
			Sim->SetSimulationPaused(false);
			Sim->SetSimulationSpeed(5);
			while (Sim->GetEconomySnapshot().FixedStepId < 80)
				Sim->Tick(.12f);
			Sim->SetSimulationPaused(true);
			UI->ShowReports(false);
			break;
		}
		case 69:
		{
			const auto* Chart = Cast<UCityDesktopChart>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("ReportChart")));
			Test->TestTrue(TEXT("Settled monthly observations render a real chart"),
			               Chart && Chart->IsVisible() && Chart->GetReport().Bars.Num() >= 3);
			if (Chart)
			{
				Test->TestEqual(TEXT("Cooked desktop and VR expose the identical selected report"),
				                Chart->GetReport().Selected, UI->SharedReportChart.Selected);
				Test->TestTrue(TEXT("Cooked chart uses actual settled values"),
				               Chart->GetReport().Values == UI->SharedReportChart.Values);
				BeforeReportPeriod = Chart->GetReport().Periods[Chart->GetReport().Selected];
			}
			CheckMenuLabels();
			Capture(TEXT("ReportsMonthly.png"));
			break;
		}
		case 70:
			Click(ECityPaletteAction::ReportMetricNext);
			Click(ECityPaletteAction::ReportPeriodPrevious);
			break;
		case 71:
			Test->TestEqual(TEXT("Previous period moves the shared selected observation"),
			                UI->SharedReportChart.Periods[UI->SharedReportChart.Selected], BeforeReportPeriod - 1);
			Capture(TEXT("ReportsSelected.png"));
			break;
		case 72:
			Click(ECityPaletteAction::ReportAnnual);
			break;
		case 73:
			Test->TestTrue(TEXT("Annual observations render with year labels"),
			               UI->SharedReportChart.FirstPeriod.ToString().StartsWith(TEXT("Years")));
			Capture(TEXT("ReportsAnnual.png"));
			break;
		case 74:
			Control(TEXT("MenuClose"));
			UI->SetCitySummaryVisible(true);
			if (auto* Demand = UI->DesktopHud->WidgetTree->FindWidget(TEXT("DemandChart")))
				if (auto* Content = Demand->GetParent())
					if (auto* Scroll = Cast<UScrollBox>(Content->GetParent()))
						Scroll->ScrollToStart();
			break;
		case 75:
		{
			const auto* Demand = Cast<UCityDesktopChart>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("DemandChart")));
			const auto* Capacity =
			    Cast<UCityDesktopChart>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("CapacityChart")));
			const auto* Density = Cast<UCityDesktopChart>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("DensityChart")));
			Test->TestTrue(TEXT("City Info contains demand, operational capacity and density charts"),
			               Demand && Demand->IsVisible() && Demand->GetRows().Num() == 3 && Capacity &&
			                   Capacity->GetRows().Num() == 5 && Density && Density->GetRows().Num() == 9);
			Capture(TEXT("Demand.png"));
			break;
		}
		case 76:
		{
			auto* Density = UI->DesktopHud->WidgetTree->FindWidget(TEXT("DensityChart"));
			if (Density)
				if (auto* Content = Density->GetParent())
					if (auto* Scroll = Cast<UScrollBox>(Content->GetParent()))
						Scroll->ScrollWidgetIntoView(Density, false, EDescendantScrollDestination::TopOrLeft);
			break;
		}
		case 77:
			Capture(TEXT("Density.png"));
			break;
		case 100:
			Control(TEXT("Toolbar2"));
			break;
		case 101:
		{
			CheckToolLayout();
			CheckMenuLabels();
			const auto Data = UI->GetUiSnapshot();
			Test->TestEqual(TEXT("Desktop zoning has exactly three choices, no Back"), Data.MenuEntries.Num(), 3);
			Test->TestFalse(TEXT("Unused fourth zoning button is collapsed"), PanelVisible(TEXT("Menu3")));
			UI->bDesktopUi = false;
			Test->TestTrue(TEXT("VR zoning retains its shared Back entry"),
			               UI->GetUiSnapshot().MenuEntries.ContainsByPredicate(
			                   [](const auto& Entry)
			                   { return Entry.ActionId == static_cast<int32>(ECityPaletteAction::Back); }));
			UI->bDesktopUi = true;
			Capture(TEXT("PaletteZone.png"));
			break;
		}
		case 102:
			Click(ECityPaletteAction::SelectResidentialZone);
			break;
		case 103:
			Test->TestTrue(TEXT("Colored residential choice uses existing persistent paint tool"),
			               UI->IsDesktopZoningPaletteActive() &&
			                   UI->Board->GetSelectedZone() == ECityZoneType::Residential);
			CheckToolLayout();
			Capture(TEXT("PaletteResidential.png"));
			break;
		case 104:
			Click(ECityPaletteAction::SelectCommercialZone);
			break;
		case 105:
			Test->TestEqual(TEXT("Colored commercial control selects its real zone"), UI->Board->GetSelectedZone(),
			                ECityZoneType::Commercial);
			CheckToolLayout();
			Capture(TEXT("PaletteCommercial.png"));
			break;
		case 106:
			Click(ECityPaletteAction::SelectIndustrialZone);
			break;
		case 107:
		{
			Test->TestEqual(TEXT("Colored industrial control selects its real zone"), UI->Board->GetSelectedZone(),
			                ECityZoneType::Industrial);
			CheckToolLayout();
			const auto* Button = Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Menu2")));
			Test->TestTrue(TEXT("Selected control has a visible outline as well as fill"),
			               Button && Button->IsSelected() && Button->GetStyle().Normal.OutlineSettings.Width >= 2);
			Capture(TEXT("PaletteIndustrial.png"));
			break;
		}
		case 108:
			Control(TEXT("Toolbar2"));
			break;
		case 109:
			Test->TestFalse(TEXT("Repeated zoning dock click dismisses its palette"), UI->bPaletteOpen);
			Control(TEXT("Toolbar2"));
			break;
		case 110:
			UI->ExecuteDesktopUiAction(-7);
			break;
		case 111:
			Test->TestFalse(TEXT("Esc dismisses zoning without a Back button"), UI->bPaletteOpen);
			Control(TEXT("Toolbar2"));
			break;
		case 112:
			Control(TEXT("Toolbar1"));
			break;
		case 113:
			Test->TestEqual(TEXT("Another dock tool replaces zoning with Build"), UI->PalettePage,
			                ECityPalettePage::Build);
			Test->TestFalse(TEXT("Dock-level Build has no redundant Back"),
			                UI->GetUiSnapshot().MenuEntries.ContainsByPredicate(
			                    [](const auto& Entry)
			                    { return Entry.ActionId == static_cast<int32>(ECityPaletteAction::Back); }));
			CheckMenuLabels();
			CheckToolLayout();
			Capture(TEXT("PaletteBuild.png"));
			break;
		case 114:
			Click(ECityPaletteAction::OpenPowerFacilities);
			break;
		case 115:
			Test->TestTrue(TEXT("Nested power category retains useful parent Back"),
			               UI->GetUiSnapshot().MenuEntries.ContainsByPredicate(
			                   [](const auto& Entry)
			                   { return Entry.ActionId == static_cast<int32>(ECityPaletteAction::Back); }));
			CheckMenuLabels();
			Capture(TEXT("PalettePower.png"));
			break;
		case 116:
			Click(ECityPaletteAction::Back);
			break;
		case 117:
			Test->TestEqual(TEXT("Nested Back returns to the real Build category"), UI->PalettePage,
			                ECityPalettePage::Build);
			Control(TEXT("Toolbar0"));
			break;
		case 118:
			Test->TestTrue(TEXT("Inspect dock choice dismisses the workspace"),
			               !UI->bPaletteOpen && UI->Board->GetToolMode() == ECityToolMode::Inspect);
			CheckIdleInspect();
			Control(TEXT("Toolbar2"));
			break;
		case 119:
			Click(ECityPaletteAction::SelectResidentialZone);
			break;
		case 120:
			Control(TEXT("Cancel"));
			break;
		case 121:
			Test->TestTrue(TEXT("First Cancel dismisses idle zoning controls, retaining its tool"),
			               !UI->bPaletteOpen && UI->Board->GetToolMode() == ECityToolMode::Zone);
			Control(TEXT("Cancel"));
			break;
		case 122:
			Test->TestEqual(TEXT("Second Cancel button returns to Inspect"), UI->Board->GetToolMode(),
			                ECityToolMode::Inspect);
			CheckIdleInspect();
			Control(TEXT("Toolbar1"));
			break;
		case 123:
			Control(TEXT("Cancel"));
			break;
		case 124:
			Test->TestTrue(TEXT("Cancel exits Build with Inspect as the fallback"),
			               !UI->bPaletteOpen && UI->Board->GetToolMode() == ECityToolMode::Inspect);
			CheckIdleInspect();
			Capture(TEXT("PaletteInspect.png"));
			break;
		case 125:
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE PaletteOnly=1 SyntheticButtons=1 PhysicalInputPending=1"));
			return true;
		case 78:
		{
			UI->ExecuteDesktopUiAction(-4);
			Test->TestTrue(TEXT("Desktop shared save command succeeds"), Sim->SaveCity());
			UI->SetCitySummaryVisible(false);
			UI->Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::SelectInspect));
			const auto* Definition = Sim->GetBuildingCatalog()->FindDefinition(TEXT("WaterTower"));
			Test->TestNotNull(TEXT("Preview fixture uses the real water-tower definition"), Definition);
			if (Definition)
			{
				auto& Model = Sim->GetPlacementModel();
				for (int32 Y = 0; Y < Model.GetGrid().GetDimensions().GetHeight() && !PreviewEntity; ++Y)
					for (int32 X = 0; X < Model.GetGrid().GetDimensions().GetWidth() && !PreviewEntity; ++X)
					{
						auto Preview = Model.BeginPreview(*Definition, {X, Y});
						const auto Result = Model.CommitPlacement(Preview, *Definition);
						if (Result.IsSuccess())
							PreviewEntity = Result.EntityId;
					}
			}
			Test->TestTrue(TEXT("A real water tower is placed in the isolated QA city"), PreviewEntity != 0);
			break;
		}
		case 79:
			UI->InspectBuilding(static_cast<int64>(PreviewEntity));
			break;
		case 80:
		{
			auto* Preview =
			    Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			FCityDesktopModelSource Source;
			Test->TestTrue(TEXT("Inspector resolves the current rendered presentation"),
			               UI->GetDesktopModelSource(Source));
			Test->TestTrue(TEXT("Inspector is visible independently of City Info"),
			               PanelVisible(TEXT("InspectorPanel")) && !PanelVisible(TEXT("InfoPanel")));
			Test->TestTrue(TEXT("Preview uses the placed mesh instead of a baked thumbnail"),
			               Preview && Preview->HasModel() && Preview->GetPreviewEntity() == PreviewEntity &&
			                   Preview->GetPreviewMesh() == Source.Mesh);
			if (Preview && Preview->GetPreviewTarget())
			{
				TArray<FColor> Pixels;
				auto* Resource = Preview->GetPreviewTarget()->GameThread_GetRenderTargetResource();
				Test->TestTrue(TEXT("Model render target can be read back"), Resource && Resource->ReadPixels(Pixels));
				TSet<FColor> Colors;
				Colors.Append(Pixels);
				Test->TestTrue(TEXT("Live model renders visible nonuniform pixels"), Colors.Num() > 32);
			}
			CheckPreviewPixels();
			Capture(TEXT("InspectorLive.png"));
			break;
		}
		case 81:
			Control(TEXT("InspectionSection0"));
			break;
		case 82:
			Test->TestTrue(TEXT("Inspector details are available on demand"), PanelVisible(TEXT("InspectionSection0Body")));
			CheckPreviewPixels();
			Capture(TEXT("InspectorDetails.png"));
			break;
		case 83:
			UI->SetCitySummaryVisible(true);
			break;
		case 84:
		{
			const auto* Inspector = UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectorPanel"));
			const auto* City = UI->DesktopHud->WidgetTree->FindWidget(TEXT("InfoPanel"));
			Test->TestTrue(
			    TEXT("City and building cards do not overlap"),
			    Inspector && City &&
			        Inspector->GetCachedGeometry().LocalToAbsolute(Inspector->GetCachedGeometry().GetLocalSize()).Y <=
			            City->GetCachedGeometry().GetAbsolutePosition().Y);
			CheckPreviewPixels();
			Capture(TEXT("InspectorAndCity.png"));
			break;
		}
		case 85:
			Control(TEXT("InfoClose"));
			break;
		case 86:
			Test->TestTrue(TEXT("Closing city summary preserves inspector"),
			               PanelVisible(TEXT("InspectorPanel")) && !PanelVisible(TEXT("InfoPanel")));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			break;
		case 87:
		{
			const auto* Preview =
			    Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			Test->TestTrue(TEXT("Another workspace releases preview resources"),
			               Preview && !Preview->HasModel() && !Preview->GetPreviewTarget());
			Control(TEXT("MenuClose"));
			UI->InspectBuilding(static_cast<int64>(PreviewEntity));
			break;
		}
		case 88:
		{
			const auto* Preview =
			    Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			Test->TestTrue(TEXT("Reselecting restores the preview"), Preview && Preview->HasModel());
			Test->TestTrue(TEXT("QA demolition succeeds"),
			               Sim->GetPlacementModel().DemolishEntity(PreviewEntity).IsSuccess());
			break;
		}
		case 300:
			Test->TestEqual(TEXT("Simulation pause does not stop music"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 1);
			MusicOffset = UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset();
			MusicWall = FPlatformTime::Seconds();
			Sim->SetSimulationSpeed(5);
			Sim->SetSimulationPaused(false);
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Audio);
			break;
		case 301:
			Test->TestTrue(
			    TEXT("5x city speed leaves music on wall time"),
			    FMath::Abs(
			        (UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset() - MusicOffset) -
			        (FPlatformTime::Seconds() - MusicWall)) < .25);
			CheckMenuLabels();
			Capture(TEXT("MusicControls.png"));
			break;
		case 302:
			Click(ECityPaletteAction::MusicMute);
			break;
		case 303:
			Test->TestEqual(TEXT("Music mute cancels actual players"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 0);
			MusicOffset = UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset();
			UI->SetPaletteOpen(true);
			UI->SetPalettePage(ECityPalettePage::Audio);
			break;
		case 304:
			Test->TestEqual(TEXT("Mute holds position while the game runs"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset(),
			                MusicOffset);
			Capture(TEXT("MusicMuted.png"));
			break;
		case 305:
			Click(ECityPaletteAction::Music50);
			break;
		case 306:
		{
			auto* Music = UI->Board->FindComponentByClass<UCityMusicComponent>();
			Test->TestEqual(TEXT("Unmute resumes one player"), Music->GetPlayingVoiceCount(), 1);
			Test->TestTrue(TEXT("Unmute resumes rather than restarting"),
			               Music->GetPlayback().GetOffset() >= MusicOffset);
			Test->TestEqual(TEXT("Music control changes its saved preference"),
			                GetDefault<UCityAudioSettings>()->GetMusicVolume(), .5f);
			Test->TestEqual(TEXT("Music choice preserves FX"), GetDefault<UCityAudioSettings>()->GetAlarmVolume(),
			                .75f);
			Test->TestEqual(TEXT("Music choice preserves ambience"),
			                GetDefault<UCityAudioSettings>()->GetAmbienceVolume(), .5f);
			Music->Suspend();
			const double FirstOverlap = Music->Sources[0]->Duration - FCityMusicPlayback::CrossfadeSeconds * .5;
			Music->Playback.Advance(FirstOverlap - Music->Playback.GetOffset(), false);
			Music->Update(false, 1);
			break;
		}
		case 307:
			Test->TestEqual(TEXT("Cooked playlist overlaps exactly two native players"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 2);
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
			MusicOffset = UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset();
			break;
		case 308:
			Test->TestEqual(TEXT("App background immediately cancels music"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 0);
			Test->TestEqual(TEXT("Background holds overlapping track positions"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetOffset(),
			                MusicOffset);
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
			break;
		case 309:
			Test->TestTrue(TEXT("Foreground resumes the current overlap"),
			               UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount() > 0);
			UI->bDesktopFocused = false;
			Next = FPlatformTime::Seconds() + 3;
			return false;
		case 310:
		{
			auto* Music = UI->Board->FindComponentByClass<UCityMusicComponent>();
			const double BeforeLoad = Music->GetPlayback().GetOffset();
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/MusicCity.json");
			FString Error;
			Test->TestTrue(TEXT("Current city saves in isolated music fixture"), Sim->SaveCityToPath(Path, Error));
			Test->TestTrue(TEXT("Current city reload succeeds"), Sim->LoadCityFromPath(Path, Error));
			Test->TestEqual(TEXT("Reload leaves music position intact"), Music->GetPlayback().GetOffset(), BeforeLoad);
			Test->TestEqual(TEXT("Completed crossfade releases the outgoing player"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 1);
			UI->ExecuteDesktopUiAction(-1);
			break;
		}
		case 311:
			Test->TestTrue(TEXT("Return to menu keeps one music player"),
			               UI->bEntrySceneActive &&
			                   UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount() == 1);
			Click(ECityPaletteAction::OpenNewCitySetup);
			break;
		case 312:
			Click(ECityPaletteAction::ReviewCreate);
			break;
		case 313:
			Click(ECityPaletteAction::ConfirmCreate);
			break;
		case 314:
			Test->TestEqual(TEXT("City switch does not duplicate or restart music"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayback().GetTrack(), 1);
			Test->TestEqual(TEXT("City switch retains one music player"),
			                UI->Board->FindComponentByClass<UCityMusicComponent>()->GetPlayingVoiceCount(), 1);
			GetMutableDefault<UCityAudioSettings>()->SetMusicVolume(.25f);
			Sim->SetSimulationSpeed(1);
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE MusicOnly=1 SyntheticButtons=1 PhysicalListeningPending=1"));
			return true;
		case 200:
			CheckMenuLabels();
			CheckMenuBody();
			Test->TestEqual(TEXT("Cooked size button selects80"), UI->NewCityTerrain.Dimensions.GetWidth(), 80);
			Test->TestTrue(TEXT("Cooked setup explains Desktop restriction"),
			               UI->GetEntryMenuHelp().Contains(TEXT("Desktop only")));
			Capture(TEXT("Map80Setup.png"));
			break;
		case 201:
			Click(ECityPaletteAction::ReviewCreate);
			Phase = 212;
			break;
		case 212:
			CheckMenuLabels();
			CheckMenuBody();
			Capture(TEXT("Map80Review.png"));
			Phase = 3;
			break;
		case 202:
		{
			Test->TestEqual(TEXT("Cooked create commits80"), Sim->GetTerrain().GetDimensions().GetWidth(), 80);
			FString Error;
			Test->TestTrue(TEXT("Cooked80 saves"), Sim->SaveCity());
			SavedLargeId = Sim->GetCityIdentity().Id;
			FString CityPath;
			if (!FParse::Value(FCommandLine::Get(), TEXT("CityUiCity="), CityPath))
				Test->AddError(TEXT("Dimensions smoke requires the representative populated80 fixture"));
			else if (!Test->TestTrue(TEXT("Cooked populated80 loads"), Sim->LoadCityFromPath(CityPath, Error, false)))
				Test->AddError(Error);
			UI->ReturnToCity();
			Sim->SetSimulationSpeed(5);
			Sim->SetSimulationPaused(false);
			StartPopulatedStep = Sim->GetEconomySnapshot().FixedStepId;
			break;
		}
		case 203:
			Test->TestTrue(TEXT("Cooked populated80 has actual residents"), Sim->GetEconomySnapshot().Residents > 0);
			Test->TestTrue(TEXT("Cooked populated80 has actual jobs"), Sim->GetEconomySnapshot().Jobs > 0);
			Capture(TEXT("Populated80.png"));
			Next = FPlatformTime::Seconds() + 12.;
			UI->bDesktopFocused = false;
			return false;
		case 204:
			Test->TestTrue(TEXT("Populated80 advances while rendered"),
			               Sim->GetEconomySnapshot().FixedStepId > StartPopulatedStep);
			Sim->SetSimulationPaused(true);
			UI->ShowMainMenu();
			break;
		case 205:
			Click(ECityPaletteAction::ReviewLoad);
			UI->ReviewedCityIndex = UI->ReviewedCitySlots.IndexOfByPredicate(
			    [this](const auto& Slot) { return Slot.Identity.Id == SavedLargeId; });
			UI->RefreshPaletteText();
			break;
		case 206:
			CheckMenuLabels();
			CheckMenuBody();
			Capture(TEXT("Map80Library.png"));
			break;
		case 207:
			Click(ECityPaletteAction::ConfirmLoad);
			Test->TestEqual(TEXT("Cooked library returns80 city"), Sim->GetCityIdentity().Id, SavedLargeId);
			Test->TestEqual(TEXT("Cooked library keeps80 dimensions"), Sim->GetTerrain().GetDimensions().GetWidth(),
			                80);
			UI->ShowNewCitySetup();
			break;
		case 208:
			Test->TestEqual(TEXT("Cooked next proposal defaults64"), UI->NewCityTerrain.Dimensions.GetWidth(), 64);
			Click(ECityPaletteAction::ReviewCreate);
			break;
		case 209:
			Click(ECityPaletteAction::ConfirmCreate);
			break;
		case 210:
			Test->TestEqual(TEXT("Cooked switch creates64"), Sim->GetTerrain().GetDimensions().GetWidth(), 64);
			Capture(TEXT("Switched64.png"));
			break;
		case 211:
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE DimensionsOnly=1 SyntheticButtons=1 PhysicalInputPending=1"));
			return true;
		case 400:
		{
			CheckMenuLabels();
			CheckMenuBody();
			const auto* Preview =
			    Cast<UCityDesktopTerrainPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("TerrainPreview")));
			Test->TestTrue(TEXT("Setup renders the exact64 proposal"),
			               Preview && PanelVisible(TEXT("TerrainPanel")) && Preview->GetParameters().IsSet() &&
			                   Preview->GetParameters().GetValue() == UI->NewCityTerrain &&
			                   Preview->GetCells().Num() == 4096);
			if (Preview)
				BeforeTerrainCells = Preview->GetCells();
			Capture(TEXT("NewCitySetup.png"));
			Click(ECityPaletteAction::RerollTerrain);
			break;
		}
		case 401:
		{
			const auto* Preview =
			    Cast<UCityDesktopTerrainPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("TerrainPreview")));
			Test->TestTrue(TEXT("Reroll updates preview cells and exact seed"),
			               Preview && Preview->GetCells() != BeforeTerrainCells &&
			                   Preview->GetParameters().GetValue() == UI->NewCityTerrain);
			Click(ECityPaletteAction::CycleMapSize);
			break;
		}
		case 402:
		{
			const auto* Preview =
			    Cast<UCityDesktopTerrainPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("TerrainPreview")));
			Test->TestTrue(TEXT("Size choice updates to80 preview"),
			               Preview && Preview->GetCells().Num() == 6400 &&
			                   Preview->GetParameters().GetValue() == UI->NewCityTerrain);
			const auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(TEXT("TerrainPanel"));
			const auto& Geometry = Panel->GetCachedGeometry();
			const auto& Hud = UI->DesktopHud->GetCachedGeometry();
			const auto End = Hud.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
			Test->TestTrue(TEXT("Terrain panel remains inside laptop viewport"),
			               End.X <= Hud.GetLocalSize().X - 12 && End.Y <= Hud.GetLocalSize().Y - 12);
			Capture(TEXT("NewCity80.png"));
			break;
		}
		case 403:
			Click(ECityPaletteAction::ReviewCreate);
			break;
		case 404:
		{
			CheckMenuLabels();
			CheckMenuBody();
			const auto* Preview =
			    Cast<UCityDesktopTerrainPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("TerrainPreview")));
			Test->TestTrue(TEXT("Confirmation retains the selected preview"),
			               Preview && PanelVisible(TEXT("TerrainPanel")) &&
			                   Preview->GetParameters().GetValue() == UI->NewCityTerrain);
			Capture(TEXT("NewCityReview.png"));
			break;
		}
		case 405:
			Click(ECityPaletteAction::Back);
			break;
		case 406:
			Test->TestEqual(TEXT("Cancel returns to the same80 proposal"), UI->PalettePage,
			                ECityPalettePage::NewCitySetup);
			Test->TestEqual(TEXT("Cancel retains map size"), UI->NewCityTerrain.Dimensions.GetWidth(), 80);
			Click(ECityPaletteAction::ReviewCreate);
			break;
		case 407:
			Click(ECityPaletteAction::ConfirmCreate);
			break;
		case 408:
			Test->TestTrue(TEXT("Created terrain matches the previewed proposal"),
			               Sim->GetTerrain().GetParameters() == UI->NewCityTerrain);
			Test->TestFalse(TEXT("Terrain preview hides on entering the city"), PanelVisible(TEXT("TerrainPanel")));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 409:
			Click(ECityPaletteAction::OpenTaxes);
			break;
		case 410:
		{
			CheckMenuLabels();
			const auto Data = UI->GetUiSnapshot();
			Test->TestTrue(TEXT("Taxes render three dedicated rows"),
			               PanelVisible(TEXT("TaxRows")) && Data.bTaxMenu && Data.TaxRates.Num() == 3);
			Test->TestEqual(TEXT("Apply and Cancel remain together in the tax footer"), Data.MenuEntries.Num(), 2);
			if (Data.TaxRates.Num() != 3)
				return true;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const auto* Rate = Cast<UTextBlock>(
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("TaxRate%d"), Index))));
				const auto* Down =
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("TaxDown%d"), Index)));
				const auto* Up = UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("TaxUp%d"), Index)));
				Test->TestTrue(TEXT("Each category's exact pending rate is presented"),
				               Rate && Rate->GetText().ToString().Contains(Data.TaxRates[Index].Value.ToString()));
				Test->TestTrue(
				    TEXT("Tax label decrease increase share a non-overlapping row"),
				    Rate && Down && Up &&
				        Rate->GetCachedGeometry().GetAbsolutePosition().X +
				                Rate->GetCachedGeometry().GetLocalSize().X * Rate->GetCachedGeometry().Scale <=
				            Down->GetCachedGeometry().GetAbsolutePosition().X + 1 &&
				        FMath::IsNearlyEqual(Down->GetCachedGeometry().GetAbsolutePosition().Y,
				                             Up->GetCachedGeometry().GetAbsolutePosition().Y, 1.0) &&
				        Down->GetCachedGeometry().LocalToAbsolute(Down->GetCachedGeometry().GetLocalSize()).X <=
				            Up->GetCachedGeometry().GetAbsolutePosition().X);
			}
			BeforeTax = Sim->GetEconomySnapshot().ResidentialTaxBasisPoints;
			Control(TEXT("TaxUp0"));
			break;
		}
		case 411:
			Test->TestEqual(TEXT("Tax increase stays pending"), Sim->GetEconomySnapshot().ResidentialTaxBasisPoints,
			                BeforeTax);
			Test->TestEqual(TEXT("Only residential pending rate increases"),
			                UI->ResidentialTaxControl.GetPendingValue(), BeforeTax + 100);
			Capture(TEXT("Taxes.png"));
			break;
		case 412:
			Click(ECityPaletteAction::CancelTaxes);
			break;
		case 413:
			Test->TestEqual(TEXT("Cancel discards pending rate"), Sim->GetEconomySnapshot().ResidentialTaxBasisPoints,
			                BeforeTax);
			Test->TestFalse(TEXT("Cancel dismisses tax rows"), PanelVisible(TEXT("TaxRows")));
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 414:
			Click(ECityPaletteAction::OpenTaxes);
			break;
		case 415:
			Control(TEXT("TaxUp0"));
			break;
		case 416:
			Click(ECityPaletteAction::ApplyTaxes);
			break;
		case 417:
			Test->TestEqual(TEXT("Apply commits the pending rate"), Sim->GetEconomySnapshot().ResidentialTaxBasisPoints,
			                BeforeTax + 100);
			UI->ShowMainMenu();
			break;
		case 418:
			Click(ECityPaletteAction::ReviewLoad);
			break;
		case 419:
			Test->TestTrue(TEXT("Saved-city review also shows terrain"), PanelVisible(TEXT("TerrainPanel")));
			Capture(TEXT("LoadPreview.png"));
			break;
		case 420:
			UE_LOG(
			    LogTemp, Display,
			    TEXT("CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE MenusOnly=1 SyntheticButtons=1 PhysicalInputPending=1"));
			return true;
		case 89:
		{
			const auto* Preview =
			    Cast<UCityDesktopModelPreview>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("BuildingPreview")));
			Test->TestTrue(TEXT("Demolition clears inspector and render resources"),
			               Preview && !Preview->HasModel() && !Preview->GetPreviewTarget() &&
			                   !PanelVisible(TEXT("InspectorPanel")));
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE SyntheticButtons=1 PhysicalInputPending=1 LivePreview=1"));
			return true;
		}
		}
		// Restore the sampled state, not an assumed offscreen false. Either edge
		// now dispatches a lifecycle event; inventing an edge resets entry pages.
		UI->bDesktopFocused = bObservedDesktopFocus;
		Next = FPlatformTime::Seconds() + 1.0;
		return false;
	}

private:
	FAutomationTestBase* Test;
	int32 Phase = -3, Attempts = 0;
	bool BeforeVSync = false;
	TArray<FVector2D> ToolPositions, ToolSizes;
	TArray<FLinearColor> BeforeTerrainCells;
	int32 BeforeTax = 0;
	int64 BeforeReportPeriod = 0;
	uint64 PreviewEntity = 0;
	FString SavedLargeId;
	double MusicOffset = 0;
	double MusicWall = 0;
	int64 StartPopulatedStep = 0;
	double Next = FPlatformTime::Seconds() + 3.0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopPackagedSmokeTest, "CitySim.Desktop.PackagedSmoke",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopPackagedSmokeTest::RunTest(const FString& Parameters)
{
	FString UserDirectory;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory) ||
	    !UserDirectory.Contains(TEXT("DesktopSmoke-Automation")))
	{
		AddError(TEXT("Packaged smoke requires -CityDesktop and an isolated -UserDir containing DesktopSmoke-Automation"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopPackagedSmokeCommand(this));
	return true;
}
#endif
