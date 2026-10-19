#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopFocusRecoveryTest, "CitySim.Desktop.FocusRestoresEntryMenu",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityDesktopFocusRecoveryTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityDesktopFocusRecoveryTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* CameraOwner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->bDesktopUi = true;
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	const auto Funds = Sim->GetPlacementModel().GetFunds();
	const auto Step = Sim->GetEconomySnapshot().FixedStepId;
	UI->ShowMainMenu();
	for (int32 Cycle = 0; Cycle < 3; ++Cycle)
	{
		UI->UpdateDesktopFocus(false);
		TestTrue(TEXT("Focus loss keeps entry scene suspended and city hidden"),
		         UI->IsMainMenuEnvironmentActive() && UI->Board->IsHidden());
		TestFalse(TEXT("Focus loss dismisses the menu"), UI->GetUiSnapshot().bMenuOpen);
		TestFalse(TEXT("Unfocused input remains blocked"), UI->LifecycleInputGate.CanAcceptInput());
		UI->UpdateDesktopFocus(false);
		// Exercise the real polled transition without an OS foreground delegate.
		UI->UpdateDesktopFocus(true);
		const auto Recovered = UI->GetUiSnapshot();
		TestTrue(TEXT("Focus return restores the entry menu without relaunch"),
		         Recovered.bEntryScene && Recovered.bMenuOpen && !Recovered.MenuEntries.IsEmpty());
		TestEqual(TEXT("Focus return presents the main menu"), UI->PalettePage, ECityPalettePage::MainMenu);
		UI->LifecycleInputGate.Update(true);
		TestFalse(TEXT("Held input cannot activate the restored menu"), UI->LifecycleInputGate.CanAcceptInput());
		UI->LifecycleInputGate.Update(false);
		TestTrue(TEXT("Releasing input rearms the restored menu"), UI->LifecycleInputGate.CanAcceptInput());
		UI->ShowGameSettings();
		UI->UpdateDesktopFocus(true);
		TestEqual(TEXT("Unchanged focus does not reset a menu page"), UI->PalettePage, ECityPalettePage::Settings);
		TestTrue(TEXT("Unchanged focus does not repeatedly suspend input"), UI->LifecycleInputGate.CanAcceptInput());
	}
	UI->ReturnToCity();
	UI->SetPaletteOpen(true);
	UI->SetPalettePage(ECityPalettePage::Build);
	UI->UpdateDesktopFocus(false);
	UI->UpdateDesktopFocus(true);
	TestFalse(TEXT("Returning to an active city does not open the entry scene"), UI->IsMainMenuEnvironmentActive());
	TestFalse(TEXT("In-city transient tool menus remain dismissed"), UI->GetUiSnapshot().bMenuOpen);
	TestFalse(TEXT("In-city focus return also requires fresh release"), UI->LifecycleInputGate.CanAcceptInput());
	TestEqual(TEXT("Focus transitions preserve city funds"), Sim->GetPlacementModel().GetFunds(), Funds);
	TestEqual(TEXT("Focus transitions do not advance simulation"), Sim->GetEconomySnapshot().FixedStepId, Step);
	UI->RestoreDebugInfoDisplay();
	World->DestroyWorld(false);
	return true;
}
#endif
