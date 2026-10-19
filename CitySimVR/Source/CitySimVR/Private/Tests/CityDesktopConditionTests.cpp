#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
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
#include "Sandbox/CitySandboxDesktopPawn.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "UnrealClient.h"

class FCityDesktopConditionsCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopConditionsCommand(FAutomationTestBase* InTest) : Test(InTest)
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
			Test->AddError(TEXT("Conditions HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		auto* Hud = UI->DesktopHud.Get();
		auto Capture = [](const TCHAR* Name) {
			const auto Dir = FPaths::ProjectSavedDir() / TEXT("Automation/ConditionIcons");
			IFileManager::Get().MakeDirectory(*Dir, true);
			FScreenshotRequest::RequestScreenshot(Dir / Name, true, false);
		};
		auto* Button = Cast<UCityDesktopActionButton>(Hud->WidgetTree->FindWidget(TEXT("Condition0")));
		auto* Focus = Cast<UCityDesktopActionButton>(Hud->WidgetTree->FindWidget(TEXT("ConditionNext")));
		FString Error;

		switch (Phase)
		{
		case 0: {
			UI->ReturnToCity();
			Sim->ResetCity(false);
			Sim->SetSimulationPaused(true);
			auto& City = Sim->GetPlacementModel();
			City.SetTerrain(nullptr);
			City.ApplyMonthlyEconomy(100000, 0);
			UI->SetObjectivesVisible(false);
			UI->SetCitySummaryVisible(false);
			const auto* Def = Sim->GetBuildingCatalog()->FindDefinition(TEXT("Home"));
			for (int32 I = 0; I < 20; ++I)
			{
				auto P = City.BeginPreview(*Def, {4 + (I % 5) * 6, 4 + (I / 5) * 6});
				const auto R = City.CommitPlacement(P, *Def);
				if (!Test->TestTrue(TEXT("Condition lot placed"), R.IsSuccess()))
					return true;
				Ids.Add(R.EntityId);
			}
			Sim->Development.InitializeExistingAsOperational(City, Sim->GetBuildingCatalog(), 0);
			FCityFireSnapshot Fire;
			Fire.NextIgnitionStep = 10000;
			for (auto Id : Ids)
				if (Id != Ids[1] && Id != Ids[2])
					Fire.DamagedBuildings.Add(Id, {30, -1, 1});
			if (!Test->TestTrue(TEXT("Structural damage fixture restores"),
			                    Sim->Fire.RestoreState(Fire, 0, Error)))
				return true;
			if (!Test->TestTrue(TEXT("Crime uses real test command"),
			                    Sim->StartSafetyTest(Ids[1], false, true)))
				return true;
			FCityPopulationSnapshot People;
			FCityHouseholdRecord H;
			H.Id = H.HomeEntityId = Ids[2];
			People.Households.Add(H);
			auto& C = People.Citizens.Emplace_GetRef();
			C.Id = 1;
			C.HouseholdId = C.HomeEntityId = C.CurrentLocationEntityId = Ids[2];
			C.UnemployedSinceStep = 0;
			C.Happiness = C.HappinessTarget = 20;
			C.DiscontentMonths = 3;
			C.LastHappinessMonth = 3;
			C.HousingStatus = ECityHousingStatus::Searching;
			People.NextCitizenId = 2;
			People.LastSettledMonthId = 3;
			if (!Test->TestTrue(TEXT("Recorded housing-search fixture restores"),
			                    Sim->Population.RestoreState(People, Error)))
				return true;
			UI->Board->PrepareCityView();
			Sim->ServiceSnapshot.EntityFailures.Reset();
			UI->SelectInspect();
			Cash = City.GetFunds();
			Revision = City.GetRevision();
			break;
		}
		case 1:
			Test->TestNotNull(TEXT("Blueprint owns condition layer"),
			                  Hud->WidgetTree->FindWidget(TEXT("ConditionCanvas")));
			Test->TestEqual(TEXT("All issues retained beyond pool"), Hud->AllConditionTargets.Num(), 20);
			if (!Focus)
			{
				Test->AddError(TEXT("Focus button missing"));
				return true;
			}
			Focus->OnClicked.Broadcast();
			break;
		case 2:
			Test->TestEqual(TEXT("Focus opens highest-priority actual lot"), UI->InspectedEntityId, Ids[1]);
			Test->TestTrue(TEXT("Selected issue explains active crime"),
			               UI->GetUiSnapshot().InspectionSummary.ToString().Contains(TEXT("CRIME")));
			UI->ClearInspection();
			break;
		case 3:
			Test->TestTrue(TEXT("Projected marker exists"), !Hud->ConditionTargets.IsEmpty());
			Capture(TEXT("Ordinary.png"));
			break;
		case 4:
			if (Hud->ConditionTargets.IsEmpty())
			{
				Test->AddError(TEXT("No projected marker to select"));
				return true;
			}
			Picked = Hud->ConditionTargets[0];
			Button->OnClicked.Broadcast();
			break;
		case 5:
			Test->TestEqual(TEXT("Marker selects projected lot"), UI->InspectedEntityId, Picked);
			Test->TestEqual(TEXT("Selection spends nothing"), Sim->GetPlacementModel().GetFunds(), Cash);
			Test->TestEqual(TEXT("Selection never edits city"), Sim->GetPlacementModel().GetRevision(),
			                Revision);
			Capture(TEXT("Selected.png"));
			break;
		case 6:
			UI->ClearInspection();
			UI->Board->SetToolMode(ECityToolMode::Park);
			if (auto* Player = W->GetFirstPlayerController())
				if (auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn()))
				{
					Pawn->CameraState.Orbit(FVector2D(40, 10));
					Pawn->CameraState.Zoom(2);
					Pawn->CameraState.Pan(FVector2D(.1, .05));
				}
			break;
		case 7:
			for (int32 I = 0; I < Hud->ConditionTargets.Num(); ++I)
			{
				FVector Roof;
				FVector2D Pixel;
				UI->Board->GetBuildingMarkerAnchor(Hud->ConditionTargets[I], Roof);
				W->GetFirstPlayerController()->ProjectWorldLocationToScreen(Roof, Pixel, true);
				const auto* Marker =
				    Hud->WidgetTree->FindWidget(FName(FString::Printf(TEXT("Condition%d"), I)));
				const auto Position = Cast<UCanvasPanelSlot>(Marker->Slot)->GetPosition();
				Test->TestTrue(
				    TEXT("Pan orbit zoom and DPI preserve the actual roof anchor"),
				    Position.Equals(Pixel / UWidgetLayoutLibrary::GetViewportScale(Hud) - FVector2D(36, 50),
				                    .5));
			}
			Test->TestEqual(TEXT("Building markers cannot intercept placement"), Button->GetVisibility(),
			                ESlateVisibility::HitTestInvisible);
			Button->OnClicked.Broadcast();
			Test->TestEqual(TEXT("Stale marker cannot change build tool"), UI->Board->GetToolMode(),
			                ECityToolMode::Park);
			Capture(TEXT("Building.png"));
			break;
		case 8:
			UI->ExecuteDesktopUiAction(-24);
			Test->TestTrue(TEXT("Persistent zoning keeps a nonmodal menu"),
			               UI->IsDesktopZoningPaletteActive());
			Focus->OnClicked.Broadcast();
			Test->TestTrue(TEXT("Next issue leaves nonmodal zoning for inspection"),
			               !UI->bPaletteOpen && UI->Board->GetToolMode() == ECityToolMode::Inspect);
			UI->Board->SetToolMode(ECityToolMode::Demolish);
			Button->OnClicked.Broadcast();
			Test->TestEqual(TEXT("Marker cannot intercept demolition"), UI->Board->GetToolMode(),
			                ECityToolMode::Demolish);
			UI->ShowMainMenu();
			break;
		case 9:
			Test->TestTrue(TEXT("Modal clears marker hitboxes"), Hud->ConditionTargets.IsEmpty());
			Focus->OnClicked.Broadcast();
			Test->TestTrue(TEXT("Stale focus cannot dismiss modal"),
			               UI->bEntrySceneActive || UI->bPaletteOpen);
			UI->ReturnToCity();
			UI->SelectInspect();
			break;
		case 10:
			UI->SetConditionIndicatorsVisible(false);
			break;
		case 11:
			Test->TestTrue(TEXT("Hidden setting clears targets"), Hud->ConditionTargets.IsEmpty());
			UI->SetConditionIndicatorsVisible(true);
			break;
		case 12:
			if (!Test->TestTrue(TEXT("Repair uses authority"), Sim->RepairBuilding(Ids[0])))
				return true;
			UI->Board->PrepareCityView();
			UI->InspectBuilding(static_cast<int64>(Ids[0]));
			break;
		case 13:
			Test->TestTrue(TEXT("Response reclassifies to repairing"),
			               UI->GetUiSnapshot().InspectionSummary.ToString().Contains(TEXT("REPAIRING")));
			Capture(TEXT("Repairing.png"));
			break;
		case 14:
			UI->ClearInspection();
			Sim->GetPlacementModel().DemolishEntity(Ids[0]);
			UI->Board->PrepareCityView();
			break;
		case 15:
			Test->TestFalse(TEXT("Demolition clears stale identity"),
			                Hud->AllConditionTargets.Contains(Ids[0]));
			UI->InspectBuilding(static_cast<int64>(Ids[2]));
			if (auto* Player = W->GetFirstPlayerController())
				if (auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn()))
				{
					FVector Roof;
					UI->Board->GetBuildingMarkerAnchor(Ids[2], Roof);
					Pawn->FocusCondition(Roof);
				}
			break;
		case 16:
			Test->TestTrue(
			    TEXT("Housing search remains discoverable beside a higher-priority failure"),
			    UI->GetUiSnapshot().InspectionSummary.ToString().Contains(TEXT("searching for housing")));
			Capture(TEXT("HousingSearch.png"));
			break;
		case 17:
			UI->ClearInspection();
			if (auto* Player = W->GetFirstPlayerController())
				if (auto* Pawn = Cast<ACitySandboxDesktopPawn>(Player->GetPawn()))
					Pawn->FocusCondition(FVector(1000000, 1000000, 0));
			break;
		case 18:
			Test->TestTrue(TEXT("Offscreen anchors have no hitbox"), Hud->ConditionTargets.IsEmpty());
			Test->TestTrue(TEXT("Offscreen issues stay discoverable"), !Hud->AllConditionTargets.IsEmpty());
			Focus->OnClicked.Broadcast();
			break;
		case 19:
			Test->TestTrue(TEXT("Focus can recover an offscreen issue"), UI->InspectedEntityId != 0);
			Sim->ResetCity(false);
			UI->Board->PrepareCityView();
			break;
		case 20:
			Test->TestTrue(TEXT("City reset clears pool and focus list"), Hud->AllConditionTargets.IsEmpty());
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_DESKTOP_CONDITIONS_COMPLETE Captures=5 SyntheticButtons=1 "
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
	TArray<uint64> Ids;
	uint64 Picked = 0, Revision = 0;
	int64 Cash = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopConditionsTest, "CitySim.Desktop.BuildingConditions",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopConditionsTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("ConditionIcons")))
	{
		AddError(TEXT("Requires isolated ConditionIcons Desktop UserDir"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopConditionsCommand(this));
	return true;
}
#endif
