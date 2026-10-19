#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/CityVRToolController.h"
#include "Interaction/CityBoardPicking.h"
#include "Presentation/CityBoardActor.h"
#include "Sandbox/CityDesktopCamera.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUiParityTest, "CitySim.Desktop.SharedContentAndActions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUiParityTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityUiParityTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	FindFProperty<FObjectProperty>(ACityBoardActor::StaticClass(), TEXT("BuildingCatalog"))
	    ->SetObjectPropertyValue_InContainer(UI->Board, Catalog);
	UI->Board->DispatchBeginPlay();
	auto* CameraOwner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	const int64 Funds = Sim->GetPlacementModel().GetFunds();
	for (const auto Page :
	     {ECityPalettePage::Build, ECityPalettePage::Zone, ECityPalettePage::City, ECityPalettePage::Overlays,
	      ECityPalettePage::PowerFacilities, ECityPalettePage::EducationFacilities, ECityPalettePage::WasteFacilities,
	      ECityPalettePage::Settings, ECityPalettePage::Reports, ECityPalettePage::Taxes})
	{
		UI->SetPaletteOpen(true);
		UI->SetPalettePage(Page);
		const auto Shared = UI->GetUiSnapshot();
		TestEqual(TEXT("All visible VR entries have shared semantic views"), Shared.MenuEntries.Num(),
		          UI->VisiblePaletteEntries.Num());
		for (const auto& View : Shared.MenuEntries)
		{
			TestEqual(TEXT("VR label matches shared UI label"), UI->PaletteLabels[View.Index]->Text.ToString(),
			          View.Label.ToString());
			TestEqual(TEXT("Shared disabled state matches the action guard"), View.bEnabled,
			          !UI->IsPaletteActionDisabled(UI->VisiblePaletteEntries[View.Index].Action));
			TestEqual(TEXT("Shared selected state matches VR state"), View.bSelected,
			          UI->IsPaletteActionSelected(UI->VisiblePaletteEntries[View.Index].Action));
		}
	}
	UI->SetPaletteOpen(false);
	UI->SetCitySummaryVisible(true);
	UI->UpdateControllerRay();
	const auto Hud = UI->GetUiSnapshot();
	TestEqual(TEXT("Calendar parity"), UI->CalendarText->Text.ToString(), Hud.Calendar.ToString());
	TestEqual(TEXT("Budget parity"), UI->CityBudgetDetail->Text.ToString(), Hud.Budget.ToString());
	TestEqual(TEXT("Last settlement parity"), UI->CityLastNetText->Text.ToString(), Hud.LastNet.ToString());
	TestEqual(TEXT("Objective parity"), UI->ObjectiveText->Text.ToString(), Hud.Objectives.ToString());
	TestEqual(TEXT("Growth guidance parity"), UI->NeedsText->Text.ToString(), Hud.Growth.ToString());
	for (int32 Index = 0; Index < Hud.CityCards.Num(); ++Index)
	{
		TestEqual(TEXT("City card labels parity"), UI->CityInfoLabels[Index]->Text.ToString(),
		          Hud.CityCards[Index].Labels.ToString());
		TestEqual(TEXT("City card values parity"), UI->CityInfoValues[Index]->Text.ToString(),
		          Hud.CityCards[Index].Values.ToString());
	}
	UI->bDesktopUi = true;
	const auto CheckIdleInspect = [&]()
	{
		const auto Data = UI->GetUiSnapshot();
		TestFalse(TEXT("Idle Inspect offers no redundant Cancel control"), Data.bCanCancelTool);
		int32 Selected = 0;
		for (const auto& Entry : Data.ToolbarEntries)
			if (Entry.bSelected)
			{
				++Selected;
				TestEqual(TEXT("Inspect clears previous workspace highlights"), Entry.ActionId,
				          static_cast<int32>(ECityPaletteAction::SelectInspect));
			}
		TestEqual(TEXT("Idle Inspect has one selected toolbar button"), Selected, 1);
	};
	for (const auto Page : {ECityPalettePage::Build, ECityPalettePage::Zone, ECityPalettePage::City,
	                        ECityPalettePage::Overlays, ECityPalettePage::Settings})
	{
		UI->SetPaletteOpen(true);
		UI->SetPalettePage(Page);
		TestFalse(TEXT("Desktop dock-level pages omit redundant Back without altering VR entries"),
		          UI->GetUiSnapshot().MenuEntries.ContainsByPredicate(
		              [](const auto& Entry)
		              { return Entry.ActionId == static_cast<int32>(ECityPaletteAction::Back); }));
		TestTrue(TEXT("Shared source still retains VR Back"),
		         UI->VisiblePaletteEntries.ContainsByPredicate([](const auto& Entry)
		                                                       { return Entry.Action == ECityPaletteAction::Back; }));
	}
	UI->SetPaletteOpen(false);
	TestFalse(TEXT("Desktop calendar omits duplicate speed text"),
	          UI->GetUiSnapshot().Calendar.ToString().Contains(TEXT("Time:")));
	CheckIdleInspect();
	for (const auto Page : {ECityPalettePage::Build, ECityPalettePage::Zone, ECityPalettePage::City,
	                        ECityPalettePage::Overlays, ECityPalettePage::Settings})
	{
		UI->SetPaletteOpen(true);
		UI->SetPalettePage(Page);
		UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::SelectInspect));
		CheckIdleInspect();
	}
	UI->ExecuteDesktopUiAction(-1);
	TestTrue(TEXT("Menu opens actual main menu with city management choices"),
	         UI->bEntrySceneActive && UI->PalettePage == ECityPalettePage::MainMenu &&
	             UI->GetUiSnapshot().MenuEntries.Num() >= 4);
	UI->ReturnToCity();
	UI->bDesktopFocused = true;
	UI->SetPalettePage(ECityPalettePage::Root);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	TestTrue(TEXT("Desktop toolbar uses real shared navigation"),
	         UI->bPaletteOpen && UI->PalettePage == ECityPalettePage::Build);
	TestTrue(TEXT("Open menu blocks world placement"), UI->IsDesktopPointerBlocked());
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	TestFalse(TEXT("Repeated toolbar click dismisses its menu"), UI->bPaletteOpen);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::Back));
	TestFalse(TEXT("Back to desktop toolbar never opens an empty tools dialog"), UI->bPaletteOpen);
	UI->ExecuteDesktopUiAction(-24);
	TestTrue(TEXT("Z begins residential painting with a persistent zoning palette"),
	         UI->IsDesktopZoningPaletteActive() && UI->Board->GetSelectedZone() == ECityZoneType::Residential);
	TestFalse(TEXT("Persistent zoning palette permits map picking"), UI->IsDesktopPointerBlocked());
	TestTrue(TEXT("Persistent zoning palette permits camera movement"), UI->IsDesktopCameraInputAllowed());
	UI->ExecuteDesktopUiAction(-24);
	TestEqual(TEXT("Z cycles to commercial"), UI->Board->GetSelectedZone(), ECityZoneType::Commercial);
	UI->ExecuteDesktopUiAction(-24);
	TestEqual(TEXT("Z cycles to industrial"), UI->Board->GetSelectedZone(), ECityZoneType::Industrial);
	UI->ExecuteDesktopUiAction(-24);
	TestEqual(TEXT("Z wraps to residential"), UI->Board->GetSelectedZone(), ECityZoneType::Residential);
	UI->ExecuteDesktopUiAction(-7);
	TestFalse(TEXT("Esc closes persistent zoning"), UI->bPaletteOpen);
	TestEqual(TEXT("Closing zoning first retains the armed tool"), UI->Board->GetToolMode(), ECityToolMode::Zone);
	UI->ExecuteDesktopUiAction(-7);
	TestEqual(TEXT("Second Esc falls back to Inspect"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	CheckIdleInspect();
	UI->ExecuteDesktopUiAction(-7);
	TestEqual(TEXT("Repeated cancel stays in Inspect"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	const auto HasAction = [UI](const ECityPaletteAction Action)
	{
		return UI->VisiblePaletteEntries.ContainsByPredicate([Action](const FCityPaletteEntry& Entry)
		                                                     { return Entry.Action == Action; });
	};
	TestFalse(TEXT("Normal build menu excludes direct homes and shops"),
	          HasAction(ECityPaletteAction::SelectHome) || HasAction(ECityPaletteAction::SelectShop));
	TestTrue(TEXT("Debug-off guards direct home/shop actions on both platforms"),
	         UI->IsPaletteActionDisabled(ECityPaletteAction::SelectHome) &&
	             UI->IsPaletteActionDisabled(ECityPaletteAction::SelectShop));
	UI->SetDebugModeEnabled(true);
	UI->ShowDebugTools();
	TestTrue(TEXT("Shared debug menu contains direct homes and shops"),
	         HasAction(ECityPaletteAction::SelectHome) && HasAction(ECityPaletteAction::SelectShop));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::SelectHome));
	TestEqual(TEXT("Debug selection uses the shared tool command"), UI->Board->GetToolMode(), ECityToolMode::Home);
	TestFalse(TEXT("Debug construction selection dismisses desktop menu"), UI->bPaletteOpen);
	UI->SetDebugModeEnabled(false);
	TestEqual(TEXT("Disabling debug disarms direct construction"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	UI->SelectHome();
	UI->SelectShop();
	TestEqual(TEXT("Legacy keyboard shortcuts cannot bypass debug"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	UI->SetPalettePage(ECityPalettePage::Build);
	TestEqual(TEXT("Selecting tools cannot spend city funds"), Sim->GetPlacementModel().GetFunds(), Funds);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::ConfirmArchive));
	TestEqual(TEXT("Stale/unavailable menu action cannot navigate"), UI->PalettePage, ECityPalettePage::Build);
	for (int32 Repeat = 0; Repeat < 2; ++Repeat)
	{
		UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenOverlays));
		TestTrue(TEXT("Overlay navigation remains open"), UI->bPaletteOpen);
		UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OverlayPower));
		TestFalse(TEXT("Overlay choice closes even when already selected"), UI->bPaletteOpen);
	}
	UI->SetPaletteOpen(true);
	UI->SetPalettePage(ECityPalettePage::Lighting);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::LightingNight));
	TestFalse(TEXT("Lighting choice closes menu"), UI->bPaletteOpen);
	UI->SetPaletteOpen(true);
	UI->SetPalettePage(ECityPalettePage::Taxes);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::TaxResidentialUp));
	TestTrue(TEXT("Pending tax edit retains menu for Apply/Cancel"), UI->bPaletteOpen);
	UI->ExecuteDesktopUiAction(-24);
	TestEqual(TEXT("Z leaves an unfinished tax dialog alone"), UI->PalettePage, ECityPalettePage::Taxes);
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::CancelTaxes));
	TestFalse(TEXT("Finishing tax edits closes menu"), UI->bPaletteOpen);
	UI->SetPaletteOpen(true);
	UI->SetPalettePage(ECityPalettePage::Taxes);
	UI->BeginTaxEdit();
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::TaxResidentialUp));
	TestTrue(TEXT("Tax close check begins with an unfinished edit"), UI->ResidentialTaxControl.IsEditing());
	UI->ExecuteDesktopUiAction(-13);
	TestFalse(TEXT("Explicit close dismisses a nested menu in one action"), UI->bPaletteOpen);
	TestFalse(TEXT("Closing tax dialog discards its pending edit"), UI->ResidentialTaxControl.IsEditing());
	UI->LastInspectionText = TEXT("Selected building");
	UI->SetCitySummaryVisible(true);
	UI->ExecuteDesktopUiAction(-14);
	TestTrue(TEXT("Info close clears both selection and stats"), UI->LastInspectionText.IsEmpty() && !UI->bNeedsOpen);
	UI->SetCitySummaryVisible(true);
	UI->SetObjectivesVisible(true);
	UI->ExecuteDesktopUiAction(-2);
	TestTrue(TEXT("Hiding stats leaves objectives open"), !UI->bNeedsOpen && UI->bObjectivesOpen);
	UI->ExecuteDesktopUiAction(-3);
	TestTrue(TEXT("Both side panels can be hidden"), !UI->bNeedsOpen && !UI->bObjectivesOpen);
	UI->ExecuteDesktopUiAction(-2);
	TestTrue(TEXT("Showing stats leaves objectives hidden"), UI->bNeedsOpen && !UI->bObjectivesOpen);
	UI->SetPaletteOpen(false);
	UI->LifecycleInputGate.Suspend();
	const bool Paused = Sim->IsSimulationPaused();
	UI->ExecuteDesktopUiAction(-20);
	TestEqual(TEXT("Suspended desktop input cannot pause/unpause"), Sim->IsSimulationPaused(), Paused);
	UI->LifecycleInputGate.Update(false);
	UI->ShowMainMenu();
	UI->ExecuteDesktopUiAction(-20);
	TestEqual(TEXT("Entry scene cannot alter running-clock preference"), Sim->IsSimulationPaused(), Paused);
	UI->ReturnToCity();
	UI->bDesktopFocused = true;
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenPowerFacilities));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::SelectWindPower));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenPowerFacilities));
	TestTrue(TEXT("Last selected power model is highlighted when reopened"),
	         UI->IsPaletteActionSelected(ECityPaletteAction::SelectWindPower));
	TestFalse(TEXT("Other power model is not highlighted"),
	          UI->IsPaletteActionSelected(ECityPaletteAction::SelectCoalPower));
	UI->ExecuteDesktopUiAction(-13);
	// Exercise real board gestures, including cancel while pointing away from the map.
	auto* Player = World->SpawnActor<APlayerController>();
	// This isolated world has not initialized gameplay actors, so register the input owner explicitly.
	World->AddController(Player);
	TestEqual(TEXT("Gesture fixture has its input owner"), World->GetFirstPlayerController(), Player);
	UI->Board->SetCommandInputBound(true);
	const auto AimAtMap = [&]()
	{
		const auto Transform = UI->Board->GetActorTransform();
		UI->Board->SetControllerRay(
		    Transform.TransformPosition(CityBoardPicking::GetCellCenterLocal({8, 8}) + FVector(0, 0, 10000)),
		    Transform.TransformVector(-FVector::UpVector).GetSafeNormal(), true, true);
	};
	const auto AimAway = [&]()
	{ UI->Board->SetControllerRay(FVector::ZeroVector, FVector::ForwardVector, false, true); };
	UI->ExecuteDesktopUiAction(-24);
	AimAtMap();
	UI->Board->RequestConfirm();
	UI->Board->Tick(0);
	TestTrue(TEXT("Zoning first corner starts an unfinished action"), UI->Board->HasActiveToolAction());
	AimAway();
	UI->ExecuteDesktopUiAction(-7);
	UI->Board->Tick(0);
	TestFalse(TEXT("First Esc aborts zoning rectangle without painting"), UI->Board->HasActiveToolAction());
	TestTrue(TEXT("Aborting zoning retains its controls and tool"), UI->IsDesktopZoningPaletteActive());
	UI->ExecuteDesktopUiAction(-7);
	TestFalse(TEXT("Next Esc dismisses zoning controls"), UI->bPaletteOpen);
	UI->ExecuteDesktopUiAction(-7);
	TestEqual(TEXT("Final Esc disarms zoning to Inspect"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	UI->SelectRoad();
	AimAtMap();
	UI->Board->RequestConfirm();
	UI->Board->Tick(0);
	TestTrue(TEXT("Road first click starts an unfinished action"), UI->Board->HasActiveToolAction());
	AimAway();
	UI->ExecuteDesktopUiAction(-7);
	UI->Board->Tick(0);
	TestTrue(TEXT("Road cancel clears its gesture but retains the tool"),
	         !UI->Board->HasActiveToolAction() && UI->Board->GetToolMode() == ECityToolMode::Road);
	UI->ExecuteDesktopUiAction(-7);
	TestEqual(TEXT("Next road cancel returns to Inspect"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	UI->SelectPower();
	AimAtMap();
	UI->Board->Tick(0);
	TestTrue(TEXT("Building hover creates a placement preview"), UI->Board->HasActiveToolAction());
	AimAway();
	UI->ExecuteDesktopUiAction(-7);
	UI->Board->Tick(0);
	TestTrue(TEXT("Building cancel works even away from the map and keeps its tool"),
	         !UI->Board->HasActiveToolAction() && UI->Board->GetToolMode() == ECityToolMode::Power);
	UI->ExecuteDesktopUiAction(-7);
	TestEqual(TEXT("Next building cancel returns to Inspect"), UI->Board->GetToolMode(), ECityToolMode::Inspect);
	CheckIdleInspect();
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::OpenBuild));
	UI->ExecuteDesktopUiAction(-7);
	UI->ExecuteDesktopUiAction(-7);
	TestTrue(TEXT("Cancel exits the Build workspace with Inspect as default"),
	         !UI->bPaletteOpen && UI->Board->GetToolMode() == ECityToolMode::Inspect);
	TestEqual(TEXT("Aborted tool actions never spend city funds"), Sim->GetPlacementModel().GetFunds(), Funds);
	for (const int32 Speed : {1, 3, 5})
	{
		UI->ExecuteDesktopUiAction(Speed == 1 ? -21 : Speed == 3 ? -22 : -23);
		TestEqual(TEXT("Shared speed snapshot follows desktop choices"), UI->GetUiSnapshot().SpeedMultiplier, Speed);
		TestFalse(TEXT("Choosing a speed resumes time"), UI->GetUiSnapshot().bPaused);
	}
	UI->ExecuteDesktopUiAction(-20);
	TestTrue(TEXT("Pause is represented separately from the saved speed"), UI->GetUiSnapshot().bPaused);
	UI->bDesktopFocused = false;
	const ECityToolMode ToolBeforeFocusLoss = UI->Board->GetToolMode();
	UI->ExecuteDesktopUiAction(static_cast<int32>(ECityPaletteAction::SelectDemolish));
	TestEqual(TEXT("Unfocused desktop cannot change tools"), UI->Board->GetToolMode(), ToolBeforeFocusLoss);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopCameraTest, "CitySim.Desktop.CameraBoundsAndReset",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopCameraTest::RunTest(const FString& Parameters)
{
	FCityDesktopCamera Camera;
	const FVector Home(12, -45, 30);
	Camera.Reset(Home, 200);
	Camera.Zoom(10000);
	TestEqual(TEXT("Detail zoom reaches 2.56m on the default 512m city"), Camera.Distance, 1.0);
	TestTrue(TEXT("Detail clipping stays before the focused ground"),
	         Camera.NearClipPlane() > 0 && Camera.NearClipPlane() < Camera.Distance * .1);
	Camera.Pan(FVector2D(.1, 0));
	TestTrue(TEXT("Detail pan scales down with distance"), FVector::Dist(Camera.Home, Camera.Focus) < 1.);
	Camera.Zoom(-10000);
	TestEqual(TEXT("Zoom retains far limit"), Camera.Distance, 800.0);
	Camera.Orbit(FVector2D(100000, 100000));
	TestEqual(TEXT("Orbit cannot move beneath board"), Camera.Pitch, -20.0);
	Camera.Orbit(FVector2D(-100000, -100000));
	TestEqual(TEXT("Orbit cannot flip camera"), Camera.Pitch, -85.0);
	Camera.Pan(FVector2D(10000, 10000));
	TestTrue(TEXT("Pan stays near current board"),
	         FMath::Abs(Camera.Focus.X - Home.X) <= 200 && FMath::Abs(Camera.Focus.Y - Home.Y) <= 200);
	TestEqual(TEXT("Camera pan preserves board elevation"), Camera.Focus.Z, Home.Z);
	TestFalse(TEXT("Bounded camera position is finite"), Camera.Location().ContainsNaN());
	Camera.Reset(Home, 200);
	TestEqual(TEXT("Home restores focus"), Camera.Focus, Home);
	TestEqual(TEXT("Home restores framing"), Camera.Distance, 260.0);
	const FVector Sightline = (Camera.Focus - Camera.Location()).GetSafeNormal();
	TestTrue(TEXT("Camera always looks at its focus"),
	         FVector::DotProduct(Sightline, Camera.Rotation().Vector()) > .9999);
	for (const double Yaw : {0.0, 90.0, -45.0, 180.0})
	{
		for (const FVector2D Drag : {FVector2D(10, 0), FVector2D(0, 10)})
		{
			Camera.Reset(Home, 200);
			Camera.Yaw = Yaw;
			const FVector Before = Camera.Location();
			Camera.Drag(Drag);
			// A stationary board point moves with the cursor in camera space.
			const FVector ApparentMotion = Before - Camera.Location();
			const FRotationMatrix View(Camera.Rotation());
			const double AlongDrag = Drag.X * FVector::DotProduct(ApparentMotion, View.GetUnitAxis(EAxis::Y)) +
			                         Drag.Y * FVector::DotProduct(ApparentMotion, View.GetUnitAxis(EAxis::Z));
			TestTrue(TEXT("Board follows mouse drag on both screen axes at every orbit yaw"), AlongDrag > 0);
			TestEqual(TEXT("Mouse drag preserves board elevation"), Camera.Focus.Z, Home.Z);
		}
	}
	return true;
}
#endif
