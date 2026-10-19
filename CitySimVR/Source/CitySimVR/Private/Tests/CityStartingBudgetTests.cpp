#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityStartingBudgetTest, "CitySim.Progression.NormalStartingBudget",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityStartingBudgetTest::RunTest(const FString& Parameters)
{
	// Diagnostic target: occupied homes and jobs within six months; retain enough
	// cash to pay the first year. This is a modest starter route, not final balance.
	FString Report = TEXT("seed,month,residents,employed,treasury,income,expenses,net\n");
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	for (int32 Seed : {7429, 1337, 2026})
	{
		UWorld* World =
		    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
		auto* Catalog = LoadObject<UCityBuildingCatalog>(
		    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
		if (!TestNotNull(TEXT("Authored catalog"), Catalog))
		{
			World->DestroyWorld(false);
			continue;
		}
		Sim->RegisterBuildingCatalog(Catalog);
		FCityIdentity Identity;
		Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
		Identity.Name = TEXT("Budget regression");
		Identity.DisasterFrequency = ECityDisasterFrequency::Off;
		FCityTerrainGenerationParameters Terrain;
		Terrain.Seed = Seed;
		const FString Directory =
		    FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CivicBudget-Budget"), Identity.Id);
		FString Error;
		if (!TestTrue(TEXT("New Normal city uses actual terrain and creation"),
		              Sim->CreateCity(Identity, Terrain, Directory, Error)))
		{
			AddError(Error);
			World->DestroyWorld(false);
			continue;
		}
		auto& City = Sim->GetPlacementModel();
		TestEqual(TEXT("Normal starting treasury"), City.GetFunds(), int64(10000));
		auto Road = City.BeginRoadPreview(FCityPlacementModel::EntranceCell);
		Road.End = {10, 8};
		TestTrue(TEXT("Short starter road is affordable and valid"), City.CommitRoad(Road).IsSuccess());
		for (int32 Utility = 0; Utility < 2; ++Utility)
		{
			const auto* Def = Catalog->FindDefinition(Utility == 0 ? TEXT("Power") : TEXT("Service"));
			if (!TestNotNull(TEXT("Starter utility definition"), Def))
				continue;
			auto Preview = City.BeginRoadFacingPreview(*Def, {Utility * 2, 9});
			TestTrue(TEXT("Actual utility construction purchased"), City.CommitPlacement(Preview, *Def).IsSuccess());
		}
		TestTrue(TEXT("Residential zoning"), City.SetZoneArea({4, 9}, {6, 2}, ECityZoneType::Residential));
		TestTrue(TEXT("Commercial zoning"), City.SetZoneArea({0, 6}, {4, 2}, ECityZoneType::Commercial));
		TestTrue(TEXT("Industrial zoning"), City.SetZoneArea({4, 6}, {6, 2}, ECityZoneType::Industrial));
		Sim->SetSimulationPaused(false);
		const auto Tuning = Sim->GetEconomyTuning();
		int32 FirstResidents = 0, FirstJobs = 0;
		const FString ResumePath = FPaths::Combine(Directory, TEXT("checkpoint.json"));
		for (int32 Month = 1; Month <= 12; ++Month)
		{
			for (int32 Step = 0; Step < Tuning.FixedStepsPerMonth; ++Step)
				Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
			const auto& People = Sim->GetPopulationSnapshot();
			const auto Budget = Sim->GetProjectedBudget();
			if (!FirstResidents && People.Citizens.Num() > 0)
				FirstResidents = Month;
			if (!FirstJobs && Sim->GetEconomySnapshot().EmployedResidents > 0)
				FirstJobs = Month;
			Report += FString::Printf(TEXT("%d,%d,%d,%d,%lld,%lld,%lld,%lld\n"), Seed, Month, People.Citizens.Num(),
			                          Sim->GetEconomySnapshot().EmployedResidents, Sim->GetPlacementModel().GetFunds(),
			                          Budget.Income, Budget.Expenses, Budget.Net());
			if (Month == 6)
				TestTrue(TEXT("Starter progression checkpoint saves"), Sim->SaveCityToPath(ResumePath, Error));
		}
		const int64 FinalFunds = Sim->GetPlacementModel().GetFunds();
		const int32 FinalResidents = Sim->GetPopulationSnapshot().Citizens.Num();
		TestTrue(TEXT("Occupied homes within six months"), FirstResidents > 0 && FirstResidents <= 6);
		TestTrue(TEXT("Actual employment within six months"), FirstJobs > 0 && FirstJobs <= 6);
		TestTrue(TEXT("First-year treasury remains positive"), FinalFunds > 0);
		for (int32 Speed : {3, 5})
		{
			TestTrue(TEXT("Real starter checkpoint resumes"), Sim->LoadCityFromPath(ResumePath, Error, false));
			Sim->SetSimulationSpeed(Speed);
			Sim->SetSimulationPaused(false);
			for (int32 Batch = 0; Batch < 6 * Tuning.FixedStepsPerMonth / Speed; ++Batch)
				Sim->Tick(static_cast<float>(Tuning.FixedStepSeconds));
			TestEqual(TEXT("Starting-budget resumed cash agrees across speeds"), Sim->GetPlacementModel().GetFunds(),
			          FinalFunds);
			TestEqual(TEXT("Starting-budget population agrees across speeds"),
			          Sim->GetPopulationSnapshot().Citizens.Num(), FinalResidents);
		}
		AddInfo(FString::Printf(
		    TEXT("Seed %d: first residents month %d, jobs month %d, year-end residents %d, treasury %lld"), Seed,
		    FirstResidents, FirstJobs, FinalResidents, FinalFunds));
		// Only remove files inside this test's fresh GUID directory.
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*")), true, false);
		for (const auto& File : Files)
			IFileManager::Get().Delete(*(Directory / File), false, true, true);
		IFileManager::Get().DeleteDirectory(*Directory, false, false);
		World->DestroyWorld(false);
	}
	const FString ReportPath =
	    FPaths::Combine(FPaths::ProjectDir(), TEXT("../Artifacts/Diagnostics/CivicBudget-starting-budget.csv"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	TestTrue(TEXT("Monthly budget evidence writes"),
	         FFileHelper::SaveStringToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}
#endif
