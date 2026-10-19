#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Interaction/CityVRToolController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UnrealClient.h"

class FCityDesktopLoansCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopLoansCommand(FAutomationTestBase* InTest) : Test(InTest)
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
			Test->AddError(TEXT("Loan HUD missing"));
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
					const FString Name = FString::Printf(TEXT("%s%d"), Data.bNewspaper ? TEXT("NewsAction") : TEXT("Menu"), I);
					auto* Button =
					    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(FName(Name)));
					if (Button && Button->GetIsEnabled())
						Button->OnClicked.Broadcast();
					else
						Test->AddError(TEXT("Loan button unavailable"));
					return;
				}
			Test->AddError(TEXT("Loan action missing"));
		};
		auto Capture = [](const TCHAR* Name) {
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/CityLoans");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / Name, true, false);
		};
		FString Error;
		switch (Phase)
		{
		case 0:
			UI->ReturnToCity();
			Sim->ResetCity(false);
			Sim->StartScenario();
			Sim->SetSimulationSpeed(1);
			Sim->SetSimulationPaused(true);
			Cash = Sim->GetPlacementModel().GetFunds();
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity));
			break;
		case 1:
			Click(ECityPaletteAction::OpenLoans);
			break;
		case 2:
			Click(ECityPaletteAction::ReviewLoanLarge);
			break;
		case 3:
			Test->TestEqual(TEXT("Term preview does not borrow"), Sim->GetPlacementModel().GetFunds(), Cash);
			Test->TestTrue(TEXT("Interest terms rendered"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("0.5% / month")));
			Capture(TEXT("Terms.png"));
			break;
		case 4:
			Click(ECityPaletteAction::Back);
			break;
		case 5:
			Click(ECityPaletteAction::ReviewLoanSmall);
			break;
		case 6:
			Click(ECityPaletteAction::ConfirmLoan);
			break;
		case 7:
			Test->TestEqual(TEXT("Borrowing actual button adds cash"), Sim->GetPlacementModel().GetFunds(),
			                Cash + 5000);
			Capture(TEXT("Borrowed.png"));
			break;
		case 8:
			Sim->SetSimulationPaused(false);
			for (int32 I = 0; I < Sim->GetEconomyTuning().FixedStepsPerMonth; ++I)
				Sim->Tick(static_cast<float>(Sim->GetEconomyTuning().FixedStepSeconds));
			Sim->SetSimulationPaused(true);
			Test->TestEqual(TEXT("Actual monthly charge"), Sim->GetEconomySnapshot().Loan.Principal,
			                int64(4791));
			Test->TestEqual(TEXT("Monthly source flow"),
			                Sim->GetReportHistory().Months.Last().Get(ECityReportMetric::LoanBorrowed),
			                int64(5000));
			{
				const FString Save = FPaths::ProjectSavedDir() / TEXT("Automation/Technology-Loan-City.json");
				Test->TestTrue(TEXT("Live city debt save"), Sim->SaveCityToPath(Save, Error));
				Test->TestTrue(TEXT("Live city debt reload"), Sim->LoadCityFromPath(Save, Error, false));
			}
			UI->RefreshReports();
			break;
		case 9:
			Capture(TEXT("SettledDebt.png"));
			break;
		case 10:
			Click(ECityPaletteAction::ReviewLoanPayoff);
			break;
		case 11:
			Test->TestTrue(TEXT("Current payoff shown"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Full payoff now $4791")));
			Capture(TEXT("Payoff.png"));
			break;
		case 12:
			Click(ECityPaletteAction::ConfirmLoan);
			break;
		case 13:
			Test->TestEqual(TEXT("Payoff clears loan"), Sim->GetEconomySnapshot().Loan.Payoff(), int64(0));
			Click(ECityPaletteAction::Back);
			break;
		case 14:
			Click(ECityPaletteAction::OpenQuarterlyNews);
			break;
		case 15:
			Test->TestTrue(
			    TEXT("Closing-month debt article"),
			    UI->SharedNewspaper.Articles.Last().Body.ToString().Contains(TEXT("borrowed $5000")));
			Capture(TEXT("DebtNews.png"));
			break;
		case 16:
			UE_LOG(
			    LogTemp, Display,
			    TEXT("CITYSIM_DESKTOP_LOANS_COMPLETE Captures=5 SyntheticButtons=1 PhysicalInputPending=1"));
			return true;
		}
		++Phase;
		Next = FPlatformTime::Seconds() + .8;
		return false;
	}

  private:
	FAutomationTestBase* Test;
	int32 Phase = 0, Attempts = 0;
	int64 Cash = 0;
	double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopLoansTest, "CitySim.Desktop.CityLoans",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopLoansTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("Technology")))
	{
		AddError(TEXT("Loan smoke requires isolated Technology desktop UserDir"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopLoansCommand(this));
	return true;
}
#endif
