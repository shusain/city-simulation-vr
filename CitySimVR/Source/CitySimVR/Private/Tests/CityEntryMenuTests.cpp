#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Presentation/CityNewCitySetup.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Commands/CityPlacementModel.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "Sandbox/CitySandboxEnvironment.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySetupDraftTest, "CitySim.Menu.SetupDraft",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySetupDraftTest::RunTest(const FString& Parameters)
{
	const FCityEconomyTuning Baseline;
	const auto Normal = FCityNewCitySetup::Proposal(ECityDifficultyDraft::Normal, Baseline);
	const auto Relaxed = FCityNewCitySetup::Proposal(ECityDifficultyDraft::Relaxed, Baseline);
	const auto Hard = FCityNewCitySetup::Proposal(ECityDifficultyDraft::Challenging, Baseline);
	TestEqual(TEXT("Normal starting funds keep baseline"), Normal.StartingFunds, int64(10000));
	TestTrue(TEXT("Normal retains existing incident/recovery rules"),
	         Normal.Tuning.CrimePopulationThreshold == Baseline.CrimePopulationThreshold &&
	             Normal.Tuning.FirePopulationThreshold == Baseline.FirePopulationThreshold &&
	             Normal.Tuning.CrimeGraceMonths == Baseline.CrimeGraceMonths &&
	             Normal.Tuning.FireGraceMonths == Baseline.FireGraceMonths &&
	             Normal.Tuning.CrimeCheckIntervalSteps == Baseline.CrimeCheckIntervalSteps &&
	             Normal.Tuning.FireCheckIntervalSteps == Baseline.FireCheckIntervalSteps &&
	             Normal.Tuning.FireRepairCostPercent == Baseline.FireRepairCostPercent &&
	             Normal.Tuning.FireRepairSteps == Baseline.FireRepairSteps);
	TestTrue(TEXT("Relaxed provides bounded easier proposals"),
	         Relaxed.StartingFunds > Normal.StartingFunds &&
	             Relaxed.Tuning.FirePopulationThreshold > Normal.Tuning.FirePopulationThreshold &&
	             Relaxed.Tuning.CrimeGraceMonths > Normal.Tuning.CrimeGraceMonths &&
	             Relaxed.Tuning.FireCheckIntervalSteps > Normal.Tuning.FireCheckIntervalSteps &&
	             Relaxed.Tuning.FireRepairCostPercent < Normal.Tuning.FireRepairCostPercent);
	TestTrue(TEXT("Challenging provides bounded harder proposals"),
	         Hard.StartingFunds < Normal.StartingFunds &&
	             Hard.Tuning.CrimePopulationThreshold < Normal.Tuning.CrimePopulationThreshold &&
	             Hard.Tuning.FireGraceMonths < Normal.Tuning.FireGraceMonths &&
	             Hard.Tuning.FireRepairSteps > Normal.Tuning.FireRepairSteps);
	for (const auto* Proposal : {&Normal, &Relaxed, &Hard})
	{
		TestEqual(TEXT("Accepted fire damage cadence is unchanged"), Proposal->Tuning.FireDamagePerStep,
		          Baseline.FireDamagePerStep);
		TestEqual(TEXT("Accepted fire spread cadence is unchanged"), Proposal->Tuning.FireSpreadIntervalSteps,
		          Baseline.FireSpreadIntervalSteps);
		TestTrue(TEXT("Preview honestly names creation boundary"),
		         Proposal->Summary().Contains(TEXT("Review before creating")) &&
		             Proposal->Summary().Contains(TEXT("chances")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEntryMenuTest, "CitySim.Menu.EntryNavigationAndLoad",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEntryMenuTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityEntryMenuTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	FindFProperty<FObjectProperty>(ACityBoardActor::StaticClass(), TEXT("BuildingCatalog"))
	    ->SetObjectPropertyValue_InContainer(UI->Board, Catalog);
	UI->Board->DispatchBeginPlay();
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* CameraOwner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	const int64 Funds = Simulation->GetPlacementModel().GetFunds();
	const int32 CrimeThreshold = Simulation->GetEconomyTuning().CrimePopulationThreshold;
	const auto Invoke = [&](const ECityPaletteAction Action)
	{
		const int32 Index =
		    UI->VisiblePaletteEntries.IndexOfByPredicate([&](const auto& Entry) { return Entry.Action == Action; });
		if (TestTrue(TEXT("Requested action exists on real menu"), Index != INDEX_NONE))
			UI->ExecutePaletteAction(Index);
	};
	for (const auto Hand : {ECityControllerHand::Left, ECityControllerHand::Right})
	{
		UI->DominantHand = Hand;
		Simulation->SetSimulationPaused(true);
		UI->ShowMainMenu();
		UI->HandleGrip(FCityToolInputRouter::OtherHand(Hand), true);
		UI->HandleGrip(FCityToolInputRouter::OtherHand(Hand), false);
		TestTrue(TEXT("Entry menu stays open without holding grip"),
		         UI->bPaletteOpen && UI->PalettePage == ECityPalettePage::MainMenu);
		Invoke(ECityPaletteAction::OpenSettings);
		TestTrue(TEXT("Settings retains pause"), Simulation->IsSimulationPaused());
		Invoke(ECityPaletteAction::OpenLighting);
		UI->Cancel();
		UI->Cancel();
		TestEqual(TEXT("Settings returns to the entry menu"), UI->PalettePage, ECityPalettePage::MainMenu);
		Invoke(ECityPaletteAction::OpenNewCitySetup);
		TestEqual(TEXT("Setup starts Normal"), UI->NewCityDraft.Difficulty, ECityDifficultyDraft::Normal);
		Invoke(ECityPaletteAction::DifficultyChallenging);
		TestEqual(TEXT("Selection edits only draft"), UI->NewCityDraft.Difficulty, ECityDifficultyDraft::Challenging);
		TestEqual(TEXT("Setup cannot spend funds"), Simulation->GetPlacementModel().GetFunds(), Funds);
		TestEqual(TEXT("Setup cannot mutate rules"), Simulation->GetEconomyTuning().CrimePopulationThreshold,
		          CrimeThreshold);
		const int32 Seed = UI->NewCityTerrain.Seed;
		Invoke(ECityPaletteAction::ReviewCreate);
		TestEqual(TEXT("Review retains draft seed"), UI->NewCityTerrain.Seed, Seed);
		UI->Cancel();
		TestEqual(TEXT("Cancel creation returns to editable setup"), UI->PalettePage, ECityPalettePage::NewCitySetup);
		TestEqual(TEXT("Cancel creation keeps chosen preset"), UI->NewCityDraft.Difficulty,
		          ECityDifficultyDraft::Challenging);
		TestEqual(TEXT("Cancel creation cannot spend funds"), Simulation->GetPlacementModel().GetFunds(), Funds);
		UI->ConfirmReviewedCityCreation();
		TestEqual(TEXT("Creation outside review cannot replace city"), Simulation->GetPlacementModel().GetFunds(),
		          Funds);
		UI->Cancel();
		Invoke(ECityPaletteAction::OpenNewCitySetup);
		TestEqual(TEXT("Canceled draft is discarded"), UI->NewCityDraft.Difficulty, ECityDifficultyDraft::Normal);
		for (const bool bHand : {false, true})
		{
			UI->PaletteAnchorMode = bHand ? 2 : 0;
			for (const auto Page : {ECityPalettePage::MainMenu, ECityPalettePage::NewCitySetup,
			                        ECityPalettePage::NewCityReview, ECityPalettePage::LoadReview})
			{
				UI->SetPalettePage(Page);
				const auto Surface = FCityPaletteModel::SurfaceRect(Page, UI->UsesHandPalette());
				const auto Help = UI->PaletteHelp->CalcBounds(UI->PaletteHelp->GetRelativeTransform()).GetBox();
				TestTrue(TEXT("Entry help stays inside background above buttons"),
				         Help.Min.Y >= -Surface.HalfSize.X && Help.Max.Y <= Surface.HalfSize.X && Help.Min.Z > 3 &&
				             Help.Max.Z <= Surface.Center.Z + Surface.HalfSize.Y);
				for (int32 I = 0; I < UI->VisiblePaletteEntries.Num(); ++I)
				{
					const auto Rect = UI->GetPaletteButtonRect(I, UI->VisiblePaletteEntries[I].Action);
					const auto Bounds =
					    UI->PaletteLabels[I]->CalcBounds(UI->PaletteLabels[I]->GetRelativeTransform()).GetBox();
					TestTrue(TEXT("Entry labels fit their targets"),
					         Bounds.Min.Y >= Rect.Center.Y - Rect.HalfSize.X &&
					             Bounds.Max.Y <= Rect.Center.Y + Rect.HalfSize.X);
					const FVector Point = UI->PaletteRoot->GetComponentTransform().TransformPosition(Rect.Center);
					const FVector Normal = UI->PaletteRoot->GetForwardVector();
					TestEqual(TEXT("Actual menu ray identifies independent action"),
					          UI->FindPaletteButton(Point + Normal * 40, -Normal), I);
				}
			}
		}
		UI->ShowMainMenu();
		Invoke(ECityPaletteAction::ReviewLoad);
		UI->Cancel();
		TestEqual(TEXT("Canceled load preserves funds"), Simulation->GetPlacementModel().GetFunds(), Funds);
		Invoke(ECityPaletteAction::ContinueCity);
		TestFalse(TEXT("Continue closes the menu"), UI->bPaletteOpen);
		TestTrue(TEXT("Continue retains paused clock"), Simulation->IsSimulationPaused());
		Simulation->SetSimulationPaused(false);
		UI->ShowMainMenu();
		Invoke(ECityPaletteAction::ContinueCity);
		TestFalse(TEXT("Continue retains running clock"), Simulation->IsSimulationPaused());
		Simulation->SetSimulationPaused(true);
	}
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/EntryMenu-menu-city.json"));
	FString Error, Before, After;
	Simulation->SetSimulationPaused(false);
	TestTrue(TEXT("Create isolated saved running fixture"), Simulation->SaveCityToPath(Path, Error));
	FFileHelper::LoadFileToString(Before, *Path);
	Simulation->GetPlacementModel().TrySpendFunds(123);
	UI->ShowMainMenu();
	UI->ConfirmCityLoad(Path);
	TestEqual(TEXT("Confirmation outside review cannot load"), Simulation->GetPlacementModel().GetFunds(), Funds - 123);
	UI->ReviewCityLoad();
	UI->ConfirmCityLoad(Path + TEXT(".missing"));
	TestEqual(TEXT("Failed load leaves active city intact"), Simulation->GetPlacementModel().GetFunds(), Funds - 123);
	TestTrue(TEXT("Failed load remains on review"),
	         UI->PalettePage == ECityPalettePage::LoadReview && UI->GetEntryMenuHelp().Contains(TEXT("failed")));
	UI->ConfirmCityLoad(Path);
	TestEqual(TEXT("Confirmed load restores saved funds"), Simulation->GetPlacementModel().GetFunds(), Funds);
	TestTrue(TEXT("Confirmed load pauses saved running clock"), Simulation->IsSimulationPaused());
	TestTrue(TEXT("Load reveals city with success status"),
	         !UI->bPaletteOpen && !UI->IsMainMenuEnvironmentActive() &&
	             Simulation->GetPersistenceStatus().Contains(TEXT("loaded")));
	FFileHelper::LoadFileToString(After, *Path);
	TestEqual(TEXT("Load never overwrites the saved city"), After, Before);
	UI->ShowNewCitySetup();
	Invoke(ECityPaletteAction::DifficultyRelaxed);
	UI->HandleApplicationBackgrounded();
	TestFalse(TEXT("Lifecycle dismissal closes entry"), UI->bPaletteOpen);
	TestEqual(TEXT("Lifecycle dismissal discards draft"), UI->NewCityDraft.Difficulty, ECityDifficultyDraft::Normal);
	UI->ReturnToCity();
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	UI->RestoreDebugInfoDisplay();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEntryEnvironmentTest, "CitySim.Menu.EnvironmentAndTransitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEntryEnvironmentTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityEntryEnvironmentTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* Environment = World->SpawnActor<ACitySandboxEnvironment>();
	auto* OriginallyHidden = World->SpawnActor<ACityBoardActor>();
	OriginallyHidden->SetActorHiddenInGame(true);
	auto* Simulation = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* CameraOwner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	UI->PaletteAnchorMode = 2;
	Simulation->SetSimulationPaused(false);
	const int64 Step = Simulation->GetEconomySnapshot().FixedStepId;
	UI->ShowMainMenu();
	UI->UpdatePaletteAttachment();
	TestTrue(TEXT("Menu hides the whole map and sandbox floor/sky"), UI->Board->IsHidden() && Environment->IsHidden());
	TestTrue(TEXT("City HUD is hidden independently of visibility refresh"),
	         UI->CalendarText->bHiddenInGame && UI->ObjectiveText->bHiddenInGame && UI->NeedsToggleIcon->bHiddenInGame);
	TestTrue(TEXT("Dedicated menu sky is visible without collision"),
	         UI->EntrySky->IsVisible() && UI->EntrySky->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Start menu stays in view even with a hand-drawer preference"),
	         UI->PaletteRoot->GetAttachParent() == Camera && !UI->UsesHandPalette());
	TestFalse(TEXT("No Tools or back-to-tools escape on start menu"),
	          UI->VisiblePaletteEntries.ContainsByPredicate(
	              [](const auto& E)
	              { return E.Action == ECityPaletteAction::Back || E.Action == ECityPaletteAction::OpenBuild; }));
	Simulation->Tick(100.0f);
	TestEqual(TEXT("No hidden city advancement while in menu"), Simulation->GetEconomySnapshot().FixedStepId, Step);
	TestFalse(TEXT("Presentation suspension keeps the prior running preference"), Simulation->IsSimulationPaused());
	const auto MenuHorizon = UI->EntrySkyMaterial->K2_GetVectorParameterValue(TEXT("HorizonColor"));
	Environment->SetLightingPreset(ECityLightingPreset::Night);
	TestEqual(TEXT("City lighting does not change menu background"),
	          UI->EntrySkyMaterial->K2_GetVectorParameterValue(TEXT("HorizonColor")), MenuHorizon);
	UI->Cancel();
	TestTrue(TEXT("Back on main menu stays in sky scene"),
	         UI->bEntrySceneActive && UI->PalettePage == ECityPalettePage::MainMenu);
	UI->BeginManipulationSession();
	TestFalse(TEXT("Grabbing cannot reveal or manipulate the hidden map"), UI->bManipulationSession);
	UI->SwapHandedness(FCityToolInputRouter::OtherHand(UI->DominantHand));
	TestTrue(TEXT("Changing hands keeps the sky scene/menu"), UI->bEntrySceneActive && UI->bPaletteOpen);
	auto* LateScenery = World->SpawnActor<ACityBoardActor>();
	UI->UpdateEntryScenePresentation();
	TestTrue(TEXT("Late-created scenery is hidden too"), LateScenery->IsHidden());
	UI->SetPalettePage(ECityPalettePage::Settings);
	TestFalse(
	    TEXT("Start settings do not expose map tools"),
	    UI->VisiblePaletteEntries.ContainsByPredicate(
	        [](const auto& E)
	        { return E.Action == ECityPaletteAction::OpenBuild || E.Action == ECityPaletteAction::SelectDemolish; }));
	for (int32 I = 0; I < UI->VisiblePaletteEntries.Num(); ++I)
	{
		const auto Rect = UI->GetPaletteButtonRect(I, UI->VisiblePaletteEntries[I].Action);
		const auto Surface = FCityPaletteModel::SurfaceRect(UI->PalettePage, false);
		TestTrue(TEXT("Settings targets still fit after category-bar removal"),
		         Surface.Contains(Rect.Center + FVector(0, Rect.HalfSize.X, Rect.HalfSize.Y)));
	}
	UI->HandleApplicationBackgrounded();
	TestTrue(TEXT("Suspend retains sky and freezes hidden city"),
	         UI->bEntrySceneActive && Simulation->IsEntryMenuSuspended());
	TestFalse(TEXT("Suspended menu accepts no stale input"), UI->LifecycleInputGate.CanAcceptInput());
	UI->HandleApplicationForegrounded();
	TestTrue(TEXT("Resume reopens main menu over sky"),
	         UI->bPaletteOpen && UI->PalettePage == ECityPalettePage::MainMenu);
	UI->ReturnToCity();
	TestTrue(TEXT("Continue restores visible scenery and original hidden flags"),
	         !UI->Board->IsHidden() && !Environment->IsHidden() && !LateScenery->IsHidden() &&
	             OriginallyHidden->IsHidden());
	TestTrue(TEXT("HUD flags and sky restore"), !UI->CalendarText->bHiddenInGame && !UI->EntrySky->IsVisible());
	TestTrue(TEXT("Continue releases presentation suspension without pausing running city"),
	         !Simulation->IsEntryMenuSuspended() && !Simulation->IsSimulationPaused());
	Simulation->Tick(2.4f);
	TestTrue(TEXT("City clock advances after returning"), Simulation->GetEconomySnapshot().FixedStepId > Step);
	UI->RestoreDebugInfoDisplay();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDisasterMenuTest, "CitySim.Disasters.MenuDraftAndAuthority",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDisasterMenuTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityDisasterMenuTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	Sim->RegisterBuildingCatalog(LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog")));
	const auto Select = [&](const ECityPaletteAction Action)
	{
		const int32 Index =
		    UI->VisiblePaletteEntries.IndexOfByPredicate([&](const auto& E) { return E.Action == Action; });
		TestTrue(TEXT("Policy action is reachable"), Index != INDEX_NONE);
		UI->ExecutePaletteAction(Index);
	};
	UI->ShowNewCitySetup();
	Select(ECityPaletteAction::CycleDisasters);
	TestEqual(TEXT("New draft cycles to high"), UI->NewCityIdentity.DisasterFrequency, ECityDisasterFrequency::High);
	TestEqual(TEXT("Draft cannot change active policy"), Sim->GetCityIdentity().DisasterFrequency,
	          ECityDisasterFrequency::Medium);
	Select(ECityPaletteAction::DifficultyRelaxed);
	TestEqual(TEXT("Difficulty retains independent policy"), UI->NewCityIdentity.DisasterFrequency,
	          ECityDisasterFrequency::High);
	Select(ECityPaletteAction::CycleDisasters);
	UI->ReviewCityCreation();
	TestEqual(TEXT("Review retains chosen Off"), UI->NewCityIdentity.DisasterFrequency, ECityDisasterFrequency::Off);
	TestTrue(TEXT("Disaster policy leaves ordinary fire cadence intact"),
	         UI->NewCityDraft.Summary().Contains(
	             FString::Printf(TEXT("fire %d days"), UI->NewCityDraft.Tuning.FireCheckIntervalSteps)));
	TestTrue(TEXT("Confirmation exposes disaster policy"),
	         UI->GetEntryMenuHelp().Contains(TEXT("Natural disasters: OFF")));
	UI->Cancel();
	UI->Cancel();
	UI->ShowNewCitySetup();
	TestEqual(TEXT("Canceled draft discarded"), UI->NewCityIdentity.DisasterFrequency, ECityDisasterFrequency::Medium);
	UI->ReturnToCity();
	UI->SetPaletteOpen(true);
	UI->SetPalettePage(ECityPalettePage::Settings);
	Select(ECityPaletteAction::OpenDisasters);
	Select(ECityPaletteAction::DisastersOff);
	TestEqual(TEXT("Settings modify current city"), Sim->GetCityIdentity().DisasterFrequency,
	          ECityDisasterFrequency::Off);
	TestTrue(TEXT("Off selection shown"), UI->IsPaletteActionSelected(ECityPaletteAction::DisastersOff));
	TestFalse(TEXT("Invalid policy cannot alter authority"),
	          Sim->SetDisasterFrequency(static_cast<ECityDisasterFrequency>(4)));
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/DisasterPolicy/"),
	                                     FGuid::NewGuid().ToString() + TEXT(".json"));
	FString Error;
	TestTrue(TEXT("Current policy saves through subsystem"), Sim->SaveCityToPath(Path, Error));
	Sim->SetDisasterFrequency(ECityDisasterFrequency::High);
	TestTrue(TEXT("Actual load restores saved policy"), Sim->LoadCityFromPath(Path, Error));
	TestEqual(TEXT("Loaded policy retained"), Sim->GetCityIdentity().DisasterFrequency, ECityDisasterFrequency::Off);
	UI->SetPalettePage(ECityPalettePage::Disasters);
	Select(ECityPaletteAction::ReportDisasters);
	TestEqual(TEXT("Event history is reachable"), UI->PalettePage, ECityPalettePage::DisasterHistory);
	TestTrue(TEXT("No backdated disaster history"),
	         UI->ReportText->Text.ToString().Contains(TEXT("No natural disaster")));
	UI->PaletteAnchorMode = 2;
	TestFalse(TEXT("Event reports remain view readable in hand mode"), UI->UsesHandPalette());
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")));
	IFileManager::Get().Delete(*(Path + TEXT(".tmp")));
	World->DestroyWorld(false);
	return true;
}
#endif
