#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/CityConditionIndicators.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionModelTest, "CitySim.Indicators.ConditionsBoundsAndRay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionModelTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FCityBuildingDefinition Def;
	Def.DefinitionId = TEXT("Home");
	Def.DisplayName = TEXT("Home");
	Def.Footprint = {1, 1};
	Def.Cost = 1;
	FCityFireSnapshot Fire;
	Fire.LastFixedStepId = 10;
	FCityCrimeSnapshot Crime;
	Crime.LastFixedStepId = 10;
	TArray<uint64> Ids;
	for (int32 I = 0; I < 20; ++I)
	{
		auto P = City.BeginPreview(Def, {(I % 5) * 12, 2 + (I / 5) * 12});
		const auto Id = City.CommitPlacement(P, Def).EntityId;
		Ids.Add(Id);
		Fire.DamagedBuildings.Add(Id, {60, -1});
	}
	Fire.DamagedBuildings.FindChecked(Ids[1]).RepairCompleteStep = 15;
	Crime.RecentIncidents.Add({1, Ids[0], 9, 14, true});
	Crime.RecentIncidents.Add({2, Ids[0], 9, 12, true});
	Crime.RecentIncidents.Add({3, Ids[2], 1, 10, true});
	Crime.RecentIncidents.Add({4, Ids[3], 11, 14, true});
	Fire.RecentIncidents.Add({1, Ids[4], 0, 9, -1, 0, true});
	const int64 Funds = City.GetFunds();
	const uint64 Revision = City.GetRevision();
	auto Rows = FCityConditionIndicators::Build(City, Fire, Crime, 10);
	if (!TestEqual(TEXT("Badge pool stays bounded even with excess damage"), Rows.Num(),
	               FCityConditionIndicators::MaximumBadges))
		return false;
	TestEqual(TEXT("Active fire has first priority"), Rows[0].EntityId, Ids[4]);
	Rows.RemoveAt(0);
	TestEqual(TEXT("Active crime follows fire"), Rows[0].EntityId, Ids[0]);
	TestTrue(TEXT("Conditions consolidate on one lot"), Rows[0].bCrime && Rows[0].bNeedsRepair);
	TestEqual(TEXT("Consolidated crime uses actual remaining days"), Rows[0].CrimeDays, int64(4));
	const auto RepairRows = FCityConditionIndicators::Build(City, Fire, Crime, 10, nullptr, nullptr, 0,
	                                                        nullptr, nullptr, MAX_int32);
	const auto* RepairRow = RepairRows.FindByPredicate([&](const auto& R) { return R.EntityId == Ids[1]; });
	if (!TestNotNull(TEXT("In-progress repair retained after urgent faults"), RepairRow))
		return false;
	TestEqual(TEXT("Repair percent uses saved completion and duration"), RepairRow->RepairPercent, 50);
	TestEqual(TEXT("Repair time remains exact"), RepairRow->RepairDays, int64(5));
	TestTrue(TEXT("Symbols have distinct written labels"),
	         Rows[0].Title(42).Contains(TEXT("CRIME")) &&
	             RepairRow->Title(0).Contains(TEXT("REPAIRING 50%")));
	TestFalse(
	    TEXT("Burning damage does not offer premature repair"),
	    Rows.ContainsByPredicate([&](const auto& R) { return R.EntityId == Ids[4] && R.bNeedsRepair; }));
	TestFalse(TEXT("Expired/future incidents are not marked"), Rows.ContainsByPredicate([&](const auto& R) {
		return (R.EntityId == Ids[2] || R.EntityId == Ids[3]) && R.bCrime;
	}));
	Fire.LastFixedStepId = 15;
	Crime.LastFixedStepId = 14;
	Rows = FCityConditionIndicators::Build(City, Fire, Crime, 10);
	TestFalse(TEXT("Completed repair clears immediately"),
	          Rows.ContainsByPredicate([&](const auto& R) { return R.EntityId == Ids[1]; }));
	TestFalse(TEXT("Resolved crime clears immediately"),
	          Rows.ContainsByPredicate([](const auto& R) { return R.bCrime; }));
	City.DemolishEntity(Ids[0]);
	Rows = FCityConditionIndicators::Build(City, Fire, Crime, 10);
	TestFalse(TEXT("Demolished identity cannot leave a marker"),
	          Rows.ContainsByPredicate([&](const auto& R) { return R.EntityId == Ids[0]; }));
	TestEqual(TEXT("Presentation never spends money; one-dollar demolition rounds refund to zero"),
	          City.GetFunds(), Funds);
	TestEqual(TEXT("Only explicit demolition changed revision"), City.GetRevision(), Revision + 1);
	const FTransform Pose(FRotator(20, 35, 0), FVector(50, -20, 80), FVector(.6));
	const FVector Normal = Pose.GetRotation().GetForwardVector();
	double Distance = 0;
	TestTrue(TEXT("Facing scaled badge is selectable"),
	         FCityConditionIndicators::Hit(Pose, Pose.GetLocation() + Normal * 40, -Normal * 7, Distance));
	TestTrue(TEXT("Ray distance is normalized"), FMath::IsNearlyEqual(Distance, 40.0));
	TestFalse(TEXT("Badge gap passes through"),
	          FCityConditionIndicators::Hit(Pose, Pose.TransformPosition(FVector(0, 10, 0)) + Normal * 40,
	                                        -Normal, Distance));
	TestFalse(TEXT("Rear face cannot capture a selection"),
	          FCityConditionIndicators::Hit(Pose, Pose.GetLocation() - Normal * 40, Normal, Distance));
	TestFalse(TEXT("Parallel ray has no marker hit"),
	          FCityConditionIndicators::Hit(Pose, Pose.GetLocation() + Normal * 40,
	                                        Pose.GetRotation().GetUpVector(), Distance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityConditionPresentationTest,
                                 "CitySim.Indicators.PooledPresentationAndLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityConditionPresentationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ConditionPresentation"), nullptr, true,
	                                ERHIFeatureLevel::Num, &Init);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = W->SpawnActor<ACityBoardActor>();
	auto* UI = W->SpawnActor<ACityVRToolController>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->DispatchBeginPlay();
	UI->Board = Board;
	Board->SetActorTransform(FTransform(FRotator(0, 20, 0), FVector(-80, -40, 80), FVector(.004)));
	Board->SetToolMode(ECityToolMode::Inspect);
	auto* CameraOwner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(CameraOwner);
	CameraOwner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocationAndRotation(FVector(0, 0, 160), FRotator(-45, 0, 0));
	UI->HeadsetCamera = Camera;
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	const auto* Home = Board->BuildingCatalog->FindDefinition(TEXT("Home"));
	if (!TestNotNull(TEXT("Condition fixture home exists"), Home))
	{
		W->DestroyWorld(false);
		return false;
	}
	TArray<uint64> Ids;
	for (int32 X : {4, 7, 10})
	{
		auto P = City.BeginPreview(*Home, {X, 12});
		Ids.Add(City.CommitPlacement(P, *Home).EntityId);
	}
	Sim->Development.InitializeExistingAsOperational(City, Board->BuildingCatalog, 0);
	Sim->StartScenario();
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FireRepairSteps = 30;
	Sim->EconomyTuning.FireCheckIntervalSteps = 365;
	Sim->EconomyTuning.CrimePopulationThreshold = 100000;
	FCityFireSnapshot Fire;
	Fire.ActivationStep = Fire.GraceEndStep = 0;
	Fire.NextIgnitionStep = 10000;
	Fire.DamagedBuildings.Add(Ids[0], {60, -1});
	Fire.DamagedBuildings.Add(Ids[1], {40, 30});
	FString Error;
	TestTrue(TEXT("Authoritative damage fixture validates"), Sim->Fire.RestoreState(Fire, 0, Error));
	TestTrue(TEXT("Crime uses the real test command"), Sim->StartSafetyTest(Ids[0], false, true));
	TestTrue(TEXT("Second crime uses the real test command"), Sim->StartSafetyTest(Ids[2], false, true));
	Board->RebuildAllPresentation();
	// This fixture isolates incident and repair presentation from utility faults.
	Sim->ServiceSnapshot.EntityFailures.Reset();
	UI->UpdateConditionIndicators();
	if (!TestEqual(TEXT("Conditions consolidate to three badges"), UI->ConditionIndicators.Num(), 3))
	{
		W->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Component pool is fixed"), UI->ConditionRoots.Num(),
	          FCityConditionIndicators::MaximumBadges);
	for (int32 I = 0; I < 3; ++I)
	{
		TestTrue(TEXT("Active badge renders its background, symbol and label"),
		         UI->ConditionBackgrounds[I]->IsVisible() && UI->ConditionIcons[I]->IsVisible() &&
		             UI->ConditionTitles[I]->IsVisible());
		TestTrue(TEXT("Active badge faces camera"),
		         FVector::DotProduct(
		             (Camera->GetComponentLocation() - UI->ConditionRoots[I]->GetComponentLocation())
		                 .GetSafeNormal(),
		             UI->ConditionRoots[I]->GetForwardVector()) > .999);
		TestEqual(TEXT("Badge cannot block physics/tool picking"),
		          UI->ConditionBackgrounds[I]->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(
		    TEXT("Badge and icon remain unlit at night"),
		    UI->ConditionBackgrounds[I]->GetMaterial(0)->GetMaterial()->GetShadingModels().HasShadingModel(
		        MSM_Unlit) &&
		        UI->ConditionIcons[I]->GetMaterial(0)->GetMaterial()->GetShadingModels().HasShadingModel(
		            MSM_Unlit));
		const auto Bounds = UI->ConditionBackgrounds[I]
		                        ->CalcBounds(UI->ConditionBackgrounds[I]->GetRelativeTransform())
		                        .GetBox();
		for (auto* Text : {UI->ConditionTitles[I].Get(), UI->ConditionDetails[I].Get()})
		{
			const auto TextBounds = Text->CalcBounds(Text->GetRelativeTransform()).GetBox();
			TestTrue(TEXT("Label fits badge background"),
			         TextBounds.Min.Y >= Bounds.Min.Y && TextBounds.Max.Y <= Bounds.Max.Y &&
			             TextBounds.Min.Z >= Bounds.Min.Z && TextBounds.Max.Z <= Bounds.Max.Z);
		}
	}
	FString Before, After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Before, Error);
	const FVector Normal = UI->ConditionRoots[0]->GetForwardVector(),
	              Origin = UI->ConditionRoots[0]->GetComponentLocation() + Normal * 30;
	double Distance = 0;
	TestEqual(TEXT("Actual marker ray resolves displayed lot"),
	          UI->FindConditionIndicator(Origin, -Normal, Distance), 0);
	UI->ToggleInspection(Origin, -Normal);
	TestEqual(TEXT("Marker trigger opens its building inspection"), UI->InspectedEntityId, Ids[0]);
	TestTrue(TEXT("Crime inspection explains active event and action"),
	         UI->LastInspectionText.Contains(TEXT("Active crime")) &&
	             UI->LastInspectionText.Contains(TEXT("POLICE")));
	UI->ClearInspection();
	UI->UpdateConditionIndicators();
	UI->SetConditionIndicatorsVisible(false);
	TestTrue(TEXT("Hidden setting clears marker hits"), UI->ConditionIndicators.IsEmpty());
	UI->SetConditionIndicatorsVisible(true);
	UI->SetPalettePage(ECityPalettePage::Settings);
	int32 Toggle = UI->VisiblePaletteEntries.IndexOfByPredicate(
	    [](const auto& E) { return E.Action == ECityPaletteAction::ToggleConditionIndicators; });
	TestTrue(TEXT("Settings provides marker visibility"), Toggle != INDEX_NONE);
	if (Toggle != INDEX_NONE)
	{
		UI->ExecutePaletteAction(Toggle);
		TestFalse(TEXT("Settings action hides badges"), UI->bConditionIndicatorsVisible);
		UI->ExecutePaletteAction(Toggle);
	}
	Board->SetToolMode(ECityToolMode::Home);
	UI->UpdateConditionIndicators();
	TestFalse(TEXT("Conditions remain visible while building"), UI->ConditionIndicators.IsEmpty());
	Board->SetToolMode(ECityToolMode::Inspect);
	UI->bManipulationSession = true;
	UI->UpdateConditionIndicators();
	TestTrue(TEXT("Manipulation hides badges"), UI->ConditionIndicators.IsEmpty());
	UI->bManipulationSession = false;
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::Crime);
	UI->UpdateConditionIndicators();
	TestTrue(TEXT("Risk overlay is separate from incidents"), UI->ConditionIndicators.IsEmpty());
	Board->SetDiagnosticOverlay(ECityDiagnosticOverlay::None);
	Sim->SetSimulationPaused(true);
	Sim->Tick(5);
	UI->UpdateConditionIndicators();
	TestEqual(TEXT("Pause retains current crime days"), UI->ConditionIndicators[0].CrimeDays, int64(5));
	Sim->SetSimulationPaused(false);
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Visibility/inspection never changes city authority"), After, Before);
	Sim->SetSimulationPaused(false);
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ConditionIndicators-Indicators.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("Indicator city saves through normal validation"), Sim->SaveCityToPath(Path, Error));
	const FString Preview =
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/ConditionIndicators-Indicator-City.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Preview), true);
	FFileHelper::SaveStringToFile(Before, *Preview);
	for (int32 Speed : {1, 3, 5})
	{
		TestTrue(TEXT("Indicator city reloads"), Sim->LoadCityFromPath(Path, Error, false));
		UI->UpdateConditionIndicators();
		TestTrue(TEXT("Load suppresses stale board identities until geometry refresh"),
		         UI->ConditionIndicators.IsEmpty());
		Board->PlacementModel = &Sim->GetPlacementModel();
		Board->RebuildAllPresentation();
		Sim->SetSimulationSpeed(Speed);
		Sim->Tick(1.875f / Speed);
		UI->UpdateConditionIndicators();
		TestFalse(TEXT("Crime clears at the same step at every speed"),
		          UI->ConditionIndicators.ContainsByPredicate([](const auto& R) { return R.bCrime; }));
		const auto* Row =
		    UI->ConditionIndicators.FindByPredicate([&](const auto& R) { return R.EntityId == Ids[1]; });
		if (TestNotNull(TEXT("Repair still underway"), Row))
			TestEqual(TEXT("Exact progress after fifteen days"), Row->RepairPercent, 50);
	}
	TestTrue(TEXT("Repair-needed action uses normal authority"), Sim->RepairBuilding(Ids[0]));
	UI->UpdateConditionIndicators();
	TestTrue(TEXT("Repair changes the badge to underway"),
	         UI->ConditionIndicators.ContainsByPredicate(
	             [&](const auto& R) { return R.EntityId == Ids[0] && R.bRepairing; }));
	Sim->GetPlacementModel().DemolishEntity(Ids[0]);
	UI->UpdateConditionIndicators();
	TestFalse(
	    TEXT("Demolition immediately drops the badge"),
	    UI->ConditionIndicators.ContainsByPredicate([&](const auto& R) { return R.EntityId == Ids[0]; }));
	Sim->SetSimulationSpeed(1);
	Sim->Tick(1.875f);
	UI->UpdateConditionIndicators();
	TestFalse(TEXT("Repair completion leaves no damage badge"),
	          UI->ConditionIndicators.ContainsByPredicate(
	              [](const auto& R) { return R.bNeedsRepair || R.bRepairing || R.bCrime; }));
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	W->DestroyWorld(false);
	return true;
}
#endif
