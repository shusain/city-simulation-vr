#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityDisasterAlarm.h"
#include "Interaction/CityPaletteModel.h"
#include "Misc/AutomationTest.h"
#include "MetasoundSource.h"
#include "Presentation/CityAudioSettings.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDisasterAlarmTest, "CitySim.Audio.DisasterLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDisasterAlarmTest::RunTest(const FString& Parameters)
{
	FCityDisasterSnapshot State;
	FCityDisasterAlarmGate Gate;
	TestFalse(TEXT("Empty city is quiet"), Gate.Observe(State, 1, false));
	FCityDisasterEvent Event;
	Event.Id = 1;
	Event.StartStep = 5;
	Event.EndStep = 20;
	State.Events.Add(Event);
	State.TotalEvents = 1;
	State.LastFixedStepId = 4;
	TestFalse(TEXT("Pending manual test is quiet"), Gate.Observe(State, 1, false));
	State.LastFixedStepId = 5;
	TestTrue(TEXT("Onset plays"), Gate.Observe(State, 1, false));
	TestFalse(TEXT("Repeated presentation frame is quiet"), Gate.Observe(State, 1, false));
	TestFalse(TEXT("Reloading an active event is quiet"), Gate.Observe(State, 2, false));
	State.Events.Last().Id = 2;
	State.TotalEvents = 2;
	TestFalse(TEXT("Muted or suspended onset is consumed"), Gate.Observe(State, 2, true));
	TestFalse(TEXT("Unmute or resume does not replay"), Gate.Observe(State, 2, false));
	State.Events.Last().Id = 3;
	State.TotalEvents = 3;
	TestTrue(TEXT("Next event still plays"), Gate.Observe(State, 2, false));
	State.LastFixedStepId = 4;
	TestFalse(TEXT("A loaded pending legacy event baselines without playing"), Gate.Observe(State, 3, false));
	State.LastFixedStepId = 5;
	TestTrue(TEXT("Loaded pending event plays when it starts"), Gate.Observe(State, 3, false));
	State.LastFixedStepId = 25;
	State.Events.Last().Id = 4;
	State.TotalEvents = 4;
	TestFalse(TEXT("Ended catch-up events do not burst"), Gate.Observe(State, 3, false));
	TestEqual(TEXT("Audio returns to game settings"), FCityPaletteModel::ParentPage(ECityPalettePage::Audio),
	          ECityPalettePage::Settings);
	TestTrue(TEXT("Audio has navigation wiring"), FCityPaletteModel::IsNavigationAction(ECityPaletteAction::OpenAudio));
	auto* Source =
	    LoadObject<UMetaSoundSource>(nullptr, TEXT("/Game/CitySim/Audio/MS_CityDisasterAlarm.MS_CityDisasterAlarm"));
	TestNotNull(TEXT("Cook-referenced procedural alarm asset exists"), Source);
	if (Source)
		TestTrue(TEXT("Graph exposes its one-shot finish interface"), Source->IsOneShot());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityAudioPreferenceTest, "CitySim.Audio.PreferencePersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityAudioPreferenceTest::RunTest(const FString& Parameters)
{
	const FString Filename = FPaths::ConvertRelativePathToFull(
	    FPaths::ProjectSavedDir() / TEXT("Automation") / (TEXT("Alarm-") + FGuid::NewGuid().ToString() + TEXT(".ini")));
	auto* Settings = NewObject<UCityAudioSettings>();
	Settings->AlarmVolume = .25f;
	Settings->AmbienceVolume = .5f;
	Settings->MusicVolume = .25f;
	Settings->SaveConfig(CPF_Config, *Filename);
	auto* Restored = NewObject<UCityAudioSettings>();
	Restored->ReloadConfig(nullptr, *Filename);
	TestEqual(TEXT("Volume preference survives a config reload"), Restored->GetAlarmVolume(), .25f);
	TestEqual(TEXT("Ambience preference survives independently"), Restored->GetAmbienceVolume(), .5f);
	TestEqual(TEXT("Music preference survives independently"), Restored->GetMusicVolume(), .25f);
	Settings->MusicVolume = 0.f;
	Settings->AlarmVolume = 0.f;
	Settings->SaveConfig(CPF_Config, *Filename);
	Restored->ReloadConfig(nullptr, *Filename);
	TestEqual(TEXT("Mute survives a config reload"), Restored->GetAlarmVolume(), 0.f);
	TestEqual(TEXT("Music mute survives a config reload"), Restored->GetMusicVolume(), 0.f);
	Restored->AlarmVolume = 2.f;
	TestEqual(TEXT("Malformed volume is bounded"), Restored->GetAlarmVolume(), 1.f);
	Restored->AmbienceVolume = -.5f;
	TestEqual(TEXT("Malformed ambience volume is bounded"), Restored->GetAmbienceVolume(), 0.f);
	Restored->MusicVolume = 2.f;
	TestEqual(TEXT("Malformed music volume is bounded"), Restored->GetMusicVolume(), 1.f);
	IFileManager::Get().Delete(*Filename);
	return true;
}
#endif
