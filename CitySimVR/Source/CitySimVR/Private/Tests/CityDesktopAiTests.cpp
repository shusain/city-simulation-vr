#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "CityTechnologyFixture.h"
#include "Simulation/CityCommercialAi.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityDesktopHud.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

class FCityDesktopAiCommand : public IAutomationLatentCommand
{
public:
    explicit FCityDesktopAiCommand(FAutomationTestBase* In) : Test(In) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() < Next) return false;
        auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        ACityVRToolController* UI = nullptr;
        if (W) for (TActorIterator<ACityVRToolController> It(W); It; ++It) { UI = *It; break; }
        if (!UI || !UI->DesktopHud || !UI->Board)
        {
            if (++Attempts < 120) { Next = FPlatformTime::Seconds()+.5; return false; }
            Test->AddError(TEXT("AI desktop HUD missing")); return true;
        }
        UI->bDesktopFocused = true;
        UI->LifecycleInputGate.Update(false);
        auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
        const auto Click = [&](ECityPaletteAction Action)
        {
            const auto Data = UI->GetUiSnapshot();
            for (int32 I=0; I<Data.MenuEntries.Num(); ++I)
                if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
                {
                    auto* Button = Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Menu%d"),I))));
                    if (Button && Button->GetIsEnabled()) Button->OnClicked.Broadcast();
                    else Test->AddError(TEXT("AI menu button unavailable"));
                    return;
                }
            Test->AddError(FString::Printf(TEXT("AI action missing phase%d action%d"),Phase,static_cast<int32>(Action)));
        };
        const auto Capture = [&](const TCHAR* Name)
        {
            const auto* Body = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuBody"));
            const auto* Choices = UI->DesktopHud->WidgetTree->FindWidget(TEXT("MenuButtons"));
            if (UI->GetUiSnapshot().bDetailedMenu && Body && Choices)
                Test->TestTrue(TEXT("AI review content fits above controls"), Body->GetCachedGeometry().LocalToAbsolute(Body->GetCachedGeometry().GetLocalSize()).Y <= Choices->GetCachedGeometry().GetAbsolutePosition().Y+1);
            const FString Dir = FPaths::ProjectSavedDir()/TEXT("Automation/CommercialAi");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);
        };
        FString Error;
        switch(Phase)
        {
        case 0:
            UI->ReturnToCity();
            FixtureState = MakeUnique<FCityTechnologyFixture::FScopedState>(Sim, UI->Board);
            if (!FCityTechnologyFixture::SeedAi(Sim,UI->Board,true) || !FCityTechnologyFixture::FundAiScenario(Sim))
            { Test->AddError(TEXT("AI operating-chain fixture failed")); return true; }
            UI->SetObjectivesVisible(false); UI->SetCitySummaryVisible(false);
            UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild)); break;
        case 1: Click(ECityPaletteAction::OpenTechnologyFacilities); break;
        case 2: Capture(TEXT("FacilityPicker.png")); break;
        case 3: Click(ECityPaletteAction::SelectAIInstitute); break;
        case 4:
            Test->TestFalse(TEXT("AI facility selection directly returns to board"),UI->bPaletteOpen);
            Test->TestEqual(TEXT("Explicit AI construction selection"),UI->Board->GetSelectedFacilityId(),FName(TEXT("AIInstitute")));
            BeforeReview = UI->Board->GetSelectedFacilityId();
            UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity)); break;
        case 5: Click(ECityPaletteAction::OpenTechnology); break;
        case 6: Click(ECityPaletteAction::ReviewEnterpriseAICenter); break;
        case 7:
            Test->TestEqual(TEXT("AI review preserves selected tool"),UI->Board->GetSelectedFacilityId(),BeforeReview);
            Capture(TEXT("EnterpriseReview.png")); break;
        case 8: Click(ECityPaletteAction::OpenCommercialAi); break;
        case 9:
            Test->TestFalse(TEXT("Pilot defaults OFF"),Sim->GetPopulationSnapshot().bCommercialAiPilot);
            Capture(TEXT("PilotOff.png")); break;
        case 10: Click(ECityPaletteAction::EnableCommercialAi); break;
        case 11:
            // First settlement buys access; the following fine updates reconcile the new
            // Higher vacancy, then the next day pays its actual staffed role.
            for (int32 Day=0; Day<2; ++Day) FCityTechnologyFixture::Month(Sim);
            UI->RefreshReports(); break;
        case 12:
            Test->TestTrue(TEXT("START command selects optional pilot"),Sim->GetPopulationSnapshot().bCommercialAiPilot);
            Test->TestTrue(TEXT("Real settlement buys at least one contract"),!Sim->GetServiceSnapshot().CommercialAiShops.IsEmpty());
            Test->TestTrue(TEXT("Live paid and staffed retail bonus"),!Sim->GetServiceSnapshot().CommercialAiStaffedShops.IsEmpty());
            Capture(TEXT("PilotPaid.png"));
            Test->TestTrue(TEXT("Current funded city saves"),Sim->SaveCityToPath(FPaths::ProjectSavedDir()/TEXT("Automation/Technology-AiPlaytest.json"),Error));
            break;
        case 13:
            Test->TestTrue(TEXT("Funded playtest city reloads with ordinary authored tuning"),FCityTechnologyFixture::LoadWithAuthoredTuning(Sim,FPaths::ProjectSavedDir()/TEXT("Automation/Technology-AiPlaytest.json"),Error));
            Test->TestTrue(TEXT("Shortage fixture changes real payroll"),FCityTechnologyFixture::FundAiScenario(Sim,true));
            UI->RefreshReports(); break;
        case 14:
            Test->TestTrue(TEXT("Funding shortage is explicit"),UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("payroll")));
            Capture(TEXT("PayrollShortage.png")); break;
        case 15:
            Sim->GetPlacementModel().DemolishEntity(FCityTechnologyFixture::Find(Sim,TEXT("AIInstitute")));
            FCityTechnologyFixture::RefreshAi(Sim); UI->RefreshReports(); break;
        case 16:
            Test->TestTrue(TEXT("Demolition removes actual paid access"),Sim->GetServiceSnapshot().CommercialAiShops.IsEmpty());
            Test->TestTrue(TEXT("Outage is distinct"),UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("Provider offline")));
            Capture(TEXT("ProviderOutage.png")); break;
        case 17: Click(ECityPaletteAction::DisableCommercialAi); break;
        case 18:
            Test->TestFalse(TEXT("PAUSE command clears selection"),Sim->GetPopulationSnapshot().bCommercialAiPilot);
            Capture(TEXT("PilotPaused.png")); break;
        case 19: Click(ECityPaletteAction::Back); break;
        case 20:
            Test->TestTrue(TEXT("Pilot Back returns to City technology review"),UI->PalettePage==ECityPalettePage::TechnologyReview);
            Test->TestTrue(TEXT("Current playtest copy reloads original paid chain"),Sim->LoadCityFromPath(FPaths::ProjectSavedDir()/TEXT("Automation/Technology-AiPlaytest.json"),Error,false));
            UI->ReturnToCity();
            UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenCity)); break;
        case 21: Click(ECityPaletteAction::OpenTechnology); break;
        case 22: Click(ECityPaletteAction::OpenCommercialAi); break;
        case 23: Click(ECityPaletteAction::OverlayCommercialAi); break;
        case 24:
            Test->TestEqual(TEXT("Pilot view uses existing pooled city overlay"),UI->Board->GetDiagnosticOverlay(),ECityDiagnosticOverlay::CommercialAi);
            Test->TestFalse(TEXT("Pilot view leaves report for city inspection"),UI->bPaletteOpen);
            Capture(TEXT("PilotView.png")); break;
        case 25:
            UE_LOG(LogTemp,Display,TEXT("CITYSIM_DESKTOP_AI_COMPLETE Captures=8 DirectSelection=1 Paid=1 PayrollShortage=1 Outage=1 Pause=1 SaveReload=1 SyntheticButtons=1 PhysicalInputPending=1"));
            return true;
        }
        ++Phase; Next=FPlatformTime::Seconds()+.8; return false;
    }
private:
    FAutomationTestBase* Test;
    int32 Phase=0, Attempts=0;
    double Next=0;
    FName BeforeReview;
    TUniquePtr<FCityTechnologyFixture::FScopedState> FixtureState;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopAiTest,"CitySim.Desktop.CommercialAiPilot",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FCityDesktopAiTest::RunTest(const FString& Parameters)
{
    FString UserDir;
    if (!FParse::Param(FCommandLine::Get(),TEXT("CityDesktop")) || !FParse::Param(FCommandLine::Get(),TEXT("CityPackagedAutomation")) ||
        !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || !UserDir.Contains(TEXT("Client-")))
    { AddError(TEXT("AI smoke requires isolated CI packaged automation profile")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopAiCommand(this)); return true;
}
#endif
