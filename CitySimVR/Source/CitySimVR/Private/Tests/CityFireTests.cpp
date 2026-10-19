#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Interaction/CityPaletteModel.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FCityBuildingDefinition FireDefinition(const TCHAR* Id)
{
	FCityBuildingDefinition D;
	D.DefinitionId = Id;
	D.DisplayName = Id;
	D.Footprint = {1, 1};
	D.Cost = 200;
	if (D.DefinitionId == TEXT("Fire"))
	{
		D.PowerDemand = 10;
		D.WaterDemand = 15;
		D.MonthlyUpkeep = 30;
	}
	return D;
}
uint64 FirePlace(FCityPlacementModel& City, const TCHAR* Id, const FCityCellCoord Cell)
{
	const auto D = FireDefinition(Id);
	auto P = City.BeginPreview(D, Cell);
	return City.CommitPlacement(P, D).EntityId;
}
void FireRoad(FCityPlacementModel& City)
{
	auto P = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(P, {30, 8});
	City.CommitRoad(P);
}
FCityFireSnapshot Burning(const uint64 Id)
{
	FCityFireSnapshot S;
	S.LastFixedStepId = 1;
	S.ActivationStep = 0;
	S.GraceEndStep = 0;
	S.NextIgnitionStep = 1000;
	S.TotalIncidents = 1;
	S.RecentIncidents.Add({1, Id, 0, 1, -1, 0});
	S.DamagedBuildings.Add(Id, {1, -1});
	return S;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireCoverageTest, "CitySim.Safety.FireWarningResponseRepair",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireCoverageTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FireRoad(City);
	const uint64 Power = FirePlace(City, TEXT("Power"), {1, 9});
	const uint64 Water = FirePlace(City, TEXT("Service"), {2, 9});
	const uint64 Home = FirePlace(City, TEXT("Home"), {4, 9});
	const uint64 Station = FirePlace(City, TEXT("Fire"), {8, 9});
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = {FireDefinition(TEXT("Home")), FireDefinition(TEXT("Fire"))};
	FCityEconomyTuning T;
	FCityDevelopmentSnapshot D;
	FCityEnvironmentSnapshot W;
	FString Error;
	auto Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	TestEqual(TEXT("Fire station consumes real water"), Services.ServiceDemand, 25);
	TestEqual(TEXT("Fire station named upkeep"), Services.CivicUpkeep, int64(30));
	Services.Residents = 149;
	FCityFireSimulation Grace;
	Grace.AdvanceToStep(1, City, Services, D, W, T, false);
	TestFalse(TEXT("Small city has no fire challenge"), Grace.GetSnapshot().IsIntroduced());
	Services.Residents = 150;
	Grace.AdvanceToStep(2, City, Services, D, W, T, false);
	TestEqual(TEXT("Two month fire warning starts at threshold crossing"), Grace.GetSnapshot().GraceEndStep,
	          int64(52));
	Grace.AdvanceToStep(51, City, Services, D, W, T, false);
	TestEqual(TEXT("No ignition during warning"), Grace.GetSnapshot().TotalIncidents, int64(0));
	TestTrue(TEXT("Fire protection lowers risk"), Grace.GetSnapshot().EntityProtection.FindRef(Home) > 0);
	T.FireProtectionCapacity = 2;
	Grace.Refresh(City, Services, D, W, T, false);
	TestEqual(TEXT("Protection capacity bounded"), Grace.GetSnapshot().ProtectedDemand, 2);
	Grace.Refresh(City, Services, D, W, T, true);
	TestEqual(TEXT("Unpaid upkeep stops fire response"), Grace.GetSnapshot().OperationalStations, 0);
	T.FireProtectionCapacity = 160;
	FCityFireSimulation Fire;
	auto Severe = Burning(Home);
	Severe.DamagedBuildings[Home].Damage = 31;
	TestTrue(TEXT("Restore a burning lot with structural damage"), Fire.RestoreState(Severe, 1, Error));
	Fire.Refresh(City, Services, D, W, T, false);
	TestEqual(TEXT("Connected station assigned to the incident"),
	          Fire.GetSnapshot().ResponseStations.FindRef(Home), Station);
	const auto Disabled =
	    FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, &W, nullptr, &Fire.GetSnapshot());
	TestEqual(TEXT("Burning homes lose operational housing capacity"), Disabled.HousingCapacity, 0);
	TestEqual(TEXT("Burning failure explicit"), Disabled.GetFailure(Home), ECityOperationFailure::OnFire);
	Fire.AdvanceToStep(4, City, Services, D, W, T, false);
	TestFalse(TEXT("Three response steps extinguish"), Fire.GetSnapshot().IsBurning(Home));
	TestEqual(TEXT("Structural damage still needs repair after suppression"),
	          Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 34);
	const int64 Funds = City.GetFunds();
	const auto Definition = FireDefinition(TEXT("Home"));
	TestTrue(TEXT("Paid repair begins"), Fire.Repair(Home, City, Definition, T, Error));
	TestEqual(TEXT("Named repair transfers only its bill"), Funds - City.GetFunds(), int64(25));
	TestFalse(TEXT("Repair cannot charge twice"), Fire.Repair(Home, City, Definition, T, Error));
	Fire.AdvanceToStep(13, City, Services, D, W, T, false);
	TestTrue(TEXT("Repair remains disabled until deadline"), Fire.GetSnapshot().IsDisabled(Home));
	Fire.AdvanceToStep(14, City, Services, D, W, T, false);
	TestFalse(TEXT("Repair restores operation"), Fire.GetSnapshot().IsDisabled(Home));
	TestEqual(TEXT("Repaired home capacity returns"),
	          FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, &W, nullptr, &Fire.GetSnapshot())
	              .HousingCapacity,
	          10);
	// A damaged employer removes actual jobs while citizen identity/wallets survive.
	const uint64 Shop = FirePlace(City, TEXT("Shop"), {10, 9});
	Catalog->Definitions.Add(FireDefinition(TEXT("Shop")));
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	FCityPopulationSimulation People;
	FCityEconomySnapshot Economy;
	Economy.FixedStepId = 14;
	Economy.Residents = 10;
	Economy.Jobs = 10;
	Economy.RetailCapacity = 10;
	T.NewArrivalUneducatedPercent = 0; // Isolate job loss from arrival qualifications.
	People.Reconcile(City, Economy, T, &Services);
	People.MarkExistingBusinessesOpenForScenario();
	TestEqual(TEXT("Before fire, actual citizens hold jobs"), People.BuildAggregate(T).EmployedResidents, 10);
	const auto Citizens = People.GetSnapshot().Citizens;
	FCityFireSimulation EmployerFire;
	auto EmployerState = Burning(Shop);
	EmployerState.LastFixedStepId = EmployerState.RecentIncidents[0].StartStep = 14;
	EmployerFire.RestoreState(EmployerState, 14, Error);
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, &W, &People.GetSnapshot(),
	                                                &EmployerFire.GetSnapshot());
	Economy.Jobs = Services.Jobs;
	People.Reconcile(City, Economy, T, &Services);
	TestEqual(TEXT("Burning employer removes jobs from citizen census"),
	          People.BuildAggregate(T).UnemployedResidents, 2);
	TestEqual(TEXT("Unemployment starts at actual job loss"),
	          People.GetSnapshot().Citizens.Last().UnemployedSinceStep, int64(14));
	TestEqual(TEXT("Fire does not discard citizens"), People.GetSnapshot().Citizens.Num(), Citizens.Num());
	TestEqual(TEXT("Fire preserves citizen identity"), People.GetSnapshot().Citizens[0].Id, Citizens[0].Id);
	TestEqual(TEXT("Fire preserves citizen wallet"), People.GetSnapshot().Citizens[0].AvailableMoney,
	          Citizens[0].AvailableMoney);
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, &W, &People.GetSnapshot(),
	                                                &EmployerFire.GetSnapshot());
	FCityCrimeSimulation Crime;
	T.CrimePopulationThreshold = 1;
	T.CrimeGraceMonths = 0;
	Crime.AdvanceToStep(14, City, Services, D, T, false);
	TestEqual(TEXT("Crime consumes fire-induced unemployment"), Crime.GetSnapshot().JobShortage, 2);
	const int64 EmployerBurnout = 14 + FCityFireSimulation::MaximumBurnSteps;
	EmployerFire.AdvanceToStep(EmployerBurnout, City, Services, D, W, T, false);
	EmployerFire.Repair(Shop, City, FireDefinition(TEXT("Shop")), T, Error);
	EmployerFire.AdvanceToStep(EmployerBurnout + T.FireRepairSteps, City, Services, D, W, T, false);
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog, &D, &W, &People.GetSnapshot(),
	                                                &EmployerFire.GetSnapshot());
	Economy.FixedStepId = EmployerBurnout + T.FireRepairSteps;
	Economy.Jobs = Services.Jobs;
	People.Reconcile(City, Economy, T, &Services);
	TestEqual(TEXT("Employer repair restores actual jobs"), People.BuildAggregate(T).EmployedResidents, 10);
	TestEqual(TEXT("Restored employment clears the unemployment clock"),
	          People.GetSnapshot().Citizens[0].UnemployedSinceStep, int64(-1));
	City.RemoveRoad({8, 8});
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	Fire.Refresh(City, Services, D, W, T, false);
	TestEqual(TEXT("Disconnected frontage stops response"), Fire.GetSnapshot().OperationalStations, 0);
	FireRoad(City);
	City.DemolishEntity(Water);
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	Fire.Refresh(City, Services, D, W, T, false);
	TestEqual(TEXT("Water loss stops response"), Fire.GetSnapshot().OperationalStations, 0);
	FirePlace(City, TEXT("Service"), {2, 9});
	City.DemolishEntity(Power);
	Services = FCityServiceEvaluator::BuildSnapshot(City, T, Catalog);
	Fire.Refresh(City, Services, D, W, T, false);
	TestEqual(TEXT("Power loss stops response"), Fire.GetSnapshot().OperationalStations, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireSpreadTest, "CitySim.Safety.FireAdjacencyWindAndCaps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireSpreadTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FireRoad(City);
	const uint64 A = FirePlace(City, TEXT("Home"), {4, 9});
	const uint64 B = FirePlace(City, TEXT("Shop"), {5, 9});
	const uint64 C = FirePlace(City, TEXT("Home"), {6, 9});
	const uint64 Gap = FirePlace(City, TEXT("Home"), {8, 9});
	const uint64 Corner = FirePlace(City, TEXT("Home"), {3, 10});
	FCityEconomyTuning T;
	FCityEnvironmentSnapshot W;
	FCityDevelopmentSnapshot D;
	FString Error;
	TestTrue(TEXT("Orthogonal edges touch"),
	         FCityFireSimulation::AreAdjacent(City.GetEntities().FindChecked(A),
	                                          City.GetEntities().FindChecked(B)));
	TestFalse(TEXT("Gaps/roads interrupt spread"),
	          FCityFireSimulation::AreAdjacent(City.GetEntities().FindChecked(C),
	                                           City.GetEntities().FindChecked(Gap)));
	TestFalse(TEXT("Corner-only contact cannot spread"),
	          FCityFireSimulation::AreAdjacent(City.GetEntities().FindChecked(A),
	                                           City.GetEntities().FindChecked(Corner)));
	W.WindDirection = ECityWindDirection::East;
	W.WindStrength = 2;
	const auto& Source = City.GetEntities().FindChecked(A);
	const auto& Target = City.GetEntities().FindChecked(B);
	const int32 Down = FCityFireSimulation::SpreadChance(Source, Target, W, T, 0);
	W.WindDirection = ECityWindDirection::West;
	const int32 Up = FCityFireSimulation::SpreadChance(Source, Target, W, T, 0);
	TestTrue(TEXT("Downwind chance exceeds upwind"), Down > Up);
	W.WindDirection = ECityWindDirection::East;
	W.bPrecipitation = true;
	TestTrue(TEXT("Rain reduces spread"), FCityFireSimulation::SpreadChance(Source, Target, W, T, 0) < Down);
	TestEqual(TEXT("Full protection blocks spread"),
	          FCityFireSimulation::SpreadChance(Source, Target, W, T, 100), 0);
	T.FireSpreadChance = 100;
	T.FireWindSpreadModifier = 0;
	T.FireSpreadIntervalSteps = 1;
	W.bPrecipitation = false;
	auto Services = FCityServiceEvaluator::BuildSnapshot(City, T);
	FCityFireSimulation Fire;
	Fire.RestoreState(Burning(A), 1, Error);
	Fire.AdvanceToStep(2, City, Services, D, W, T, false);
	TestTrue(TEXT("First neighbor catches fire"), Fire.GetSnapshot().IsBurning(B));
	TestFalse(TEXT("No same-step chain to second neighbor"), Fire.GetSnapshot().IsBurning(C));
	Fire.AdvanceToStep(3, City, Services, D, W, T, false);
	TestTrue(TEXT("Next step can reach second neighbor"), Fire.GetSnapshot().IsBurning(C));
	TestEqual(TEXT("Three active cap"), Fire.GetSnapshot().ActiveIncidents, 3);
	TestFalse(TEXT("Separated home stays safe from spread"), Fire.GetSnapshot().IsBurning(Gap));
	City.DemolishEntity(B);
	Fire.Refresh(City, Services, D, W, T, false);
	TestFalse(TEXT("Demolition clears incident/damage"), Fire.GetSnapshot().IsDisabled(B));
	TestTrue(TEXT("Demolition state remains save-valid"),
	         FCityFireSimulation::ValidateState(Fire.GetSnapshot(), 3, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireDeterminismTest, "CitySim.Safety.FireCatchUpAndResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireDeterminismTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FireRoad(City);
	for (int32 X = 1; X < 20; ++X)
		FirePlace(City, TEXT("Home"), {X, 9});
	FCityEconomyTuning T;
	T.FirePopulationThreshold = 0;
	T.FireGraceMonths = 0;
	T.FireCheckIntervalSteps = 1;
	T.FireBaseRisk = 100;
	FCityEnvironmentSnapshot W;
	FCityDevelopmentSnapshot D;
	const auto Services = FCityServiceEvaluator::BuildSnapshot(City, T);
	FCityFireSimulation Fine, Coarse;
	FString Error;
	for (int64 Step = 1; Step <= 300; ++Step)
	{
		Fine.AdvanceToStep(Step, City, Services, D, W, T, false);
		TestTrue(TEXT("Active/damaged caps hold throughout run"),
		         Fine.GetSnapshot().ActiveIncidents <= 3 && Fine.GetSnapshot().DamagedBuildings.Num() <= 8);
		TestTrue(TEXT("Every step has a valid save state"),
		         FCityFireSimulation::ValidateState(Fine.GetSnapshot(), Step, Error));
	}
	Coarse.AdvanceToStep(300, City, Services, D, W, T, false);
	TestEqual(TEXT("Catch-up random state identical"), Coarse.GetSnapshot().RandomState,
	          Fine.GetSnapshot().RandomState);
	TestEqual(TEXT("Catch-up incident count identical"), Coarse.GetSnapshot().TotalIncidents,
	          Fine.GetSnapshot().TotalIncidents);
	TestTrue(TEXT("Ignitions happen; damage prevents runaway loss"),
	         Fine.GetSnapshot().TotalIncidents > 0 && Fine.GetSnapshot().DamagedBuildings.Num() == 8);
	FCityFireSimulation Resumed;
	Resumed.RestoreState(Fine.GetSnapshot(), 300, Error);
	Fine.AdvanceToStep(400, City, Services, D, W, T, false);
	Resumed.AdvanceToStep(400, City, Services, D, W, T, false);
	TestEqual(TEXT("Resume preserves cadence"), Resumed.GetSnapshot().NextIgnitionStep,
	          Fine.GetSnapshot().NextIgnitionStep);
	const uint32 Seed = Fine.GetSnapshot().RandomState;
	Fine.Refresh(City, Services, D, W, T, false);
	Fine.AdvanceToStep(400, City, Services, D, W, T, false);
	TestEqual(TEXT("Pause/render refresh draws no random samples"), Fine.GetSnapshot().RandomState, Seed);
	auto Invalid = Fine.GetSnapshot();
	TArray<uint64> InvalidIds;
	Invalid.DamagedBuildings.GetKeys(InvalidIds);
	Invalid.DamagedBuildings.FindChecked(InvalidIds[0]).Damage = 101;
	TestFalse(TEXT("Invalid damage rejected"), FCityFireSimulation::ValidateState(Invalid, 400, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireIntegrationTest,
                                 "CitySim.Persistence.FirePlacementMigrationRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FireSaveTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Real fire definition exists"),
	                 Catalog ? Catalog->FindDefinition(TEXT("Fire")) : nullptr))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	FireRoad(City);
	const TCHAR* Ids[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Police"), TEXT("Fire")};
	const FCityCellCoord Cells[] = {{1, 9}, {3, 9}, {6, 9}, {10, 9}, {14, 9}};
	uint64 Home = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
	{
		const auto* Def = Catalog->FindDefinition(Ids[Index]);
		auto P = City.BeginPreview(*Def, Cells[Index]);
		const auto Result = City.CommitPlacement(P, *Def);
		TestTrue(TEXT("Both departments place beside roads"), Result.IsSuccess());
		if (Index == 2)
			Home = Result.EntityId;
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.NewArrivalUneducatedPercent = 0; // Small fully qualified response fixture.
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.FirePopulationThreshold = 5;
	Sim->EconomyTuning.FireGraceMonths = 2;
	Sim->EconomyTuning.FireBaseRisk = 0;
	Sim->StartScenario();
	Sim->Tick(.125f);
	TestTrue(TEXT("Fire introduced with warning"), Sim->GetFireSnapshot().IsInGrace());
	TestEqual(TEXT("Both departments upkeep billed separately"), Sim->GetEconomySnapshot().CivicUpkeep,
	          int64(55));
	TestTrue(TEXT("Both departments operating"), Sim->GetCrimeSnapshot().OperationalStations == 1 &&
	                                                 Sim->GetFireSnapshot().OperationalStations == 1);
	const auto Build = FCityPaletteModel::Entries(ECityPalettePage::Build);
	const auto Menu = FCityPaletteModel::Entries(ECityPalettePage::City);
	TestTrue(TEXT("Fire is visible in Build drawer"), Build.ContainsByPredicate([](const auto& Entry) {
		return Entry.Action == ECityPaletteAction::SelectFire;
	}));
	TestTrue(TEXT("Fire View and Repair are visible in City drawer"),
	         Menu.ContainsByPredicate(
	             [](const auto& Entry) { return Entry.Action == ECityPaletteAction::OverlayFire; }) &&
	             Menu.ContainsByPredicate(
	                 [](const auto& Entry) { return Entry.Action == ECityPaletteAction::SelectRepair; }));
	FString Error, Json;
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Fire"));
	IFileManager::Get().MakeDirectory(*Dir, true);
	const FString Path = FPaths::Combine(Dir, TEXT("City.json"));
	Sim->SaveCityToPath(Path, Error);
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Json, Error);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetNumberField(TEXT("schemaVersion"), 24);
	Json.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *Path);
	TestFalse(TEXT("Historical format rejected"), Sim->LoadCityFromPath(Path, Error, false));
	// Inject a save-valid incident to exercise citywide capacity, presentation data and transactional
	// recovery.
	for (int32 Index = 0; Index < 8; ++Index)
		Sim->Tick(.125f);
	auto State = Burning(Home);
	State.LastFixedStepId = Sim->Economy.GetSnapshot().FixedStepId;
	State.RecentIncidents[0].StartStep = State.LastFixedStepId;
	State.NextIgnitionStep = State.LastFixedStepId + 1000;
	State.DamagedBuildings[Home].Damage =
	    31; // Keep the paid-repair integration fixture structurally damaged.
	TestTrue(TEXT("Active city fire restore"), Sim->Fire.RestoreState(State, State.LastFixedStepId, Error));
	Sim->RefreshDerivedSnapshots();
	TestEqual(TEXT("Burning city home has explicit failure"), Sim->ServiceSnapshot.GetFailure(Home),
	          ECityOperationFailure::OnFire);
	FCityDiagnosticOverlayResult Overlay;
	FCityDiagnosticOverlayEvaluator::Build(
	    Sim->GetPlacementModel(), Sim->ServiceSnapshot, Sim->NeedsSnapshot, Sim->EconomyTuning, nullptr,
	    nullptr, nullptr, Sim->Environment.GetSnapshot(), ECityDiagnosticOverlay::Fire, 0, Overlay);
	TestEqual(TEXT("Fire overlay labels authoritative burning cells"),
	          Overlay.Status[9 * FCityGrid::Width + 6], uint8(4));
	TestTrue(TEXT("Active fire saves"), Sim->SaveCityToPath(Path, Error));
	const uint32 Seed = Sim->Fire.GetSnapshot().RandomState;
	TestTrue(TEXT("Active fire loads"), Sim->LoadCityFromPath(Path, Error, false));
	TestEqual(TEXT("Fire random state survives load"), Sim->Fire.GetSnapshot().RandomState, Seed);
	// Moving emergency trips are saved together with actual traffic position.
	Sim->Tick(.125f);
	Sim->Tick(.125f);
	const FString MovingPath = FPaths::Combine(Dir, TEXT("Moving.json"));
	TestTrue(TEXT("City saves an in-flight engine"), Sim->SaveCityToPath(MovingPath, Error));
	const auto MovingTraffic = Sim->GetTrafficSnapshot();
	TestTrue(TEXT("City restores in-flight staffing and response"),
	         Sim->LoadCityFromPath(MovingPath, Error, false));
	TestEqual(TEXT("Saved engine pool retains vehicles"), Sim->GetTrafficSnapshot().Vehicles.Num(),
	          MovingTraffic.Vehicles.Num());
	TestTrue(TEXT("Resume original active fire for speed comparison"),
	         Sim->LoadCityFromPath(Path, Error, false));
	IFileManager::Get().Delete(*MovingPath, false, true, true);
	for (int32 Index = 0; Index < 15; ++Index)
		Sim->Tick(.125f);
	FString FineJson;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), FineJson, Error);
	const auto FineFire = Sim->Fire.GetSnapshot();
	const int64 FineFunds = Sim->GetPlacementModel().GetFunds();
	const int64 FineStep = Sim->Economy.GetSnapshot().FixedStepId;
	TestTrue(TEXT("Resume active city for coarse/speed comparison"),
	         Sim->LoadCityFromPath(Path, Error, false));
	Sim->SetSimulationSpeed(3);
	Sim->Tick(.625f);
	Sim->SetSimulationSpeed(1);
	TestEqual(TEXT("3x catch-up advances identical fixed steps"), Sim->Economy.GetSnapshot().FixedStepId,
	          FineStep);
	TestEqual(TEXT("3x catch-up preserves fire random state"), Sim->Fire.GetSnapshot().RandomState,
	          FineFire.RandomState);
	TestEqual(TEXT("3x catch-up preserves incident outcome"),
	          Sim->Fire.GetSnapshot().RecentIncidents[0].ResolvedStep,
	          FineFire.RecentIncidents[0].ResolvedStep);
	TestEqual(TEXT("3x catch-up preserves damage"),
	          Sim->Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage,
	          FineFire.DamagedBuildings.FindChecked(Home).Damage);
	TestEqual(TEXT("3x catch-up preserves treasury"), Sim->GetPlacementModel().GetFunds(), FineFunds);
	FString CoarseJson;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), CoarseJson, Error);
	TestEqual(TEXT("3x preserves wallets, public payroll, response trips and route positions"), CoarseJson,
	          FineJson);
	TestTrue(TEXT("Resume initial fire for repair path"), Sim->LoadCityFromPath(Path, Error, false));
	for (int32 Index = 0; Index < FCityFireSimulation::MaximumBurnSteps; ++Index)
		Sim->Tick(.125f);
	TestFalse(TEXT("Actual road response or burn deadline resolves incident"),
	          Sim->Fire.GetSnapshot().IsBurning(Home));
	TestTrue(TEXT("City command begins paid repair"), Sim->RepairBuilding(Home));
	TestTrue(TEXT("Repair deadline saves"), Sim->SaveCityToPath(Path, Error));
	const int64 Deadline = Sim->Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).RepairCompleteStep;
	TestTrue(TEXT("Repair deadline loads"), Sim->LoadCityFromPath(Path, Error, false));
	TestEqual(TEXT("Repair deadline exact after load"),
	          Sim->Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).RepairCompleteStep, Deadline);
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Json, Error);
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->GetObjectField(TEXT("fire"))
	    ->GetArrayField(TEXT("damage"))[0]
	    ->AsObject()
	    ->SetStringField(TEXT("entityId"), TEXT("999999"));
	FString Bad;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Bad));
	FFileHelper::SaveStringToFile(Bad, *Path);
	TestFalse(TEXT("Missing damage reference rejects load transaction"),
	          Sim->LoadCityFromPath(Path, Error, false));
	FString After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Rejected load leaves city intact"), After, Json);
	for (int32 Index = 0; Index < Sim->EconomyTuning.FireRepairSteps; ++Index)
		Sim->Tick(.125f);
	TestFalse(TEXT("City repair completes"), Sim->Fire.GetSnapshot().IsDisabled(Home));
	TestEqual(TEXT("Repaired home operation restored"), Sim->ServiceSnapshot.GetFailure(Home),
	          ECityOperationFailure::None);
	Sim->RequestResetScenario();
	Sim->RequestResetScenario();
	TestEqual(TEXT("Reset clears fire history"), Sim->Fire.GetSnapshot().TotalIncidents, int64(0));
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMinorFireRecoveryTest, "CitySim.Safety.MinorFireRecoveryBoundaries",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityMinorFireRecoveryTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FireRoad(City);
	const uint64 Home = FirePlace(City, TEXT("Home"), {6, 9}),
	             Station = FirePlace(City, TEXT("Fire"), {2, 9});
	FCityServiceSnapshot Services;
	Services.bUsesPublicStaffing = true;
	Services.PublicStaffPercent.Add(Station, 100);
	Services.PublicJobs.Add(Station, 4);
	FCityDevelopmentSnapshot Development;
	FCityEnvironmentSnapshot Weather;
	FCityEconomyTuning T;
	T.FireBaseRisk = 0;
	T.FireSpreadChance = 0;
	FCityPopulationSnapshot Population;
	FCityTransitTripRecord Trip;
	Trip.Id = 1;
	Trip.StationEntityId = Trip.OriginEntityId = Station;
	Trip.DestinationEntityId = Home;
	Trip.VehicleType = ECityTripVehicleType::FireEngine;
	Trip.Purpose = ECityTripPurpose::FireResponse;
	Trip.IncidentId = 1;
	Trip.StartStep = 1;
	Population.TransitTrips.Add(Trip);
	FString Error;
	for (int32 Damage : {1, 27, 28, 97})
	{
		FCityFireSimulation Fire;
		auto State = Burning(Home);
		State.DamagedBuildings[Home].Damage = Damage;
		TestTrue(TEXT("boundary burning state restores"), Fire.RestoreState(State, 1, Error));
		Population.TransitTrips[0].bCompleted = true;
		Population.TransitTrips[0].ArrivalStep = 1;
		Fire.AdvanceToStep(3, City, Services, Development, Weather, T, false, &Population);
		TestTrue(TEXT("partial suppression never recovers an active fire"),
		         Fire.GetSnapshot().IsBurning(Home) && Fire.GetSnapshot().IsDisabled(Home));
		FCitySaveDocument Doc, Read;
		Doc.Placement = City.ExportState();
		Doc.Economy.FixedStepId = 3;
		Doc.Fire = Fire.GetSnapshot();
		FString Json;
		TestTrue(TEXT("partly extinguished fire saves"), FCityPersistence::Serialize(Doc, Json, Error));
		if (!TestTrue(TEXT("partly extinguished fire loads"),
		              FCityPersistence::Deserialize(Json, Read, Error)))
			return false;
		FCityFireSimulation Loaded;
		TestTrue(TEXT("partly extinguished state resumes"), Loaded.RestoreState(Read.Fire, 3, Error));
		const int64 Funds = City.GetFunds();
		Fire.AdvanceToStep(4, City, Services, Development, Weather, T, false, &Population);
		Loaded.AdvanceToStep(4, City, Services, Development, Weather, T, false, &Population);
		const bool bMinor = Damage + 3 <= 30;
		TestEqual(TEXT("recovery applies at inclusive30 threshold"), Fire.GetSnapshot().IsDisabled(Home),
		          !bMinor);
		TestEqual(TEXT("save continuation preserves recovery"), Loaded.GetSnapshot().IsDisabled(Home),
		          !bMinor);
		TestFalse(TEXT("extinguishing resolves incident"), Fire.GetSnapshot().IsBurning(Home));
		TestEqual(TEXT("incident history retains real response steps"),
		          Fire.GetSnapshot().RecentIncidents[0].ResponseSteps, 3);
		TestEqual(TEXT("automatic recovery never spends treasury"), City.GetFunds(), Funds);
		if (bMinor)
		{
			TestEqual(TEXT("restored building needs no repair bill"),
			          Fire.RepairCost(Home, FireDefinition(TEXT("Home")), T), int64(0));
			TestFalse(TEXT("recovered building cannot be charged for repair"),
			          Fire.Repair(Home, City, FireDefinition(TEXT("Home")), T, Error));
			TestTrue(TEXT("recovered building can have a subsequent incident"), Fire.StartTest(Home, Error));
			TestEqual(TEXT("new incident starts fresh damage"),
			          Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 1);
		}
	}
	FCityFireSimulation Unattended;
	Unattended.RestoreState(Burning(Home), 1, Error);
	Population.TransitTrips[0].bCompleted = false;
	Unattended.AdvanceToStep(1 + FCityFireSimulation::MaximumBurnSteps, City, Services, Development, Weather,
	                         T, false, &Population);
	TestEqual(TEXT("unattended burnout retains full damage"),
	          Unattended.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 100);
	FCityFireSimulation Disabled;
	Disabled.RestoreState(Burning(Home), 1, Error);
	T.FireMinorDamageAutoRepairLimit = 0;
	Population.TransitTrips[0].bCompleted = true;
	Disabled.AdvanceToStep(4, City, Services, Development, Weather, T, false, &Population);
	TestTrue(TEXT("tuning can disable automatic recovery"), Disabled.GetSnapshot().IsDisabled(Home));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityFireCadenceTest, "CitySim.Safety.FireResponseWindowAndSpreadCadence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityFireCadenceTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(100000);
	FireRoad(City);
	const uint64 Home = FirePlace(City, TEXT("Home"), {6, 9}),
	             Station = FirePlace(City, TEXT("Fire"), {2, 9});
	FCityEconomyTuning T;
	T.FireBaseRisk = 0;
	T.FireSpreadChance = 0;
	FCityServiceSnapshot Services;
	Services.bUsesPublicStaffing = true;
	Services.PublicStaffPercent.Add(Station, 100);
	Services.PublicJobs.Add(Station, 4);
	FCityDevelopmentSnapshot D;
	FCityEnvironmentSnapshot W;
	FString Error;
	FCityPopulationSnapshot Population;
	FCityTransitTripRecord Trip;
	Trip.Id = 1;
	Trip.StationEntityId = Trip.OriginEntityId = Station;
	Trip.DestinationEntityId = Home;
	Trip.VehicleType = ECityTripVehicleType::FireEngine;
	Trip.Purpose = ECityTripPurpose::FireResponse;
	Trip.IncidentId = 1;
	Trip.StartStep = 1;
	Population.TransitTrips.Add(Trip);
	// Nine unattended days cover a20-second1x response, including fixed-step rounding.
	for (int32 Delay : {9, 13, 14})
	{
		FCityFireSimulation Fire;
		Fire.RestoreState(Burning(Home), 1, Error);
		Population.TransitTrips[0].bCompleted = false;
		Fire.AdvanceToStep(1 + Delay, City, Services, D, W, T, false, &Population);
		TestTrue(TEXT("Response window stays burning before arrival"), Fire.GetSnapshot().IsBurning(Home));
		TestEqual(TEXT("Unattended damage grows slowly"),
		          Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 1 + Delay * 2);
		FCityFireSimulation Loaded;
		TestTrue(TEXT("Delayed response restores"),
		         Loaded.RestoreState(Fire.GetSnapshot(), 1 + Delay, Error));
		Population.TransitTrips[0].bCompleted = true;
		Population.TransitTrips[0].ArrivalStep = 1 + Delay;
		const int64 Funds = City.GetFunds();
		for (int64 Step = 2 + Delay; Step <= 4 + Delay; ++Step)
			Fire.AdvanceToStep(Step, City, Services, D, W, T, false, &Population);
		Loaded.AdvanceToStep(4 + Delay, City, Services, D, W, T, false, &Population);
		TestFalse(TEXT("Complete response resolves fire"), Fire.GetSnapshot().IsBurning(Home));
		TestEqual(TEXT("Prompt response auto recovers; later response requires repair"),
		          Fire.GetSnapshot().IsDisabled(Home), Delay >= 14);
		TestEqual(TEXT("Saved coarse response matches fine outcome"), Loaded.GetSnapshot().IsDisabled(Home),
		          Fire.GetSnapshot().IsDisabled(Home));
		TestEqual(TEXT("Recovery spends no funds"), City.GetFunds(), Funds);
		if (Delay >= 14)
			TestEqual(TEXT("Late response keeps measured structural damage"),
			          Fire.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 32);
	}
	Population.TransitTrips.Reset();
	FCityFireSimulation Burnout;
	Burnout.RestoreState(Burning(Home), 1, Error);
	Burnout.AdvanceToStep(FCityFireSimulation::MaximumBurnSteps, City, Services, D, W, T, false, &Population);
	TestTrue(TEXT("Unattended fire lasts until its extended deadline"),
	         Burnout.GetSnapshot().IsBurning(Home));
	Burnout.AdvanceToStep(1 + FCityFireSimulation::MaximumBurnSteps, City, Services, D, W, T, false,
	                      &Population);
	TestFalse(TEXT("Extended deadline resolves unattended fire"), Burnout.GetSnapshot().IsBurning(Home));
	TestEqual(TEXT("Burnout still leaves full paid damage"),
	          Burnout.GetSnapshot().DamagedBuildings.FindChecked(Home).Damage, 100);
	// Start just before the old global cadence boundary: each new fire waits its own interval.
	const uint64 B = FirePlace(City, TEXT("Home"), {7, 9}), C = FirePlace(City, TEXT("Home"), {8, 9});
	auto State = Burning(Home);
	State.LastFixedStepId = State.RecentIncidents[0].StartStep = 4;
	T.FireSpreadChance = 100;
	T.FireWindSpreadModifier = 0;
	T.FireProtectionCapacity = 0;
	FCityFireSimulation Spread, Coarse;
	Spread.RestoreState(State, 4, Error);
	Coarse.RestoreState(State, 4, Error);
	Spread.AdvanceToStep(8, City, Services, D, W, T, false, &Population);
	TestFalse(TEXT("Neighbor cannot ignite before source ages five days"), Spread.GetSnapshot().IsBurning(B));
	Spread.AdvanceToStep(9, City, Services, D, W, T, false, &Population);
	TestTrue(TEXT("First neighbor ignites on source age five"), Spread.GetSnapshot().IsBurning(B));
	Spread.AdvanceToStep(13, City, Services, D, W, T, false, &Population);
	TestFalse(TEXT("Newly spread fire gets its own full spread interval"), Spread.GetSnapshot().IsBurning(C));
	FCityFireSimulation Resume;
	TestTrue(TEXT("Spread clock survives save restore"),
	         Resume.RestoreState(Spread.GetSnapshot(), 13, Error));
	Spread.AdvanceToStep(14, City, Services, D, W, T, false, &Population);
	Resume.AdvanceToStep(14, City, Services, D, W, T, false, &Population);
	Coarse.AdvanceToStep(14, City, Services, D, W, T, false, &Population);
	TestTrue(TEXT("Second neighbor ignites after its parent ages five"), Spread.GetSnapshot().IsBurning(C));
	TestEqual(TEXT("Spread catch-up is deterministic"), Coarse.GetSnapshot().RandomState,
	          Spread.GetSnapshot().RandomState);
	TestEqual(TEXT("Restored spread is deterministic"), Resume.GetSnapshot().RandomState,
	          Spread.GetSnapshot().RandomState);
	TestTrue(TEXT("Spread state stays save valid"),
	         FCityFireSimulation::ValidateState(Spread.GetSnapshot(), 14, Error));
	return true;
}

#endif
