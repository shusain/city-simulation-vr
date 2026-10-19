#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Serialization/JsonSerializer.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenHousingTest, "CitySim.Simulation.CitizenUnemploymentHousing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCitizenHousingTest::RunTest(const FString& Parameters)
{
	FCityPlacementModel City(10000);
	const auto Place = [&](const TCHAR* Id, FCityCellCoord Cell) {
		FCityBuildingDefinition D;
		D.DefinitionId = Id;
		D.DisplayName = Id;
		D.Footprint = {1, 1};
		D.Cost = 0;
		auto Preview = City.BeginPreview(D, Cell);
		return City.CommitPlacement(Preview, D).EntityId;
	};
	const uint64 Home = Place(TEXT("Home"), {1, 1}), Shop = Place(TEXT("Shop"), {3, 1});
	FCityEconomyTuning T;
	FCityEconomySnapshot E;
	E.Residents = 4;
	FCityServiceSnapshot S;
	S.EntityResidents.Add(Home, 4);
	S.EntityJobs.Add(Shop, 0);
	FCityPopulationSimulation P;
	P.Reconcile(City, E, T, &S);
	auto State = P.GetSnapshot();
	State.Citizens[0].AvailableMoney = 73;
	State.Citizens[0].Education = ECityEducationLevel::Higher;
	const uint64 Id = State.Citizens[0].Id;
	FString Error;
	TestTrue(TEXT("Supported household restores"), P.RestoreState(State, Error));
	E.FixedStepId = 100;
	S.EntityResidents[Home] = 0;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Temporary utility outage retains physical home"), P.BuildAggregate(T).HomelessResidents,
	          0);
	S.EntityResidents[Home] = 4;
	E.FixedStepId = 300;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Unemployment year alone no longer evicts residents"),
	          P.BuildAggregate(T).HomelessResidents, 0);
	TestEqual(TEXT("Housing retains lifetime identity"), P.GetSnapshot().Citizens[0].Id, Id);
	TestEqual(TEXT("Housing retains funds"), P.GetSnapshot().Citizens[0].AvailableMoney, int64(73));
	TestEqual(TEXT("Housing retains education"), P.GetSnapshot().Citizens[0].Education,
	          ECityEducationLevel::Higher);
	TestEqual(TEXT("Reconciliation never invents discontent history"),
	          P.GetSnapshot().Citizens[0].DiscontentMonths, 0);
	City.DemolishEntity(Home);
	S.EntityResidents.Remove(Home);
	E.Residents = 0;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Demolition causes real displacement"), P.BuildAggregate(T).HomelessResidents, 4);
	TestEqual(TEXT("Displaced people remain actual city records"), P.GetSnapshot().Citizens.Num(), 4);
	const uint64 Replacement = Place(TEXT("Home"), {5, 1});
	S.EntityResidents.Add(Replacement, 4);
	E.Residents = 4;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Available habitable housing does not require a conventional job"),
	          P.BuildAggregate(T).HomelessResidents, 0);
	TestEqual(TEXT("Rehousing retains money"), P.GetSnapshot().Citizens[0].AvailableMoney, int64(73));
	E.FixedStepId = 301;
	E.Jobs = 2;
	S.EntityJobs[Shop] = 2;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Employment still resets tracking clock"), P.GetSnapshot().Citizens[0].UnemployedSinceStep,
	          int64(-1));
	E.FixedStepId = 302;
	E.Jobs = 0;
	S.EntityJobs[Shop] = 0;
	P.Reconcile(City, E, T, &S);
	TestEqual(TEXT("Lost employment is still tracked for census"),
	          P.GetSnapshot().Citizens[0].UnemployedSinceStep, int64(302));
	TestEqual(TEXT("Lost employment is distinct from housing security"),
	          P.BuildAggregate(T).HomelessResidents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenHousingSaveTest,
                                 "CitySim.Persistence.CitizenEducationAndUnemployment",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCitizenHousingSaveTest::RunTest(const FString& Parameters)
{
	FCitySaveDocument D;
	D.Placement = FCityPlacementModel(10000).ExportState();
	D.Economy.FixedStepId = 300;
	D.Economy.MonthId = 12;
	FCityCitizenRecord C;
	C.Id = 7;
	C.Education = ECityEducationLevel::Secondary;
	C.UnemployedSinceStep = 0;
	C.AvailableMoney = 91;
	D.Population.Citizens.Add(C);
	D.Population.NextCitizenId = 8;
	FString Json, Error;
	FCitySaveDocument Restored;
	TestTrue(TEXT("Homeless citizen serializes"), FCityPersistence::Serialize(D, Json, Error));
	if (!TestTrue(TEXT("Homeless citizen loads"), FCityPersistence::Deserialize(Json, Restored, Error)))
	{
		AddError(Error);
		return false;
	}
	FCityPopulationSimulation P;
	TestTrue(TEXT("Homeless citizen restores without a household"),
	         P.RestoreState(Restored.Population, Error));
	TestEqual(TEXT("Saved education"), Restored.Population.Citizens[0].Education,
	          ECityEducationLevel::Secondary);
	TestEqual(TEXT("Saved unemployment start"), Restored.Population.Citizens[0].UnemployedSinceStep,
	          int64(0));
	TestEqual(TEXT("Saved homeless wallet"), Restored.Population.Citizens[0].AvailableMoney, int64(91));
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	const auto Citizen =
	    Root->GetObjectField(TEXT("population"))->GetArrayField(TEXT("citizens"))[0]->AsObject();
	Citizen->SetNumberField(TEXT("education"), 4);
	FString Invalid;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("Unknown education rejects save"),
	          FCityPersistence::Deserialize(Invalid, Restored, Error));
	Citizen->SetNumberField(TEXT("education"), 2);
	Citizen->SetStringField(TEXT("unemployedSinceStep"), TEXT("301"));
	Invalid.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Invalid));
	TestFalse(TEXT("Future unemployment clock rejects save"),
	          FCityPersistence::Deserialize(Invalid, Restored, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCitizenHousingIntegrationTest,
                                 "CitySim.Simulation.CitywideHousingResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCitizenHousingIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* FineWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HousingFine"), nullptr, true,
	                                        ERHIFeatureLevel::Num, &Values);
	UWorld* CoarseWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HousingCoarse"), nullptr, true,
	                                          ERHIFeatureLevel::Num, &Values);
	auto* Fine = FineWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Coarse = CoarseWorld->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Real catalog loads"), Catalog))
	{
		FineWorld->DestroyWorld(false);
		CoarseWorld->DestroyWorld(false);
		return false;
	}
	// Hold housing capacity at ten so automatic density upgrades do not add
	// a second incoming cohort while testing the first unemployment year.
	auto* FixedCatalog = NewObject<UCityBuildingCatalog>();
	FixedCatalog->Definitions = Catalog->Definitions;
	for (auto& Definition : FixedCatalog->Definitions)
		if (Definition.DefinitionId == TEXT("Home") && Definition.DensityLevels.Num() > 1)
			Definition.DensityLevels.SetNum(1);
	Catalog = FixedCatalog;
	for (auto* Sim : {Fine, Coarse})
	{
		Sim->RegisterBuildingCatalog(Catalog);
		auto& City = Sim->GetPlacementModel();
		City.SetTerrain(nullptr);
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		City.UpdateRoadPreviewEnd(Road, {30, 8});
		City.CommitRoad(Road);
		int32 X = 1;
		for (const TCHAR* Id : {TEXT("Power"), TEXT("Service"), TEXT("Home")})
		{
			const auto* D = Catalog->FindDefinition(Id);
			auto P = City.BeginPreview(*D, {X, 9});
			TestTrue(TEXT("Housing fixture places"), City.CommitPlacement(P, *D).IsSuccess());
			X += 3;
		}
		Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
		Sim->EconomyTuning.FixedStepSeconds = .125;
		Sim->EconomyTuning.FixedStepsPerMonth = 4;
		Sim->EconomyTuning.MaxCatchUpSteps = 8;
		Sim->EconomyTuning.CrimePopulationThreshold = 100000;
		Sim->EconomyTuning.FirePopulationThreshold = 100000;
		Sim->Scenario.Start();
		Sim->SetSimulationPaused(false);
	}
	FString Error, A, B;
	for (int32 Group = 0; Group < 8; ++Group)
	{
		for (int32 Step = 0; Step < 8; ++Step)
			Fine->Tick(.125f);
		Coarse->Tick(1.f);
		FCityPersistence::Serialize(Fine->BuildSaveDocument(), A, Error);
		FCityPersistence::Serialize(Coarse->BuildSaveDocument(), B, Error);
		TestEqual(TEXT("Fine/coarse city equality through prolonged unemployment"), A, B);
	}
	TestEqual(TEXT("Total population includes homeless people"), Fine->GetEconomySnapshot().Residents, 10);
	TestEqual(TEXT("Unemployment alone does not manufacture homelessness"),
	          Fine->GetCrimeSnapshot().HomelessResidents, 0);
	TestEqual(TEXT("Needs shows actual housed occupancy"), Fine->GetNeedsSnapshot().OccupiedResidents, 10);
	const auto Citizens = Fine->GetPopulationSnapshot().Citizens;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Housing/City.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("Whole city saves supported unemployed residents"), Fine->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Whole city restores supported unemployed residents"),
	         Fine->LoadCityFromPath(Path, Error, false));
	FCityPersistence::Serialize(Fine->BuildSaveDocument(), A, Error);
	auto* D = Catalog->FindDefinition(TEXT("Shop"));
	auto Preview = Fine->PlacementModel->BeginPreview(*D, {14, 9});
	const uint64 Shop = Fine->PlacementModel->CommitPlacement(Preview, *D).EntityId;
	Fine->Development.InitializeExistingAsOperational(*Fine->PlacementModel, Catalog, 64);
	Fine->Population.Reconcile(*Fine->PlacementModel, Fine->GetEconomySnapshot(), Fine->EconomyTuning,
	                           &Fine->ServiceSnapshot);
	auto PopulationState = Fine->GetPopulationSnapshot();
	// The response is specifically to usable jobs, after the normal opening delivery.
	auto* Business = PopulationState.Businesses.FindByPredicate(
	    [&](const auto& Record) { return Record.EntityId == Shop; });
	if (Business)
		Business->bInitialFreightReceived = true;
	TestTrue(TEXT("Opened shop fixture restores"), Fine->Population.RestoreState(PopulationState, Error));
	Fine->SetSimulationPaused(false);
	Fine->Tick(.125f);
	TestEqual(TEXT("Job recovery retains current housing"), Fine->GetCrimeSnapshot().HomelessResidents, 0);
	TestEqual(TEXT("Job recovery retains lifetime IDs"), Fine->GetPopulationSnapshot().Citizens[0].Id,
	          Citizens[0].Id);
	TestEqual(TEXT("Job recovery does not create additional people"),
	          Fine->GetPopulationSnapshot().Citizens.Num(), 10);
	FineWorld->DestroyWorld(false);
	CoarseWorld->DestroyWorld(false);
	return true;
}
#endif
