#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CityTechnologyFixture.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityDesktopHud.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

class FCityDesktopTechnologyCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopTechnologyCommand(FAutomationTestBase* In) : Test(In)
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
			Test->AddError(TEXT("Technology HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		auto Click = [&](const ECityPaletteAction Action) {
			const auto Data = UI->GetUiSnapshot();
			for (int32 I = 0; I < Data.MenuEntries.Num(); ++I)
				if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
				{
					auto* Button = Cast<UCityDesktopActionButton>(
					    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Menu%d"), I))));
					if (Button && Button->GetIsEnabled())
						Button->OnClicked.Broadcast();
					else
						Test->AddError(TEXT("Technology button unavailable"));
					return;
				}
			Test->AddError(FString::Printf(TEXT("Technology menu action missing: phase=%d action=%d page=%d open=%d"),
			                              Phase, static_cast<int32>(Action), static_cast<int32>(UI->PalettePage),
			                              UI->bPaletteOpen));
		};
		auto Capture = [&](const TCHAR* Name) {
			const auto* Body = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuBody"));
			const auto* Choices = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuButtons"));
			const auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuPanel"));
			if (UI->PalettePage == ECityPalettePage::TechnologyReview && (!Body || !Choices || !Panel))
				Test->AddError(TEXT("Technology layout widgets missing"));
			else if (UI->PalettePage == ECityPalettePage::TechnologyReview)
			{
				const double Bottom =
				    Body->GetCachedGeometry().LocalToAbsolute(Body->GetCachedGeometry().GetLocalSize()).Y;
				Test->TestTrue(TEXT("Facility requirements above controls"),
				               Bottom <= Choices->GetCachedGeometry().GetAbsolutePosition().Y + 1);
				Test->TestTrue(TEXT("Requirements fit menu panel"),
				               Bottom <= Panel->GetCachedGeometry()
				                                 .LocalToAbsolute(Panel->GetCachedGeometry().GetLocalSize())
				                                 .Y -
				                             4);
			}
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/Technology");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / Name, true, false);
		};
		FString Error;
		switch (Phase)
		{
		case 0:
			UI->ReturnToCity();
			FixtureState = MakeUnique<FCityTechnologyFixture::FScopedState>(Sim, UI->Board);
			if (!FCityTechnologyFixture::Seed(Sim, UI->Board))
			{
				Test->AddError(TEXT("Technology populated city fixture failed"));
				return true;
			}
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			break;
		case 1:
			Capture(TEXT("BuildMenu.png"));
			break;
		case 2:
			Click(ECityPaletteAction::OpenTechnologyFacilities);
			break;
		case 3:
			Test->TestFalse(TEXT("Construction uses ordinary drawer"), UI->GetUiSnapshot().bDetailedMenu);
			Test->TestFalse(TEXT("No report-only USE TOOL step"),
			                UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("USE TOOL")));
			for (const auto& Entry : UI->GetUiSnapshot().MenuEntries)
				if (Entry.ActionId == static_cast<int32>(ECityPaletteAction::SelectResearchCampus))
					Test->TestTrue(TEXT("Picker explains actual research lock"),
					               Entry.Help.ToString().Contains(TEXT("LOCKED")) &&
					                   Entry.Help.ToString().Contains(TEXT("NEED Computing center operating")));
			Capture(TEXT("FacilityPicker.png"));
			break;
		case 4:
			Click(ECityPaletteAction::SelectDigitalExchange);
			break;
		case 5:
			Test->TestFalse(TEXT("Facility selection returns directly to board"), UI->bPaletteOpen);
			Test->TestEqual(TEXT("Explicit construction choice armed"), UI->Board->GetSelectedFacilityId(),
			                FName(TEXT("DigitalExchange")));
			Capture(TEXT("ConnectivityPlacement.png"));
			break;
		case 6:
			BeforeReviewFacility = UI->Board->GetSelectedFacilityId();
			BeforeReviewTool = UI->Board->GetToolMode();
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 7:
			Capture(TEXT("CityMenu.png"));
			break;
		case 8:
			Click(ECityPaletteAction::OpenTechnology);
			break;
		case 9:
			Click(ECityPaletteAction::ReviewResearchCampus);
			break;
		case 10:
			Test->TestTrue(TEXT("Locked research explains missing computing"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("NEED Computing center operating")));
			Test->TestEqual(TEXT("Browsing review preserves facility"), UI->Board->GetSelectedFacilityId(),
			                BeforeReviewFacility);
			Test->TestEqual(TEXT("Browsing review preserves active tool"), UI->Board->GetToolMode(), BeforeReviewTool);
			Capture(TEXT("LockedResearch.png"));
			break;
		case 11:
			Click(ECityPaletteAction::ReviewDigitalExchange);
			break;
		case 12:
			Capture(TEXT("ConnectivityReady.png"));
			break;
		case 13:
			if (!FCityTechnologyFixture::Place(Sim, TEXT("DigitalExchange"), 12))
			{
				Test->AddError(TEXT("Actual exchange construction/hiring failed"));
				return true;
			}
			UI->Board->PrepareCityView();
			Click(ECityPaletteAction::ReviewComputingCenter);
			break;
		case 14:
			Capture(TEXT("ComputingReview.png"));
			break;
		case 15:
			if (!FCityTechnologyFixture::Place(Sim, TEXT("ComputingCenter"), 16) ||
			    !FCityTechnologyFixture::Place(Sim, TEXT("ResearchCampus"), 20))
			{
				Test->AddError(TEXT("Actual technology construction/hiring failed"));
				return true;
			}
			{
				const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/TechnologyNavigation-Technology-Desktop.json");
				Test->TestTrue(TEXT("Populated technology saves"), Sim->SaveCityToPath(Path, Error));
				Test->TestTrue(TEXT("Populated technology reloads"), Sim->LoadCityFromPath(Path, Error, false));
			}
			UI->Board->PrepareCityView();
			break;
		case 16:
			Click(ECityPaletteAction::ReviewResearchCampus);
			break;
		case 17:
			Test->TestTrue(TEXT("Actual operating chain visible"), UI->GetUiSnapshot().MenuBody.ToString().Contains(
			                                                           TEXT("exchange 1 | computing 1 | research 1")));
			Capture(TEXT("ResearchOnline.png"));
			break;
		case 18:
			Sim->GetPlacementModel().DemolishEntity(
			    FCityTechnologyFixture::Find(Sim, TEXT("DigitalExchange")));
			UI->Board->PrepareCityView();
			UI->RefreshReports();
			break;
		case 19:
			Test->TestEqual(TEXT("Dependency outage remains authoritative"),
			                Sim->GetServiceSnapshot().OperatingFacilities.FindRef(TEXT("ResearchCampus")), 0);
			Capture(TEXT("DependencyOutage.png"));
			break;
		case 20:
			Click(ECityPaletteAction::Back);
			break;
		case 21:
			Test->TestTrue(TEXT("Review Back returns to City"), UI->PalettePage == ECityPalettePage::City);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
			break;
		case 22:
			// Menu widgets are pooled; allow the HUD to bind Build's action IDs before clicking.
			Test->TestTrue(TEXT("Build drawer opens before choosing technology"),
			               UI->bPaletteOpen && UI->PalettePage == ECityPalettePage::Build);
			Click(ECityPaletteAction::OpenTechnologyFacilities);
			break;
		case 23:
			Test->TestTrue(TEXT("Technology picker opens before Back"),
			               UI->bPaletteOpen && UI->PalettePage == ECityPalettePage::TechnologyFacilities);
			Click(ECityPaletteAction::Back);
			break;
		case 24:
			Test->TestTrue(TEXT("Picker Back returns to Build"), UI->PalettePage == ECityPalettePage::Build);
			UE_LOG(
			    LogTemp, Display,
			    TEXT(
			        "CITYSIM_DESKTOP_TECHNOLOGY_COMPLETE Captures=9 ReviewDoesNotArm=1 DirectSelection=1 DistinctBack=1 ActualSettlement=1 SaveReload=1 SyntheticButtons=1 PhysicalInputPending=1"));
			return true;
		}

		++Phase;
		Next = FPlatformTime::Seconds() + .8;
		return false;
	}

  private:
	FAutomationTestBase* Test;
	TUniquePtr<FCityTechnologyFixture::FScopedState> FixtureState;
	int32 Phase = 0, Attempts = 0;
	FName BeforeReviewFacility;
	ECityToolMode BeforeReviewTool = ECityToolMode::Inspect;
	double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopTechnologyTest, "CitySim.Desktop.TechnologyFoundation",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopTechnologyTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) ||
	    (!UserDir.Contains(TEXT("Technology")) &&
	     !(UserDir.Contains(TEXT("Client-")) && FParse::Param(FCommandLine::Get(), TEXT("CityPackagedAutomation")))))
	{
		AddError(TEXT("Technology smoke requires an isolated desktop automation profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopTechnologyCommand(this));
	return true;
}
#endif
