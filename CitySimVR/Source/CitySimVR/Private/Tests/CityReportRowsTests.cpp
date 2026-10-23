#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityDesktopHud.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

class FCityReportRowsCommand : public IAutomationLatentCommand
{
public:
    explicit FCityReportRowsCommand(FAutomationTestBase* In) : Test(In) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() < Next) return false;
        auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
        ACityVRToolController* UI = nullptr;
        if (W) for (TActorIterator<ACityVRToolController> It(W); It; ++It) { UI = *It; break; }
        if (!UI || !UI->DesktopHud)
        {
            if (++Attempts < 120) { Next = FPlatformTime::Seconds()+.5; return false; }
            Test->AddError(TEXT("Reports desktop HUD missing")); return true;
        }
        if (Phase == 0)
        {
            UI->ReturnToCity();
            UI->SetObjectivesVisible(false); UI->SetCitySummaryVisible(false);
            auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
            Sim->SetSimulationPaused(false);
            for (int32 I=0; I<150; ++I) Sim->Tick(2.40001f);
            Sim->SetSimulationPaused(true);
            UI->ShowReports();
        }
        else if (Phase == 1 || Phase == 3)
        {
            const auto Data = UI->GetUiSnapshot();
            const auto Widget = [&](ECityPaletteAction Action) -> UWidget* {
                for (int32 I=0; I<Data.MenuEntries.Num(); ++I)
                    if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
                        return UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Menu%d"),I)));
                return nullptr;
            };
            const ECityPaletteAction Range[] = {ECityPaletteAction::ReportHourly, ECityPaletteAction::ReportDaily,
                ECityPaletteAction::ReportWeekly, ECityPaletteAction::ReportMonthly};
            const ECityPaletteAction Navigation[] = {ECityPaletteAction::ReportMetricPrevious, ECityPaletteAction::ReportMetricNext,
                ECityPaletteAction::ReportPeriodPrevious, ECityPaletteAction::ReportPeriodNext};
            float RangeBottom=0, NavigationTop=0;
            for (const auto* Group : {Range,Navigation})
            {
                float Y=-1;
                for (int32 I=0; I<4; ++I)
                {
                    auto* B=Widget(Group[I]);
                    if (!B) { Test->AddError(TEXT("Missing report row control")); return true; }
                    const auto G=B->GetCachedGeometry(); const float Current=G.GetAbsolutePosition().Y;
                    Test->TestTrue(TEXT("Report button has visible geometry"),G.GetLocalSize().X>0 && G.GetLocalSize().Y>0);
                    if (Y>=0) Test->TestTrue(TEXT("Controls in each report group share one line"),FMath::Abs(Current-Y)<1);
                    Y=Current;
                    if (Group==Range) RangeBottom=FMath::Max(RangeBottom,static_cast<float>(G.LocalToAbsolute(G.GetLocalSize()).Y));
                    else NavigationTop=Current;
                }
            }
            auto* People=Widget(ECityPaletteAction::ReportPeople);
            auto* Utilities=Widget(ECityPaletteAction::ReportUtilities);
            if (!People || !Utilities) { Test->AddError(TEXT("Missing report category")); return true; }
            Test->TestTrue(TEXT("Categories start below time ranges"),People->GetCachedGeometry().GetAbsolutePosition().Y>=RangeBottom);
            Test->TestTrue(TEXT("Navigation starts after all categories"),NavigationTop>=Utilities->GetCachedGeometry().LocalToAbsolute(Utilities->GetCachedGeometry().GetLocalSize()).Y);
            Test->TestTrue(TEXT("Housing absent from Reports"),Widget(ECityPaletteAction::OpenHousingHistory)==nullptr);
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/ReportRows");
            IFileManager::Get().MakeDirectory(*Dir,true);
            FScreenshotRequest::RequestScreenshot(Dir/(Phase==1 ? TEXT("Monthly.png") : TEXT("Hourly.png")),true,false);
            Test->AddInfo(TEXT("REPORT_ROWS281 range/category/navigation bands separated; four controls per range/navigation line"));
        }
        else if (Phase == 2) UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::ReportHourly));
        else { UI->SetPaletteOpen(false); return true; }
        ++Phase; Next=FPlatformTime::Seconds()+.6; return false;
    }
private:
    FAutomationTestBase* Test; int32 Phase=0,Attempts=0; double Next=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityReportRowsTest,"CitySim.Desktop.ReportRows",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityReportRowsTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FCityReportRowsCommand(this));
    return true;
}
#endif
