#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Interaction/CityPaletteModel.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityVRToolController.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHospitalServiceTest,
                                 "CitySim.Health.HospitalStaffingUtilitiesUpgradeAndSave",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHospitalServiceTest::RunTest(const FString& Parameters)
{
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Catalog"), Catalog))
		return false;
	const auto* Definition = Catalog->FindDefinition(TEXT("Hospital"));
	if (!TestNotNull(TEXT("Hospital"), Definition))
		return false;
	TestTrue(TEXT("Valid two paid stages"), Definition->IsValid() && Definition->DensityLevels.Num() == 2 &&
	                                            Definition->bPlayerPurchasedLevels);
	TestEqual(TEXT("Secondary staff required"), Definition->MinimumStaffEducation, 2);
	TestEqual(TEXT("Shared hover cost identity"),
	          FCityPaletteModel::ConstructionDefinition(ECityPaletteAction::SelectHospital),
	          FName(TEXT("Hospital")));
	const auto Entries = FCityPaletteModel::Entries(ECityPalettePage::Build);
	TestTrue(TEXT("Build offers hospital"), Entries.ContainsByPredicate([](const auto& E) {
		return E.Action == ECityPaletteAction::SelectHospital;
	}));
	for (const auto& Level : Definition->DensityLevels)
	{
		auto* Mesh = Level.PresentationMesh.LoadSynchronous();
		if (TestNotNull(TEXT("Stage mesh"), Mesh))
			TestTrue(TEXT("Reviewed opaque windows and civic surface geometry"),
			         Mesh->GetNumTriangles(0) <= 1028 && Mesh->GetNumSections(0) == 7);
		const FString Package =
		    Level.PresentationMesh.ToSoftObjectPath().GetLongPackageName().LeftChop(2) + TEXT("_Damaged_A");
		TestNotNull(
		    TEXT("Damaged stage exists"),
		    LoadObject<UStaticMesh>(nullptr, *(Package + TEXT(".") +
		                                       Package.Mid(Package.Find(TEXT("/"), ESearchCase::CaseSensitive,
		                                                                ESearchDir::FromEnd) +
		                                                   1))));
	}
	FCityPlacementModel City(100000);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {40, 8});
	City.CommitRoad(Road);
	const auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto Preview = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(Preview, *D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {4, 9});
	Place(TEXT("Power"), {1, 9});
	Place(TEXT("Service"), {20, 9});
	// Admission respects actual housing: the fixture requests16 residents,
	// including ten clinical staff plus four utility staff after expansion.
	Place(TEXT("Home"), {14, 9});
	const int64 Before = City.GetFunds();
	const uint64 Hospital = Place(TEXT("Hospital"), {8, 9});
	if (!TestTrue(TEXT("Hospital placed"), Hospital != 0))
		return false;
	TestEqual(TEXT("Placement charged once"), Before - City.GetFunds(), Definition->Cost);
	FCityDevelopmentSimulation Development;
	Development.InitializeExistingAsOperational(City, Catalog, 0);
	FCityEconomyTuning Tuning;
	FCityEconomySnapshot Economy;
	Economy.Residents = 16;
	FCityPopulationSimulation People;
	FString Error;
	const auto Services = [&](const FCityFireSnapshot* Fire = nullptr) {
		return FCityServiceEvaluator::BuildSnapshot(City, Tuning, Catalog, &Development.GetSnapshot(),
		                                            nullptr, &People.GetSnapshot(), Fire);
	};
	auto Initial = Services();
	People.Reconcile(City, Economy, Tuning, &Initial);
	auto Basic = Services();
	People.Reconcile(City, Economy, Tuning, &Basic);
	TestEqual(TEXT("Basic citizens cannot fill clinical roles"), People.GetSnapshot().Workers(Hospital), 0);
	TestEqual(TEXT("No uneducated treatment capacity"), Services().HospitalCapacities.FindRef(Hospital), 0);
	auto Educated = People.GetSnapshot();
	for (auto& Citizen : Educated.Citizens)
		Citizen.Education = ECityEducationLevel::Secondary;
	TestTrue(TEXT("Qualified citizens restore"), People.RestoreState(Educated, Error));
	Basic = Services();
	People.Reconcile(City, Economy, Tuning, &Basic);
	TestEqual(TEXT("Six actual staff hire"), People.GetSnapshot().Workers(Hospital), 6);
	Economy.MonthId = 1;
	People.SettleMonth(Economy, Tuning, false);
	auto Ready = Services();
	TestEqual(TEXT("Paid staff open 24 places"), Ready.HospitalCapacities.FindRef(Hospital), 24);
	TestEqual(TEXT("Starter ambulance capacity"), Ready.HospitalFleetCapacities.FindRef(Hospital), 1);
	TestEqual(TEXT("Hospital upkeep counted"), Ready.CivicUpkeep, int64(70));
	Economy.MonthId = 2;
	People.SettleMonth(Economy, Tuning, false, false, 0);
	TestEqual(TEXT("Unpaid payroll closes care"), Services().HospitalCapacities.FindRef(Hospital), 0);
	Economy.MonthId = 3;
	People.SettleMonth(Economy, Tuning, false);
	TestEqual(TEXT("Payroll recovery restores care"), Services().HospitalCapacities.FindRef(Hospital), 24);
	Tuning.PowerCapacityPerBuilding = 20;
	Tuning.WaterCapacityPerTreatment = 20;
	auto Scarce = Services();
	TestTrue(TEXT("Hospital receives scarce utilities before earlier-ID home"),
	         Scarce.GetFailure(Hospital) == ECityOperationFailure::None &&
	             Scarce.GetFailure(Home) != ECityOperationFailure::None);
	Tuning.PowerCapacityPerBuilding = 300;
	Tuning.WaterCapacityPerTreatment = 300;
	Ready = Services();
	const int64 UpgradeFunds = City.GetFunds();
	TestTrue(TEXT("Ward purchase"),
	         Development.PurchaseCivicUpgrade(Hospital, ECityDensityLevel::Low, City, Catalog, Ready, Error));
	TestEqual(TEXT("Upgrade charged once"), UpgradeFunds - City.GetFunds(), int64(1800));
	TestFalse(
	    TEXT("Repeated upgrade rejected"),
	    Development.PurchaseCivicUpgrade(Hospital, ECityDensityLevel::Low, City, Catalog, Ready, Error));
	TestEqual(TEXT("Pending ward retains starter capacity"), Services().HospitalCapacities.FindRef(Hospital),
	          24);
	FCityDevelopmentSimulation Resumed;
	TestTrue(TEXT("Pending ward restores"),
	         Resumed.RestoreState(Development.GetSnapshot(), City, Catalog, 0, Error));
	FCityFireSnapshot Damage;
	Damage.DamagedBuildings.Add(Hospital, {50, -1, 1});
	TestTrue(TEXT("Hospital can burn"), FCityFireSimulation::IsFireTarget(TEXT("Hospital"), Tuning));
	TestEqual(TEXT("Damage closes care"), Services(&Damage).HospitalCapacities.FindRef(Hospital), 0);
	FCityNeedsSnapshot Needs;
	FCityEnvironmentSnapshot Weather;
	auto Disabled = Services(&Damage);
	Development.AdvanceToStep(5, City, Catalog, Disabled, Needs, Weather, Tuning);
	TestEqual(TEXT("Damage holds ward completion"), Development.GetSnapshot().Find(Hospital)->DensityLevel,
	          ECityDensityLevel::Low);
	Ready = Services();
	Development.AdvanceToStep(6, City, Catalog, Ready, Needs, Weather, Tuning);
	TestEqual(TEXT("Recovery permits ward completion"),
	          Development.GetSnapshot().Find(Hospital)->DensityLevel, ECityDensityLevel::Medium);
	Ready = Services();
	People.Reconcile(City, Economy, Tuning, &Ready);
	Economy.MonthId = 4;
	People.SettleMonth(Economy, Tuning, false);
	TestEqual(TEXT("Expanded hospital hires ten qualified staff"), People.GetSnapshot().Workers(Hospital),
	          10);
	TestEqual(TEXT("Paid expanded capacity"), Services().HospitalCapacities.FindRef(Hospital), 48);
	TestEqual(TEXT("Expanded fleet"), Services().HospitalFleetCapacities.FindRef(Hospital), 2);
	TestFalse(TEXT("No third ward purchase"),
	          Development.PurchaseCivicUpgrade(Hospital, ECityDensityLevel::Medium, City, Catalog, Services(),
	                                           Error));
	FCitySaveDocument Document, Loaded;
	Document.Placement = City.ExportState();
	Document.Development = Development.GetSnapshot();
	Document.Economy.FixedStepId = 6;
	Document.Disasters.LastFixedStepId = 6;
	FString Json;
	if (!TestTrue(*FString::Printf(TEXT("Hospital stage saves: %s"), *Error),
	              FCityPersistence::Serialize(Document, Json, Error)))
		return false;
	if (!TestTrue(*FString::Printf(TEXT("Hospital stage loads: %s"), *Error),
	              FCityPersistence::Deserialize(Json, Loaded, Error)))
		return false;
	if (!TestNotNull(TEXT("Loaded hospital stage exists"), Loaded.Development.Find(Hospital)))
		return false;
	TestEqual(TEXT("Paid ward identity retained"), Loaded.Development.Find(Hospital)->DensityLevel,
	          ECityDensityLevel::Medium);
	Document.SchemaVersion = 53;
	TestFalse(TEXT("Old schema cannot lose hospital mechanics"),
	          FCityPersistence::Serialize(Document, Json, Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHospitalIntegrationTest,
                                 "CitySim.Health.HospitalNativeConstructionInspectRepairAndReplay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHospitalIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HospitalIntegration"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	City.ApplyMonthlyEconomy(100000, 0);
	const auto Road = [&](FCityCellCoord A, FCityCellCoord B) {
		auto P = City.BeginRoadPreview(A);
		City.UpdateRoadPreviewEnd(P, B);
		City.CommitRoad(P);
	};
	Road(FCityPlacementModel::EntranceCell, {19, 8});
	Road({3, 8}, {3, 12});
	Road({1, 12}, {19, 12});
	Road({19, 8}, {19, 12});
	const auto Place = [&](FName Name, FCityCellCoord Cell) {
		const auto* D = Catalog->FindDefinition(Name);
		auto P = City.BeginPreview(*D, Cell);
		return City.CommitPlacement(P, *D).EntityId;
	};
	Place(TEXT("Power"), {1, 9});
	Place(TEXT("Service"), {1, 13});
	Place(TEXT("SecondarySchool"), {13, 13});
	for (int32 X : {4, 8, 12, 16})
		Place(TEXT("Home"), {X, 9});
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.NewArrivalUneducatedPercent = 0;
	Sim->EconomyTuning.FirePopulationThreshold = Sim->EconomyTuning.CrimePopulationThreshold = 100000;
	Sim->SetDisasterFrequency(ECityDisasterFrequency::Off);
	Sim->StartScenario();
	for (int32 Step = 0; Step < 40; ++Step)
		Sim->Tick(.125f);
	TestTrue(TEXT("Real school graduates clinical candidates"),
	         Sim->GetServiceSnapshot().SecondaryResidents >= 6);
	const uint64 Hospital = Place(TEXT("Hospital"), {8, 13});
	if (!TestTrue(TEXT("Native hospital placed"), Hospital != 0))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->SetSimulationPaused(true);
	Sim->Tick(0);
	TestFalse(TEXT("Placement initially under construction"),
	          Sim->GetDevelopmentSnapshot().IsOperational(Hospital));
	TestEqual(TEXT("No construction treatment capacity"),
	          Sim->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital), 0);
	Sim->SetSimulationPaused(false);
	for (int32 Step = 0; Step < 8; ++Step)
		Sim->Tick(.125f);
	TestTrue(TEXT("Hospital construction completes"), Sim->GetDevelopmentSnapshot().IsOperational(Hospital));
    // This normal admission city may be partially staffed. Full24/48-bed and
    // two-ambulance contracts are checked by the controlled qualified-roster
    // service test; native inspection must show this city's actual snapshot.
    const int32 StarterCapacity=Sim->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital);
    TestTrue(TEXT("Actual staffed starter hospital provides care"),StarterCapacity>0 && StarterCapacity<=24);
    for(const auto& Employer:Sim->GetPopulationSnapshot().PublicEmployers) if(Employer.EntityId==Hospital)
        AddInfo(FString::Printf(TEXT("Native clinical roster: citizens=%d Secondary=%d jobs=%d workers=%d payroll=%lld/%lld effective=%d/24"),
            Sim->GetPopulationSnapshot().Citizens.Num(),Sim->GetServiceSnapshot().SecondaryResidents,Employer.JobCapacity,
            Sim->GetPopulationSnapshot().Workers(Hospital),Employer.PayrollPaid,Employer.PayrollDue,StarterCapacity));
	const auto ExportFixture = [&](const TCHAR* Name) {
		FString Json, SaveError;
		TestTrue(TEXT("Preview fixture serializes"),
		         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Json, SaveError));
		const FString Folder = FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview"));
		IFileManager::Get().MakeDirectory(*Folder, true);
		TestTrue(TEXT("Preview fixture writes"),
		         FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Folder, Name)));
	};
	ExportFixture(TEXT("Hospital-Starter.json"));
	auto* Board = World->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = Catalog;
	Board->DispatchBeginPlay();
	Board->SetToolMode(ECityToolMode::Hospital);
	TestEqual(TEXT("Hospital tool selects hospital"), Board->GetSelectedFacilityId(),
	          FName(TEXT("Hospital")));
	TestTrue(TEXT("Shared hospital hover costs include payroll"),
	         Board->DescribeConstructionCost(TEXT("Hospital")).Contains(TEXT("$2400")) &&
	             Board->DescribeConstructionCost(TEXT("Hospital")).Contains(TEXT("full staff")));
	Board->SetToolMode(ECityToolMode::Inspect);
	Board->RebuildAllPresentation();
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = Board;
	auto* Owner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	Camera->SetWorldLocationAndRotation(FVector(0, 0, 160), FRotator(-45, 0, 0));
	UI->HeadsetCamera = Camera;
	FVector Roof;
	FString Detail;
	TArray<FCityInspectionMetric> Metrics;
	Board->GetInspectionBuilding(Hospital, Roof, Detail, &Metrics);
	TestTrue(TEXT("Inspect exposes treatment capacity and education"),
	         Detail.Contains(FString::Printf(TEXT("%d/24"),StarterCapacity)) && Detail.Contains(TEXT("Secondary staff")));
	UI->ToggleInspection(Roof + FVector(0, 0, 20), -FVector::UpVector);
	TestTrue(TEXT("Inspect offers priced ward"),
	         UI->InspectorUpgradeLabel->Text.ToString().Contains(TEXT("$1800")));
	const auto Click = [&]() {
		const auto N = UI->InspectorStatusRoot->GetForwardVector();
		return UI->HandleInspectorUpgradeRay(UI->InspectorUpgradeButton->GetComponentLocation() + N * 10, -N);
	};
	Sim->SetSimulationPaused(true);
	const int64 Funds = City.GetFunds();
	Click();
	TestEqual(TEXT("Review does not spend"), City.GetFunds(), Funds);
	Click();
	TestEqual(TEXT("Confirm spends ward price"), Funds - City.GetFunds(), int64(1800));
	FString Error;
	const FString Path =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Hospital-Pending.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!TestTrue(TEXT("Pending hospital saves"), Sim->SaveCityToPath(Path, Error)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
    int32 WardCapacity=-1;
	for (int32 Speed : {1, 3, 5})
	{
		if (!TestTrue(TEXT("Native pending hospital reloads"), Sim->LoadCityFromPath(Path, Error, false)))
		{
			AddError(Error);
			break;
		}
		Sim->SetSimulationSpeed(Speed);
		Sim->SetSimulationPaused(false);
		for (int32 Step = 0; Step < 4; ++Step)
			Sim->Tick(.125f / Speed);
		TestEqual(TEXT("No early ward benefit"), Sim->GetDevelopmentSnapshot().Find(Hospital)->DensityLevel,
		          ECityDensityLevel::Low);
		for (int32 Step = 0; Step < 4; ++Step)
			Sim->Tick(.125f / Speed);
		TestEqual(TEXT("Ward completes at every speed"),
		          Sim->GetDevelopmentSnapshot().Find(Hospital)->DensityLevel, ECityDensityLevel::Medium);
        const int32 ActualWardCapacity=Sim->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital);
        if(WardCapacity<0) WardCapacity=ActualWardCapacity;
        TestTrue(TEXT("Expanded staffed operation provides additional care"),ActualWardCapacity>StarterCapacity && ActualWardCapacity<=48);
        TestEqual(TEXT("Actual expanded operation matches across all speeds"),ActualWardCapacity,WardCapacity);
	}
	Sim->SetSimulationPaused(true);
	Board->PlacementModel = &Sim->GetPlacementModel();
	auto FireState = Sim->GetFireSnapshot();
	FireState.DamagedBuildings.Add(Hospital, {40, -1, 1});
	TestTrue(TEXT("Disaster damage restores"),
	         Sim->Fire.RestoreState(FireState, Sim->GetEconomySnapshot().FixedStepId, Error));
	Sim->RefreshDerivedSnapshots();
	const auto Damaged =
	    Board->ResolvePresentationDefinition(Hospital, *Catalog->FindDefinition(TEXT("Hospital")));
	TestTrue(TEXT("Correct expanded damage mesh"),
	         Damaged.PresentationMesh.ToSoftObjectPath().GetAssetName() ==
	             TEXT("SM_CivicGround_Building_Hospital_Medium_Damaged_A"));
	TestNotNull(TEXT("Expanded damage mesh loads"), Damaged.PresentationMesh.LoadSynchronous());
	TestEqual(TEXT("Disaster closes native hospital"),
	          Sim->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital), 0);
	ExportFixture(TEXT("Hospital-Damaged.json"));
	TestTrue(TEXT("Paid native hospital repair starts"), Sim->RepairBuilding(Hospital));
	Sim->SetSimulationPaused(false);
	for (int32 Step = 0; Step < Sim->EconomyTuning.FireRepairSteps + 8; ++Step)
		Sim->Tick(.125f / Sim->GetSimulationSpeed());
	TestEqual(TEXT("Repair restores expanded ward"),
	          Sim->GetServiceSnapshot().HospitalCapacities.FindRef(Hospital), WardCapacity);
	TestTrue(TEXT("Native payroll and employment reconcile"),
	         Sim->Population.MatchesEconomy(Sim->GetEconomySnapshot(), Sim->EconomyTuning));
	FString Fixture;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fixture, Error);
	IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview")),
	                                  true);
	FFileHelper::SaveStringToFile(
	    Fixture, *FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Preview/Hospital-City.json")));
	TArray<uint64> Patients;
	for (const auto& C : Sim->GetPopulationSnapshot().Citizens)
		if (!C.WorkplaceEntityId && !C.IsDetained() && Patients.Num() < 2)
			Patients.Add(C.Id);
	if (TestEqual(TEXT("Two stable emergency patients available"), Patients.Num(), 2))
	{
		for (const auto Id : Patients)
			TestTrue(TEXT("Start real severe illness"), Sim->Health.StartIllness(Id, ECityIllness::Severe));
		Sim->Population.SyncHealthAvailability(Sim->GetHealthSnapshot());
		Sim->SetSimulationPaused(false);
		Sim->Tick(.125f / Sim->GetSimulationSpeed());
		Sim->SetSimulationPaused(true);
        const int32 StaffedFleet=Sim->GetServiceSnapshot().HospitalFleetCapacities.FindRef(Hospital);
        TestTrue(TEXT("Expanded hospital has an actual staffed fleet"),StaffedFleet>0 && StaffedFleet<=2);
		TestEqual(
		    TEXT("Expanded hospital dispatches its actual staffed fleet"),
		    Sim->GetPopulationSnapshot()
		        .TransitTrips
		        .FilterByPredicate([](const auto& T) { return T.Purpose == ECityTripPurpose::HealthPickup; })
		        .Num(),
		    FMath::Min(StaffedFleet,Patients.Num()));
		ExportFixture(TEXT("Hospital-Responding.json"));
		TestTrue(TEXT("Pending ambulances save"), Sim->SaveCityToPath(Path, Error));
		int64 ExpectedRecovered = -1;
		for (int32 Speed : {1, 3, 5})
		{
			if (!TestTrue(TEXT("Pending medical journeys reload natively"),
			              Sim->LoadCityFromPath(Path, Error, false)))
			{
				AddError(Error);
				break;
			}
			TestTrue(TEXT("Load reconstructs sick leave"),
			         Sim->GetPopulationSnapshot().Citizens.ContainsByPredicate(
			             [&](const auto& C) { return Patients.Contains(C.Id) && C.bOnMedicalLeave; }));
			Sim->SetSimulationSpeed(Speed);
			Sim->SetSimulationPaused(false);
			for (int32 Step = 0; Step < 180; ++Step)
				Sim->Tick(.125f / Speed);
			TestTrue(TEXT("Visible native ambulances eventually treat both patients"),
			         Sim->GetHealthSnapshot().TotalRecovered >= 2);
			if (ExpectedRecovered < 0)
				ExpectedRecovered = Sim->GetHealthSnapshot().TotalRecovered;
			TestEqual(TEXT("Saved transport/treatment is deterministic at every speed"),
			          Sim->GetHealthSnapshot().TotalRecovered, ExpectedRecovered);
			TestFalse(TEXT("No duplicate or orphan medical journeys"),
			          Sim->GetPopulationSnapshot().TransitTrips.ContainsByPredicate(
			              [](const auto& T) { return T.IsHealthJourney(); }));
		}
		ExportFixture(TEXT("Hospital-Recovered.json"));
	}
	for (const FString& File : {Path, Path + TEXT(".bak")})
		IFileManager::Get().Delete(*File, false, true, true);
	World->DestroyWorld(false);
	return true;
}
#endif
