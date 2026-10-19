#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Persistence/CitySaveLibrary.h"
#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCitySaveLibraryTest, "CitySim.Persistence.LibraryRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCitySaveLibraryTest::RunTest(const FString& Parameters)
{
	const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Library"),
	                                    FGuid::NewGuid().ToString(EGuidFormats::Digits));
	FCitySaveDocument D;
	D.Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	D.Placement = FCityPlacementModel(12345).ExportState();
	D.Terrain.Seed = 21042;
	FString Json, Error;
	TestTrue(TEXT("Library fixture serializes"), FCityPersistence::Serialize(D, Json, Error));
	const FString Path = FCitySaveLibrary::PathFor(Dir, D.Identity.Id);
	TestTrue(TEXT("Commit city"), FCityPersistence::WriteAtomic(Path, Json, Error));
	TestTrue(TEXT("Retain backup"), FCityPersistence::WriteAtomic(Path, Json, Error));
	auto Slots = FCitySaveLibrary::List(Dir);
	TestEqual(TEXT("One canonical city"), Slots.Num(), 1);
	if (Slots.IsEmpty())
		return false;
	TestEqual(TEXT("Metadata derived from city treasury"), Slots[0].Funds, int64(12345));
	TestEqual(TEXT("Terrain parameters retained"), Slots[0].Terrain.Seed, 21042);
	const FDateTime Saved = Slots[0].LastSaved;
	FString Index;
	TestTrue(TEXT("Bounded index is present"),
	         FFileHelper::LoadFileToString(Index, *FCitySaveLibrary::IndexPath(Dir)));
	TestTrue(TEXT("Index contains stable identity"), Index.Contains(D.Identity.Id));
	TestFalse(TEXT("Interrupted index commit reports failure"),
	          FCitySaveLibrary::RefreshIndex(Dir, Error, true));
	FString IndexAfter;
	FFileHelper::LoadFileToString(IndexAfter, *FCitySaveLibrary::IndexPath(Dir));
	TestEqual(TEXT("Interrupted index keeps committed metadata"), IndexAfter, Index);
	TestEqual(TEXT("Pending index cannot hide city"), FCitySaveLibrary::List(Dir).Num(), 1);
	FFileHelper::SaveStringToFile(TEXT("broken"), *FCitySaveLibrary::IndexPath(Dir));
	TestEqual(TEXT("Corrupt index cannot hide city"), FCitySaveLibrary::List(Dir).Num(), 1);
	IFileManager::Get().Delete(*FCitySaveLibrary::IndexPath(Dir));
	TestEqual(TEXT("Missing index rebuilds from documents"), FCitySaveLibrary::List(Dir).Num(), 1);
	TestTrue(TEXT("Archive is reversible"), FCitySaveLibrary::SetArchived(Dir, Slots[0], true, Error));
	TestEqual(TEXT("Archived city leaves active view"), FCitySaveLibrary::List(Dir).Num(), 0);
	auto Archived = FCitySaveLibrary::List(Dir, FString(), true);
	TestEqual(TEXT("Archived view retains city"), Archived.Num(), 1);
	TestEqual(TEXT("Archive still counts toward city limit"), FCitySaveLibrary::Count(Dir), 1);
	if (!Archived.IsEmpty())
	{
		FString Bytes;
		FFileHelper::LoadFileToString(Bytes, *Archived[0].Path);
		TestEqual(TEXT("Archive preserves primary bytes"), Bytes, Json);
		FFileHelper::LoadFileToString(Bytes, *(Archived[0].Path + TEXT(".bak")));
		TestEqual(TEXT("Archive preserves backup bytes"), Bytes, Json);
		TestEqual(TEXT("Archive preserves saved timestamp"), Archived[0].LastSaved, Saved);
		TestTrue(TEXT("Restore retains identity and data"),
		         FCitySaveLibrary::SetArchived(Dir, Archived[0], false, Error));
	}
	Slots = FCitySaveLibrary::List(Dir);
	FFileHelper::SaveStringToFile(TEXT("broken"), *Path);
	Slots = FCitySaveLibrary::List(Dir);
	TestTrue(TEXT("Corrupt primary discovered through backup"), Slots.Num() == 1 && Slots[0].bBackupOnly);
	TestTrue(TEXT("Recoverable backup can be archived"),
	         FCitySaveLibrary::SetArchived(Dir, Slots[0], true, Error));
	Archived = FCitySaveLibrary::List(Dir, FString(), true);
	TestTrue(TEXT("Archived backup remains recoverable"), Archived.Num() == 1 && Archived[0].bBackupOnly);
	// A destination collision never overwrites either city copy.
	FCityPersistence::WriteAtomic(Path, Json, Error);
	TestFalse(TEXT("Restore refuses conflicting destination"),
	          FCitySaveLibrary::SetArchived(Dir, Archived[0], false, Error));
	TestTrue(TEXT("Conflict retains archived backup"),
	         IFileManager::Get().FileExists(*(Archived[0].Path + TEXT(".bak"))));
	// A foreign/legacy path cannot be moved even if its identity happens to match.
	auto Foreign = Archived[0];
	Foreign.Path = Dir / TEXT("../foreign.json");
	TestFalse(TEXT("Archive cannot move a foreign path"),
	          FCitySaveLibrary::SetArchived(Dir, Foreign, false, Error));
	Foreign = Archived[0];
	Foreign.bOriginalSingleFile = true;
	TestFalse(TEXT("Original legacy file is preserved"),
	          FCitySaveLibrary::SetArchived(Dir, Foreign, false, Error));
	IFileManager::Get().DeleteDirectory(*Dir, false, true);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityLibraryMenuTest, "CitySim.Menu.TerrainPreview",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityLibraryMenuTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues()
	                      .AllowAudioPlayback(false)
	                      .CreatePhysicsScene(false)
	                      .CreateNavigation(false)
	                      .CreateAISystem(false)
	                      .ShouldSimulatePhysics(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("LibraryMenuTest"), nullptr, true,
	                                  ERHIFeatureLevel::Num, &Init);
	auto* UI = World->SpawnActor<ACityVRToolController>();
	UI->Board = World->SpawnActor<ACityBoardActor>();
	auto* Owner = World->SpawnActor<AActor>();
	auto* Camera = NewObject<UCameraComponent>(Owner);
	Owner->SetRootComponent(Camera);
	Camera->RegisterComponent();
	UI->HeadsetCamera = Camera;
	const auto* Sim = World->GetSubsystem<UCitySimulationSubsystem>();
	const auto Funds = Sim->GetPlacementModel().GetFunds();
	const auto Seed = Sim->GetTerrain().GetParameters().Seed;
	UI->ShowMainMenu();
	UI->ShowNewCitySetup();
	TestTrue(TEXT("New terrain preview is visible"), UI->PaletteTerrainPreview->IsVisible());
	const int32 DraftSeed = UI->NewCityTerrain.Seed;
	FCityTerrainHeightfield Terrain;
	Terrain.Generate(UI->NewCityTerrain);
	int32 Counts[3] = {};
	for (int32 Y = 0; Y < FCityGrid::Height; ++Y)
		for (int32 X = 0; X < FCityGrid::Width; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			++Counts[Terrain.IsWaterCell(Cell)                                ? 1
			         : Terrain.GetCellSlope(Cell) == ECityTerrainSlope::Steep ? 2
			                                                                  : 0];
		}
	int32 TotalVertices = 0;
	for (int32 I = 0; I < 3; ++I)
	{
		const auto* Section = UI->PaletteTerrainPreview->GetProcMeshSection(I);
		TestEqual(TEXT("Preview class matches generated terrain"),
		          Section ? Section->ProcVertexBuffer.Num() : 0, Counts[I] * 4);
		if (Section && !Section->ProcVertexBuffer.IsEmpty())
		{
			TotalVertices += Section->ProcVertexBuffer.Num();
			const auto& V = Section->ProcVertexBuffer;
			TestTrue(TEXT("Preview triangles face the viewer in Unreal"),
			         FVector::CrossProduct(V[1].Position - V[0].Position, V[2].Position - V[0].Position).X <
			             0);
		}
	}
	const auto* Land = UI->PaletteTerrainPreview->GetProcMeshSection(0);
	TestTrue(TEXT("Elevation changes land shading"),
	         Land && Land->ProcVertexBuffer.ContainsByPredicate(
	                     [&](const auto& V) { return V.Color != Land->ProcVertexBuffer[0].Color; }));
	TestEqual(TEXT("Preview has a fixed vertex budget"), TotalVertices, FCityGrid::CellCount * 4);
	TestTrue(TEXT("Entry marker is visible"), UI->PaletteTerrainEntryMarker->IsVisible());
	UI->ReviewCityCreation();
	TestEqual(TEXT("Review uses chosen seed"), UI->PreviewTerrainParameters->Seed, DraftSeed);
	UI->Cancel();
	UI->RerollNewCityTerrain();
	TestEqual(TEXT("Reroll updates preview without changing city terrain"),
	          Sim->GetTerrain().GetParameters().Seed, Seed);
	TestEqual(TEXT("Reroll cannot spend funds"), Sim->GetPlacementModel().GetFunds(), Funds);
	// Exercise load/archive views using in-memory metadata; no real library files are moved by UI tests.
	UI->SetPalettePage(ECityPalettePage::LoadReview);
	UI->ReviewedCitySlots.Reset();
	FCitySaveSlot Slot;
	Slot.Identity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	Slot.Identity.Name = TEXT("City 123456");
	Slot.Terrain.Seed = 21042;
	Slot.Funds = 12345;
	Slot.LastSaved = FDateTime(2026, 10, 2);
	UI->ReviewedCitySlots.Add(Slot);
	UI->ReviewedCityIndex = 0;
	UI->RefreshPaletteText();
	TestEqual(TEXT("Load preview uses selected city's seed"), UI->PreviewTerrainParameters->Seed, 21042);
	TestTrue(TEXT("Load metadata exposes treasury"), UI->GetEntryMenuHelp().Contains(TEXT("$12345")));
	const auto Help = UI->PaletteHelp->CalcBounds(UI->PaletteHelp->GetRelativeTransform()).GetBox();
	TestTrue(TEXT("Preview and text have separate bounds"), Help.Max.Y < 6);
	TestTrue(TEXT("Load help remains inside background"), Help.Min.Y >= -34 && Help.Min.Z > 0);
	UI->ReviewSelectedCityArchive();
	TestEqual(TEXT("Archive requires review"), UI->PalettePage, ECityPalettePage::ArchiveReview);
	UI->Cancel();
	TestEqual(TEXT("Archive cancel returns to library"), UI->PalettePage, ECityPalettePage::LoadReview);
	TestEqual(TEXT("Review/cancel cannot change active city"), Sim->GetPlacementModel().GetFunds(), Funds);
	UI->bReviewArchivedCities = true;
	UI->ReviewedCitySlots[0].bArchived = true;
	TestTrue(TEXT("Archived city cannot load without restore"),
	         UI->IsPaletteActionDisabled(ECityPaletteAction::ConfirmLoad));
	UI->ConfirmReviewedCityLoad();
	TestEqual(TEXT("Archived confirmation retains city"), Sim->GetPlacementModel().GetFunds(), Funds);
	UI->ReturnToCity();
	TestFalse(TEXT("Return hides terrain preview"), UI->PaletteTerrainPreview->IsVisible());
	UI->SetPaletteOpen(true);
	UI->ReviewCityLoad();
	TestTrue(TEXT("Load review from tools uses the menu environment"), UI->IsMainMenuEnvironmentActive());
	TestEqual(TEXT("Opening review still preserves authority"), Sim->GetPlacementModel().GetFunds(), Funds);
	UI->ReturnToCity();
	World->DestroyWorld(false);
	return true;
}
#endif
