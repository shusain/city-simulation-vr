#include "Interaction/CityVRToolController.h"
#include "Simulation/CitySimulationSubsystem.h"
#include "Presentation/CityBoardActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Persistence/CityPersistence.h"
#include "ProceduralMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Commands/CityPlacementModel.h"

void ACityVRToolController::ShowGameSettings()
{
	bSettingsFromMainMenu = PalettePage == ECityPalettePage::MainMenu;
	SetPaletteOpen(true);
	SetPalettePage(ECityPalettePage::Settings);
}

void ACityVRToolController::ShowDebugTools()
{
	if (!bDebugInfoVisible)
		return;
	SetPaletteOpen(true);
	SetPalettePage(ECityPalettePage::SafetyTests);
}

void ACityVRToolController::SetDebugModeEnabled(const bool bEnabled)
{
	SetDebugInfoVisible(bEnabled);
}

void ACityVRToolController::SetCitySummaryVisible(const bool bVisible)
{
	bNeedsOpen = bVisible;
	RefreshNeedsVisibility();
}

void ACityVRToolController::ShowMainMenu()
{
	SetEntrySceneActive(true);
	SetPaletteOpen(true);
	bMainMenuPinned = true;
	EntryMenuStatus.Reset();
	SetPalettePage(ECityPalettePage::MainMenu);
}

void ACityVRToolController::ShowNewCitySetup()
{
	if (!bEntrySceneActive || !bPaletteOpen)
		ShowMainMenu();
	NewCityDraft =
	    FCityNewCitySetup::Proposal(ECityDifficultyDraft::Normal,
	                                GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetEconomyTuning());
	EntryMenuStatus.Reset();
	NewCityIdentity = FCityIdentity();
	NewCityIdentity.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	NewCityTerrain = FCityTerrainGenerationParameters();
	RerollNewCityTerrain();
	SetPalettePage(ECityPalettePage::NewCitySetup);
}

void ACityVRToolController::ReviewCityLoad()
{
	if (!bEntrySceneActive || !bPaletteOpen)
		ShowMainMenu();
	EntryMenuStatus.Reset();
	bReviewArchivedCities = false;
	ReviewedCitySlots =
	    FCitySaveLibrary::List(FCitySaveLibrary::Directory(), FCityPersistence::GetDefaultSavePath(), false);
	ReviewedCityIndex = 0;
	SetPalettePage(ECityPalettePage::LoadReview);
}

void ACityVRToolController::ConfirmReviewedCityLoad()
{
	if (bReviewArchivedCities)
	{
		EntryMenuStatus = TEXT("Restore this city before loading it");
		RefreshPaletteText();
		return;
	}
	if (!ReviewedCitySlots.IsValidIndex(ReviewedCityIndex))
	{
		EntryMenuStatus = TEXT("No saved cities found.\nCancel to keep the current city.");
		RefreshPaletteText();
		return;
	}
	if (!ReviewedCitySlots[ReviewedCityIndex].bSupported)
	{
		EntryMenuStatus = ReviewedCitySlots[ReviewedCityIndex].UnsupportedReason;
		RefreshPaletteText();
		return;
	}
	ConfirmCityLoad(ReviewedCitySlots[ReviewedCityIndex].Path);
}

void ACityVRToolController::ConfirmCityLoad(const FString& SavePath)
{
	if (PalettePage != ECityPalettePage::LoadReview)
		return;
	auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	FString Error;
	if (Simulation->LoadCityFromPath(SavePath, Error))
	{
		// A saved running clock must not advance behind the review screen.
		Simulation->SetSimulationPaused(true);
		ClearInspection();
		if (Board)
			Board->RequestCancel();
		ReturnToCity();
	}
	else
	{
		EntryMenuStatus = TEXT("Load failed; current city retained\n") + Error;
		RefreshPaletteText();
	}
}

void ACityVRToolController::ReturnToCity()
{
	// Desktop has no world-space START-card hit target. Continuing/loading a
	// dormant city must enable its guide while retaining the city and clock.
	if (bDesktopUi)
		if (auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
		    Sim && Sim->GetScenarioSnapshot().Phase == ECityScenarioPhase::NotStarted)
		{
			const bool bPaused = Sim->IsSimulationPaused();
			Sim->StartScenario();
			Sim->SetSimulationPaused(bPaused);
		}
	if (Board)
		Board->PrepareCityView();
	SetEntrySceneActive(false);
	SetPaletteOpen(false);
}

void ACityVRToolController::SetEntrySceneActive(const bool bActive)
{
	if (bActive == bEntrySceneActive)
		return;
	bEntrySceneActive = bActive;
	if (auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>())
		Simulation->SetEntryMenuSuspended(bActive);
	SetDebugInfoVisible(bDebugInfoVisible);
	if (bActive)
	{
		ClearInspection();
		if (Board)
		{
			Board->RequestCancel();
			Board->SetControllerRay(FVector::ZeroVector, FVector::ForwardVector, false, true);
		}
		if (!EntrySkyMaterial)
		{
			EntrySkyMaterial = EntrySky->CreateDynamicMaterialInstance(0);
			if (EntrySkyMaterial)
			{
				EntrySkyMaterial->SetVectorParameterValue(TEXT("HorizonColor"),
				                                          FLinearColor(.22f, .40f, .62f));
				EntrySkyMaterial->SetVectorParameterValue(TEXT("ZenithColor"),
				                                          FLinearColor(.025f, .09f, .23f));
			}
		}
		// Hidden-in-game is independent of the dashboard's own visibility updates.
		// Preserve its prior flags so reopening/closing the menu cannot reveal hidden HUD.
		TInlineComponentArray<UPrimitiveComponent*> Components(this);
		for (UPrimitiveComponent* Component : Components)
		{
			if (Component == EntrySky || Component->GetName().StartsWith(TEXT("Palette")))
				continue;
			EntryHiddenUi.Add(Component, Component->bHiddenInGame);
			Component->SetHiddenInGame(true);
		}
		UpdateEntryScenePresentation();
	}
	else
	{
		for (const auto& Pair : EntryHiddenActors)
			if (AActor* Actor = Pair.Key.Get())
				Actor->SetActorHiddenInGame(Pair.Value);
		for (const auto& Pair : EntryHiddenUi)
			if (UPrimitiveComponent* Component = Pair.Key.Get())
				Component->SetHiddenInGame(Pair.Value);
		EntryHiddenActors.Reset();
		EntryHiddenUi.Reset();
	}
	EntrySky->SetVisibility(bActive);
}

void ACityVRToolController::UpdateEntryScenePresentation()
{
	if (!bEntrySceneActive)
		return;
	if (HeadsetCamera)
		EntrySky->SetWorldLocation(HeadsetCamera->GetComponentLocation());
	// Include late-created scenery; pawn/controller tracking and the menu remain available.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || Cast<APawn>(Actor) || !Actor->FindComponentByClass<UPrimitiveComponent>())
			continue;
		bool bPawnOwned = false;
		for (AActor* ParentOwner = Actor->GetOwner(); ParentOwner; ParentOwner = ParentOwner->GetOwner())
			if (Cast<APawn>(ParentOwner))
			{
				bPawnOwned = true;
				break;
			}
		if (bPawnOwned)
			continue;
		if (!EntryHiddenActors.Contains(Actor))
			EntryHiddenActors.Add(Actor, Actor->IsHidden());
		Actor->SetActorHiddenInGame(true);
	}
}

FCityPaletteRect ACityVRToolController::GetPaletteButtonRect(const int32 Index,
                                                             const ECityPaletteAction Action) const
{
	const int32 LayoutIndex = bEntrySceneActive && !FCityPaletteModel::IsEntryPage(PalettePage)
	                              ? Index + FCityPaletteModel::BarEntryCount
	                              : Index;
	return FCityPaletteModel::ButtonRect(PalettePage, LayoutIndex, Action, UsesHandPalette());
}

void ACityVRToolController::NavigatePaletteBack()
{
	EntryMenuStatus.Reset();
	const ECityPalettePage Parent = PalettePage == ECityPalettePage::Settings && bSettingsFromMainMenu
	                                    ? ECityPalettePage::MainMenu
	                                    : FCityPaletteModel::ParentPage(PalettePage);
	SetPalettePage(Parent);
}

void ACityVRToolController::RerollNewCityTerrain()
{
	const auto Dimensions = NewCityTerrain.Dimensions;
	NewCityTerrain = FCityTerrainGenerationParameters();
	NewCityTerrain.Dimensions = Dimensions;
	NewCityTerrain.GeneratorVersion = 2;
	NewCityTerrain.WaterRulesVersion = 1;
	NewCityTerrain.SceneryVersion = 1;
	NewCityTerrain.Seed = static_cast<int32>(GetTypeHash(FGuid::NewGuid()) & 0x7fffffff);
	NewCityIdentity.Name = FString::Printf(TEXT("City %06d"), NewCityTerrain.Seed % 1000000);
	EntryMenuStatus.Reset();
	RefreshPaletteText();
}
void ACityVRToolController::ReviewCityCreation()
{
	if (PalettePage != ECityPalettePage::NewCitySetup)
		return;
	NewCityIdentity.Difficulty = NewCityDraft.Difficulty;
	EntryMenuStatus.Reset();
	SetPalettePage(ECityPalettePage::NewCityReview);
}
void ACityVRToolController::ConfirmReviewedCityCreation()
{
	if (PalettePage != ECityPalettePage::NewCityReview)
		return;
	auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	FString Error;
	if (Simulation->CreateCity(NewCityIdentity, NewCityTerrain, FCitySaveLibrary::Directory(), Error))
		ReturnToCity();
	else
	{
		EntryMenuStatus = TEXT("Creation failed; current city retained\n") + Error;
		RefreshPaletteText();
	}
}
FString ACityVRToolController::GetEntryMenuHelp() const
{
	if (!EntryMenuStatus.IsEmpty())
		return EntryMenuStatus;
	if (PalettePage == ECityPalettePage::NewCitySetup)
		return FString::Printf(TEXT("%s | Seed %d\n"), *NewCityIdentity.Name, NewCityTerrain.Seed) +
		       FString::Printf(TEXT("Map: %d x %d%s\n"), NewCityTerrain.Dimensions.GetWidth(),
		                       NewCityTerrain.Dimensions.GetHeight(),
		                       NewCityTerrain.Dimensions.GetWidth() > 64 ? TEXT(" | Desktop only")
		                                                                 : TEXT(" | All platforms")) +
		       FString::Printf(TEXT("Natural disasters: %s\n"),
		                       FCityIdentity::DisasterFrequencyName(NewCityIdentity.DisasterFrequency)) +
		       NewCityDraft.Summary();
	if (PalettePage == ECityPalettePage::NewCityReview)
		return FString::Printf(
		    TEXT("Create %s? | %s | $%lld\nMap: %d x %d%s | Seed: %d\nNatural disasters: %s\nStarts paused "
		         "with fresh objectives.\n") TEXT("Unsaved current changes will be discarded.\nSAVE & BACK "
		                                          "saves the current city first.\nCANCEL returns to setup."),
		    *NewCityIdentity.Name, FCityIdentity::DifficultyName(NewCityIdentity.Difficulty),
		    FCityIdentity::StartingFunds(NewCityIdentity.Difficulty), NewCityTerrain.Dimensions.GetWidth(),
		    NewCityTerrain.Dimensions.GetHeight(),
		    NewCityTerrain.Dimensions.GetWidth() > 64 ? TEXT(" | Desktop only") : TEXT(""),
		    NewCityTerrain.Seed, FCityIdentity::DisasterFrequencyName(NewCityIdentity.DisasterFrequency));
	if (PalettePage == ECityPalettePage::ArchiveReview)
		return FString::Printf(TEXT("Archive %s?\nSaved city and backup are retained.\nRestore later from "
		                            "ARCHIVED CITIES.\nCurrent city stays unchanged."),
		                       *ArchiveReviewSlot.Identity.Name);
	if (PalettePage == ECityPalettePage::LoadReview)
	{
		if (!ReviewedCitySlots.IsValidIndex(ReviewedCityIndex))
			return bReviewArchivedCities ? TEXT("No archived cities.\nACTIVE CITIES returns to saved "
			                                    "cities.\nCANCEL keeps the current city.")
			                             : TEXT("No saved cities found.\nSAVE & BACK saves the current "
			                                    "city.\nCANCEL keeps the current city.");
		const auto& Slot = ReviewedCitySlots[ReviewedCityIndex];
		FString Compatibility;
		const bool bSupported =
		    Slot.bSupported &&
		    FCityPersistence::ValidatePlatform(Slot.Dimensions, FCityPersistence::NativePlatform(!bDesktopUi),
		                                       Compatibility);
		if (!Slot.bSupported)
			Compatibility = Slot.UnsupportedReason;
		return FString::Printf(
		    TEXT("%s %d/%d: %s\n%s | Saved UTC %s%s\nMap %dx%d | Month %lld | %d residents\n$%lld | %d "
		         "report months\nCreated: %s\nLast saved: %s | Format %d\n%s"),
		    bReviewArchivedCities ? TEXT("Archived") : TEXT("City"), ReviewedCityIndex + 1,
		    ReviewedCitySlots.Num(), *Slot.Identity.Name,
		    FCityIdentity::DifficultyName(Slot.Identity.Difficulty),
		    *Slot.LastSaved.ToString(TEXT("%Y-%m-%d %H:%M")), Slot.bBackupOnly ? TEXT(" | backup") : TEXT(""),
		    Slot.Dimensions.GetWidth(), Slot.Dimensions.GetHeight(), Slot.MonthId, Slot.Residents, Slot.Funds,
		    Slot.ReportMonths,
		    Slot.Identity.CreatedGameRevision.IsEmpty() ? TEXT("unknown")
		                                                : *Slot.Identity.CreatedGameRevision,
		    Slot.LastSavedGameRevision.IsEmpty() ? TEXT("unknown") : *Slot.LastSavedGameRevision,
		    Slot.SchemaVersion,
		    bReviewArchivedCities ? TEXT("RESTORE returns files to active cities.")
		    : !bSupported         ? *Compatibility
		    : Slot.Identity.Id == GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCityIdentity().Id
		        ? TEXT("Current city; archive another city.")
		        : TEXT("LOAD replaces current city, paused. Save unsaved changes first."));
	}
	const auto* Simulation = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
	return FString::Printf(
	    TEXT("CONTINUE: %s (%s)\nLOAD chooses a saved city\nNEW CITY creates fresh terrain and objectives"),
	    *Simulation->GetCityIdentity().Name,
	    Simulation->IsSimulationPaused() ? TEXT("paused") : TEXT("running"));
}

void ACityVRToolController::RefreshCityLibrary()
{
	FString PriorId;
	if (ReviewedCitySlots.IsValidIndex(ReviewedCityIndex))
		PriorId = ReviewedCitySlots[ReviewedCityIndex].Identity.Id;
	ReviewedCitySlots = FCitySaveLibrary::List(FCitySaveLibrary::Directory(),
	                                           FCityPersistence::GetDefaultSavePath(), bReviewArchivedCities);
	ReviewedCityIndex =
	    ReviewedCitySlots.IndexOfByPredicate([&](const auto& S) { return S.Identity.Id == PriorId; });
	if (ReviewedCityIndex == INDEX_NONE)
		ReviewedCityIndex = 0;
	EntryMenuStatus.Reset();
	SetPalettePage(ECityPalettePage::LoadReview);
}
void ACityVRToolController::ReviewSelectedCityArchive()
{
	if (PalettePage != ECityPalettePage::LoadReview ||
	    IsPaletteActionDisabled(ECityPaletteAction::ReviewArchive))
		return;
	ArchiveReviewSlot = ReviewedCitySlots[ReviewedCityIndex];
	EntryMenuStatus.Reset();
	SetPalettePage(ECityPalettePage::ArchiveReview);
}
void ACityVRToolController::ConfirmSelectedCityArchive()
{
	if (PalettePage != ECityPalettePage::ArchiveReview)
		return;
	if (ArchiveReviewSlot.Identity.Id ==
	    GetWorld()->GetSubsystem<UCitySimulationSubsystem>()->GetCityIdentity().Id)
	{
		EntryMenuStatus = TEXT("Switch cities before archiving the current city");
		RefreshPaletteText();
		return;
	}
	FString Error;
	if (FCitySaveLibrary::SetArchived(FCitySaveLibrary::Directory(), ArchiveReviewSlot, true, Error))
		RefreshCityLibrary();
	else
	{
		EntryMenuStatus = Error;
		RefreshPaletteText();
	}
}
void ACityVRToolController::RestoreSelectedCity()
{
	if (PalettePage != ECityPalettePage::LoadReview ||
	    IsPaletteActionDisabled(ECityPaletteAction::RestoreCity))
		return;
	const auto Slot = ReviewedCitySlots[ReviewedCityIndex];
	FString Error;
	if (FCitySaveLibrary::SetArchived(FCitySaveLibrary::Directory(), Slot, false, Error))
	{
		bReviewArchivedCities = false;
		RefreshCityLibrary();
		ReviewedCityIndex = ReviewedCitySlots.IndexOfByPredicate(
		    [&](const auto& S) { return S.Identity.Id == Slot.Identity.Id; });
		RefreshPaletteText();
	}
	else
	{
		EntryMenuStatus = Error;
		RefreshPaletteText();
	}
}
void ACityVRToolController::UpdateTerrainPreview()
{
	const FCityTerrainGenerationParameters* Parameters = nullptr;
	if (bPaletteOpen)
	{
		if (PalettePage == ECityPalettePage::NewCitySetup || PalettePage == ECityPalettePage::NewCityReview)
			Parameters = &NewCityTerrain;
		else if (PalettePage == ECityPalettePage::ArchiveReview && ArchiveReviewSlot.bSupported)
			Parameters = &ArchiveReviewSlot.Terrain;
		else if (PalettePage == ECityPalettePage::LoadReview &&
		         ReviewedCitySlots.IsValidIndex(ReviewedCityIndex) &&
		         ReviewedCitySlots[ReviewedCityIndex].bSupported)
			Parameters = &ReviewedCitySlots[ReviewedCityIndex].Terrain;
	}
	PaletteTerrainPreview->SetVisibility(Parameters != nullptr);
	PaletteTerrainLegend->SetVisibility(Parameters != nullptr);
	PaletteTerrainEntryMarker->SetVisibility(Parameters != nullptr);
	if (!Parameters)
		return;
	if (PreviewTerrainParameters.IsSet() && PreviewTerrainParameters.GetValue() == *Parameters)
		return;
	FCityTerrainHeightfield Terrain;
	if (!Terrain.Generate(*Parameters))
	{
		PaletteTerrainPreview->SetVisibility(false);
		PaletteTerrainEntryMarker->SetVisibility(false);
		PaletteTerrainLegend->SetText(FText::FromString(TEXT("Terrain unavailable")));
		return;
	}
	PreviewTerrainParameters = *Parameters;
	PaletteTerrainPreview->ClearAllMeshSections();
	TArray<FVector> Vertices[3];
	TArray<int32> Indices[3];
	TArray<FLinearColor> Colors[3];
	int32 Water = 0, Steep = 0;
	double Minimum = MAX_int32, Maximum = MIN_int32;
	for (int32 Y = 0; Y < Terrain.GetDimensions().GetHeight(); ++Y)
		for (int32 X = 0; X < Terrain.GetDimensions().GetWidth(); ++X)
		{
			const double Height = Terrain.GetCellCenterHeightCm(FCityCellCoord(X, Y));
			Minimum = FMath::Min(Minimum, Height);
			Maximum = FMath::Max(Maximum, Height);
		}
	const double Scale = 24.0 / Terrain.GetDimensions().GetWidth();
	for (int32 Y = 0; Y < Terrain.GetDimensions().GetHeight(); ++Y)
		for (int32 X = 0; X < Terrain.GetDimensions().GetWidth(); ++X)
		{
			const FCityCellCoord Cell(X, Y);
			const int32 Type = Terrain.IsWaterCell(Cell)                                ? 1
			                   : Terrain.GetCellSlope(Cell) == ECityTerrainSlope::Steep ? 2
			                                                                            : 0;
			Water += Type == 1;
			Steep += Type == 2;
			const int32 Base = Vertices[Type].Num();
			const float Height = static_cast<float>((Terrain.GetCellCenterHeightCm(Cell) - Minimum) /
			                                        FMath::Max(1.0, Maximum - Minimum));
			const FLinearColor Color =
			    Type == 1   ? FLinearColor(.12f, .46f, .92f)
			    : Type == 2 ? FLinearColor(.95f, .78f, .08f)
			                : FLinearColor(.06f + .14f * Height, .22f + .40f * Height, .07f + .13f * Height);
			for (int32 Corner = 0; Corner < 4; ++Corner)
				Colors[Type].Add(Color);
			const double Left = 30 - X * Scale, Bottom = 6 + Y * Scale;
			Vertices[Type].Append({FVector(1.5, Left, Bottom), FVector(1.5, Left - Scale, Bottom),
			                       FVector(1.5, Left - Scale, Bottom + Scale),
			                       FVector(1.5, Left, Bottom + Scale)});
			// Unreal front faces use clockwise winding when viewed from local +X.
			Indices[Type].Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
		}
	for (int32 Type = 0; Type < 3; ++Type)
		if (!Vertices[Type].IsEmpty())
			PaletteTerrainPreview->CreateMeshSection_LinearColor(
			    Type, Vertices[Type], Indices[Type], TArray<FVector>(), TArray<FVector2D>(), Colors[Type],
			    TArray<FProcMeshTangent>(), false);
	if (auto* MarkerMaterial = PaletteTerrainEntryMarker->CreateAndSetMaterialInstanceDynamic(0))
		MarkerMaterial->SetVectorParameterValue(TEXT("BarColor"), FLinearColor::White);
	const FCityCellCoord Entrance = Parameters->GetEntranceCell();
	PaletteTerrainEntryMarker->SetRelativeLocation(
	    FVector(1.9, 30 - (Entrance.X + .5) * Scale, 6 + (Entrance.Y + .5) * Scale));
	PaletteTerrainLegend->SetText(FText::FromString(FString::Printf(
	    TEXT("%s | White entry\nBlue water %d%% | Gold steep %d%%\nGreen height: dark = low"),
	    *Parameters->DescribeLandscape(), Water * 100 / Terrain.GetDimensions().GetCellCount(),
	    Steep * 100 / Terrain.GetDimensions().GetCellCount())));
}
