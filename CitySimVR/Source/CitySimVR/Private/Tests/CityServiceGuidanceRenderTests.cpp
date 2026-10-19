#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityDesktopHud.h"
#include "Presentation/CityDesktopChart.h"
#include "Presentation/CityDesktopIcon.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Real shared snapshots and actual Desktop composition, in a disposable test city.
class FCityDesktopServiceGuidanceCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopServiceGuidanceCommand(FAutomationTestBase* T) : Test(T)
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
			Test->AddError(TEXT("Service guidance Desktop HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		auto* Board = UI->Board.Get();
		auto Capture = [](const TCHAR* Name) {
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/ServiceGuidance");
			IFileManager::Get().MakeDirectory(*Directory, true);
			FScreenshotRequest::RequestScreenshot(Directory / Name, true, false);
		};
		auto CheckContext = [this, UI]() {
			auto* Label = UI->DesktopHud->WidgetTree->FindWidget(TEXT("Construction"));
			auto* Panel = UI->DesktopHud->WidgetTree->FindWidget(TEXT("ToolContextPanel"));
			Test->TestNull(TEXT("Bottom guidance has no inaccessible scrollbox"),
			               Cast<UScrollBox>(Label->GetParent()));
			const auto& TextGeometry = Label->GetCachedGeometry();
			const auto& PanelGeometry = Panel->GetCachedGeometry();
			Test->TestTrue(TEXT("All current tool text fits inside the panel"),
			               TextGeometry.LocalToAbsolute(TextGeometry.GetLocalSize()).Y <=
			                   PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()).Y - 4);
		};
		switch (Phase)
		{
		case 0: {
			UI->ReturnToCity();
			auto Params = Sim->Terrain.GetParameters();
			Params.GeneratorVersion = 0;
			Params.WaterRulesVersion = 1;
			Params.BroadElevationAmplitudeCm = Params.HillAmplitudeCm = Params.RidgeAmplitudeCm = 0;
			Sim->Terrain.Generate(Params);
			Sim->ResetCity(false);
			Board->PrepareCityView();
			auto& City = Sim->GetPlacementModel();
			auto Road = City.BeginRoadPreview(City.GetEntranceCell());
			City.UpdateRoadPreviewEnd(Road, {50, 8});
			if (!Test->TestTrue(TEXT("Review road placed"), City.CommitRoad(Road).IsSuccess()))
				return true;
			const TCHAR* Names[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"),
			                        TEXT("Home"),  TEXT("Home"),    TEXT("PrimarySchool")};
			const int32 Xs[] = {1, 5, 13, 17, 21, 25};
			for (int32 I = 0; I < 6; ++I)
			{
				const auto* D = Sim->GetBuildingCatalog()->FindDefinition(Names[I]);
				if (!D)
				{
					Test->AddError(TEXT("Missing review definition"));
					return true;
				}
				auto P = City.BeginPreview(*D, {Xs[I], 9});
				const auto R = City.CommitPlacement(P, *D);
				if (!Test->TestTrue(FString::Printf(TEXT("Review %s placed"), Names[I]), R.IsSuccess()))
					return true;
				if (I == 2)
					Home = R.EntityId;
				if (I == 5)
					School = R.EntityId;
			}
			Sim->Development.InitializeExistingAsOperational(City, Sim->GetBuildingCatalog(), 0);
			Sim->EconomyTuning.SchoolCapacity = 1;
			Sim->EconomyTuning.FixedStepSeconds = .125;
			Sim->EconomyTuning.FixedStepsPerMonth = 25;
			Sim->StartScenario();
			Sim->Tick(.625f);
			Sim->SetSimulationPaused(true);
			Sim->RefreshDerivedSnapshots();
			++Sim->PresentationGeneration;
			Board->PrepareCityView();
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			Board->SetToolMode(ECityToolMode::Inspect);
			UI->InspectBuilding(Home);
			break;
		}
		case 1:
			for (int32 I = 0; I < 6; ++I)
				Test->TestFalse(TEXT("A newly inspected home starts collapsed"),
				                UI->DesktopHud->IsInspectionSectionOpen(I));
			Capture(TEXT("HomeOverview.png"));
			break;
		case 2:
			Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectionSection2")))
			    ->OnClicked.Broadcast();
			break;
		case 3: {
			const auto Data = UI->GetUiSnapshot();
			Test->TestEqual(TEXT("Inspector groups six independent subjects"), Data.InspectionSections.Num(),
			                6);
			Test->TestTrue(
			    TEXT("Education remedy stays in education"),
			    Data.InspectionSections[2].Help.ToString().Contains(TEXT("build Secondary School")));
			Test->TestTrue(TEXT("Vacancies stay in work section"),
			               Data.InspectionSections[1].Help.ToString().Contains(TEXT("City vacancies:")));
			Test->TestFalse(TEXT("Education does not include water details"),
			                Data.InspectionSections[2].Detail.ToString().Contains(TEXT("Fix water:")));
			Test->TestEqual(TEXT("Home overview exposes trend slots"), Data.InspectionOverview.Num(), 3);
			Test->TestTrue(TEXT("Education uses quantities and a compact remedy"),
			               Data.InspectionSections[2].Rows.Num() > 0 &&
			                   Data.InspectionSections[2].Gauges.Num() > 0 &&
			                   Data.InspectionSections[2].Detail.ToString().Len() < 110);
			auto* Table = Cast<UCityDesktopChart>(
			    UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectionSection2Table")));
			Test->TestTrue(TEXT("Actual inspector renders a table"),
			               Table && Table->IsTable() && Table->GetRows().Num() > 0);
			Test->TestTrue(TEXT("Real home details diagnose missing Secondary education"),
			               Data.Inspection.ToString().Contains(TEXT("build Secondary School or School")));
			Test->TestTrue(TEXT("Real home details expose employment qualifications"),
			               Data.Inspection.ToString().Contains(TEXT("City vacancies:")));
			Capture(TEXT("HomeNeeds.png"));
			break;
		}
		case 4:
			UI->InspectBuilding(School);
			break;
		case 5: {
			const auto Data = UI->GetUiSnapshot();
			for (int32 I = 0; I < 6; ++I)
				Test->TestFalse(TEXT("Switching building resets section state"),
				                UI->DesktopHud->IsInspectionSectionOpen(I));
			Test->TestTrue(TEXT("A school has no resident overview"), Data.InspectionOverview.IsEmpty());
			Test->TestTrue(TEXT("No resident wellbeing appears for a school"),
			               Data.InspectionSections[4].Gauges.IsEmpty() &&
			                   Data.InspectionSections[4].Rows.IsEmpty());
			for (int32 I = 0; I < 3; ++I)
				Test->TestFalse(
				    TEXT("Nonresident overview rows are collapsed in the actual HUD"),
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Overview%dRow"), I)))
				        ->IsVisible());
			Test->TestTrue(TEXT("Desktop context carries actual school legend"),
			               Data.Construction.ToString().Contains(TEXT("potential reach")));
			Test->TestTrue(TEXT("Legend keeps actual study places distinct"),
			               Data.Construction.ToString().Contains(TEXT("Study places:")));
			CheckContext();
			Capture(TEXT("SchoolReach.png"));
			break;
		}
		case 6:
			UI->InspectBuilding(0);
			Board->SetSelectedFacility(TEXT("Service"));
			Sim->EconomyTuning.WaterCapacityPerTreatment = 5;
			Sim->RefreshDerivedSnapshots();
			++Sim->PresentationGeneration;
			break;
		case 7:
			UE_LOG(LogTemp, Display, TEXT("SERVICEGUIDANCE_LAYOUT Hud=%s Root=%s Context=%s"),
			       *UI->DesktopHud->GetCachedGeometry().GetLocalSize().ToString(),
			       *UI->DesktopHud->WidgetTree->RootWidget->GetCachedGeometry().GetLocalSize().ToString(),
			       *UI->DesktopHud->WidgetTree->FindWidget(TEXT("ToolContextPanel"))
			            ->GetCachedGeometry()
			            .GetLocalSize()
			            .ToString());
			Test->TestTrue(TEXT("Water placement context includes capacity distinction"),
			               UI->GetUiSnapshot().Construction.ToString().Contains(TEXT("not spare supply")));
			CheckContext();
			Capture(TEXT("WaterCapacity.png"));
			break;
		case 8:
			Board->SetToolMode(ECityToolMode::Inspect);
			for (const auto& E : Sim->GetPlacementModel().GetEntities())
				if (E.Value.DefinitionId == TEXT("Home") &&
				    Sim->GetServiceSnapshot().Water.EntityCauses.FindRef(E.Key) ==
				        ECityWaterFailure::SourceCapacity)
				{
					ShortHome = E.Key;
					break;
				}
			if (!Test->TestTrue(TEXT("Actual city has capacity shortage under healthy pipes"),
			                    ShortHome != 0))
				return true;
			UI->InspectBuilding(ShortHome);
			break;
		case 9:
			Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectionSection0")))
			    ->OnClicked.Broadcast();
			break;
		case 10:
			Test->TestTrue(
			    TEXT("Actual inspector gives actionable capacity remedy"),
			    UI->GetUiSnapshot().Inspection.ToString().Contains(TEXT("Fix water: add/upgrade")));
			Capture(TEXT("WaterShortage.png"));
			break;
		case 11: {
			UI->InspectBuilding(0);
			auto* Repair =
			    Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Toolbar7")));
			if (!Test->TestNotNull(TEXT("Repair is a direct toolbar button"), Repair))
				return true;
			Repair->OnClicked.Broadcast();
			Test->TestEqual(TEXT("Top-level Repair selects the existing tool"), Board->GetToolMode(),
			                ECityToolMode::Repair);
			Test->TestFalse(TEXT("Repair needs no City submenu"), UI->bPaletteOpen);
			break;
		}
		case 12: {
			CheckContext();
			auto* Inspect = UI->DesktopHud->WidgetTree->FindWidget(TEXT("Toolbar0"));
			auto* Repair = UI->DesktopHud->WidgetTree->FindWidget(TEXT("Toolbar7"));
			Test->TestTrue(TEXT("Repair is beside Inspect"),
			               Repair->GetCachedGeometry().GetAbsolutePosition().X >
			                       Inspect->GetCachedGeometry().GetAbsolutePosition().X &&
			                   Repair->GetCachedGeometry().GetAbsolutePosition().X -
			                           Inspect->GetCachedGeometry()
			                               .LocalToAbsolute(Inspect->GetCachedGeometry().GetLocalSize())
			                               .X <
			                       12);
			Capture(TEXT("RepairToolbar.png"));
			break;
		}
		case 13:
			Board->SetToolMode(ECityToolMode::Inspect);
			UI->InspectBuilding(Home);
			break;
		case 14:
			for (const TCHAR* Name : {TEXT("InspectionSection1"), TEXT("InspectionSection2")})
				Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(Name))
				    ->OnClicked.Broadcast();
			break;
		case 15:
			Test->TestTrue(TEXT("Work and Education may stay open together"),
			               UI->DesktopHud->IsInspectionSectionOpen(1) &&
			                   UI->DesktopHud->IsInspectionSectionOpen(2));
			Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("InspectionSection1")))
			    ->OnClicked.Broadcast();
			Test->TestTrue(TEXT("Closing Work leaves Education open"),
			               !UI->DesktopHud->IsInspectionSectionOpen(1) &&
			                   UI->DesktopHud->IsInspectionSectionOpen(2));
			break;
		case 16:
			for (const auto& E : Sim->GetPlacementModel().GetEntities())
				if (E.Value.DefinitionId == TEXT("Service"))
				{
					UI->InspectBuilding(E.Key);
					break;
				}
			break;
		case 17:
			for (const TCHAR* Name : {TEXT("InspectionSection0"), TEXT("InspectionSection1")})
				Cast<UCityDesktopActionButton>(UI->DesktopHud->WidgetTree->FindWidget(Name))
				    ->OnClicked.Broadcast();
			break;
		case 18: {
			const auto Data = UI->GetUiSnapshot();
			Test->TestTrue(TEXT("Utility has supply and staffing quantities"),
			               Data.InspectionSections[0].Gauges.Num() > 0 &&
			                   Data.InspectionSections[1].Rows.Num() > 0);
			const auto* Flow = Data.InspectionSections[0].Gauges.FindByPredicate(
			    [](const auto& G) { return G.Label.ToString() == TEXT("City flow / need"); });
			Test->TestNotNull(TEXT("A source exposes delivered city flow versus demand"), Flow);
			if (Flow)
				Test->TestEqual(TEXT("Actual shortage remains visible while the source is fully used"),
				                Flow->Value.ToString(), FString(TEXT("5 / 40")));
			Test->TestTrue(TEXT("Utility does not show resident information"),
			               Data.InspectionOverview.IsEmpty());
			for (int32 I = 0; I < 3; ++I)
				Test->TestFalse(
				    TEXT("Utility overview widgets are collapsed"),
				    UI->DesktopHud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Overview%dRow"), I)))
				        ->IsVisible());
			Capture(TEXT("UtilityQuantities.png"));
			break;
		}
		case 19:
			Sim->SetSimulationPaused(false);
			Sim->Tick(.125f);
			Sim->SetSimulationPaused(true);
			Sim->RefreshDerivedSnapshots();
			Board->PrepareCityView();
			UI->InspectBuilding(Home);
			break;
		case 20: {
			auto* PopulationIcon =
			    Cast<UCityDesktopIcon>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Overview0Icon")));
			auto* HappinessIcon =
			    Cast<UCityDesktopIcon>(UI->DesktopHud->WidgetTree->FindWidget(TEXT("Overview1Icon")));
			Test->TestTrue(TEXT("Actual UI acquires a population trend after time advances"),
			               PopulationIcon && PopulationIcon->Symbol != TEXT("TrendUnknown"));
			Test->TestTrue(TEXT("Actual unchanged happiness uses the steady arrow"),
			               HappinessIcon && HappinessIcon->Symbol == TEXT("TrendSteady"));
			Capture(TEXT("HomeTrends.png"));
			break;
		}
		case 21:
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_SERVICE_GUIDANCE_RENDER_COMPLETE Captures=8 PhysicalInputPending=1"));
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
	uint64 Home = 0, School = 0, ShortHome = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopServiceGuidanceRenderTest,
                                 "CitySim.Desktop.ServiceGuidanceRender",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopServiceGuidanceRenderTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("ServiceGuidance")))
	{
		AddError(TEXT("Service guidance render requires an isolated ServiceGuidance UserDir and CityDesktop"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopServiceGuidanceCommand(this));
	return true;
}
#endif
