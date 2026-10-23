#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Persistence/CitySaveLibrary.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityNamedCityTest, "CitySim.Persistence.NamedCityCreation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityNamedCityTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CityNamedCityTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/NamedCities"),
	                                          FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FString Error;
	FCityIdentity First;
	First.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	First.Name = TEXT("Same City Name");
	First.Difficulty = ECityDifficulty::Relaxed;
	FCityTerrainGenerationParameters Terrain;
	Terrain.Seed = 19041;
	TestTrue(TEXT("Create independent relaxed city"), Sim->CreateCity(First, Terrain, Directory, Error));
	TestEqual(TEXT("Starting funds from authoritative preset"), Sim->GetPlacementModel().GetFunds(),
	          int64(20000));
	TestEqual(TEXT("New city starts paused"), Sim->IsSimulationPaused(), true);
	TestEqual(TEXT("Fresh clock"), Sim->GetEconomySnapshot().FixedStepId, int64(0));
	TestEqual(TEXT("Fresh population"), Sim->GetPopulationSnapshot().Citizens.Num(), 0);
	TestEqual(TEXT("Fresh incident history"), Sim->GetFireSnapshot().RecentIncidents.Num(), 0);
	TestEqual(TEXT("Fresh traffic"), Sim->GetTrafficSnapshot().Vehicles.Num(), 0);
	TestEqual(TEXT("Fresh buildings"), Sim->GetPlacementModel().GetEntities().Num(), 0);
	TestEqual(TEXT("Fresh placement identity counter"), Sim->GetPlacementModel().ExportState().NextEntityId,
	          uint64(1));
	TestTrue(TEXT("Fresh objectives running"),
	         Sim->GetScenarioSnapshot().Phase == ECityScenarioPhase::Running);
	TestEqual(TEXT("Terrain seed committed"), Sim->GetTerrain().GetParameters().Seed, Terrain.Seed);
	const FString FirstPath = FCitySaveLibrary::PathFor(Directory, First.Id);
	FString FirstBytes;
	FFileHelper::LoadFileToString(FirstBytes, *FirstPath);
	Sim->GetPlacementModel().TrySpendFunds(321);
	Sim->SetSimulationPaused(false);
	Sim->Tick(4.8f);
	FCityIdentity Second = First;
	Second.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Second.Difficulty = ECityDifficulty::Normal;
	Terrain.Seed = 21042;
	TestTrue(TEXT("Duplicate display names have independent slots"),
	         Sim->CreateCity(Second, Terrain, Directory, Error));
	TestEqual(TEXT("Normal does not inherit relaxed incident timing"),
	          Sim->GetEconomyTuning().FireGraceMonths, FCityEconomyTuning().FireGraceMonths);
	TestEqual(TEXT("Normal starting funds"), Sim->GetPlacementModel().GetFunds(), int64(15000));
	TestEqual(TEXT("Creation clears clock from previous city"), Sim->GetEconomySnapshot().FixedStepId,
	          int64(0));
	FString Unchanged;
	FFileHelper::LoadFileToString(Unchanged, *FirstPath);
	TestEqual(TEXT("Creating second city cannot overwrite first"), Unchanged, FirstBytes);
	TestEqual(TEXT("Two independent slots discovered"), FCitySaveLibrary::List(Directory).Num(), 2);
	const auto Generation = Sim->GetPresentationGeneration();
	TestFalse(TEXT("Creating into existing identity rejected"),
	          Sim->CreateCity(First, Terrain, Directory, Error));
	TestEqual(TEXT("Failed create keeps current identity"), Sim->GetCityIdentity().Id, Second.Id);
	TestEqual(TEXT("Failed create cannot rebuild the board"), Sim->GetPresentationGeneration(), Generation);
	FCityIdentity Invalid = First;
	Invalid.Id = TEXT("../escape");
	TestFalse(TEXT("Invalid identity cannot escape save directory"),
	          Sim->CreateCity(Invalid, Terrain, Directory, Error));
	TestTrue(TEXT("Unsafe identity has no file path"),
	         FCitySaveLibrary::PathFor(Directory, Invalid.Id).IsEmpty());
	Invalid = First;
	Invalid.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Invalid.RulesVersion = 2;
	TestFalse(TEXT("Future rule versions cannot silently change rules"),
	          Sim->CreateCity(Invalid, Terrain, Directory, Error));
	Invalid = First;
	Invalid.Name = TEXT("Bad\nName");
	TestFalse(TEXT("Control characters rejected in city name"),
	          Sim->CreateCity(Invalid, Terrain, Directory, Error));
	const auto GoodTerrain = Terrain;
	Terrain.SmoothingPasses = -1;
	TestFalse(TEXT("Invalid terrain cannot replace active city"),
	          Sim->CreateCity(Second, Terrain, Directory, Error));
	Terrain = GoodTerrain;
	TestTrue(TEXT("Load restores first independent city"), Sim->LoadCityFromPath(FirstPath, Error));
	TestEqual(TEXT("Saved difficulty restored with its city"), Sim->GetEconomyTuning().FireGraceMonths, 3);
	TestEqual(TEXT("Saved terrain restored with its city"), Sim->GetTerrain().GetParameters().Seed, 19041);
	TestEqual(TEXT("Saved original funds restored"), Sim->GetPlacementModel().GetFunds(), int64(20000));
	// An interrupted replacement leaves the committed file readable and the temporary file undiscoverable.
	TestFalse(TEXT("Interrupted save reports failure"), Sim->SaveCityToPath(FirstPath, Error, true));
	TestEqual(TEXT("Temporary file cannot create another slot"), FCitySaveLibrary::List(Directory).Num(), 2);
	TestTrue(TEXT("Successful replacement retains backup"), Sim->SaveCityToPath(FirstPath, Error));
	FFileHelper::SaveStringToFile(TEXT("truncated"), *FirstPath);
	const auto Recoverable = FCitySaveLibrary::List(Directory);
	TestTrue(TEXT("Corrupt primary remains selectable through backup"),
	         Recoverable.ContainsByPredicate(
	             [&](const auto& Slot) { return Slot.Identity.Id == First.Id && Slot.bBackupOnly; }));
	TestTrue(TEXT("Corrupt primary recovers from backup"), Sim->LoadCityFromPath(FirstPath, Error));
	const FString IdBefore = Sim->GetCityIdentity().Id;
	TestFalse(TEXT("Missing load rejected"), Sim->LoadCityFromPath(Directory / TEXT("missing.json"), Error));
	TestEqual(TEXT("Failed load keeps city and rules"), Sim->GetCityIdentity().Id, IdBefore);
	// Same seed/preset produces identical authoritative initial documents except stable city ID.
	FCityIdentity Third = Second;
	Third.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Terrain.Seed = 21042;
	TestTrue(TEXT("Repeat deterministic setup"), Sim->CreateCity(Third, Terrain, Directory, Error));
	FString A, B;
	FFileHelper::LoadFileToString(A, *FCitySaveLibrary::PathFor(Directory, Second.Id));
	FFileHelper::LoadFileToString(B, *FCitySaveLibrary::PathFor(Directory, Third.Id));
	B.ReplaceInline(*Third.Id, *Second.Id);
	TestEqual(TEXT("Identical preset seed gives identical fresh authority"), B, A);
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*")), true, false);
	for (const FString& File : Files)
		IFileManager::Get().Delete(*(Directory / File));
	IFileManager::Get().DeleteDirectory(*Directory, false, false);
	World->DestroyWorld(false);
	return true;
}
#endif
