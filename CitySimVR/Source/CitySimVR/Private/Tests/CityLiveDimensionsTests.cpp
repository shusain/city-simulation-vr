#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityBoardActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLiveDimensionsTest, "CitySim.Dimensions.LiveCityLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLiveDimensionsTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LiveCityDimensions"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->bDesktopUi = true;
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
	const auto Invoke = [&](ECityPaletteAction Action) { UI->ExecuteSharedPaletteAction(Action); };
	const auto CheckDimensions = [&](int32 Size) {
		FString Error;
		const auto Document = Simulation->BuildSaveDocument();
		TestTrue(TEXT("every live authority agrees before saving"), Document.ValidateDimensions(Error));
		TestEqual(TEXT("live city width"), Document.GetDimensions().GetWidth(), Size);
		TestEqual(TEXT("environment storage follows city"), Document.Environment.Pollution.Num(),
		          Size * Size);
		TestEqual(TEXT("crime storage follows city"), Document.Crime.CellRisk.Num(), Size * Size);
		TestEqual(TEXT("fire storage follows city"), Document.Fire.CellRisk.Num(), Size * Size);
		TestEqual(TEXT("board follows active city"), UI->Board->GetDimensions().GetWidth(), Size);
	};
	UI->ShowNewCitySetup();
	TestEqual(TEXT("Desktop defaults to64"), UI->NewCityTerrain.Dimensions.GetWidth(), 64);
	Invoke(ECityPaletteAction::CycleMapSize);
	TestEqual(TEXT("shared action selects80"), UI->NewCityTerrain.Dimensions.GetWidth(), 80);
	TestTrue(TEXT("setup states Desktop restriction"), UI->GetEntryMenuHelp().Contains(TEXT("Desktop only")));
	Invoke(ECityPaletteAction::DifficultyChallenging);
	Invoke(ECityPaletteAction::RerollTerrain);
	TestEqual(TEXT("difficulty and reroll retain chosen size"), UI->NewCityTerrain.Dimensions.GetWidth(), 80);
	Invoke(ECityPaletteAction::ReviewCreate);
	TestTrue(TEXT("review states dimensions"), UI->GetEntryMenuHelp().Contains(TEXT("80 x 80")));
	UI->Cancel();
	TestEqual(TEXT("cancel review retains size"), UI->NewCityTerrain.Dimensions.GetWidth(), 80);
	Invoke(ECityPaletteAction::DifficultyNormal);
	// Flat terrain isolates the lifecycle/edge behavior from randomly unbuildable land.
	UI->NewCityTerrain = FCityBenchmark::CapacityTerrainParameters();
	UI->NewCityTerrain.Dimensions = FCityGridDimensions(80, 80);
	Invoke(ECityPaletteAction::ReviewCreate);
	Invoke(ECityPaletteAction::ConfirmCreate);
	if (!TestEqual(TEXT("real reviewed creation commits80"),
	               Simulation->GetTerrain().GetDimensions().GetWidth(), 80))
	{
		AddError(UI->GetEntryMenuHelp());
		World->DestroyWorld(false);
		return false;
	}
	CheckDimensions(80);
	const auto Identity = Simulation->GetCityIdentity();
	const auto LargePath = FCitySaveLibrary::PathFor(FCitySaveLibrary::Directory(), Identity.Id);
	auto& City = Simulation->GetPlacementModel();
	auto Road = City.BeginRoadPreview({0, 2});
	City.UpdateRoadPreviewEnd(Road, {79, 2});
	TestTrue(TEXT("road reaches beyond old boundary"), City.CommitRoad(Road).IsSuccess());
	Road = City.BeginRoadPreview(Simulation->GetTerrain().GetParameters().GetEntranceCell());
	City.UpdateRoadPreviewEnd(Road, {Simulation->GetTerrain().GetParameters().GetEntranceCell().X, 2});
	TestTrue(TEXT("road joins actual80 entrance"), City.CommitRoad(Road).IsSuccess());
	TestTrue(TEXT("far corner zoning"), City.SetZoneArea({78, 78}, {2, 2}, ECityZoneType::Industrial));
	const auto* Power = Catalog->FindDefinition(TEXT("Power"));
	auto PowerPreview = City.BeginPreview(*Power, {70, 3});
	const auto Placed = City.CommitPlacement(PowerPreview, *Power);
	TestTrue(TEXT("civic lot beyond64"), Placed.IsSuccess());
	auto Raised = City.BeginRoadPreview({65, 30});
	Raised.End = {79, 30};
	Raised.Profile = ECityRoadProfile::Elevated;
	TestTrue(TEXT("raised span beyond64"), City.CommitRoad(Raised).IsSuccess());
	auto Crossing = City.BeginRoadPreview({71, 26});
	Crossing.End = {71, 34};
	Crossing.Profile = ECityRoadProfile::Ground;
	TestTrue(TEXT("ground crossing beyond64"), City.CommitRoad(Crossing).IsSuccess());
	Simulation->RefreshDerivedSnapshots();
	FString Error;
	if (!TestTrue(TEXT("edited80 saves"), Simulation->SaveCityToPath(LargePath, Error)))
		AddError(Error);
	FString Before;
	FFileHelper::LoadFileToString(Before, *LargePath);
	Simulation->RequestResetScenario();
	Simulation->RequestResetScenario();
	CheckDimensions(80);
	TestTrue(TEXT("reset clears city occupants"), Simulation->GetPlacementModel().GetEntities().IsEmpty());
	TestEqual(TEXT("reset keeps city identity"), Simulation->GetCityIdentity().Id, Identity.Id);
	TestTrue(TEXT("reset80 is immediately saveable"),
	         Simulation->SaveCityToPath(LargePath + TEXT(".reset"), Error));
	TestTrue(TEXT("saved80 reloads after reset"), Simulation->LoadCityFromPath(LargePath, Error, false));
	CheckDimensions(80);
	TestEqual(TEXT("far lot survives reload"), Simulation->GetPlacementModel().GetGrid().GetOccupant({70, 3}),
	          Placed.EntityId);
	TestTrue(TEXT("both road levels survive reload"),
	         Simulation->GetPlacementModel().IsRoad({71, 30}) &&
	             Simulation->GetPlacementModel().IsRoad({71, 30, 1}));
	TestTrue(TEXT("far zoning survives reload"),
	         Simulation->GetPlacementModel().GetZone({79, 79}) == ECityZoneType::Industrial);
	UI->ShowNewCitySetup();
	TestEqual(TEXT("new proposal restores64 default"), UI->NewCityTerrain.Dimensions.GetWidth(), 64);
	Invoke(ECityPaletteAction::ReviewCreate);
	Invoke(ECityPaletteAction::ConfirmCreate);
	CheckDimensions(64);
	TestTrue(TEXT("switch64 back to80"), Simulation->LoadCityFromPath(LargePath, Error, false));
	UI->Board->PrepareCityView();
	CheckDimensions(80);
	TestTrue(TEXT("mixed library indexes80"),
	         FCitySaveLibrary::List(FCitySaveLibrary::Directory()).ContainsByPredicate([&](const auto& Slot) {
		         return Slot.Identity.Id == Identity.Id && Slot.Dimensions.GetWidth() == 80;
	         }));
	UI->ShowMainMenu();
	UI->PalettePage = ECityPalettePage::LoadReview;
	UI->ReviewedCitySlots = FCitySaveLibrary::List(FCitySaveLibrary::Directory());
	UI->ReviewedCityIndex = UI->ReviewedCitySlots.IndexOfByPredicate(
	    [&](const auto& Slot) { return Slot.Identity.Id == Identity.Id; });
	TestTrue(TEXT("Desktop80 load enabled"), !UI->IsPaletteActionDisabled(ECityPaletteAction::ConfirmLoad));
	UI->bDesktopUi = false;
	TestTrue(TEXT("XR80 load disabled"), UI->IsPaletteActionDisabled(ECityPaletteAction::ConfirmLoad));
	TestTrue(TEXT("XR library explains supported size"),
	         UI->GetEntryMenuHelp().Contains(TEXT("supports64x64")));
	UI->ShowNewCitySetup();
	TestTrue(TEXT("XR has no larger-map selector"),
	         !UI->VisiblePaletteEntries.ContainsByPredicate(
	             [](const auto& Entry) { return Entry.Action == ECityPaletteAction::CycleMapSize; }));
	Invoke(ECityPaletteAction::CycleMapSize);
	TestEqual(TEXT("stale larger-map action cannot change XR size"), UI->NewCityTerrain.Dimensions.GetWidth(),
	          64);
	FString After;
	FFileHelper::LoadFileToString(After, *LargePath);
	TestEqual(TEXT("library browsing never rewrites saved city"), After, Before);
	// A new world models restart: the file restores every authority independently of prior size.
	auto* Restart = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LiveDimensionsRestart"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Init);
	auto* Restored = Restart->GetSubsystem<UCitySimulationSubsystem>();
	Restored->RegisterBuildingCatalog(Catalog);
	TestTrue(TEXT("80 survives restart"), Restored->LoadCityFromPath(LargePath, Error, false));
	TestTrue(TEXT("restarted authorities agree"), Restored->BuildSaveDocument().ValidateDimensions(Error));
	Restart->DestroyWorld(false);
	// One bounded populated80 fixture exercises services/routing/settlement with
	// normal traffic budgets. This is integration evidence, not a capacity study.
	FCityBenchmarkConfig Config;
	Config.RequestedLots = 350;
	Config.bPopulated = Config.bLateGame = true;
	FCityPlacementModelState State;
	FCityBenchmarkResult Result;
	const FCityGridDimensions Dimensions(80, 80);
	if (!TestTrue(TEXT("representative80 layout builds"),
	              FCityBenchmark::BuildScene(Config, *Catalog, State, Result, Error, Dimensions)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	FCityIdentity PopulatedIdentity;
	PopulatedIdentity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	PopulatedIdentity.Name = TEXT("LiveDimensions populated80 integration");
	PopulatedIdentity.DisasterFrequency = ECityDisasterFrequency::Off;
	auto Terrain = FCityBenchmark::CapacityTerrainParameters();
	Terrain.Dimensions = Dimensions;
	TestTrue(TEXT("populated city uses real80 creation"),
	         Simulation->CreateCity(PopulatedIdentity, Terrain, FCitySaveLibrary::Directory(), Error));
	TMap<FName, FCityBuildingDefinition> Definitions;
	for (const auto& Definition : Catalog->Definitions)
		Definitions.Add(Definition.DefinitionId, Definition);
	// Attach the grid's actual entrance to the generated neighbourhood streets.
	for (const FCityCellCoord Cell :
	     {Terrain.GetEntranceCell(), FCityCellCoord(Terrain.GetEntranceCell().X, 1)})
		if (!State.RoadCells.Contains(Cell))
		{
			State.RoadCells.Add(Cell);
			State.RoadRecords.Add({Cell, TEXT("LocalRoad")});
		}
	auto Model = FCityPlacementModel::CreateValidatedFromState(State, Definitions, Error);
	if (!TestTrue(TEXT("populated layout restores"), Model.IsValid()))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	Simulation->PlacementModel = MoveTemp(Model);
	Simulation->PlacementModel->SetTerrain(&Simulation->Terrain);
	Simulation->RoadNetworkOwner = Simulation->DerivedOwner = nullptr;
	Simulation->Development.InitializeExistingAsOperational(*Simulation->PlacementModel, Catalog, 0);
	Simulation->RefreshDerivedSnapshots();
	Simulation->SetEntryMenuSuspended(false);
	Simulation->SetSimulationSpeed(5);
	Simulation->SetSimulationPaused(false);
	for (int32 Day = 0; Day < 40; ++Day)
		Simulation->Tick(Simulation->EconomyTuning.FixedStepSeconds / 5.f + .001f);
	Simulation->SetSimulationPaused(true);
	TestEqual(TEXT("bounded populated simulation advances40 days"),
	          Simulation->GetEconomySnapshot().FixedStepId, int64(40));
	TestTrue(TEXT("actual residents arrive"), Simulation->GetEconomySnapshot().Residents > 0);
	TestTrue(TEXT("actual jobs operate"), Simulation->GetEconomySnapshot().Jobs > 0);
	TestTrue(TEXT("populated fixture has buildings beyond64"),
	         Simulation->GetPlacementModel().ExportState().Entities.ContainsByPredicate(
	             [](const auto& Entity) { return Entity.Anchor.X >= 64; }));
	TestTrue(TEXT("populated authorities agree"), Simulation->BuildSaveDocument().ValidateDimensions(Error));
	FString FixturePath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("CityDimensionsFixture="), FixturePath))
		FixturePath = FPaths::ProjectSavedDir() / TEXT("Automation/Dimensions/Populated80.json");
	if (!TestTrue(TEXT("populated80 fixture saves"), Simulation->SaveCityToPath(FixturePath, Error)))
		AddError(Error);
	const auto Residents = Simulation->GetEconomySnapshot().Residents;
	TestTrue(TEXT("populated80 reloads"), Simulation->LoadCityFromPath(FixturePath, Error, false));
	TestEqual(TEXT("population survives populated reload"), Simulation->GetEconomySnapshot().Residents,
	          Residents);
	TestTrue(TEXT("populated reload authorities agree"),
	         Simulation->BuildSaveDocument().ValidateDimensions(Error));
	UI->Board->PrepareCityView();
	UE_LOG(
	    LogTemp, Display,
	    TEXT("CITYSIM_LIVEDIMENSIONS_POPULATED Size=80 Buildings=%d Residents=%d Jobs=%d Days=40 Routes=%d Fixture=%s"),
	    Simulation->GetPlacementModel().GetEntities().Num(), Residents, Simulation->GetEconomySnapshot().Jobs,
	    Simulation->GetTrafficSnapshot().Vehicles.Num(), *FixturePath);
	World->DestroyWorld(false);
	return true;
}
#endif
