#include "Interaction/CityVRToolController.h"
#include "Engine/World.h"
#include "Presentation/CityTutorialSettings.h"
#include "Presentation/CityBoardActor.h"
#include "Simulation/CityDisasterSimulation.h"
#include "Simulation/CitySimulationSubsystem.h"

void ACityVRToolController::PopulateDesktopSettings(FCityUiSnapshot& Data) const
{
    using A = ECityPaletteAction;
    const bool Settings = PalettePage == ECityPalettePage::Settings;
    const bool Audio = PalettePage == ECityPalettePage::Audio;
    const bool Disasters = PalettePage == ECityPalettePage::Disasters;
    const bool History = PalettePage == ECityPalettePage::DisasterHistory;
    const bool Lighting = PalettePage == ECityPalettePage::Lighting;
    const bool Graphics = PalettePage == ECityPalettePage::Graphics;
    const bool Debug = PalettePage == ECityPalettePage::SafetyTests;
    if (!Settings && !Audio && !Disasters && !History && !Lighting && !Graphics && !Debug) return;
    Data.MenuBody = FText::GetEmpty();
    if (Settings && !bDebugInfoVisible) Data.MenuEntries.RemoveAll([](const FCityUiActionView& E) {
        return E.ActionId == static_cast<int32>(A::ToggleConditionIndicators) || E.ActionId == static_cast<int32>(A::FocusNextCondition);
    });
    auto Section = [&](const FString& Title, const FString& Body, bool Warning = false) -> FCityUiMenuSection& {
        auto& R = Data.MenuSections.AddDefaulted_GetRef(); R.Id = Title;
        R.Title = FText::FromString(Title); R.Body = FText::FromString(Body); R.bWarning = Warning; return R;
    };
    auto Row = [](const FString& Label, const FString& Value) {
        FCityUiGauge R; R.Label = FText::FromString(Label); R.Value = FText::FromString(Value); return R;
    };
    TArray<A> Order;
    if (Settings)
        Order = {A::OpenAudio, A::OpenGraphics, A::OpenLighting, A::OpenDisasters,
            A::ToggleTutorial, A::ResetTutorial, A::Save, A::ReviewLoad, A::OpenMainMenu, A::ToggleDebugInfo, A::ToggleConditionIndicators,
            A::FocusNextCondition, A::OpenSafetyTests, A::Reset, A::Back};
    if (Lighting) Order = {A::LightingAutomatic, A::LightingDay, A::LightingDusk, A::LightingNight, A::Back};
    if (Disasters) Order = {A::DisastersOff, A::DisastersLow, A::DisastersMedium, A::DisastersHigh, A::ReportDisasters, A::Back};
    if (Debug) Order = {A::SelectHome, A::SelectShop, A::PreviewAlarm, A::SelectTestSickness,
        A::SelectTestSevereSickness, A::SelectTestWaste, A::SelectTestFire, A::SelectTestCrime,
        A::SelectTestArson, A::TestEarthquake, A::TestTornado, A::Back};
    if (!Order.IsEmpty()) Data.MenuEntries.StableSort([&](const FCityUiActionView& L, const FCityUiActionView& R) {
        return Order.IndexOfByKey(static_cast<A>(L.ActionId)) < Order.IndexOfByKey(static_cast<A>(R.ActionId));
    });
    const auto* Sim = GetWorld()->GetSubsystem<UCitySimulationSubsystem>();
    if (Disasters && Sim)
    {
        const auto Frequency = Sim->GetCityIdentity().DisasterFrequency;
        const int32 Days = Frequency == ECityDisasterFrequency::Low ? 24 : Frequency == ECityDisasterFrequency::Medium ? 12 : 6;
        Section(FCityIdentity::DisasterFrequencyName(Frequency), Frequency == ECityDisasterFrequency::Off ?
            TEXT("Random earthquakes and tornadoes are off. Ordinary building fires continue.") :
            FString::Printf(TEXT("A disaster check every %d days after establishment grace. A check is a chance of an event, not a guarantee. Ordinary building fires continue."), Days));
    }
    if (History && Sim)
    {
        const auto& State = Sim->GetDisasterSnapshot();
        if (State.Events.IsEmpty())
            Section(TEXT("NO EVENTS RECORDED"), TEXT("Future natural disaster warnings and events appear here. History starts when recording begins; ordinary building fires are separate."));
        else
        {
            const int32 I = ReportPeriodIndex == INDEX_NONE ? State.Events.Num()-1 : FMath::Clamp(ReportPeriodIndex, 0, State.Events.Num()-1);
            const auto& E = State.Events[I];
            Data.MenuStatus = FText::FromString(FString::Printf(TEXT("Record %d / %d | latest 16 warnings and events"), I+1, State.Events.Num()));
            auto& R = Section(FCityDisasterSimulation::Name(E.Kind), TEXT(""));
            R.Rows = {Row(TEXT("Cause"), E.bTest ? TEXT("Test event") : TEXT("Natural disaster")),
                Row(TEXT("City day"), FString::Printf(TEXT("%lld"), E.StartStep)),
                Row(TEXT("Affected buildings"), FString::FromInt(E.Affected.Num())),
                Row(TEXT("Status"), State.LastFixedStepId < E.StartStep ? TEXT("Warning") : State.LastFixedStepId < E.EndStep ? TEXT("Active") : TEXT("Ended")),
                Row(TEXT("Duration / days"), FString::Printf(TEXT("%lld"), E.EndStep-E.StartStep)),
                Row(TEXT("Severity"), E.Profile >= 2 ? FString::Printf(TEXT("%d%%"), E.SeverityPercent) : TEXT("Original profile"))};
        }
    }
    if (Graphics && DesktopVideo.IsConfirming())
    {
        Data.MenuTitle = FText::FromString(TEXT("KEEP DISPLAY CHANGES?"));
        Section(TEXT("DISPLAY PREVIEW"), DesktopVideo.Description(FPlatformTime::Seconds()), true);
        auto& R = Section(TEXT("Previewing"), TEXT("Do nothing to restore the previous settings. Esc also reverts."));
        R.Rows = {Row(TEXT("Window mode"), DesktopVideo.ModeLabel()),
            Row(TEXT("Scene scale"), FString::Printf(TEXT("%d%%"), DesktopVideo.GetRenderScale())),
            Row(TEXT("Anti-aliasing"), FString::Printf(TEXT("%dx MSAA"), DesktopVideo.GetAntiAliasingSamples()))};
        Data.MenuEntries.RemoveAll([](const FCityUiActionView& E) { return E.ActionId == static_cast<int32>(A::Back); });
    }
    if (Debug) Section(TEXT("REAL CITY ACTIONS"), TEXT("Events can cause damage, illness, crime and repair costs."), true);
    for (auto& E : Data.MenuEntries)
    {
        const auto Id = static_cast<A>(E.ActionId);
        E.Columns = 2;
        auto Label = [&](const TCHAR* Text) { E.Label = FText::FromString(Text); };
        auto Group = [&](const TCHAR* Title, const TCHAR* Help = TEXT("")) { E.Group = FText::FromString(Title); E.GroupHelp = FText::FromString(Help); };
        if (Id == A::Back) { Group(TEXT("")); Label(TEXT("Back")); E.bStartNewRow = true; }
        if (Settings)
        {
            if (Id == A::ToggleTutorial) { Group(TEXT("TUTORIAL"), TEXT("Opening guidance follows real city progress. Disable to skip; resume at any time.")); Label(TEXT("Tutorial attention")); E.ControlValue = FText::FromString(GetDefault<UCityTutorialSettings>()->bEnabled ? TEXT("On >") : TEXT("Off >")); E.Columns = 1; }
            if (Id == A::ResetTutorial) { Label(TEXT("Reset tutorial")); E.Help = FText::FromString(TEXT("Resume opening guidance without changing your city, funds or objective history.")); E.Columns = 1; }
            if (Id == A::OpenAudio) Group(TEXT("PRESENTATION"));
            if (Id == A::OpenGraphics) Label(TEXT("Graphics / display"));
            if (Id == A::OpenDisasters) Label(TEXT("Natural disasters"));
            if (Id == A::Save) { Group(TEXT("CITY FILE")); Label(TEXT("Save city")); }
            if (Id == A::ReviewLoad) Label(TEXT("Review load"));
            if (Id == A::ToggleDebugInfo) { Group(TEXT("DIAGNOSTICS"), TEXT("Show diagnostic text and unlock Debug Tools.")); Label(TEXT("Debug mode")); E.ControlValue = FText::FromString(bDebugInfoVisible ? TEXT("On >") : TEXT("Off >")); E.Columns = 1; }
            if (Id == A::ToggleConditionIndicators) { Label(TEXT("Map badges")); E.ControlValue = FText::FromString(bConditionIndicatorsVisible ? TEXT("On >") : TEXT("Off >")); E.Columns = 1; }
            if (Id == A::FocusNextCondition) E.bEnabled = E.bEnabled && !E.Label.ToString().Contains(TEXT("(0)"));
            if (Id == A::Reset) { Group(TEXT("RESET CITY"), TEXT("Reset clears the current city. Select again to confirm.")); Label(Sim && Sim->IsResetConfirmationPending() ? TEXT("Confirm reset") : TEXT("Reset city...")); E.bStartNewRow = true; }
        }
        if (Audio && Id != A::Back)
        {
            const int32 Raw = static_cast<int32>(Id);
            const int32 Starts[] = {static_cast<int32>(A::AlarmMute), static_cast<int32>(A::AmbienceMute), static_cast<int32>(A::MusicMute)};
            for (int32 Channel = 0; Channel < 3; ++Channel)
                if (Raw >= Starts[Channel] && Raw < Starts[Channel] + 5)
                {
                    const int32 Level = Raw - Starts[Channel];
                    E.Columns = 5; E.Label = FText::FromString(Level == 0 ? TEXT("Mute") : FString::Printf(TEXT("%d%%"), Level*25));
                    if (Level == 0) Group(Channel == 0 ? TEXT("EFFECTS") : Channel == 1 ? TEXT("AMBIENCE") : TEXT("MUSIC"),
                        Channel == 0 ? TEXT("Sirens and notification effects") : Channel == 1 ? TEXT("City, wildlife, traffic and weather") : TEXT("Muting holds the playlist position."));
                }
        }
        if (Disasters)
        {
            if (Id == A::DisastersOff) Group(TEXT("FREQUENCY"));
            if (Id >= A::DisastersOff && Id <= A::DisastersHigh) E.Columns = 4;
            if (Id == A::ReportDisasters) Group(TEXT("RECORDED EVENTS"));
        }
        if (History && (Id == A::ReportPeriodPrevious || Id == A::ReportPeriodNext))
        {
            if (Id == A::ReportPeriodPrevious) Group(TEXT("BROWSE EVENTS"));
            Label(Id == A::ReportPeriodPrevious ? TEXT("Previous event") : TEXT("Next event"));
            E.bEnabled = E.bEnabled && Sim && Sim->GetDisasterSnapshot().Events.Num() > 1;
        }
        if (Lighting)
        {
            if (Id == A::LightingAutomatic) { Group(TEXT("LIGHTING MODE"), TEXT("Automatic follows the city day/night cycle, speed and pause.")); Label(TEXT("Automatic")); E.Columns = 1; }
            if (Id == A::LightingDay) Group(TEXT("FIXED PRESET"), TEXT("Changes lighting without advancing city time."));
            if (Id != A::Back && Id != A::LightingAutomatic) E.Columns = 3;
        }
        if (Graphics)
        {
            E.Columns = 1;
            if (Id == A::GraphicsMode) { Group(TEXT("DISPLAY"));
                if (DesktopVideo.GetMode() == EWindowMode::WindowedFullscreen) E.GroupHelp = FText::FromString(FString::Printf(TEXT("Desktop resolution: %d x %d. Borderless uses the desktop size."), DesktopVideo.GetResolution().X, DesktopVideo.GetResolution().Y));
                Label(TEXT("Window mode")); E.ControlValue = FText::FromString(DesktopVideo.ModeLabel() + TEXT(" >")); }
            if (Id == A::GraphicsPreviousResolution) { Group(TEXT("RESOLUTION")); Label(TEXT("< Previous")); E.Columns = 2; }
            if (Id == A::GraphicsNextResolution) { E.Label = FText::FromString(FString::Printf(TEXT("%d x %d >"), DesktopVideo.GetResolution().X, DesktopVideo.GetResolution().Y)); E.Columns = 2; }
            if (Id == A::GraphicsVSync) { Label(TEXT("VSync")); E.ControlValue = FText::FromString(DesktopVideo.IsVSyncEnabled() ? TEXT("On >") : TEXT("Off >")); }
            if (Id == A::GraphicsRenderScale) { Group(TEXT("RENDERING"), TEXT("Scene detail changes; UI stays crisp.")); Label(TEXT("Scene scale")); E.ControlValue = FText::FromString(FString::Printf(TEXT("%d%% >"), DesktopVideo.GetRenderScale())); }
            if (Id == A::GraphicsAntiAliasing) { Label(TEXT("Anti-aliasing")); E.ControlValue = FText::FromString(FString::Printf(TEXT("%dx MSAA >"), DesktopVideo.GetAntiAliasingSamples())); }
            if (Id == A::GraphicsApply) { Group(TEXT("PREVIEW CHANGES"), TEXT("Apply starts a 15-second preview. Keep confirms; otherwise previous settings return.")); Label(TEXT("Apply changes")); E.Columns = 2; }
            if (Id == A::GraphicsKeep) { Label(TEXT("Keep changes")); E.Columns = 2; }
            if (Id == A::GraphicsRevert) { Label(TEXT("Revert now")); E.Columns = 2; }
            if (Id == A::Back) { Label(TEXT("Back / discard")); E.Columns = 2; E.bStartNewRow = false; }
        }
        if (Debug)
        {
            if (Id == A::SelectHome) Group(TEXT("PLACEMENT"));
            if (Id == A::PreviewAlarm) Group(TEXT("AUDIO PREVIEW"), TEXT("Five-second preview; available while paused."));
            if (Id == A::SelectTestSickness) Group(TEXT("POINT AT A BUILDING"), TEXT("Select, close the menu, point at a building and trigger."));
            if (Id == A::TestEarthquake) Group(TEXT("CITY DISASTERS"));
        }
    }
}
