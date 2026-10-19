#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Persistence/CityPersistence.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEducationRecoveryTest,
                                 "CitySim.Progression.DefaultEducationOutageRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityEducationRecoveryTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EducationRecovery"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	if (!TestNotNull(TEXT("Real authored building catalog"), Catalog))
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->RegisterBuildingCatalog(Catalog);
	// This controlled education fixture isolates staffing and learning from treasury pressure.
	Sim->PlacementModel = MakeUnique<FCityPlacementModel>(200000);
	auto& City = Sim->GetPlacementModel();
	auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
	Road.End = {40, 8};
	City.CommitRoad(Road);
	const TCHAR* Names[] = {
	    TEXT("Power"),  TEXT("Service"), TEXT("Home"),    TEXT("Home"),          TEXT("Home"),
	    TEXT("Home"),   TEXT("Shop"),    TEXT("Factory"), TEXT("PrimarySchool"), TEXT("SecondarySchool"),
	    TEXT("College")};
	const int32 Xs[] = {1, 3, 6, 8, 10, 12, 14, 17, 21, 24, 28};
	uint64 Primary = 0, College = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(Names); ++I)
	{
		const auto* Def = Catalog->FindDefinition(Names[I]);
		if (!TestNotNull(TEXT("Fixture definition"), Def))
			continue;
		auto Preview = City.BeginPreview(*Def, {Xs[I], 9});
		const auto Result = City.CommitPlacement(Preview, *Def);
		TestTrue(FString::Printf(TEXT("Authored %s placement succeeds"), Names[I]), Result.IsSuccess());
		if (I == 8)
			Primary = Result.EntityId;
		if (I == 10)
			College = Result.EntityId;
	}
	Sim->Development.InitializeExistingAsOperational(City, Catalog, 0);
	Sim->SetDisasterFrequency(ECityDisasterFrequency::Off);
	Sim->StartScenario();
	const auto Tuning = Sim->GetEconomyTuning();
	auto AdvanceMonths = [&](int32 Count) {
		for (int32 I = 0; I < Count * Tuning.FixedStepsPerMonth; ++I)
			Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
	};
	Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
	auto People = Sim->Population.GetSnapshot();
	uint64 Student = 0;
	for (auto& Citizen : People.Citizens)
		if (!People.PublicEmployers.ContainsByPredicate(
		        [&](const auto& Employer) { return Employer.EntityId == Citizen.WorkplaceEntityId; }))
		{
			Student = Citizen.Id;
			Citizen.Education = ECityEducationLevel::None;
			Citizen.EducationMonths = 0;
			break;
		}
	FString Error;
	TestTrue(TEXT("Fixture has a non-civic entry-level student"), Student > 0);
	TestTrue(TEXT("Explicit uneducated fixture restores"), Sim->Population.RestoreState(People, Error));
	auto FindStudent = [&]() {
		return Sim->Population.GetSnapshot().Citizens.FindByPredicate(
		    [&](const auto& C) { return C.Id == Student; });
	};
	AdvanceMonths(3);
	TestNotNull(TEXT("Student survives the default primary course"), FindStudent());
	if (const auto* C = FindStudent())
		TestTrue(TEXT("Default six-month primary course retains partial progress"),
		         C->Education == ECityEducationLevel::None && C->EducationMonths == 3);
	TestTrue(TEXT("Primary school has actual staffing and students"),
	         Sim->ServiceSnapshot.SchoolUsedCapacity.FindRef(Primary) > 0);
	TestTrue(TEXT("Road outage can be introduced"), City.RemoveRoad({20, 8}));
	AdvanceMonths(2);
	if (const auto* C = FindStudent())
		TestEqual(TEXT("Two disconnected months grant no learning"), C->EducationMonths, 3);
	const FString Path =
	    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CivicBudget-EducationRecovery.json"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	TestTrue(TEXT("Outage with partial learning saves"), Sim->SaveCityToPath(Path, Error));
	TestTrue(TEXT("Outage with partial learning resumes"), Sim->LoadCityFromPath(Path, Error, false));
	auto Repaired = Sim->GetPlacementModel().BeginRoadPreview({20, 8});
	Repaired.End = {20, 8};
	TestTrue(TEXT("Affordable single-road recovery succeeds"),
	         Sim->GetPlacementModel().CommitRoad(Repaired).IsSuccess());
	AdvanceMonths(3);
	if (const auto* C = FindStudent())
		TestEqual(TEXT("Restored progress graduates to Basic after six served months"), C->Education,
		          ECityEducationLevel::Basic);
	AdvanceMonths(6);
	TestNotNull(TEXT("Student survives secondary education"), FindStudent());
	if (const auto* C = FindStudent())
		TestEqual(TEXT("Secondary stage graduates after six more served months"), C->Education,
		          ECityEducationLevel::Secondary);
	AdvanceMonths(6);
	TestNotNull(TEXT("Student survives college education"), FindStudent());
	if (const auto* C = FindStudent())
		TestEqual(TEXT("College stage graduates after six more served months"), C->Education,
		          ECityEducationLevel::Higher);
	TestTrue(TEXT("College bootstraps actual qualified staffing"),
	         Sim->Population.GetSnapshot().Workers(College) > 0);
	const auto Baseline = Sim->Population.GetSnapshot();
	const int64 FinalStep = Sim->GetEconomySnapshot().FixedStepId,
	            FinalFunds = Sim->GetPlacementModel().GetFunds();
	for (int32 Speed : {3, 5})
	{
		TestTrue(TEXT("Fast recovery reloads partial progress"), Sim->LoadCityFromPath(Path, Error, false));
		auto Repair = Sim->GetPlacementModel().BeginRoadPreview({20, 8});
		Repair.End = {20, 8};
		TestTrue(TEXT("Fast run restores the same road"),
		         Sim->GetPlacementModel().CommitRoad(Repair).IsSuccess());
		Sim->SetSimulationSpeed(Speed);
		const int32 Steps = 15 * Tuning.FixedStepsPerMonth;
		TestEqual(TEXT("Recovery duration divides into speed batches"), Steps % Speed, 0);
		for (int32 I = 0; I < Steps / Speed; ++I)
			Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
		TestEqual(TEXT("Fast catch-up reaches the same game day"), Sim->GetEconomySnapshot().FixedStepId,
		          FinalStep);
		TestEqual(TEXT("Fast catch-up settles identical treasury"), Sim->GetPlacementModel().GetFunds(),
		          FinalFunds);
		const auto& Resumed = Sim->Population.GetSnapshot();
		TestEqual(TEXT("Fast recovery keeps every citizen"), Resumed.Citizens.Num(), Baseline.Citizens.Num());
		for (const auto& Expected : Baseline.Citizens)
		{
			const auto* Actual =
			    Resumed.Citizens.FindByPredicate([&](const auto& C) { return C.Id == Expected.Id; });
			if (!TestNotNull(TEXT("Stable citizen identity resumes"), Actual))
				continue;
			TestTrue(TEXT("Education and skilled employment match fine stepping"),
			         Actual->Education == Expected.Education &&
			             Actual->EducationMonths == Expected.EducationMonths &&
			             Actual->LastEducationMonth == Expected.LastEducationMonth &&
			             Actual->WorkplaceEntityId == Expected.WorkplaceEntityId &&
			             Actual->bSkilledJob == Expected.bSkilledJob);
			TestEqual(TEXT("Citizen payroll/wallet agrees across speeds"), Actual->AvailableMoney,
			          Expected.AvailableMoney);
		}
		AddInfo(FString::Printf(TEXT("%dx recovery matches 1x education, employment, wallets and treasury"),
		                        Speed));
	}
	AddInfo(
	    FString::Printf(TEXT("Default education recovery fixture: %lld steps, %d citizens, %lld treasury; "
	                         "generous fixture treasury does not establish early-game affordability"),
	                    Sim->GetEconomySnapshot().FixedStepId, Sim->GetPopulationSnapshot().Citizens.Num(),
	                    Sim->GetPlacementModel().GetFunds()));
	IFileManager::Get().Delete(*Path, false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bak")), false, true, true);
	World->DestroyWorld(false);
	return true;
}
#endif
