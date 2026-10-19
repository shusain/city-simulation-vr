#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Persistence/CityPersistence.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
struct FMigrationFixture
{
	FCityPlacementModel City{10000, FCityGridDimensions(80, 80)};
	FCityPopulationSimulation People;
	FCityEconomyTuning T;
	FCityEconomySnapshot E;
	FCityServiceSnapshot S;
	FCityEnvironmentSnapshot Env{FCityGridDimensions(80, 80)};
	FCityHealthSnapshot Health;
	FCityWasteSnapshot Waste;
	FCityCrimeSnapshot Crime;
	uint64 Source = 0, Destination = 0, Shop = 0;
	FMigrationFixture()
	{
		auto Road = City.BeginRoadPreview(City.GetEntranceCell());
		City.UpdateRoadPreviewEnd(Road, {25, 8});
		City.CommitRoad(Road);
		const auto Place = [&](const TCHAR* Id, int32 X) {
			FCityBuildingDefinition D;
			D.DefinitionId = Id;
			D.DisplayName = Id;
			D.Footprint = {1, 1};
			D.Cost = 0;
			auto Preview = City.BeginPreview(D, {X, 9});
			return City.CommitPlacement(Preview, D).EntityId;
		};
		Source = Place(TEXT("Home"), 1);
		Destination = Place(TEXT("Home"), 9);
		Shop = Place(TEXT("Shop"), 17);
		FCityPopulationSnapshot P;
		P.LastSettledMonthId = 3;
		P.NextCitizenId = 3;
		P.bUsesPurchaseReservations = true;
		for (uint64 Id = 1; Id <= 2; ++Id)
		{
			auto& C = P.Citizens.Emplace_GetRef();
			C.Id = Id;
			C.HomeEntityId = C.HouseholdId = C.CurrentLocationEntityId = Source;
			C.WorkplaceEntityId = Shop;
			C.Education = Id == 1 ? ECityEducationLevel::Secondary : ECityEducationLevel::Basic;
			C.AvailableMoney = 30;
			C.ConsumptionNeed = Id == 1 ? 1 : 0;
			C.Happiness = 20;
			C.HappinessTarget = 25;
			C.DiscontentMonths = 3;
			C.LastHappinessMonth = 3;
		}
		auto& H = P.Households.Emplace_GetRef();
		H.Id = H.HomeEntityId = Source;
		H.Balance = 60;
		H.ConsumptionOrders = H.CompletedPurchases = 1;
		auto& B = P.Businesses.Emplace_GetRef();
		B.EntityId = Shop;
		B.JobCapacity = 2;
		B.bInitialFreightReceived = true;
		B.Inventory = 2;
		B.PendingOrders = 2;
		P.PurchaseReservations.Add({1, Source, Shop, 5, {{1, 5}}});
		auto& Buyer = P.Citizens[0];
		Buyer.PendingShopEntityId = Buyer.TripDestinationEntityId = Shop;
		Buyer.TripPurpose = ECityTripPurpose::Purchase;
		Buyer.Schedule = ECityCitizenSchedule::ToShop;
		S.Dimensions = FCityGridDimensions(80, 80);
		S.EntityResidents.Add(Source, 0);
		S.EntityResidents.Add(Destination, 2);
		S.EntityJobs.Add(Shop, 2);
		S.EntityFailures.Add(Source, ECityOperationFailure::NoPower);
		S.CrimeRisk.Init(0, 80 * 80);
		S.CrimeRisk[9 * 80 + 1] = 100;
		Env.Pollution[9 * 80 + 1] = 1000;
		Env.TotalPollution = Env.PeakPollution = 1000;
		E.Residents = E.Jobs = E.EmployedResidents = 2;
		E.LastBilledMonthId = E.MonthId = 3;
		E.FixedStepId = 75;
		FString Error;
		check(People.RestoreState(P, Error));
	}
	void Month(int64 M)
	{
		auto P = People.GetSnapshot();
		P.LastSettledMonthId = M;
		for (auto& C : P.Citizens)
		{
			C.LastHappinessMonth = M;
			C.DiscontentMonths = static_cast<int32>(M);
		}
		E.LastBilledMonthId = E.MonthId = M;
		E.FixedStepId = M * 25;
		FString Error;
		check(People.RestoreState(P, Error));
	}
	bool Review()
	{
		return People.AdvanceHousingMigration(E.LastBilledMonthId, City, S, Env, E, T, Health, Waste, Crime);
	}
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHousingMoveTest, "CitySim.HousingMigration.MoveLedgerIdentityAndTravel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHousingMoveTest::RunTest(const FString& Parameters)
{
	FMigrationFixture F;
	const auto Plan = FCityHousingMigration::AdmissionPlan(F.People.GetSnapshot(), F.S);
	TestEqual(TEXT("Migration reservation prevents replacement arrivals"), Plan.TargetResidents, 2);
	TestTrue(TEXT("A fully vacant destination is reserved"), Plan.ReservedHomes.Contains(F.Destination));
	FCityRoadNetworkSnapshot Network;
	FCityRoadNetworkBuilder::Build(F.City, FCityRoadDefinition::Local(), Network);
	FCityTrafficSimulation Traffic;
	Traffic.UpdateTrips(F.City, nullptr, F.People.GetSnapshot(), Network, 75);
	TestTrue(TEXT("Fixture has a real shopping route before migration"),
	         !Traffic.GetSnapshot().Vehicles.IsEmpty());
	const auto Before = F.People.GetSnapshot();
	const auto Funds = F.City.GetFunds();
	TestTrue(TEXT("Persistent household moves to materially better home"), F.Review());
	const auto& After = F.People.GetSnapshot();
	TestEqual(TEXT("Whole household moves together"), After.Citizens[0].HomeEntityId, F.Destination);
	TestEqual(TEXT("A single factual move event is recorded"), After.HousingEvents.Num(), 1);
	TestTrue(TEXT("Move event retains exact source, destination, people and fee"),
	         After.HousingEvents[0].SourceHome == F.Source &&
	             After.HousingEvents[0].DestinationHome == F.Destination &&
	             After.HousingEvents[0].CitizenIds == TArray<uint64>({1, 2}) &&
	             After.HousingEvents[0].Money == 5 &&
	             After.HousingEvents[0].Kind == ECityHousingEventKind::Relocated);
	TestEqual(TEXT("Other member moves too"), After.Citizens[1].HomeEntityId, F.Destination);
	TestEqual(TEXT("Citizen IDs retained"), After.Citizens[0].Id, Before.Citizens[0].Id);
	TestEqual(TEXT("Education retained"), After.Citizens[0].Education, Before.Citizens[0].Education);
	TestEqual(TEXT("Job retained"), After.Citizens[0].WorkplaceEntityId, F.Shop);
	TestEqual(TEXT("Named fee leaves resident wallets exactly once"),
	          After.Citizens[0].AvailableMoney + After.Citizens[1].AvailableMoney + After.MovingExpenses,
	          int64(60));
	TestEqual(TEXT("Moving fee is one goods-order budget"), After.MovingExpenses, int64(5));
	TestEqual(TEXT("Private moving fee never debits treasury"), F.City.GetFunds(), Funds);
	TestEqual(TEXT("Past completed purchases follow household"), After.Households[0].CompletedPurchases, 1);
	TestEqual(TEXT("No stock refund or sale invented by cancellation"), After.Businesses[0].Inventory, 2);
	TestTrue(TEXT("Purchase locks released"), After.PurchaseReservations.IsEmpty());
	F.People.CompleteCitizenTrip(1, F.Source, F.Shop, 76);
	TestEqual(TEXT("Old arrival cannot charge moved household"), F.People.GetSnapshot().SalesThisMonth, 0);
	Traffic.UpdateTrips(F.City, nullptr, F.People.GetSnapshot(), Network, 75);
	TestTrue(TEXT("Old vehicles and lane occupancy cleared"),
	         Traffic.GetSnapshot().Vehicles.IsEmpty() && Traffic.GetSnapshot().LaneLoads.IsEmpty());
	TestEqual(TEXT("Moving does not instantly improve happiness"), After.Citizens[0].Happiness, 20);
	TestEqual(TEXT("Improved outlook is based on real retained purchases"), After.Citizens[0].HappinessTarget,
	          85);
	TestFalse(TEXT("Repeated month cannot charge twice"), F.Review());
	TestEqual(TEXT("No duplicate resident moves"), F.People.GetSnapshot().RelocatedResidents, int64(2));
	TestEqual(TEXT("Repeated month does not duplicate move events"),
	          F.People.GetSnapshot().HousingEvents.Num(), 1);
	FString Error;
	TestTrue(TEXT("Moved current state validates"), FCityHousingMigration::Validate(After, Error, &F.City));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHousingDepartureTest,
                                 "CitySim.HousingMigration.WarningRecoveryCareAndDeparture",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHousingDepartureTest::RunTest(const FString& Parameters)
{
	FMigrationFixture F;
	F.S.EntityResidents[F.Destination] = 0;
	TestFalse(TEXT("First search warns rather than removes residents"), F.Review());
	TestEqual(TEXT("No better home explains pending move"), F.People.GetSnapshot().Citizens[0].HousingStatus,
	          ECityHousingStatus::Searching);
	for (int64 M : {4, 5})
	{
		F.Month(M);
		TestFalse(TEXT("Persistent warnings leave time to fix city"), F.Review());
	}
	F.Month(6);
	TestTrue(TEXT("Six unresolved low months permits departure"), F.Review());
	TestTrue(TEXT("Departed citizens and their household removed"),
	         F.People.GetSnapshot().Citizens.IsEmpty() && F.People.GetSnapshot().Households.IsEmpty());
	TestEqual(TEXT("Resident money crosses city boundary explicitly"), F.People.GetSnapshot().DepartureMoney,
	          int64(60));
	TestEqual(TEXT("Departure population counter exact"), F.People.GetSnapshot().DepartedResidents, int64(2));
	TestEqual(TEXT("Search warning and departure each recorded once"),
	          F.People.GetSnapshot().HousingEvents.Num(), 2);
	TestTrue(TEXT("Removed citizens remain identifiable in history"),
	         FCityHousingHistory::Describe(F.People.GetSnapshot().HousingEvents.Last())
	             .Contains(TEXT("Citizens #1, #2")));
	FCityEconomySimulation Closing;
	Closing.UpdateResidentCensus(F.People.GetSnapshot());
	TestEqual(TEXT("Closing census reports real decline"), Closing.GetSnapshot().Residents, 0);
	F.S.EntityResidents[F.Source] = 2;
	F.S.EntityFailures.Remove(F.Source);
	F.People.Reconcile(F.City, F.E, F.T, &F.S);
	TestTrue(TEXT("Vacant housing cannot instantly recreate departing residents"),
	         F.People.GetSnapshot().Citizens.IsEmpty());
	F.Month(8);
	F.Review();
	F.People.Reconcile(F.City, F.E, F.T, &F.S);
	TestTrue(TEXT("Admission cooldown persists"), F.People.GetSnapshot().Citizens.IsEmpty());
	F.Month(9);
	F.Review();
	F.People.Reconcile(F.City, F.E, F.T, &F.S);
	TestEqual(TEXT("Finite cooldown allows later real newcomers"), F.People.GetSnapshot().Citizens.Num(), 2);
	TestTrue(TEXT("Departed identities are never reused"), F.People.GetSnapshot().Citizens[0].Id >= 3);
	FMigrationFixture Recovering;
	auto P = Recovering.People.GetSnapshot();
	for (auto& C : P.Citizens)
		C.HappinessTarget = 80;
	FString Error;
	Recovering.People.RestoreState(P, Error);
	TestFalse(TEXT("Improved outlook stops migration despite lagging mood"), Recovering.Review());
	TestEqual(TEXT("Recovery is explained"), Recovering.People.GetSnapshot().Citizens[0].HousingStatus,
	          ECityHousingStatus::Recovering);
	FMigrationFixture Care;
	P = Care.People.GetSnapshot();
	for (auto& C : P.Citizens)
		C.AvailableMoney = 0;
	P.Households[0].Balance = 0;
	P.PurchaseReservations.Reset();
	Care.People.RestoreState(P, Error);
	Care.Month(6);
	Care.Health.Patients.Add({1, 0, ECityIllness::Mild, ECityHealthCare::HomeRecovery});
	TestFalse(TEXT("Active care preserves family/patient associations"), Care.Review());
	TestEqual(TEXT("Care delay explained"), Care.People.GetSnapshot().Citizens[0].HousingStatus,
	          ECityHousingStatus::WaitingForCare);
	Care.Health.Patients.Reset();
	Care.Month(7);
	TestTrue(TEXT("Care recovery permits unresolved unaffordable departure"), Care.Review());
	TestEqual(TEXT("Poor departing residents carry no invented money"),
	          Care.People.GetSnapshot().DepartureMoney, int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHousingSaveTest, "CitySim.HousingMigration.SaveValidationAndReports",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHousingSaveTest::RunTest(const FString& Parameters)
{
	FMigrationFixture F;
	F.Review();
	FCitySaveDocument D(FCityGridDimensions(80, 80)), Read;
	D.Placement = F.City.ExportState();
	D.Population = F.People.GetSnapshot();
	D.Economy = F.E;
	TestTrue(TEXT("Cumulative migration report captures"),
	         D.Reports.Capture(D.Economy, D.Population, F.S, D.Environment, D.Placement.Funds));
	TestEqual(TEXT("Annual reports do not add cumulative moving fees twice"),
	          D.Reports.Annual()[0].Get(ECityReportMetric::MovingExpenses), int64(5));
	FString Json, Error;
	if (!TestTrue(TEXT("Moved city serializes"), FCityPersistence::Serialize(D, Json, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestTrue(TEXT("Moved city reads"), FCityPersistence::Deserialize(Json, Read, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Move counter saved"), Read.Population.RelocatedResidents, int64(2));
	TestEqual(TEXT("Fee saved"), Read.Population.MovingExpenses, int64(5));
	TestEqual(TEXT("Vacancy hold saved"), Read.Population.HousingVacancyHolds[0].UntilMonth, int64(6));
	TestTrue(TEXT("Migration samples marked observed"), Read.Reports.Months[0].bHasMigration);
	TestEqual(TEXT("Move history survives current save round trip"), Read.Population.HousingEvents.Num(), 1);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	const auto Population = Root->GetObjectField(TEXT("population"));
	const auto Encode = [&]() {
		FString R;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&R));
		return R;
	};
	FCitySaveDocument Untouched;
	Untouched.Placement.Funds = 4321;
	const auto OriginalEvents = Population->GetArrayField(TEXT("housingEvents"));
	Population->RemoveField(TEXT("housingEvents"));
	TestFalse(TEXT("Missing current history rejects"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	Population->SetArrayField(TEXT("housingEvents"), OriginalEvents);
	const auto SavedEvent = OriginalEvents[0]->AsObject();
	SavedEvent->SetStringField(TEXT("month"), TEXT("4"));
	TestFalse(TEXT("Future history rejects transactionally"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	TestEqual(TEXT("Rejected history preserves destination"), Untouched.Placement.Funds, int64(4321));
	SavedEvent->SetStringField(TEXT("month"), TEXT("3"));
	auto RepeatedEvents = OriginalEvents;
	const auto DuplicateEvent = RepeatedEvents[0];
	RepeatedEvents.Add(DuplicateEvent);
	Population->SetArrayField(TEXT("housingEvents"), RepeatedEvents);
	TestFalse(TEXT("Duplicate event identity rejects"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	Population->SetArrayField(TEXT("housingEvents"), OriginalEvents);
	SavedEvent->GetObjectField(TEXT("sourceCell"))->SetNumberField(TEXT("x"), 80);
	TestFalse(TEXT("History coordinates beyond city reject"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	SavedEvent->GetObjectField(TEXT("sourceCell"))->SetNumberField(TEXT("x"), 1);
	for (const auto* Field : {TEXT("lastMigrationMonth"), TEXT("immigrationResumeMonth"),
	                          TEXT("relocatedResidents"), TEXT("departedResidents"), TEXT("movingExpenses"),
	                          TEXT("departureMoney"), TEXT("housingVacancyHolds")})
	{
		const auto Value = Population->Values.FindChecked(Field);
		Population->RemoveField(Field);
		TestFalse(TEXT("Missing required migration fields reject"),
		          FCityPersistence::Deserialize(Encode(), Untouched, Error));
		TestEqual(TEXT("Rejected read is transactional"), Untouched.Placement.Funds, int64(4321));
		Population->SetField(Field, Value);
	}
	Population->SetStringField(TEXT("lastMigrationMonth"), TEXT("4"));
	TestFalse(TEXT("Future migration clock rejects"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	Population->SetStringField(TEXT("lastMigrationMonth"), TEXT("3"));
	const auto C = Population->GetArrayField(TEXT("citizens"))[0]->AsObject();
	C->SetNumberField(TEXT("housingStatus"), 6);
	TestFalse(TEXT("Unknown housing state rejects"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	C->SetNumberField(TEXT("housingStatus"), 4);
	auto Holds = Population->GetArrayField(TEXT("housingVacancyHolds"));
	const auto DuplicateHold = Holds[0];
	Holds.Add(DuplicateHold);
	Population->SetArrayField(TEXT("housingVacancyHolds"), Holds);
	TestFalse(TEXT("Duplicate vacancy hold rejects"),
	          FCityPersistence::Deserialize(Encode(), Untouched, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityHousingMigrationLiveTest,
                                 "CitySim.HousingMigration.LiveDeparturePauseSaveAndSpeed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityHousingMigrationLiveTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	auto* W = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HousingMigrationLive"), nullptr, true,
	                              ERHIFeatureLevel::Num, &Values);
	auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
	auto* Original = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	auto* Catalog = NewObject<UCityBuildingCatalog>();
	Catalog->Definitions = Original->Definitions;
	for (auto& D : Catalog->Definitions)
		if (D.DefinitionId == TEXT("Home") && D.DensityLevels.Num() > 1)
			D.DensityLevels.SetNum(1);
	Sim->RegisterBuildingCatalog(Catalog);
	auto& City = Sim->GetPlacementModel();
	City.SetTerrain(nullptr);
	auto Road = City.BeginRoadPreview(City.GetEntranceCell());
	City.UpdateRoadPreviewEnd(Road, {30, 8});
	City.CommitRoad(Road);
	int32 X = 1;
	for (const auto* Id : {TEXT("Power"), TEXT("Service"), TEXT("Home")})
	{
		const auto* D = Catalog->FindDefinition(Id);
		auto Preview = City.BeginPreview(*D, {X, 9});
		TestTrue(TEXT("Live fixture places"), City.CommitPlacement(Preview, *D).IsSuccess());
		X += 3;
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->EconomyTuning.FixedStepSeconds = .125;
	Sim->EconomyTuning.FixedStepsPerMonth = 4;
	Sim->EconomyTuning.MaxCatchUpSteps = 16;
	Sim->EconomyTuning.CrimePopulationThreshold = Sim->EconomyTuning.FirePopulationThreshold = 100000;
	Sim->StartScenario();
	Sim->SetTaxRates(2500, 1000, 1000);
	for (int32 I = 0; I < 3; ++I)
		Sim->Tick(.125f);
	FString Error;
	const FString Path =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Housing-Migration-Start.json"));
	TestTrue(TEXT("Live starting city saves"), Sim->SaveCityToPath(Path, Error));
	const auto StartingIds = Sim->GetPopulationSnapshot().Citizens;
	TestEqual(TEXT("Initial admission still fills normal housing"), StartingIds.Num(), 10);
	for (int32 I = 0; I < 30; ++I)
		Sim->Tick(.125f);
	FString Fine;
	TestTrue(TEXT("Fine city serializes"),
	         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Fine, Error));
	TestEqual(TEXT("Live unresolved hardship produces genuine departures"),
	          Sim->GetPopulationSnapshot().DepartedResidents, int64(10));
	TestEqual(TEXT("Live closing census declines"), Sim->GetEconomySnapshot().Residents, 0);
	TestTrue(TEXT("No immediately manufactured replacement residents"),
	         Sim->GetPopulationSnapshot().Citizens.IsEmpty());
	const auto& Latest = Sim->Reports.Months.Last();
	TestEqual(TEXT("Reports agree with closing population"), Latest.Get(ECityReportMetric::Population),
	          int64(0));
	TestEqual(TEXT("Reports expose actual departures"), Latest.Get(ECityReportMetric::DepartedResidents),
	          int64(10));
	Sim->SetSimulationPaused(true);
	const int64 MoneyOut = Sim->GetPopulationSnapshot().DepartureMoney;
	Sim->Tick(10);
	TestEqual(TEXT("Pause cannot create extra outward transfers"),
	          Sim->GetPopulationSnapshot().DepartureMoney, MoneyOut);
	for (int32 Speed : {3, 5})
	{
		TestTrue(TEXT("Live start reloads"), Sim->LoadCityFromPath(Path, Error, false));
		Sim->SetSimulationSpeed(Speed);
		Sim->Tick(1.875f / Speed);
		Sim->Tick(1.875f / Speed);
		Sim->SetSimulationSpeed(1);
		FString Coarse;
		TestTrue(TEXT("Fast replay serializes"),
		         FCityPersistence::Serialize(Sim->BuildSaveDocument(), Coarse, Error));
		TestEqual(TEXT("Full state and transfers match at1x/3x/5x"), Coarse, Fine);
	}
	const FString DepartedPath =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Housing-Migration-Departed.json"));
	TestTrue(TEXT("Departed city saves"), Sim->SaveCityToPath(DepartedPath, Error));
	TestTrue(TEXT("Departed city reloads"), Sim->LoadCityFromPath(DepartedPath, Error, false));
	TestEqual(TEXT("Reload keeps outward money ledger"), Sim->GetPopulationSnapshot().DepartureMoney,
	          MoneyOut);
	TestTrue(TEXT("Reload does not repopulate held vacancies"),
	         Sim->GetPopulationSnapshot().Citizens.IsEmpty());
	W->DestroyWorld(false);
	return true;
}
#endif
