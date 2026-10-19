#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Presentation/CityDiagnosticOverlay.h"
#include "Presentation/CityConditionIndicators.h"
#include "Presentation/CityBoardActor.h"
#include "Persistence/CityWastePersistence.h"
#include "Commands/CityPlacementModel.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWasteAuthorityTest,
                                 "CitySim.Waste.PlannedRoutesRatesConservationAndOrdinaryTraffic",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWasteAuthorityTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog) ||
	    !TestNotNull(TEXT("Waste depot"), Catalog->FindDefinition(TEXT("WasteDepot"))))
		return false;
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {63, 8});
	City.CommitRoad(Road);
	Road=City.BeginRoadPreview({63,8});City.UpdateRoadPreviewEnd(Road,{63,22});City.CommitRoad(Road);
	Road=City.BeginRoadPreview({63,22});City.UpdateRoadPreviewEnd(Road,{45,22});City.CommitRoad(Road);
	auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto Preview = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(Preview, *D).EntityId;
	};
	const uint64 Depot = Place(TEXT("WasteDepot"), {1, 9}), Home = Place(TEXT("Home"), {8, 9}),
	             Shop = Place(TEXT("Shop"), {12, 9}), Industry = Place(TEXT("Factory"), {16, 9}),
	             Far = Place(TEXT("Factory"), {48, 23});
	if (!TestTrue(TEXT("Fixture lots placed"), Depot && Home && Shop && Industry && Far))
		return false;
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, Catalog, 0);
	FCityServiceSnapshot Services;
	Services.HospitalCapacities.Add(999, 24);
	Services.PublicStaffPercent.Add(Depot, 100);
	FCityPopulationSnapshot People;
	for (int32 I = 1; I <= 10; ++I)
	{
		FCityCitizenRecord C;
		C.Id = I;
		C.HomeEntityId = Home;
		C.WorkplaceEntityId = I <= 2 ? Shop : Industry;
		People.Citizens.Add(C);
	}
	People.NextCitizenId = 11;
	FCityWasteTuning T;
	T.PopulationThreshold = 0;
	FCityWasteSimulation Waste;
	auto Update = [&](int64 Step) {
		Waste.Update(Step, 25, City, Catalog, Network, Services, Dev.GetSnapshot(), People, false, T);
	};
	Update(0);
	Services.PublicStaffPercent.Add(Depot, 0);
	for (int64 Step = 1; Step <= 25; ++Step)
		Update(Step);
	TestEqual(TEXT("Ten residents produce20/month"), Waste.GetSnapshot().Find(Home)->Units, 20);
	TestEqual(TEXT("Two shop workers produce8/month"), Waste.GetSnapshot().Find(Shop)->Units, 8);
	TestEqual(TEXT("Eight industry workers produce64/month"), Waste.GetSnapshot().Find(Industry)->Units, 64);
	TestEqual(TEXT("Empty far factory generates no waste"), Waste.GetSnapshot().Find(Far)->Units, 0);
	TestEqual(TEXT("Exactly92 generated"), Waste.GetSnapshot().Generated, int64(92));
	TestEqual(TEXT("Introduction grace suppresses health pressure"), Waste.GetSnapshot().Pressure(Home, 25),
	          0);
	TestTrue(TEXT("Debug pile fills industry"), Waste.AddTestPile(Industry));
	Waste.AddTestPile(Home);
	Waste.AddTestPile(Shop);
	Waste.AddTestPile(Far);
	const auto Before = Waste.GetSnapshot();
	FCityDiagnosticOverlayResult Overlay;
	FCityEnvironmentSnapshot Environment;
	FCityNeedsSnapshot Needs;
	FCityEconomyTuning EconomyTuning;
	FCityDiagnosticOverlayEvaluator::Build(
	    City, Services, Needs, EconomyTuning, Catalog->FindDefinition(TEXT("Home")),
	    Catalog->FindDefinition(TEXT("Shop")), Catalog->FindDefinition(TEXT("Factory")), Environment,
	    ECityDiagnosticOverlay::Waste, 0, Overlay, nullptr, &Network, &Dev.GetSnapshot(), nullptr, &People,
	    &Waste.GetSnapshot());
	TestEqual(TEXT("Waste overlay retains actual capped100%, not demand"),
	          Overlay.Values[9 * FCityGrid::Width + 8], 100);
	TestTrue(TEXT("Waste overlay keeps collection legend"),
	         Overlay.Legend.Contains(TEXT("Largest reachable piles")));
	TestTrue(TEXT("Full cap renders red rather than blocked gray"),
	         Overlay.ColorAt(9 * FCityGrid::Width + 8).R > .9f &&
	             Overlay.ColorAt(9 * FCityGrid::Width + 8).G < .1f);
	Services.PublicStaffPercent.Add(Depot, 100);
	Update(25);
	TestTrue(TEXT("Plans created"),
	         !Waste.GetSnapshot().Trucks.IsEmpty() && !Waste.GetSnapshot().Trucks[0].Stops.IsEmpty());
	if (Waste.GetSnapshot().Trucks.IsEmpty() || Waste.GetSnapshot().Trucks[0].Stops.IsEmpty())
		return false;
	TestEqual(TEXT("Largest in-range pile first"), Waste.GetSnapshot().Trucks[0].Stops[0], Industry);
	TSet<uint64> Reserved;
	for (const auto& Truck : Waste.GetSnapshot().Trucks)
		for (uint64 Id : Truck.Stops)
		{
			TestFalse(TEXT("No duplicate reservations"), Reserved.Contains(Id));
			Reserved.Add(Id);
		}
	TestFalse(TEXT("Road-distant factory excluded despite larger pile"), Reserved.Contains(Far));
	TestEqual(TEXT("Planning alone never collects"), Waste.GetSnapshot().Collected, int64(0));
	FCityTrafficSimulation Traffic;
	FString Error;
	Traffic.AdvanceToStep(Network, 25, Error);
	auto Drive = [&](int64 Step) {
		Update(Step);
		Traffic.UpdateTrips(City, Catalog, People, Network, Step);
		Traffic.AdvanceOneStep(Network, Step);
		for (const auto& V : Traffic.GetSnapshot().Vehicles)
		{
			if (V.VehicleType == ECityTripVehicleType::GarbageTruck)
			{
				TestFalse(TEXT("Garbage never requests emergency priority"), V.bEmergencyResponding);
				TestTrue(TEXT("Truck body length"), V.LengthUnits == 550);
			}
			if (V.bCompleted)
				if (auto* Trip = People.TransitTrips.FindByPredicate(
				        [&](const auto& P) { return P.Id == V.SourceId; }))
				{
					Trip->bCompleted = true;
					Trip->ArrivalStep = Step;
				}
		}
		Update(Step);
		Traffic.UpdateTrips(City, Catalog, People, Network, Step);
		TestTrue(TEXT("Conservation after actual movement/pickup/unload/process"),
		         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), Step, Error));
		TestTrue(TEXT("Trip ownership stays consistent"),
		         FCityWasteSimulation::ValidateReferences(Waste.GetSnapshot(), City, People, Error));
	};
	for (int64 Step = 26; Step <= 65; ++Step)
		Drive(Step);
	TestTrue(TEXT("Actual arrivals collect and return waste for processing"),
	         Waste.GetSnapshot().Collected > 0 && Waste.GetSnapshot().Disposed > 0);
	TestTrue(TEXT("Far pile stays capped; overflow accounted"),
	         Waste.GetSnapshot().Find(Far)->Units <= Waste.GetSnapshot().Find(Far)->Capacity);
	FCityWasteSnapshot Copy;
	TestTrue(TEXT("State JSON roundtrip"),
	         FCityWastePersistence::Read(FCityWastePersistence::Write(Waste.GetSnapshot()), Copy, 65, Error));
	TestEqual(TEXT("Cargo survives serialization"), Copy.InTrucks(), Waste.GetSnapshot().InTrucks());
	auto Broken = Copy;
	Broken.Generated++;
	TestFalse(TEXT("Reject mass creation"), FCityWasteSimulation::ValidateState(Broken, 65, Error));
	const int64 Collected = Waste.GetSnapshot().Collected;
	Services.PublicStaffPercent.Add(Depot, 0);
	Update(66);
	Update(66);
	TestEqual(TEXT("Unstaffed stop never collects"), Waste.GetSnapshot().Collected, Collected);
	TestTrue(TEXT("Outage retains waste mass"),
	         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), 66, Error));
	TestTrue(TEXT("Restores pending fleet"), Waste.RestoreState(Copy, Error));
	// Road cut prevents dispatch/pickup; already carried material remains conserved.
	City.RemoveRoad({6, 8});
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	Services.PublicStaffPercent.Add(Depot, 100);
	Update(66);
	TestTrue(TEXT("Road outage preserves mass"),
	         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), 66, Error));
	const int64 AfterCut = Waste.GetSnapshot().Collected;
	for (int64 Step = 67; Step <= 75; ++Step)
		Update(Step);
	TestEqual(TEXT("No timer-based collection across a road cut"), Waste.GetSnapshot().Collected, AfterCut);
	City.DemolishEntity(Depot);
	Update(75);
	TestTrue(
	    TEXT("Demolition clears fleet/trips and explicitly removes stored/carried waste"),
	    Waste.GetSnapshot().Trucks.IsEmpty() && Waste.GetSnapshot().Depots.IsEmpty() &&
	        !People.TransitTrips.ContainsByPredicate([](const auto& Trip) { return Trip.IsWasteJourney(); }));
	TestTrue(TEXT("Demolition ledger remains conserved"),
	         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), 75, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWasteIntegrationTest,
                                 "CitySim.Waste.NativeFacilitiesSaveReplayAndPresentation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWasteIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WasteIntegration"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!Sim || !Catalog)
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	Sim->PlacementModel = MakeUnique<FCityPlacementModel>(100000);
	auto& City = Sim->GetPlacementModel();
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {40, 8});
	City.CommitRoad(Road);
	auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(P, *D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {8, 9});
	Place(TEXT("Home"), {4, 9});
	Place(TEXT("Home"), {18, 9});
	Place(TEXT("Home"), {30, 9});
	Place(TEXT("Power"), {1, 9});
	Place(TEXT("Service"), {20, 9});
	const uint64 Depot = Place(TEXT("WasteDepot"), {12, 9}), Recycle = Place(TEXT("Recycling"), {26, 9});
	if (!TestTrue(TEXT("Actual facilities placed"), Home && Depot && Recycle))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->SetSimulationPaused(false);
	for (int32 I = 0; I < 8; ++I)
		Sim->Tick(.125f);
	Sim->SetSimulationPaused(true);
	auto People = Sim->Population.GetSnapshot();
	for (auto& C : People.Citizens)
		C.Education = ECityEducationLevel::Secondary;
	FString Error;
	Sim->Population.RestoreState(People, Error);
	Sim->RefreshDerivedSnapshots();
	Sim->Population.Reconcile(City, Sim->GetEconomySnapshot(), Sim->EconomyTuning, &Sim->ServiceSnapshot);
	Sim->RefreshDerivedSnapshots();
	TestTrue(TEXT("Basic depot workforce is recognized"),
	         Sim->GetServiceSnapshot().PublicJobs.FindRef(Depot) == 4);
	TestTrue(TEXT("Debug pile accessible in native simulation"), Sim->StartWasteTest(Home));
	Sim->SetSimulationPaused(false);
	for (int32 I = 0; I < 8; ++I)
		Sim->Tick(.125f);
	Sim->SetSimulationPaused(true);
	TestTrue(TEXT("Native collection runs on actual traffic"), Sim->GetWasteSnapshot().Collected > 0);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = Catalog;
	Board->DispatchBeginPlay();
	Board->SetToolMode(ECityToolMode::WasteDepot);
	TestEqual(TEXT("Tool selects waste depot"), Board->GetSelectedFacilityId(), FName(TEXT("WasteDepot")));
	FVector Roof;
	FString Detail;
	TArray<FCityInspectionMetric> Metrics;
	Board->GetInspectionBuilding(Depot, Roof, Detail, &Metrics);
	TestTrue(TEXT("Inspect includes storage, rate, fleet, range"),
	         Detail.Contains(TEXT("Waste storage")) && Detail.Contains(TEXT("largest piles")));
	const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview"));
	IFileManager::Get().MakeDirectory(*Folder, true);
	FString Json;
	TestTrue(TEXT("Native document serializes"),
	         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Json, Error));
	FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Folder, TEXT("Waste-Starter.json")));
	const FString Save =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Waste-Pending.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Save), true);
	TestTrue(TEXT("Native save with truck/plan/cargo"), Sim->SaveCityToPath(Save, Error));
	int64 ExpectedGenerated = -1, ExpectedCollected = -1;
	for (int32 Speed : {1, 3, 5})
	{
		if (!TestTrue(TEXT("Native load"), Sim->LoadCityFromPath(Save, Error, false)))
		{
			AddError(Error);
			break;
		}
		Sim->SetSimulationSpeed(Speed);
		Sim->SetSimulationPaused(false);
		for (int32 I = 0; I < 20; ++I)
			Sim->Tick(.125f / Speed);
		Sim->SetSimulationPaused(true);
		if (ExpectedGenerated < 0)
		{
			ExpectedGenerated = Sim->GetWasteSnapshot().Generated;
			ExpectedCollected = Sim->GetWasteSnapshot().Collected;
		}
		TestEqual(TEXT("Generation replay independent of speed"), Sim->GetWasteSnapshot().Generated,
		          ExpectedGenerated);
		TestEqual(TEXT("Actual collection replay independent of speed"), Sim->GetWasteSnapshot().Collected,
		          ExpectedCollected);
	}
	TestTrue(TEXT("Conservation native final"),
	         FCityWasteSimulation::ValidateState(Sim->GetWasteSnapshot(),
	                                             Sim->GetEconomySnapshot().FixedStepId, Error));

	auto Export = [&](const TCHAR* Name) {
		FString J;
		TestTrue(TEXT("Native waste preview document"),
		         FCityPersistence::Serialize(Sim->BuildSaveDocument(), J, Error));
		FFileHelper::SaveStringToFile(J, *FPaths::Combine(Folder, Name));
	};
	Sim->SetSimulationSpeed(1);
	TestTrue(TEXT("Waste upgrade preview uses processing/storage and actual five-day work"),
	         Sim->DescribeStationUpgrade(Depot).Contains(TEXT("processing160/day")) &&
	             Sim->DescribeStationUpgrade(Depot).Contains(TEXT("5 game days")));
	TestTrue(TEXT("Depot paid expansion"), Sim->PurchaseStationUpgrade(Depot, ECityDensityLevel::Low));
	TestTrue(TEXT("Recycling paid expansion"), Sim->PurchaseStationUpgrade(Recycle, ECityDensityLevel::Low));
	Export(TEXT("Waste-Upgrading.json"));
	Sim->SetSimulationPaused(false);
	for (int32 I = 0; I < 12; ++I)
		Sim->Tick(.125f);
	Sim->SetSimulationPaused(true);
	TestEqual(TEXT("Depot expanded with completed paid work"),
	          Sim->GetDevelopmentSnapshot().Find(Depot)->DensityLevel, ECityDensityLevel::Medium);
	TestEqual(TEXT("Expanded storage"), Sim->GetWastePolicy(Depot).Storage, 4800);
	Export(TEXT("Waste-Expanded.json"));
	TestTrue(TEXT("Damage applies to waste facility"), Sim->Fire.ApplyNaturalDamage(Depot, 1, 50));
	Sim->RefreshDerivedSnapshots();
	TestEqual(TEXT("Damaged depot no collection fleet"), Sim->GetWastePolicy(Depot).Fleet, 0);
	Export(TEXT("Waste-Damaged.json"));
	TestTrue(TEXT("Damage preserves full waste ledger"),
	         FCityWasteSimulation::ValidateState(Sim->GetWasteSnapshot(),
	                                             Sim->GetEconomySnapshot().FixedStepId, Error));
	IFileManager::Get().Delete(*Save, false, true);
	IFileManager::Get().Delete(*(Save + TEXT(".bak")), false, true);
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityWasteLimitsTest,
                                 "CitySim.Waste.FullStorageRecyclingOutagesCapsAndMigration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityWasteLimitsTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!Catalog)
		return false;
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {40, 8});
	City.CommitRoad(Road);
	auto Place = [&](FName Name, FCityCellCoord Cell) {
		auto* D = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(P, *D).EntityId;
	};
	const uint64 Depot = Place(TEXT("WasteDepot"), {1, 9}), Recycle = Place(TEXT("Recycling"), {12, 9}),
	             Home = Place(TEXT("Home"), {8, 9});
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(City, FCityRoadDefinition::Local(), Network);
	FCityDevelopmentSimulation Dev;
	Dev.InitializeExistingAsOperational(City, Catalog, 0);
	auto Development = Dev.GetSnapshot();
	FCityServiceSnapshot Services;
	Services.PublicStaffPercent.Add(Depot, 100);
	Services.PublicStaffPercent.Add(Recycle, 100);
	FCityPopulationSnapshot People;
	FCityWasteSimulation Waste;
	Waste.Update(0, 25, City, Catalog, Network, Services, Development, People, false);
	auto Seed = Waste.GetSnapshot();
	Seed.IntroductionStep = 0;
	Seed.Generated = 2601;
	Seed.Trucks.Reset();
	Seed.NextTruckId = 2;
	Seed.Depots.FindByPredicate([&](const auto& D) { return D.EntityId == Depot; })->Stored = 2400;
	Seed.Depots.FindByPredicate([&](const auto& D) { return D.EntityId == Recycle; })->Stored = 101;
	FCityWasteTruck Truck;
	Truck.Id = 1;
	Truck.DepotId = Truck.CurrentEntityId = Depot;
	Truck.Cargo = 100;
	Seed.Trucks.Add(Truck);
	FString Error;
	TestTrue(TEXT("Conserved full-storage fixture"), Waste.RestoreState(Seed, Error));
	Waste.Update(1, 25, City, Catalog, Network, Services, Development, People, true);
	TestEqual(TEXT("Unpaid operating bills stop processing"), Waste.GetSnapshot().Disposed, int64(0));
	TestEqual(TEXT("Full unpaid depot retains cargo"), Waste.GetSnapshot().InTrucks(), int64(100));
	Services.EntityFailures.Add(Depot, ECityOperationFailure::NoPower);
	Services.EntityFailures.Add(Recycle, ECityOperationFailure::NoService);
	Waste.Update(2, 25, City, Catalog, Network, Services, Development, People, false);
	TestEqual(TEXT("Utility outage processing stops"), Waste.GetSnapshot().Disposed, int64(0));
	Services.EntityFailures.Reset();
	Waste.Update(3, 25, City, Catalog, Network, Services, Development, People, false);
	TestEqual(TEXT("Full depot accepts only newly freed80 units"), Waste.GetSnapshot().Depots[0].Stored,
	          2400);
	TestEqual(TEXT("Remaining20 waits in truck"), Waste.GetSnapshot().InTrucks(), int64(20));
	TestEqual(TEXT("First recycling batch diverts48"), Waste.GetSnapshot().Recycled, int64(48));
	Waste.Update(4, 25, City, Catalog, Network, Services, Development, People, false);
	TestEqual(TEXT("101 processed conserves floor60 recycled"), Waste.GetSnapshot().Recycled, int64(60));
	TestEqual(TEXT("Depot closes with finite2340"), Waste.GetSnapshot().Depots[0].Stored, 2340);
	TestTrue(TEXT("Transfers and recycling conserve all2601"),
	         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), 4, Error));
	Development.States.FindChecked(Depot).DensityLevel = ECityDensityLevel::Medium;
	auto P = FCityWasteSimulation::Policy(Depot, City, Catalog, Development, Services, false);
	TestTrue(TEXT("Expanded processing/storage/fleet"),
	         P.ProcessingPerStep == 160 && P.Storage == 4800 && P.Fleet == 3);
	Services.PublicStaffPercent.Add(Depot, 50);
	P = FCityWasteSimulation::Policy(Depot, City, Catalog, Development, Services, false);
	TestTrue(TEXT("Staff scales capacity"), P.ProcessingPerStep == 80 && P.Fleet == 1);
	Services.PublicStaffPercent.Add(Depot, 0);
	Services.PublicStaffPercent.Add(Recycle, 0);
	FCityCitizenRecord Resident;
	Resident.Id = 1;
	Resident.HomeEntityId = Home;
	People.Citizens.Add(Resident);
	People.NextCitizenId = 2;
	Waste.AddTestPile(Home);
	const int32 Cap = Waste.GetSnapshot().Find(Home)->Capacity;
	Waste.Update(154, 25, City, Catalog, Network, Services, Development, People, false);
	TestEqual(TEXT("Pile cannot grow beyond cap"), Waste.GetSnapshot().Find(Home)->Units, Cap);
	TestEqual(TEXT("Overflow records12 excess units"), Waste.GetSnapshot().Overflow, int64(12));
	TestEqual(TEXT("Grace expires and capped lot applies bounded pressure"),
	          Waste.GetSnapshot().Pressure(Home, 25), 15);
	TestTrue(TEXT("Cap excess remains conserved"),
	         FCityWasteSimulation::ValidateState(Waste.GetSnapshot(), 154, Error));
	return true;
}
#endif
