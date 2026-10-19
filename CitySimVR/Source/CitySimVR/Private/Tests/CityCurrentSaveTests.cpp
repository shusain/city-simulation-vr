#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Persistence/CitySaveLibrary.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Interaction/CityVRToolController.h"
#include "Engine/World.h"

namespace
{
FString EncodeCurrentSave(const TSharedPtr<FJsonObject>& Root)
{
	FString Json;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	return Json;
}
} // namespace
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCurrentFormatTest, "CitySim.Persistence.CurrentFormatAndPantryUpgrade",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCurrentFormatTest::RunTest(const FString& Parameters)
{
	FCitySaveDocument D, Loaded;
	D.Placement = FCityPlacementModel(12345).ExportState();
	D.Identity.CreatedGameRevision = TEXT("dev-original-build");
	FString Json, Error;
	if (!TestTrue(TEXT("Current fixture saves"), FCityPersistence::Serialize(D, Json, Error)))
		return false;
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	for (int32 Schema = 1; Schema <= FCitySaveDocument::CurrentSchemaVersion + 1; ++Schema)
	{
		if (Schema == FCitySaveDocument::CurrentSchemaVersion)
			continue;
		Root->SetNumberField(TEXT("schemaVersion"), Schema);
		if (Schema == FCitySaveDocument::PrePantrySchemaVersion || Schema == FCitySaveDocument::PreOutpatientSchemaVersion || Schema == FCitySaveDocument::PreAiSchemaVersion)
		{
			TestTrue(TEXT("Immediately preceding maintained cities upgrade in memory"), FCityPersistence::Deserialize(EncodeCurrentSave(Root), Loaded, Error));
			TestEqual(TEXT("Upgraded document always writes the new schema"), Loaded.SchemaVersion, FCitySaveDocument::CurrentSchemaVersion);
			D.SchemaVersion = Schema; FString Output;
			TestFalse(TEXT("Never write an incompatible basket as the old format"), FCityPersistence::Serialize(D, Output, Error));
			continue;
		}
		Loaded.Identity.Name = TEXT("untouched");
		ECitySaveReadFailure Failure;
		TestFalse(TEXT("Every old/future schema rejects"),
		          FCityPersistence::Deserialize(EncodeCurrentSave(Root), Loaded, Error,
		                                        ECitySavePlatform::Any, &Failure));
		TestEqual(TEXT("Rejection classified before reading simulation"), Failure,
		          ECitySaveReadFailure::UnsupportedSchema);
		TestEqual(TEXT("Output untouched"), Loaded.Identity.Name, FString(TEXT("untouched")));
		D.SchemaVersion = Schema;
		FString Output = TEXT("untouched");
		TestFalse(TEXT("No historical writer"), FCityPersistence::Serialize(D, Output, Error));
		TestEqual(TEXT("Failed writer preserves output"), Output, FString(TEXT("untouched")));
	}
	D.SchemaVersion = FCitySaveDocument::CurrentSchemaVersion;
	Root->SetNumberField(TEXT("schemaVersion"), D.SchemaVersion);
	for (const TCHAR* Field :
	     {TEXT("createdGameRevision"), TEXT("lastSavedGameRevision"), TEXT("traffic"), TEXT("population"),
	      TEXT("development"), TEXT("crime"), TEXT("fire"), TEXT("disasters"), TEXT("waste"), TEXT("health")})
	{
		auto Broken = MakeShared<FJsonObject>(*Root);
		Broken->RemoveField(Field);
		TestFalse(TEXT("All current state required"),
		          FCityPersistence::Deserialize(EncodeCurrentSave(Broken), Loaded, Error));
	}
	Root->SetStringField(TEXT("createdGameRevision"), TEXT("invalid revision\n"));
	TestFalse(TEXT("Invalid creation revision rejects"),
	          FCityPersistence::Deserialize(EncodeCurrentSave(Root), Loaded, Error));
	Root->SetStringField(TEXT("createdGameRevision"), D.Identity.CreatedGameRevision);
	TestTrue(TEXT("Current format loads"),
	         FCityPersistence::Deserialize(EncodeCurrentSave(Root), Loaded, Error));
	TestEqual(TEXT("Creation revision retained"), Loaded.Identity.CreatedGameRevision,
	          D.Identity.CreatedGameRevision);
	TestEqual(TEXT("Latest save revision retained"), Loaded.LastSavedGameRevision,
	          FCityGameRevision::Current());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityUnsupportedArchiveTest, "CitySim.Persistence.UnsupportedCityArchive",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityUnsupportedArchiveTest::RunTest(const FString& Parameters)
{
	const FString Dir =
	    FPaths::ProjectSavedDir() / TEXT("Automation/SaveRevision") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FCitySaveDocument D;
	D.Placement = FCityPlacementModel(12345).ExportState();
	D.Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	D.Identity.CreatedGameRevision = TEXT("dev-old-build");
	FString Json, Error;
	FCityPersistence::Serialize(D, Json, Error);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetNumberField(TEXT("schemaVersion"), FCitySaveDocument::PrePantrySchemaVersion - 1);
	Root->RemoveField(TEXT("traffic")); // Metadata discovery never needs historical simulation.
	const FString Old = EncodeCurrentSave(Root);
	const FString Path = FCitySaveLibrary::PathFor(Dir, D.Identity.Id);
	TestTrue(TEXT("Write unsupported primary"), FCityPersistence::WriteAtomic(Path, Old, Error));
	FFileHelper::SaveStringToFile(Json, *(Path + TEXT(".bak")));
	auto Slots = FCitySaveLibrary::List(Dir);
	if (!TestEqual(TEXT("Unsupported primary stays discoverable"), Slots.Num(), 1))
		return false;
	TestFalse(TEXT("Old primary cannot become a supported backup load"), Slots[0].bSupported);
	TestFalse(TEXT("Primary metadata wins"), Slots[0].bBackupOnly);
	TestEqual(TEXT("Original creation revision visible"), Slots[0].Identity.CreatedGameRevision,
	          D.Identity.CreatedGameRevision);
	TestTrue(TEXT("Unsupported city archives"), FCitySaveLibrary::SetArchived(Dir, Slots[0], true, Error));
	auto Archived = FCitySaveLibrary::List(Dir, FString(), true);
	if (!TestEqual(TEXT("Archived metadata retained"), Archived.Num(), 1))
		return false;
	FString Bytes;
	FFileHelper::LoadFileToString(Bytes, *Archived[0].Path);
	TestEqual(TEXT("Old primary preserved byte for byte"), Bytes, Old);
	FFileHelper::LoadFileToString(Bytes, *(Archived[0].Path + TEXT(".bak")));
	TestEqual(TEXT("Different backup preserved byte for byte"), Bytes, Json);
	TestTrue(TEXT("Original archive restores without migration"),
	         FCitySaveLibrary::SetArchived(Dir, Archived[0], false, Error));
	Root->RemoveField(TEXT("createdGameRevision"));
	Root->RemoveField(TEXT("lastSavedGameRevision"));
	FFileHelper::SaveStringToFile(EncodeCurrentSave(Root), *Path);
	Slots = FCitySaveLibrary::List(Dir);
	TestTrue(TEXT("Never invent a revision for an older file"),
	         Slots.Num() == 1 && Slots[0].Identity.CreatedGameRevision.IsEmpty() &&
	             Slots[0].LastSavedGameRevision.IsEmpty());
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityCreationRevisionTest,
                                 "CitySim.Persistence.LiveCreationRevisionAndRejection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityCreationRevisionTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("SaveRevisionSaveRevision"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	auto* Catalog = LoadObject<UCityBuildingCatalog>(
	    nullptr, TEXT("/Game/CitySim/Data/DA_CityBuildingCatalog.DA_CityBuildingCatalog"));
	Sim->RegisterBuildingCatalog(Catalog);
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Automation/SaveRevisionLive") /
	                    FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FCityIdentity Identity;
	Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Identity.CreatedGameRevision = TEXT("caller-cannot-fake-creation");
	FString Error, Json;
	if (!TestTrue(TEXT("Fresh city created"), Sim->CreateCity(Identity, {}, Dir, Error)))
	{
		AddError(Error);
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Actual executable stamps creation"), Sim->GetCityIdentity().CreatedGameRevision,
	          FCityGameRevision::Current());
	const FString Path = FCitySaveLibrary::PathFor(Dir, Identity.Id);
	FFileHelper::LoadFileToString(Json, *Path);
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetStringField(TEXT("createdGameRevision"), TEXT("dev-original-creation"));
	Root->SetStringField(TEXT("lastSavedGameRevision"), TEXT("dev-previous-save"));
	FFileHelper::SaveStringToFile(EncodeCurrentSave(Root), *Path);
	TestTrue(TEXT("Same format from a different build loads"), Sim->LoadCityFromPath(Path, Error, false));
	TestTrue(TEXT("Loaded city saves in current build"), Sim->SaveCityToPath(Path, Error));
	FCitySaveDocument Saved;
	FFileHelper::LoadFileToString(Json, *Path);
	TestTrue(TEXT("Saved city reads"), FCityPersistence::Deserialize(Json, Saved, Error));
	TestEqual(TEXT("Resave cannot rewrite creation revision"), Saved.Identity.CreatedGameRevision,
	          FString(TEXT("dev-original-creation")));
	TestEqual(TEXT("Resave stamps current revision"), Saved.LastSavedGameRevision,
	          FCityGameRevision::Current());
	const uint64 Generation = Sim->GetCitySessionGeneration();
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
	Root->SetNumberField(TEXT("schemaVersion"), FCitySaveDocument::PrePantrySchemaVersion - 1);
	FFileHelper::SaveStringToFile(EncodeCurrentSave(Root), *Path);
	FFileHelper::SaveStringToFile(Json, *(Path + TEXT(".bak")));
	TestFalse(TEXT("Unsupported primary cannot fall back to a different format"),
	          Sim->LoadCityFromPath(Path, Error, true));
	TestEqual(TEXT("Rejected load keeps live generation"), Sim->GetCitySessionGeneration(), Generation);
	FString After;
	FCityPersistence::Serialize(Sim->BuildSaveDocument(), After, Error);
	TestEqual(TEXT("All live authorities unchanged"), After, Json);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->bDesktopUi = true;
	UI->SetPalettePage(ECityPalettePage::LoadReview);
	UI->ReviewedCitySlots = FCitySaveLibrary::List(Dir);
	UI->ReviewedCityIndex = 0;
	TestTrue(TEXT("Unsupported load disabled"), UI->IsPaletteActionDisabled(ECityPaletteAction::ConfirmLoad));
	TestTrue(TEXT("Revision visible in shared review"),
	         UI->GetEntryMenuHelp().Contains(TEXT("dev-original-creation")));
	TestTrue(TEXT("Compatibility reason visible"),
	         UI->GetEntryMenuHelp().Contains(TEXT("Unsupported format")));
	UI->ConfirmReviewedCityLoad();
	TestEqual(TEXT("Direct confirmation remains guarded"), Sim->GetCitySessionGeneration(), Generation);
	World->DestroyWorld(false);
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}
#endif
