#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Commands/CityPlacementModel.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityVRToolController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace
{
uint64 StationPlace(FCityPlacementModel& City, const UCityBuildingCatalog* Catalog, FName Name,
                    FCityCellCoord Cell)
{
	const auto* D = Catalog->FindDefinition(Name);
	if (!D)
		return 0;
	auto P = City.BeginPreview(*D, Cell);
	return City.CommitPlacement(P, *D).EntityId;
}
void StationRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {63, 8});
	City.CommitRoad(P);
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStationAuthorityTest, "CitySim.Stations.PurchaseCompletionAndGates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStationAuthorityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	for (const FName Name : {FName(TEXT("Police")), FName(TEXT("Fire"))})
	{
		const auto* Def = Catalog->FindDefinition(Name);
		if (!TestNotNull(TEXT("Station"), Def))
			return false;
		TestTrue(TEXT("Valid explicit stages"), Def->IsValid() && Def->bPlayerPurchasedLevels);
		TestTrue(TEXT("Reserved3x2 lot"), Def->Footprint.Width == 3 && Def->Footprint.Height == 2);
		FCityPlacementModel City(100000);
		StationRoad(City);
		const uint64 Id = StationPlace(City, Catalog, Name, {8, 9});
		const auto Original = City.GetEntities().FindChecked(Id);
		FCityDevelopmentSimulation Dev;
		Dev.InitializeExistingAsOperational(City, Catalog, 0);
		FCityServiceSnapshot S;
		FCityNeedsSnapshot N;
		FCityEnvironmentSnapshot E;
		FCityEconomyTuning T;
		FString Error;
		S.EntityFailures.Add(Id, ECityOperationFailure::NoStaff);
		TestFalse(TEXT("Unstaffed purchase blocked"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		S.EntityFailures.Reset();
		const int64 Before = City.GetFunds();
		const auto* Medium = Def->FindDensityLevel(ECityDensityLevel::Medium);
		TestTrue(TEXT("Purchase starts expansion"),
		         Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestEqual(TEXT("Exact charge"), Before - City.GetFunds(), Medium->UpgradeCost);
		auto Future = Dev.GetSnapshot();
		Future.States.FindChecked(Id).CivicUpgradeCompleteStep = MAX_int64;
		FCityDevelopmentSimulation Invalid;
		TestFalse(TEXT("Impossible future deadline rejected"),
		          Invalid.RestoreState(Future, City, Catalog, 0, Error));
		TestFalse(TEXT("No repeated charge"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		TestEqual(TEXT("Still starter"), Dev.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::Low);
		TestTrue(TEXT("Old station keeps operating"), Dev.GetSnapshot().IsOperational(Id));
		Dev.AdvanceStationsToHalfDay(0, S);
		TestEqual(TEXT("No early benefit"), Dev.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::Low);
		S.EntityFailures.Add(Id, ECityOperationFailure::FireDamage);
		Dev.AdvanceStationsToHalfDay(1, S);
		TestTrue(TEXT("Damage holds completion"), Dev.GetSnapshot().Find(Id)->HasCivicUpgrade());
		FCityDevelopmentSimulation Restored;
		TestTrue(TEXT("Overdue blocked expansion restores"),
		         Restored.RestoreState(Dev.GetSnapshot(), City, Catalog, 30, Error));
		S.EntityFailures.Reset();
		Dev.AdvanceStationsToHalfDay(1, S);
		TestEqual(TEXT("Completed stage"), Dev.GetSnapshot().Find(Id)->DensityLevel,
		          ECityDensityLevel::Medium);
		const auto& Entity = City.GetEntities().FindChecked(Id);
		TestTrue(TEXT("Identity, footprint, rotation, cost unchanged"),
		         Entity.EntityId == Original.EntityId && Entity.DefinitionId == Original.DefinitionId &&
		             Entity.Anchor == Original.Anchor && Entity.FootprintSize == Original.FootprintSize &&
		             Entity.QuarterTurnsClockwise == Original.QuarterTurnsClockwise &&
		             Entity.RecordedPlacementCost == Original.RecordedPlacementCost);
		TestFalse(TEXT("Stale level rejected"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Low, City, Catalog, S, Error));
		City.TrySpendFunds(City.GetFunds());
		TestFalse(TEXT("Funds gate"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Medium, City, Catalog, S, Error));
		City.ApplyMonthlyEconomy(100000, 0);
		TestTrue(TEXT("Second paid stage"),
		         Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::Medium, City, Catalog, S, Error));
		Dev.AdvanceToStep(200, City, Catalog, S, N, E, T);
		TestEqual(TEXT("High stage"), Dev.GetSnapshot().Find(Id)->DensityLevel, ECityDensityLevel::High);
		TestFalse(TEXT("Maximum cannot charge"),
		          Dev.PurchaseCivicUpgrade(Id, ECityDensityLevel::High, City, Catalog, S, Error));
		auto Bad = Dev.GetSnapshot();
		Bad.States.FindChecked(Id).PendingCivicLevel = 1;
		Bad.States.FindChecked(Id).CivicUpgradeCompleteStep = 230;
		TestFalse(TEXT("Corrupt downgrade rejected"), Restored.RestoreState(Bad, City, Catalog, 200, Error));
		for (const auto& L : Def->DensityLevels)
		{
			auto* Mesh = L.PresentationMesh.LoadSynchronous();
			if (TestNotNull(TEXT("Stage mesh"), Mesh))
				TestTrue(TEXT("Reviewed opaque windows and civic surfaces stay bounded"),
				         Mesh->GetNumSections(0) == 5 && Mesh->GetNumTriangles(0) <= 1604);
			const FString Package =
			    L.PresentationMesh.ToSoftObjectPath().GetLongPackageName() + TEXT("_Damaged");
			TestNotNull(
			    TEXT("Damaged variant"),
			    LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") + FPaths::GetBaseFilename(Package))));
		}
		City.DemolishEntity(Id);
		Dev.Reconcile(City, Catalog);
		TestTrue(TEXT("Demolition clears stages"), Dev.GetSnapshot().States.IsEmpty());
		const uint64 Starter = StationPlace(City, Catalog, Name, {8, 9});
		Dev.Reconcile(City, Catalog, 400);
		TestEqual(TEXT("New station construction half-day deadline"),
		          Dev.GetSnapshot().Find(Starter)->StationCompleteHalfDay, int64(401));
		Dev.AdvanceStationsToHalfDay(400, S);
		TestFalse(TEXT("New station has no early operation"), Dev.GetSnapshot().IsOperational(Starter));
		Dev.AdvanceStationsToHalfDay(401, S);
		TestTrue(TEXT("New station completes within half a day"), Dev.GetSnapshot().IsOperational(Starter));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStationFleetTest, "CitySim.Stations.StaffedFleetAndCoverage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStationFleetTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	FCityPlacementModel City(100000);
	StationRoad(City);
	const uint64 Fire = StationPlace(City, Catalog, TEXT("Fire"), {8, 9}),
	             Police = StationPlace(City, Catalog, TEXT("Police"), {13, 9});
	TArray<uint64> Homes;
	for (int32 X : {20, 28, 40})
		Homes.Add(StationPlace(City, Catalog, TEXT("Home"), {X, 9}));
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityFireSnapshot Fires;
	for (int32 I = 0; I < 3; ++I)
		Fires.RecentIncidents.Add({I + 1, Homes[I], 0, 0, -1, 0});
	FCityCrimeSnapshot Crime;
	Crime.RecentIncidents.Add({1, Homes[0], 0, 5});
	Crime.RecentIncidents.Add({2, Homes[1], 0, 5});
	FCityEconomyTuning T;
	FCityDevelopmentSnapshot D;
	FCityPopulationSnapshot Empty;
	for (int32 Stage = 0; Stage < 3; ++Stage)
	{
		for (uint64 Id : {Fire, Police})
		{
			FCityDevelopmentState S;
			S.EntityId = Id;
			S.DensityLevel = static_cast<ECityDensityLevel>(Stage);
			D.States.Add(Id, S);
		}
		auto S = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, nullptr, &Empty);
		TestEqual(TEXT("Authored upkeep"), S.CivicUpkeep, int64(Stage == 0 ? 55 : Stage == 1 ? 85 : 125));
		TestEqual(TEXT("Completed range"), S.StationRoadRanges.FindRef(Fire), 24 + Stage * 8);
		TestEqual(TEXT("No utilities closes jobs"), S.PublicJobs.FindRef(Fire), 0);
		S.EntityFailures.Reset();
		S.bUsesPublicStaffing = true;
		for (uint64 Id : {Fire, Police})
		{
			S.PublicJobs.Add(Id, 4 * (Stage + 1));
			S.PublicStaffPercent.Add(Id, 100);
		}
		FCityPopulationSimulation P;
		P.UpdateEmergencyResponses(City, Catalog, Network, Fires, Crime, S, 1, false);
		const int32 F = P.GetSnapshot()
		                    .TransitTrips
		                    .FilterByPredicate([](const auto& Trip) {
			                    return Trip.Purpose == ECityTripPurpose::FireResponse;
		                    })
		                    .Num(),
		            C = P.GetSnapshot().TransitTrips.Num() - F;
		TestEqual(TEXT("Staffed engine capacity"), F, Stage + 1);
		TestEqual(TEXT("Police incident capacity"), C, FMath::Min(2, Stage + 1));
		P.UpdateEmergencyResponses(City, Catalog, Network, Fires, Crime, S, 1, false);
		TestEqual(TEXT("No duplicate dispatch"), P.GetSnapshot().TransitTrips.Num(), F + C);
		S.PublicStaffPercent.Add(Fire, 50);
		FCityPopulationSimulation Half;
		Half.UpdateEmergencyResponses(City, Catalog, Network, Fires, {}, S, 1, false);
		TestEqual(TEXT("Partial staffing scales fleet"), Half.GetSnapshot().TransitTrips.Num(),
		          FMath::Max(1, (Stage + 1) / 2));
		FCityPopulationSimulation Unpaid;
		Unpaid.UpdateEmergencyResponses(City, Catalog, Network, Fires, Crime, S, 1, true);
		TestTrue(TEXT("Bills gate dispatch"), Unpaid.GetSnapshot().TransitTrips.IsEmpty());
		S.PublicStaffPercent.Add(Fire, 0);
		FCityPopulationSimulation NoStaff;
		NoStaff.UpdateEmergencyResponses(City, Catalog, Network, Fires, {}, S, 1, false);
		TestTrue(TEXT("Parked meshes give no response"), NoStaff.GetSnapshot().TransitTrips.IsEmpty());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStationUpgradeTest, "CitySim.Stations.SelectionSaveAndSpeed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStationUpgradeTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("StationUpgrade"), nullptr, true,
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
	auto* Owner = W->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocationAndRotation(FVector(0, 0, 160), FRotator(-45, 0, 0));
	UI->HeadsetCamera = Camera;
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(200000, 0);
	StationRoad(City);
	StationPlace(City, Board->BuildingCatalog, TEXT("Power"), {1, 9});
	StationPlace(City, Board->BuildingCatalog, TEXT("Service"), {25, 9});
	for (int32 X : {4, 12, 16})
		StationPlace(City, Board->BuildingCatalog, TEXT("Home"), {X, 9});
	const uint64 Police = StationPlace(City, Board->BuildingCatalog, TEXT("Police"), {8, 9}),
	             Fire = StationPlace(City, Board->BuildingCatalog, TEXT("Fire"), {20, 9});
	Sim->Development.InitializeExistingAsOperational(City, Board->BuildingCatalog, 0);
	Sim->StartScenario();
	Sim->Tick(.5f);
	Board->PlacementModel = &Sim->GetPlacementModel();
	Board->RebuildAllPresentation();
	TestEqual(TEXT("Basic starter staffing"), Sim->GetServiceSnapshot().PublicStaffPercent.FindRef(Police),
	          100);
	FVector Roof;
	FString Detail;
	Board->GetInspectionBuilding(Police, Roof, Detail);
	const FVector Origin = Roof + FVector(0, 0, 20);
	const int64 Before = City.GetFunds();
	UI->ToggleInspection(Origin, -FVector::UpVector);
	TestTrue(TEXT("Inspect offers priced upgrade"),
	         UI->InspectorUpgradeLabel->Text.ToString().Contains(TEXT("$800")));
	const auto ClickUpgrade = [&]() {
		const FVector Target = UI->InspectorUpgradeButton->GetComponentLocation();
		const FVector Normal = UI->InspectorStatusRoot->GetForwardVector();
		return UI->HandleInspectorUpgradeRay(Target + Normal * 10, -Normal);
	};
	const FVector PanelNormal = UI->InspectorStatusRoot->GetForwardVector();
	TestTrue(TEXT("Panel body captures clicks"),
	         UI->HandleInspectorUpgradeRay(UI->InspectorStatusRoot->GetComponentLocation() + PanelNormal * 10,
	                                       -PanelNormal));
	TestEqual(TEXT("Panel body cannot charge or select map"), City.GetFunds(), Before);
	ClickUpgrade();
	TestEqual(TEXT("Review has no charge"), City.GetFunds(), Before);
	TestTrue(TEXT("Exact review text"), UI->LastInspectionText.Contains(TEXT("0.5 game day")) &&
	                                        UI->LastInspectionText.Contains(TEXT("$800")) &&
	                                        UI->LastInspectionText.Contains(TEXT("32 road hops")));
	City.TrySpendFunds(City.GetFunds());
	ClickUpgrade();
	TestEqual(TEXT("Unaffordable confirmation retains review"), UI->StationUpgradePreviewEntity, Police);
	TestTrue(TEXT("Rejected confirmation explains funds"),
	         UI->LastInspectionText.Contains(TEXT("cannot afford")));
	City.ApplyMonthlyEconomy(Before, 0);
	UI->SetPaletteOpen(true);
	TestEqual(TEXT("Drawer cancels confirmation"), UI->StationUpgradePreviewEntity, uint64(0));
	UI->SetPaletteOpen(false);
	UI->UpdateInspectionTooltip();
	ClickUpgrade();
	TestEqual(TEXT("Fresh review after cancel has no charge"), City.GetFunds(), Before);
	ClickUpgrade();
	TestEqual(TEXT("Confirmation pays"), Before - City.GetFunds(), int64(800));
	TestTrue(TEXT("Purchase saves pending stage"),
	         Sim->GetDevelopmentSnapshot().Find(Police)->HasCivicUpgrade());
	Board->RebuildLiveConstructionAnimations();
	TestTrue(TEXT("Shared construction pool"), Board->AnimatedConstructionEntityIds.Contains(Police));
	TestEqual(TEXT("Current vacancies until completion"),
	          Sim->GetServiceSnapshot().PublicJobs.FindRef(Police), 4);
	Sim->SetSimulationPaused(true);
	Sim->Tick(10);
	TestEqual(TEXT("Pause freezes stage"), Sim->GetDevelopmentSnapshot().Find(Police)->DensityLevel,
	          ECityDensityLevel::Low);
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/StationUpgrades-Upgrades.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("Native pending save"), Sim->SaveCityToPath(Path, Error));
	FString Fixture;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fixture, Error);
	auto MissingDeadline = Sim->BuildSaveDocument();
	MissingDeadline.Development.States.FindChecked(Police).StationCompleteHalfDay = -1;
	FString Broken;
	TestTrue(TEXT("Missing deadline fixture writes"),
	         FCityPersistence::Serialize(MissingDeadline, Broken, Error));
	const FString RejectPath = Path + TEXT(".missing-deadline");
	FFileHelper::SaveStringToFile(Broken, *RejectPath);
	TestFalse(TEXT("Current station never receives an invented deadline on load"),
	          Sim->LoadCityFromPath(RejectPath, Error, false));
	FString Unchanged;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Unchanged, Error);
	TestEqual(TEXT("Rejected station state preserves every authority"), Unchanged, Fixture);
    // FixedStepsPerMonth is now the daily fine-clock interval. A half day
    // spans half those steps, rather than half of one fixed update.
    const float HalfDaySeconds=static_cast<float>(Sim->EconomyTuning.FixedStepSeconds*Sim->EconomyTuning.FixedStepsPerMonth*.5);
	for (int32 Speed : {1, 3, 5})
	{
		TestTrue(TEXT("Reload pending expansion"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		Sim->SetSimulationPaused(false);
		Sim->Tick(HalfDaySeconds*.49f / Speed);
		TestEqual(TEXT("No early half-day completion"),
		          Sim->GetDevelopmentSnapshot().Find(Police)->DensityLevel, ECityDensityLevel::Low);
		Sim->Tick((HalfDaySeconds*.51f+.0001f) / Speed);
		TestEqual(TEXT("Half-day completion all speeds"),
		          Sim->GetDevelopmentSnapshot().Find(Police)->DensityLevel, ECityDensityLevel::Medium);
		TestFalse(TEXT("Deadline cleared"), Sim->GetDevelopmentSnapshot().Find(Police)->HasCivicUpgrade());
		TestEqual(TEXT("Completed vacancies"), Sim->GetServiceSnapshot().PublicJobs.FindRef(Police), 8);
		// Completion now lands on a fine update, which also reconciles hiring.
        TestEqual(TEXT("Completed vacancies hire on the same fine update"),Sim->GetPopulationSnapshot().Workers(Police),8);
        TestEqual(TEXT("Completed station has actual paid staff"),
                  Sim->GetServiceSnapshot().PublicStaffPercent.FindRef(Police),100);
		Sim->Tick((HalfDaySeconds-.0001f) / Speed);
		TestTrue(TEXT("Payroll reconciles"),
		         Sim->Population.MatchesEconomy(Sim->GetEconomySnapshot(), Sim->EconomyTuning));
	}
	TestTrue(TEXT("Completed stage save/reload"),
	         Sim->SaveCityToPath(Path, Error) && Sim->LoadCityFromPath(Path, Error, false));
	TestEqual(TEXT("Completed paid stage retained"), Sim->GetDevelopmentSnapshot().Find(Police)->DensityLevel,
	          ECityDensityLevel::Medium);
	TestTrue(TEXT("Save baseline for new construction"), Sim->SaveCityToPath(Path, Error));
	const FString PartialPath = Path + TEXT(".partial");
	for (int32 Speed : {1, 3, 5})
	{
		TestTrue(TEXT("Restore baseline before placement"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		const uint64 NewStation =
		    StationPlace(Sim->GetPlacementModel(), Board->BuildingCatalog, TEXT("Fire"), {32, 9});
		Sim->SetSimulationPaused(true);
		Sim->Tick(0);
		TestFalse(TEXT("Paused placement stays under construction"),
		          Sim->GetDevelopmentSnapshot().IsOperational(NewStation));
		Sim->SetSimulationPaused(false);
		Sim->Tick(HalfDaySeconds*.49f / Speed);
		TestFalse(TEXT("Initial station no early benefits"),
		          Sim->GetDevelopmentSnapshot().IsOperational(NewStation));
		TestTrue(TEXT("Save partial initial construction"), Sim->SaveCityToPath(PartialPath, Error));
		TestTrue(TEXT("Restore partial initial construction"),
		         Sim->LoadCityFromPath(PartialPath, Error, false));
		Sim->Tick((HalfDaySeconds*.51f+.0001f) / Speed);
		TestTrue(TEXT("Initial construction completes by half day at every speed"),
		         Sim->GetDevelopmentSnapshot().IsOperational(NewStation));
	}
	TestTrue(TEXT("Restore utility confirmation baseline"), Sim->LoadCityFromPath(Path, Error, false));
	Sim->SetSimulationPaused(true);
	Board->PlacementModel = &Sim->GetPlacementModel();
	Board->RebuildAllPresentation();
	for (const FName UtilityId : {FName(TEXT("Service")), FName(TEXT("Power"))})
	{
		uint64 Utility = 0;
		for (const auto& Pair : Sim->GetPlacementModel().GetEntities())
		{
			if (Pair.Value.DefinitionId == UtilityId)
			{
				Utility = Pair.Key;
				break;
			}
		}
		TestTrue(TEXT("Utility available for Inspect"), Utility != 0);
		UI->InspectBuilding(Utility);
		const int64 UtilityFunds = Sim->GetPlacementModel().GetFunds();
		TestTrue(TEXT("Utility upgrade review ray accepted"), ClickUpgrade());
		TestEqual(TEXT("Utility review does not charge"), Sim->GetPlacementModel().GetFunds(), UtilityFunds);
		TestTrue(TEXT("Utility confirmation ray accepted"), ClickUpgrade());
		TestTrue(TEXT("Utility confirmation starts pending expansion"),
		         Sim->GetDevelopmentSnapshot().Find(Utility)->HasCivicUpgrade());
		TestEqual(TEXT("Utility confirmation charges once"),
		          UtilityFunds - Sim->GetPlacementModel().GetFunds(),
		          int64(UtilityId == TEXT("Service") ? 450 : 600));
		Board->RebuildAllPresentation();
		for (int32 Frame = 0; Frame < 4; ++Frame)
			Board->UpdateLiveConstructionAnimations(.016f);
		TestFalse(TEXT("Utility without construction mesh excluded from crane pool"),
		          Board->AnimatedConstructionEntityIds.Contains(Utility));
	}
	IFileManager::Get().Delete(*PartialPath, false, true, true);
	IFileManager::Get().Delete(*(PartialPath + TEXT(".bak")), false, true, true);
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	W->DestroyWorld(false);
	return true;
}
#endif
