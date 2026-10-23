#include "Interaction/CityVRToolController.h"
#include "Engine/World.h"
#include "Persistence/CityPersistence.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CitySimulationSubsystem.h"

// Desktop layout only. The shared commands and VR palette retain their existing behavior.
void ACityVRToolController::PopulateDesktopEntry(FCityUiSnapshot& Data) const
{
    using A = ECityPaletteAction;
    using P = ECityPalettePage;
    const bool Overlay = PalettePage == P::Overlays;
    const bool Main = PalettePage == P::MainMenu;
    const bool Setup = PalettePage == P::NewCitySetup;
    const bool Review = PalettePage == P::NewCityReview;
    const bool Library = PalettePage == P::LoadReview;
    const bool Archive = PalettePage == P::ArchiveReview;
    if (!Overlay && !Main && !Setup && !Review && !Library && !Archive) return;
    Data.MenuBody = FText::GetEmpty();
    // These screens only review a draft or a saved city; saving the live city belongs
    // to the main city controls/settings, not the item being reviewed here.
    if (Review || Library) Data.MenuEntries.RemoveAll([](const FCityUiActionView& E) {
        return E.ActionId == static_cast<int32>(A::SaveAndBack);
    });
    auto Section = [&](const FString& Title, const FString& Body = TEXT(""), bool Warning = false) -> FCityUiMenuSection& {
        auto& S = Data.MenuSections.AddDefaulted_GetRef(); S.Id = Title;
        S.Title = FText::FromString(Title); S.Body = FText::FromString(Body); S.bWarning = Warning; return S;
    };
    auto Row = [](const FString& Label, const FString& Value) {
        FCityUiGauge R; R.Label = FText::FromString(Label); R.Value = FText::FromString(Value); return R;
    };
    auto Money = [](int64 Value) { return TEXT("$") + FText::AsNumber(Value).ToString(); };
    auto Dimensions = [](const FCityGridDimensions& D) { return FString::Printf(TEXT("%d x %d | %s"), D.GetWidth(), D.GetHeight(), D.GetWidth()>64 ? TEXT("Desktop only") : TEXT("All platforms")); };
    const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
    const bool HasSlot = ReviewedCitySlots.IsValidIndex(ReviewedCityIndex);
    const bool Current = HasSlot && Sim && ReviewedCitySlots[ReviewedCityIndex].Identity.Id == Sim->GetCityIdentity().Id;
    if (!Overlay && !EntryMenuStatus.IsEmpty()) Section(TEXT("STATUS"), EntryMenuStatus, true);
    TArray<A> Order;
    if (Main && Sim)
    {
        auto& S = Section(Sim->GetCityIdentity().Name);
        S.Rows = {Row(TEXT("Current city"), Sim->IsSimulationPaused() ? TEXT("Paused") : TEXT("Running"))};
    }
    if (Setup || Review)
    {
        auto& S = Section(NewCityIdentity.Name, Review ? TEXT("Starts paused with fresh objectives.") : TEXT("Choose difficulty, then terrain. Your current city stays unchanged."));
        S.Rows = {Row(TEXT("Difficulty"), FCityIdentity::DifficultyName(NewCityDraft.Difficulty)),
            Row(TEXT("Starting funds"), Money(FCityIdentity::StartingFunds(NewCityDraft.Difficulty)))};
        if (Setup)
        {
            auto& Rules = Section(TEXT("Difficulty rules"), NewCityDraft.Summary()); Rules.bCollapsible = true;
            Order = {A::DifficultyRelaxed,A::DifficultyNormal,A::DifficultyChallenging,A::CycleMapSize,A::CycleDisasters,A::RerollTerrain,A::ReviewCreate,A::Back};
        }
        else
        {
            S.Rows.Append({Row(TEXT("Map / platform"), Dimensions(NewCityTerrain.Dimensions)),
                Row(TEXT("Seed"), FString::FromInt(NewCityTerrain.Seed)),
                Row(TEXT("Natural disasters"), FCityIdentity::DisasterFrequencyName(NewCityIdentity.DisasterFrequency))});
            Section(TEXT("BEFORE CREATING"), TEXT("Creating this city discards unsaved changes to the city you were playing. Back to setup keeps your draft."), true);
            Order = {A::ConfirmCreate,A::Back};
        }
    }
    if (Library)
    {
        Data.MenuTitle = FText::FromString(bReviewArchivedCities ? TEXT("ARCHIVED CITIES") : TEXT("CITY LIBRARY"));
        if (!HasSlot) Section(bReviewArchivedCities ? TEXT("NO ARCHIVED CITIES") : TEXT("NO SAVED CITIES"), bReviewArchivedCities ? TEXT("Archived cities can be restored here. Return to Active cities to browse saved cities.") : TEXT("Saved cities appear here. Save while playing a city to add it to the library."));
        else
        {
            const auto& Slot = ReviewedCitySlots[ReviewedCityIndex];
            Data.MenuStatus = FText::FromString(FString::Printf(TEXT("%s | City %d / %d"), bReviewArchivedCities ? TEXT("Archived") : TEXT("Active"), ReviewedCityIndex+1, ReviewedCitySlots.Num()));
            auto& S = Section(Slot.Identity.Name, Current ? TEXT("CURRENT CITY") : TEXT(""));
            S.Rows = {Row(TEXT("Difficulty"), FCityIdentity::DifficultyName(Slot.Identity.Difficulty)),
                Row(TEXT("Saved UTC"), Slot.LastSaved.ToString(TEXT("%Y-%m-%d %H:%M"))),
                Row(TEXT("Map / platform"), Dimensions(Slot.Dimensions)), Row(TEXT("Month"), FString::Printf(TEXT("%lld"), Slot.MonthId)),
                Row(TEXT("Residents"), FText::AsNumber(Slot.Residents).ToString()), Row(TEXT("Funds"), Money(Slot.Funds))};
            auto& Details = Section(TEXT("Save details")); Details.bCollapsible = true;
            Details.Rows = {Row(TEXT("Created revision"), Slot.Identity.CreatedGameRevision.IsEmpty() ? TEXT("Unknown") : Slot.Identity.CreatedGameRevision),
                Row(TEXT("Saved revision"), Slot.LastSavedGameRevision.IsEmpty() ? TEXT("Unknown") : Slot.LastSavedGameRevision),
                Row(TEXT("Save format"), FString::FromInt(Slot.SchemaVersion)), Row(TEXT("Report months"), FString::FromInt(Slot.ReportMonths))};
            FString Compatibility;
            const bool Supported = Slot.bSupported && FCityPersistence::ValidatePlatform(Slot.Dimensions, FCityPersistence::NativePlatform(false), Compatibility);
            if (!Slot.bSupported) Compatibility = Slot.UnsupportedReason;
            if (!Supported) Section(TEXT("CANNOT LOAD THIS SAVE"), Compatibility, true);
            if (Slot.bBackupOnly) Section(TEXT("BACKUP SAVE"), TEXT("Only the backup is available for this city."), true);
            if (bReviewArchivedCities) Section(TEXT("RESTORE TO ACTIVE"), TEXT("Restore returns the saved city and backup to Active cities without loading them."));
            else if (Supported) Section(TEXT("BEFORE LOADING"), TEXT("Load saved city replaces the current view and discards unsaved changes. The loaded city starts paused."), true);
        }
        Data.MenuEntries.RemoveAll([&](const FCityUiActionView& E) {
            const auto Id = static_cast<A>(E.ActionId);
            return (!HasSlot && (Id==A::PreviousCity || Id==A::NextCity || Id==A::ConfirmLoad || Id==A::ReviewArchive || Id==A::RestoreCity)) ||
                (bReviewArchivedCities ? Id==A::ConfirmLoad || Id==A::ReviewArchive : Id==A::RestoreCity);
        });
        Order = {A::ToggleArchivedCities,A::PreviousCity,A::NextCity,A::ConfirmLoad,A::RestoreCity,A::RefreshCityLibrary,A::ReviewArchive,A::Back};
    }
    if (Archive)
    {
        Section(ArchiveReviewSlot.Identity.Name, TEXT("Saved city and backup move to a recoverable archive. Restore later from Archived cities. Your current view stays unchanged."));
    }
    if (Overlay)
    {
        FString Active = TEXT("None");
        const TPair<ECityDiagnosticOverlay,const TCHAR*> Names[] = {
            {ECityDiagnosticOverlay::Service,TEXT("Water")},{ECityDiagnosticOverlay::Power,TEXT("Power")},
            {ECityDiagnosticOverlay::ResidentialDemand,TEXT("Residential demand")},{ECityDiagnosticOverlay::CommercialDemand,TEXT("Job demand")},
            {ECityDiagnosticOverlay::IndustrialDemand,TEXT("Industrial demand")},{ECityDiagnosticOverlay::Happiness,TEXT("Happiness")},
            {ECityDiagnosticOverlay::Traffic,TEXT("Traffic")},{ECityDiagnosticOverlay::Pollution,TEXT("Pollution")},{ECityDiagnosticOverlay::Waste,TEXT("Waste")},
            {ECityDiagnosticOverlay::Crime,TEXT("Crime")},{ECityDiagnosticOverlay::Fire,TEXT("Fire")},{ECityDiagnosticOverlay::Education,TEXT("Education")},
            {ECityDiagnosticOverlay::Parks,TEXT("Parks")},{ECityDiagnosticOverlay::Health,TEXT("Health")},{ECityDiagnosticOverlay::CommercialAi,TEXT("Commercial AI")}};
        if (Board) for (const auto& N : Names) if (N.Key == Board->GetDiagnosticOverlay()) Active=N.Value;
        auto& S=Section(TEXT("MAP OVERLAY"), TEXT("Choose a view, then inspect a building for exact status."));
        S.Rows={Row(TEXT("Active overlay"),Active)};
        Order={A::OverlayPower,A::OverlayService,A::OverlayResidential,A::OverlayCommercial,A::OverlayIndustrial,A::OverlayHappiness,A::OverlayTraffic,A::OverlayPollution,A::OverlayWaste,A::OverlayClear,A::Back};
    }
    if (!Order.IsEmpty()) Data.MenuEntries.StableSort([&](const auto& L,const auto& R) {
        const int32 LI=Order.IndexOfByKey(static_cast<A>(L.ActionId)), RI=Order.IndexOfByKey(static_cast<A>(R.ActionId));
        return (LI==INDEX_NONE ? MAX_int32 : LI) < (RI==INDEX_NONE ? MAX_int32 : RI);
    });
    for (auto& E : Data.MenuEntries)
    {
        const auto Id=static_cast<A>(E.ActionId); E.Columns=2;
        auto Label=[&](const TCHAR* T) { E.Label=FText::FromString(T); };
        auto Group=[&](const TCHAR* T,const TCHAR* Help=TEXT("")) { E.Group=FText::FromString(T); E.GroupHelp=FText::FromString(Help); };
        if (Id==A::Back) { Label(Review ? TEXT("Back to setup") : TEXT("Cancel")); E.bStartNewRow=true; }
        if (Main)
        {
            if (Id==A::ContinueCity) { Label(TEXT("Continue city")); E.Columns=1; E.bPrimary=true; }
            if (Id==A::ReviewLoad) { Group(TEXT("OTHER CITIES")); Label(TEXT("Load city")); }
            if (Id==A::OpenSettings) Group(TEXT("PREFERENCES"));
        }
        if (Setup)
        {
            if (Id>=A::DifficultyRelaxed && Id<=A::DifficultyChallenging) { E.Columns=3; if (Id==A::DifficultyRelaxed) Group(TEXT("DIFFICULTY")); }
            if (Id==A::CycleMapSize) { Group(TEXT("TERRAIN & HAZARDS"),TEXT("64 x 64 supports all platforms; 80 x 80 is Desktop only.")); Label(TEXT("Map size")); E.ControlValue=FText::FromString(FString::Printf(TEXT("%d x %d >"),NewCityTerrain.Dimensions.GetWidth(),NewCityTerrain.Dimensions.GetHeight())); E.Columns=1; }
            if (Id==A::CycleDisasters) { Label(TEXT("Natural disasters")); E.ControlValue=FText::FromString(FString(FCityIdentity::DisasterFrequencyName(NewCityIdentity.DisasterFrequency))+TEXT(" >")); E.Columns=1; }
            if (Id==A::RerollTerrain) { Group(TEXT("TERRAIN SEED"),*FString::Printf(TEXT("Seed %d | New terrain chooses another seed."),NewCityTerrain.Seed)); Label(TEXT("New terrain")); }
            if (Id==A::ReviewCreate) { Group(TEXT("NEXT")); Label(TEXT("Review city")); E.bPrimary=true; E.Columns=1; }
        }
        if (Review && Id==A::ConfirmCreate) { Label(TEXT("Create city")); E.bPrimary=true; E.Columns=1; }
        if (Library)
        {
            if (Id==A::ToggleArchivedCities) { Group(TEXT("BROWSE LIBRARY"),bReviewArchivedCities ? TEXT("Showing Archived cities") : TEXT("Showing Active cities")); Label(bReviewArchivedCities ? TEXT("Active cities") : TEXT("Archived cities")); E.Columns=1; }
            if (Id==A::PreviousCity || Id==A::NextCity) { E.bEnabled &= ReviewedCitySlots.Num()>1; Label(Id==A::PreviousCity ? TEXT("< Previous city") : TEXT("Next city >")); }
            if (Id==A::ConfirmLoad || Id==A::RestoreCity) { Group(TEXT("SELECTED CITY")); Label(Id==A::ConfirmLoad ? TEXT("Load saved city") : TEXT("Restore city")); E.bPrimary=true; E.Columns=1; }
            if (Id==A::RefreshCityLibrary) { Label(TEXT("Refresh library")); E.Columns=1; }
            if (Id==A::ReviewArchive) { Group(TEXT("ARCHIVE"),Current ? TEXT("Switch cities before archiving the current city.") : TEXT("Move this saved city to the recoverable archive.")); Label(TEXT("Archive city...")); E.Columns=1; }
        }
        if (Archive && Id==A::ConfirmArchive) { Label(TEXT("Archive city")); E.bPrimary=true; E.Columns=1; }
        if (Overlay)
        {
            if (Id==A::OverlayPower) Group(TEXT("UTILITIES"));
            if (Id==A::OverlayResidential) { Group(TEXT("GROWTH")); Label(TEXT("Residential demand")); }
            if (Id==A::OverlayCommercial) Label(TEXT("Job demand"));
            if (Id==A::OverlayIndustrial) Label(TEXT("Industrial demand"));
            if (Id==A::OverlayHappiness) Group(TEXT("CITY CONDITIONS"));
            if (Id==A::OverlayClear) { Group(TEXT("")); Label(TEXT("Clear overlay")); E.Columns=1; E.bStartNewRow=true; }
            if (Id==A::Back) Label(TEXT("Back"));
        }
    }
}
