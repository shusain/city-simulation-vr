#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
struct FHappinessFixture
{
	FCityPlacementModel City{100000, FCityGridDimensions(80, 80)};
	FCityPopulationSimulation People;
	FCityEconomyTuning T;
	FCityEconomySnapshot E;
	FCityServiceSnapshot S;
	FCityEnvironmentSnapshot Environment{FCityGridDimensions(80, 80)};
	FCityHealthSnapshot Health;
	FCityWasteSnapshot Waste;
	uint64 Home = 0;
	FHappinessFixture()
	{
		FCityBuildingDefinition D;
		D.DefinitionId = TEXT("Home");
		D.DisplayName = TEXT("Home");
		D.Footprint = {1, 1};
		D.Cost = 1;
		auto Preview = City.BeginPreview(D, {78, 78});
		Home = City.CommitPlacement(Preview, D).EntityId;
		FCityPopulationSnapshot P;
		P.NextCitizenId = 2;
		auto& C = P.Citizens.Emplace_GetRef();
		C.Id = 1;
		C.HomeEntityId = C.HouseholdId = Home;
		C.AvailableMoney = 20;
		C.ConsumptionNeed = 1;
		auto& H = P.Households.Emplace_GetRef();
		H.Id = H.HomeEntityId = Home;
		H.Balance = 20;
		H.ConsumptionOrders = H.CompletedPurchases = 1;
		FString Error;
		check(People.RestoreState(P, Error));
		S.Dimensions = FCityGridDimensions(80, 80);
		S.EntityResidents.Add(Home, 1);
	}
	void Assess(int64 Month)
	{
		People.AdvanceHappiness(Month, City, S, Environment, E, T, Health, Waste);
	}
	void Hardship()
	{
		auto P = People.GetSnapshot();
		P.Households[0].CompletedPurchases = 0;
		P.Households[0].Balance = P.Citizens[0].AvailableMoney = 0;
		FString Error;
		check(People.RestoreState(P, Error));
		S.EntityFailures.Add(Home, ECityOperationFailure::NoPower);
		Environment.Pollution[78 * 80 + 78] = 1000;
		S.CrimeRisk.Init(0, 80 * 80);
		S.CrimeRisk[78 * 80 + 78] = 100;
	}
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHappinessConditionsTest,
                                 "CitySim.Happiness.NeedsSupportAndLocalConditions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHappinessConditionsTest::RunTest(const FString& Parameters)
{
	FHappinessFixture F;
	TestEqual(TEXT("New citizens begin neutral"), F.People.GetSnapshot().Citizens[0].Happiness, 60);
	F.Assess(1);
	TestEqual(TEXT("Supported unemployed citizen has a healthy outlook"),
	          F.People.GetSnapshot().Citizens[0].HappinessTarget, 85);
	TestEqual(TEXT("Monthly improvement is gradual"), F.People.GetSnapshot().Citizens[0].Happiness, 70);
	TestEqual(TEXT("No job alone causes no concern"), F.People.GetSnapshot().Citizens[0].HappinessConcerns,
	          0);
	F.S.ParkProviders.Add({2, {77, 78}, {1, 1}, 5, 30});
	F.S.EducationCitizenIds.Add(1);
	F.Assess(2);
	TestEqual(TEXT("Operating park and actual education access have bounded benefits"),
	          F.People.GetSnapshot().Citizens[0].HappinessTarget, 100);
	F.Hardship();
	F.Health.Patients.Add({1, 0, ECityIllness::Severe, ECityHealthCare::None});
	F.Waste.IntroductionStep = 0;
	F.Waste.LastStep = 200;
	F.Waste.Sites.Add({F.Home, 120, 60});
	F.E.ResidentialTaxBasisPoints = 3000;
	F.Assess(3);
	const auto& C = F.People.GetSnapshot().Citizens[0];
	TestEqual(TEXT("Severe compounded needs clamp at zero"), C.HappinessTarget, 0);
	TestEqual(TEXT("Harsh conditions do not instantly erase wellbeing"), C.Happiness, 70);
	for (const auto Flag :
	     {ECityHappinessConcern::Services, ECityHappinessConcern::Purchases, ECityHappinessConcern::Reserves,
	      ECityHappinessConcern::Pollution, ECityHappinessConcern::Safety, ECityHappinessConcern::Health,
	      ECityHappinessConcern::Waste, ECityHappinessConcern::Taxes})
		TestTrue(TEXT("Each observed concern is explained"),
		         (C.HappinessConcerns & static_cast<int32>(Flag)) != 0);
	const auto Summary = FCityHappinessEvaluator::Summarize(F.People.GetSnapshot(), F.Home);
	TestEqual(TEXT("Home summary uses resident scores"), Summary.Average, C.Happiness);
	TestEqual(TEXT("Different home does not inherit this household"),
	          FCityHappinessEvaluator::Summarize(F.People.GetSnapshot(), 999).Residents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHappinessHistoryTest,
                                 "CitySim.Happiness.PersistenceOfDiscontentAndRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHappinessHistoryTest::RunTest(const FString& Parameters)
{
	FHappinessFixture F;
	F.Hardship();
	for (int64 Month = 1; Month <= 5; ++Month)
	{
		F.Assess(Month);
		const auto Before = F.People.GetSnapshot().Citizens[0];
		F.Assess(Month);
		TestEqual(TEXT("Repeated/paused presentation cannot double assess"),
		          F.People.GetSnapshot().Citizens[0].Happiness, Before.Happiness);
		TestEqual(TEXT("Repeated month cannot extend discontent"),
		          F.People.GetSnapshot().Citizens[0].DiscontentMonths, Before.DiscontentMonths);
	}
	TestEqual(TEXT("First50 score is not below threshold; next four months are low"),
	          F.People.GetSnapshot().Citizens[0].DiscontentMonths, 4);
	TestEqual(TEXT("Persistent hardship distinguished from one bad month"),
	          FCityHappinessEvaluator::Summarize(F.People.GetSnapshot()).Sustained, 1);
	const auto Saved = F.People.GetSnapshot();
	F.E.Residents = 1;
	F.E.LastBilledMonthId = 5;
	F.People.Reconcile(F.City, F.E, F.T, &F.S);
	TestEqual(TEXT("Reconciliation preserves the citizen's wellbeing"),
	          F.People.GetSnapshot().Citizens[0].Happiness, Saved.Citizens[0].Happiness);
	TestEqual(TEXT("Reconciliation preserves the citizen's hardship history"),
	          F.People.GetSnapshot().Citizens[0].DiscontentMonths, 4);
	FHappinessFixture Healthy;
	auto P = Saved;
	P.Households[0].CompletedPurchases = 1;
	P.Households[0].Balance = P.Citizens[0].AvailableMoney = 20;
	FString Error;
	TestTrue(TEXT("Reload retains discontent"), Healthy.People.RestoreState(P, Error));
	for (int64 Month = 6; Month <= 9; ++Month)
		Healthy.Assess(Month);
	TestEqual(TEXT("Recovered at50 resets discontent only after gradual recovery"),
	          Healthy.People.GetSnapshot().Citizens[0].DiscontentMonths, 0);
	TestEqual(TEXT("Recovery clears resolved concerns"),
	          Healthy.People.GetSnapshot().Citizens[0].HappinessConcerns, 0);
	F.Assess(10);
	TestEqual(TEXT("Skipped months do not invent consecutive observations"),
	          F.People.GetSnapshot().Citizens[0].DiscontentMonths, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHappinessSaveTest,
                                 "CitySim.Happiness.SaveValidationAndNeutralOlderCities",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHappinessSaveTest::RunTest(const FString& Parameters)
{
	FHappinessFixture F;
	F.Hardship();
	for (int64 Month = 1; Month <= 4; ++Month)
		F.Assess(Month);
	FCitySaveDocument D(FCityGridDimensions(80, 80)), Read;
	D.Placement = F.City.ExportState();
	D.Population = F.People.GetSnapshot();
	D.Population.LastSettledMonthId = 4;
	D.Economy.LastBilledMonthId = D.Economy.MonthId = 4;
	D.Economy.FixedStepId = 100;
	FString Json, Error;
	if (!TestTrue(TEXT("Happiness city serializes"), FCityPersistence::Serialize(D, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("Happiness city loads"), FCityPersistence::Deserialize(Json, Read, Error)))
	{
		AddError(Error);
		return false;
	}
	const auto& C = Read.Population.Citizens[0];
	TestEqual(TEXT("Saved wellbeing retained"), C.Happiness, 20);
	TestEqual(TEXT("Saved discontent retained"), C.DiscontentMonths, 3);
	TestEqual(TEXT("Saved concerns retained"), C.HappinessConcerns,
	          D.Population.Citizens[0].HappinessConcerns);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	const auto Citizen =
	    Root->GetObjectField(TEXT("population"))->GetArrayField(TEXT("citizens"))[0]->AsObject();
	const auto Encode = [&]() {
		FString Result;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
		return Result;
	};
	FCitySaveDocument Unchanged;
	Unchanged.Placement.Funds = 1234;
	for (const auto* Field : {TEXT("happiness"), TEXT("happinessTarget"), TEXT("happinessConcerns"),
	                          TEXT("discontentMonths"), TEXT("lastHappinessMonth")})
	{
		const auto Value = Citizen->Values.FindChecked(Field);
		Citizen->RemoveField(Field);
		TestFalse(TEXT("Missing required state rejects load"),
		          FCityPersistence::Deserialize(Encode(), Unchanged, Error));
		TestEqual(TEXT("Failed load preserves destination"), Unchanged.Placement.Funds, int64(1234));
		Citizen->SetField(Field, Value);
	}
	Citizen->SetNumberField(TEXT("happiness"), 101);
	TestFalse(TEXT("Out of range score rejected"), FCityPersistence::Deserialize(Encode(), Unchanged, Error));
	Citizen->SetNumberField(TEXT("happiness"), 20);
	Citizen->SetNumberField(TEXT("happinessConcerns"), 512);
	TestFalse(TEXT("Unknown reason rejected"), FCityPersistence::Deserialize(Encode(), Unchanged, Error));
	Citizen->SetNumberField(TEXT("happinessConcerns"), C.HappinessConcerns);
	Citizen->SetStringField(TEXT("lastHappinessMonth"), TEXT("5"));
	TestFalse(TEXT("Future month rejected"), FCityPersistence::Deserialize(Encode(), Unchanged, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHappinessIntegrationTest,
                                 "CitySim.Happiness.LiveBillingPauseSpeedAndSharedInspection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHappinessIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HappinessIntegration"), nullptr, true,
	                                ERHIFeatureLevel::Num, &Values);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Board = W->SpawnActor<ACityBoardActor>();
	Board->BuildingCatalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Board->DispatchBeginPlay();
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	City.UpdateRoadPreviewEnd(Road, {63, 8});
	TestTrue(TEXT("Fixture road"), City.CommitRoad(Road).IsSuccess());
	uint64 Home = 0;
	const TCHAR* Ids[] = {TEXT("Power"), TEXT("Service"), TEXT("Home"), TEXT("Shop"), TEXT("School")};
	const int32 Xs[] = {1, 3, 6, 10, 14};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Ids); ++I)
	{
		const auto* Definition = Board->BuildingCatalog->FindDefinition(Ids[I]);
		auto Preview = City.BeginPreview(*Definition, {Xs[I], 9});
		const auto Placed = City.CommitPlacement(Preview, *Definition);
		TestTrue(TEXT("Fixture civic/private placement"), Placed.IsSuccess());
		if (I == 2)
			Home = Placed.EntityId;
	}
	Sim->Development.InitializeExistingAsOperational(City, Board->BuildingCatalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
	Sim->StartScenario();
	for (int32 I = 0; I < 3; ++I)
		Sim->Tick(.125f);
	FString Error;
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Housing-Initial.json"));
	TestTrue(TEXT("Initial neutral city saves"), Sim->SaveCityToPath(Path, Error));
	for (int32 I = 0; I < 15; ++I)
		Sim->Tick(.125f);
	FString Fine;
	TestTrue(TEXT("Fine monthly progression serializes"),
	         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fine, Error));
	TestTrue(TEXT("Actual live billing assesses existing citizens"),
	         Sim->GetPopulationSnapshot().Citizens.ContainsByPredicate(
	             [](const auto& C) { return C.LastHappinessMonth > 0; }));
	Sim->SetSimulationPaused(true);
	const auto Before = Sim->GetPopulationSnapshot().Citizens[0];
	Sim->Tick(5);
	Sim->RefreshDerivedSnapshots();
	TestEqual(TEXT("Pause and derived refresh cannot advance assessment"),
	          Sim->GetPopulationSnapshot().Citizens[0].LastHappinessMonth, Before.LastHappinessMonth);
	for (int32 Speed : {3, 5})
	{
		TestTrue(TEXT("Live city reload"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		Sim->Tick(1.875f / Speed);
		Sim->SetSimulationSpeed(1);
		FString Coarse;
		TestTrue(TEXT("Fast city serializes"),
		         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Coarse, Error));
		TestEqual(TEXT("Full live state identical across fine/fast catch-up"), Coarse, Fine);
	}
	// Like the real load flow, rebind the board after transactional model replacement.
	Board->PrepareCityView();
	const auto Cards = Board->GetCityInfoCards();
	TestTrue(TEXT("Shared overview exposes happiness and discontent"),
	         Cards.Num() == 6 && Cards[0].Labels.Contains(TEXT("Happiness")) &&
	             Cards[0].Labels.Contains(TEXT("Discontented")));
	FVector Roof;
	FString Detail;
	TArray<FCityInspectionMetric> Metrics;
	TestTrue(TEXT("Shared home inspector available"),
	         Board->GetInspectionBuilding(Home, Roof, Detail, &Metrics));
	TestTrue(TEXT("Home inspector explains sustained hardship and concerns"),
	         Detail.Contains(TEXT("Sustained discontent")) && Detail.Contains(TEXT("Recorded concerns")));
	TestTrue(TEXT("Exact values remain available in VR's text despite its three-bar limit"),
	         Detail.Contains(TEXT("Happiness ")) && Detail.Contains(TEXT("/100 | outlook ")) &&
	             Detail.Contains(TEXT("Discontented ")));
	TestTrue(TEXT("Shared inspector carries exact score"),
	         Metrics.ContainsByPredicate([](const auto& M) { return M.Label == TEXT("Happiness"); }));
	W->DestroyWorld(false);
	return true;
}
#endif
