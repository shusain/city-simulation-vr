#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Persistence/CitySaveLibrary.h"
#include "Interaction/CityVRToolController.h"
#include "Presentation/CityDesktopHud.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

class FCityDesktopSaveRevisionCommand : public IAutomationLatentCommand
{
  public:
	explicit FCityDesktopSaveRevisionCommand(FAutomationTestBase* In) : Test(In)
	{
	}
	bool Update() override
	{
		if (FPlatformTime::Seconds() < Next)
			return false;
		auto* W = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
		ACityVRToolController* UI = nullptr;
		if (W)
			for (TActorIterator<ACityVRToolController> It(W); It; ++It)
			{
				UI = *It;
				break;
			}
		if (!UI || !UI->DesktopHud || !UI->Board)
		{
			if (++Attempts < 120)
			{
				Next = FPlatformTime::Seconds() + .5;
				return false;
			}
			Test->AddError(TEXT("Save review HUD missing"));
			return true;
		}
		UI->bDesktopFocused = true;
		UI->LifecycleInputGate.Update(false);
		auto* Sim = W->GetSubsystem<UCitySimulationSubsystem>();
		const FString Dir = FCitySaveLibrary::Directory();
		const FString OldPath = FCitySaveLibrary::PathFor(Dir, OldId);
		auto Click = [&](ECityPaletteAction Action) {
			const auto Data = UI->GetUiSnapshot();
			for (int32 I = 0; I < Data.MenuEntries.Num(); ++I)
				if (Data.MenuEntries[I].ActionId == static_cast<int32>(Action))
				{
					auto* B = Cast<UCityDesktopActionButton>(
					    UI->DesktopHud->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("Menu%d"), I))));
					if (B && B->GetIsEnabled())
						B->OnClicked.Broadcast();
					else
						Test->AddError(TEXT("Save review action unavailable"));
					return;
				}
			Test->AddError(TEXT("Save review action missing"));
		};
		auto Capture = [](const TCHAR* Name) {
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/SaveRevision") / Name;
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			FScreenshotRequest::RequestScreenshot(Path, true, false);
		};
		FString Error;
		switch (Phase)
		{
		case 0: {
			FCityIdentity I;
			I.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
			I.Name = TEXT("Fresh Revision City");
			if (!Sim->CreateCity(I, {}, Dir, Error))
			{
				Test->AddError(Error);
				return true;
			}
			FCitySaveDocument D;
			D.Identity.Id = OldId;
			D.Identity.Name = TEXT("Earlier Format City");
			D.Identity.CreatedGameRevision = TEXT("dev-previous-build");
			D.LastSavedGameRevision = TEXT("dev-previous-build");
			D.Placement = FCityPlacementModel(12345).ExportState();
			FString Json;
			FCityPersistence::Serialize(D, Json, Error);
			TSharedPtr<FJsonObject> Root;
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
			Root->SetNumberField(TEXT("schemaVersion"), FCitySaveDocument::PrePantrySchemaVersion - 1);
			Json.Reset();
			FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
			OldJson = Json;
			Test->TestTrue(TEXT("Old fixture stored"), FCityPersistence::WriteAtomic(OldPath, Json, Error));
			FFileHelper::SaveStringToFile(Json, *(OldPath + TEXT(".bak")));
			UI->ReviewCityLoad();
			UI->ReviewedCityIndex = UI->ReviewedCitySlots.IndexOfByPredicate(
			    [&](const auto& S) { return S.Identity.Id == I.Id; });
			UI->RefreshPaletteText();
			break;
		}
		case 1:
			Test->TestTrue(TEXT("Current revision shown"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(FCityGameRevision::Current()));
			Capture(TEXT("Current.png"));
			break;
		case 2:
			UI->ReviewedCityIndex = UI->ReviewedCitySlots.IndexOfByPredicate(
			    [&](const auto& S) { return S.Identity.Id == OldId; });
			UI->RefreshPaletteText();
			break;
		case 3:
			Test->TestNull(TEXT("Unsupported save cannot preview terrain"), UI->GetDesktopTerrainPreview());
			Test->TestTrue(TEXT("Load disabled for old format"),
			               UI->IsPaletteActionDisabled(ECityPaletteAction::ConfirmLoad));
			Test->TestFalse(TEXT("Archive enabled"),
			                UI->IsPaletteActionDisabled(ECityPaletteAction::ReviewArchive));
			Test->TestTrue(TEXT("Old build visible"),
			               UI->GetUiSnapshot().MenuBody.ToString().Contains(TEXT("dev-previous-build")));
			Capture(TEXT("Unsupported.png"));
			break;
		case 4:
			Click(ECityPaletteAction::ReviewArchive);
			break;
		case 5:
			Capture(TEXT("ArchiveReview.png"));
			break;
		case 6:
			Click(ECityPaletteAction::ConfirmArchive);
			break;
		case 7: {
			const auto Archived = FCitySaveLibrary::List(Dir, FString(), true);
			Test->TestTrue(
			    TEXT("Actual archive action moved old city"),
			    Archived.ContainsByPredicate([&](const auto& S) { return S.Identity.Id == OldId; }));
			FString Bytes;
			const FString ArchivePath =
			    FCitySaveLibrary::PathFor(FCitySaveLibrary::ArchiveDirectory(Dir), OldId);
			FFileHelper::LoadFileToString(Bytes, *ArchivePath);
			Test->TestEqual(TEXT("Primary unchanged by UI archive"), Bytes, OldJson);
			FFileHelper::LoadFileToString(Bytes, *(ArchivePath + TEXT(".bak")));
			Test->TestEqual(TEXT("Backup unchanged by UI archive"), Bytes, OldJson);
			Test->TestEqual(TEXT("Active creation revision unchanged"),
			                Sim->GetCityIdentity().CreatedGameRevision, FCityGameRevision::Current());
			UE_LOG(LogTemp, Display,
			       TEXT("CITYSIM_DESKTOP_SAVE_REVISION_COMPLETE Captures=3 SyntheticButtons=1 "
			            "PhysicalInputPending=1"));
			return true;
		}
		}
		++Phase;
		Next = FPlatformTime::Seconds() + .8;
		return false;
	}

  private:
	FAutomationTestBase* Test;
	FString OldId = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	FString OldJson;
	int32 Phase = 0, Attempts = 0;
	double Next = 0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDesktopSaveRevisionTest, "CitySim.Desktop.SaveRevisionLibrary",
                                 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCityDesktopSaveRevisionTest::RunTest(const FString& Parameters)
{
	FString UserDir;
	if (!FParse::Param(FCommandLine::Get(), TEXT("CityDesktop")) ||
	    !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || !UserDir.Contains(TEXT("SaveRevision")))
	{
		AddError(TEXT("Save revision smoke requires isolated SaveRevision Desktop profile"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCityDesktopSaveRevisionCommand(this));
	return true;
}
#endif
