#if WITH_DEV_AUTOMATION_TESTS
#include "Simulation/CityFirstPlayableGuide.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Persistence/CityPersistence.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGuidedProgressTest, "CitySim.Acceptance.GuidedMilestoneHistory",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityGuidedProgressTest::RunTest(const FString& Parameters)
{
	FCityFirstPlayableGuideState State;
	FCityGuidedProgress Progress;
	State.Residents = 150;
	State.Jobs = 10;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestTrue(TEXT("Optional guide does not start from clicks or dormant city"),
	         Progress.CompletedIds.IsEmpty());
	State.ScenarioPhase = ECityScenarioPhase::Running;
	State.bHasConnectedEntranceRoad = true;
	State.OperationalPowerBuildings = State.OperationalServiceBuildings = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Existing city skips satisfied out-of-order teaching steps"), Progress.CurrentId,
	          FName(TEXT("goods")));
	TestTrue(TEXT("Occupied homes count without painted zones"),
	         Progress.CompletedIds.Contains(TEXT("homes_zone")));
	TestTrue(TEXT("Population achieved early remains recorded"),
	         Progress.CompletedIds.Contains(TEXT("population")));
	const TArray<FName> BeforeOutage = Progress.CompletedIds;
	State.OperationalPowerBuildings = 0;
	State.PowerShortage = 10;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestTrue(TEXT("Outage does not erase teaching history"), Progress.CompletedIds == BeforeOutage);
	TestEqual(TEXT("Recovery does not change saved next stage"), Progress.CurrentId, FName(TEXT("goods")));
	const auto Recovery = FCityFirstPlayableGuide::BuildObjective(State, Progress);
	TestEqual(TEXT("Power recovery has priority"), Recovery.Id, FName(TEXT("power")));
	TestTrue(TEXT("Card identifies recovery explicitly"), Recovery.Title.StartsWith(TEXT("Restore:")));
	State.OperationalPowerBuildings = 1;
	State.PowerShortage = 0;
	TestEqual(TEXT("Restored service returns to next teaching step"),
	          FCityFirstPlayableGuide::BuildObjective(State, Progress).Id, FName(TEXT("goods")));
	State.CompletedPurchases = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Actual completed purchase advances goods step"), Progress.CurrentId,
	          FName(TEXT("stability")));
	State.ScenarioPhase = ECityScenarioPhase::Succeeded;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Original ten milestones complete before civic services"), Progress.CompletedIds.Num(),
	          10);
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Repeated evaluation is idempotent"), Progress.CompletedIds.Num(), 10);
	TestEqual(TEXT("Police is the next persisted civic teaching step"), Progress.CurrentId,
	          FName(TEXT("police")));
	State.bCrimeIntroduced = true;
	State.OperationalPolice = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Police protection records teaching completion"), Progress.CurrentId, FName(TEXT("fire")));
	State.bFireIntroduced = true;
	State.OperationalFire = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Both civic milestones persist"), Progress.CompletedIds.Num(), 12);
	TestEqual(TEXT("School follows safety services"), Progress.CurrentId, FName(TEXT("school")));
	State.OperationalSchools = 1;
	State.EducationServed = 10;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Education goal follows operating school access"), Progress.CurrentId,
	          FName(TEXT("education")));
	State.SecondaryResidents = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Park expansion follows education"), Progress.CurrentId, FName(TEXT("park_expansion")));
	State.OperationalLargeParks = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Hospital follows park expansion"), Progress.CurrentId, FName(TEXT("hospital")));
	State.HospitalCapacity = 24;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Treatment follows operating clinical capacity"), Progress.CurrentId,
	          FName(TEXT("health_recovery")));
	State.RecoveredResidents = 1;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Collection follows actual health recovery"), Progress.CurrentId,
	          FName(TEXT("waste_collection")));
	State.WasteFleet = 2;
	State.WasteCollected = 50;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Recycling follows actual collection"), Progress.CurrentId, FName(TEXT("recycling")));
	State.WasteRecycled = 30;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Actual recycling completes the current guide"), Progress.CurrentId,
	          FName(TEXT("complete")));
	State.OperationalFire = 0;
	TestEqual(TEXT("Fire outage surfaces recovery without erasing history"),
	          FCityFirstPlayableGuide::BuildObjective(State, Progress).Id, FName(TEXT("fire")));
	for (bool Fire : {false, true})
		for (int32 Threshold : {75, 100, 150, 225})
		{
			FCityFirstPlayableGuideState Ready;
			Ready.Residents = Threshold - 10;
			Ready.BuiltPolice = Ready.BuiltFire = Ready.OperationalPolice = Ready.OperationalFire = 1;
			Ready.CrimePopulationThreshold = Ready.FirePopulationThreshold = Threshold;
			Ready.CrimeGraceMonths = Ready.FireGraceMonths = 3;
			const FName Id = Fire ? TEXT("fire") : TEXT("police");
			auto Card = FCityFirstPlayableGuide::BuildMilestone(Id, Ready);
			TestTrue(TEXT("Ready safety station points to the actual difficulty population gate"),
			         Card.Title.Contains(FString::FromInt(Threshold)) &&
			             Card.Progress.Contains(FString::Printf(TEXT("/ %d"), Threshold)) &&
			             Card.Hint.Contains(TEXT("ready for growth")) && Card.Fraction < 1.0f &&
			             Card.Reason.Contains(TEXT("3 months")));
			Ready.bCrimeIntroduced = Ready.bFireIntroduced = true;
			Card = FCityFirstPlayableGuide::BuildMilestone(Id, Ready);
			TestEqual(TEXT("Introduced and staffed safety milestone is complete"), Card.Fraction, 1.0f);
			Ready.OperationalFire = Ready.OperationalPolice = 0;
			Card = FCityFirstPlayableGuide::BuildMilestone(Id, Ready);
			TestTrue(TEXT("Real outage requests restored protection"),
			         Card.Title.StartsWith(TEXT("Restore")) && Card.Fraction < 1.0f);
		}
	FString Error;
	TestTrue(TEXT("Canonical history validates"), FCityFirstPlayableGuide::ValidateProgress(Progress, Error));
	FCityGuidedProgress Invalid = Progress;
	Invalid.CompletedIds.Add(TEXT("power"));
	TestFalse(TEXT("Duplicate record is rejected"),
	          FCityFirstPlayableGuide::ValidateProgress(Invalid, Error));
	Invalid = {};
	Invalid.CompletedIds.Add(TEXT("fire_future"));
	TestFalse(TEXT("Unavailable milestone is rejected"),
	          FCityFirstPlayableGuide::ValidateProgress(Invalid, Error));
	Invalid = {};
	Invalid.CurrentId = TEXT("goods");
	TestFalse(TEXT("Inconsistent current stage is rejected"),
	          FCityFirstPlayableGuide::ValidateProgress(Invalid, Error));
	State = {};
	State.ScenarioPhase = ECityScenarioPhase::Running;
	State.bHasConnectedEntranceRoad = true;
	State.PowerCost = 600;
	State.Funds = 100;
	const auto FundsCard = FCityFirstPlayableGuide::BuildObjective(State);
	TestTrue(TEXT("Unaffordable utility explains actual funds"),
	         FundsCard.Hint.Contains(TEXT("$600")) && FundsCard.Hint.Contains(TEXT("$100")));
	State.OperationalPowerBuildings = State.OperationalServiceBuildings = 1;
	State.Residents = 1;
	State.Jobs = 1;
	State.CompletedPurchases = 1;
	State.WaterShortage = 4;
	Progress = {};
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	TestEqual(TEXT("Service milestone waits for zero shortages"), Progress.CurrentId,
	          FName(TEXT("basic_services")));
	TestTrue(TEXT("Water gap is exact"),
	         FCityFirstPlayableGuide::BuildObjective(State, Progress).Progress.Contains(TEXT("Water gap 4")));
	State.WaterShortage = 0;
	FCityFirstPlayableGuide::UpdateProgress(State, Progress);
	State.UpkeepShortfall = 20;
	TestEqual(TEXT("Later upkeep failure gets recovery while retaining history"),
	          FCityFirstPlayableGuide::BuildObjective(State, Progress).Id, FName(TEXT("basic_services")));
	TestTrue(TEXT("Upkeep recovery points to taxes and next billing"),
	         FCityFirstPlayableGuide::BuildObjective(State, Progress).Hint.Contains(TEXT("City > TAXES")) &&
	             FCityFirstPlayableGuide::BuildObjective(State, Progress).Hint.Contains(TEXT("paid month")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityGuidedIntegrationTest, "CitySim.Persistence.GuidedHistoryAndMigration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityGuidedIntegrationTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .AllowAudioPlayback(false)
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GuidedSaveTest"), nullptr, true,
	                                    ERHIFeatureLevel::Num, &Values);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	Sim->StartScenario();
	Sim->SetSimulationPaused(true);
	Sim->GetPlacementModel().SetTerrain(nullptr);
	auto Road = Sim->GetPlacementModel().BeginRoadPreview(FCityPlacementModel::EntranceCell);
	Sim->GetPlacementModel().UpdateRoadPreviewEnd(Road, {5, 8});
	TestTrue(TEXT("Entrance road fixture commits"), Sim->GetPlacementModel().CommitRoad(Road).IsSuccess());
	Sim->Tick(0);
	TestTrue(TEXT("Paused command state still records authoritative milestone"),
	         Sim->GetGuidedProgress().CompletedIds.Contains(TEXT("entrance_road")));
	const FCitySaveDocument ConnectedCity = Sim->BuildSaveDocument();
	Sim->GetPlacementModel().RemoveRoad(FCityPlacementModel::EntranceCell);
	Sim->Tick(0);
	const auto OutageProgress = Sim->GetGuidedProgress();
	TestTrue(TEXT("Demolition preserves record"),
	         OutageProgress.CompletedIds.Contains(TEXT("entrance_road")));
	FString Error;
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/GuidedObjectives"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString SavePath = FPaths::Combine(Directory, TEXT("City.json"));
	TestTrue(TEXT("Outage city saves with history"), Sim->SaveCityToPath(SavePath, Error));
	TestTrue(TEXT("History restores during an outage"), Sim->LoadCityFromPath(SavePath, Error, false));
	TestTrue(TEXT("Completed IDs survive save/load"),
	         Sim->GetGuidedProgress().CompletedIds == OutageProgress.CompletedIds);
	TestEqual(TEXT("Saved current stage survives recovery overlay"), Sim->GetGuidedProgress().CurrentId,
	          OutageProgress.CurrentId);
	TestTrue(TEXT("Loaded outage still shows recovery"),
	         Sim->GetGuidedObjective().Title.StartsWith(TEXT("Restore:")));
	FString Json;
	FCitySaveDocument LegacyCity = ConnectedCity;
	LegacyCity.Population.TotalArrivalPurchases = 1;
	TestTrue(TEXT("Migration fixture serializes"), FCityPersistence::Serialize(LegacyCity, Json, Error));
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetNumberField(TEXT("schemaVersion"), 21);
	Json.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *SavePath);
	TestFalse(TEXT("Historical format rejected"), Sim->LoadCityFromPath(SavePath, Error, false));
	FString Before, After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), Before, Error);
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Before), Root);
	Root->GetObjectField(TEXT("guidance"))
	    ->SetArrayField(TEXT("completedIds"), {MakeShared<FJsonValueString>(TEXT("unknown_future_stage"))});
	Json.Reset();
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	FFileHelper::SaveStringToFile(Json, *SavePath);
	TestFalse(TEXT("Corrupt history is rejected without fallback"),
	          Sim->LoadCityFromPath(SavePath, Error, false));
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("Rejected history load leaves full city unchanged"), After, Before);
	Sim->RequestResetScenario();
	Sim->RequestResetScenario();
	TestTrue(TEXT("Confirmed reset clears teaching history"),
	         Sim->GetGuidedProgress().CompletedIds.IsEmpty());
	TestEqual(TEXT("Reset starts at entrance stage"), Sim->GetGuidedProgress().CurrentId,
	          FName(TEXT("entrance_road")));
	World->DestroyWorld(false);
	return true;
}
#endif
