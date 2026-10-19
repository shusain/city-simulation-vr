#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Presentation/CityMusicPlayback.h"
#include "Presentation/CityMusicComponent.h"
#include "Presentation/CityAudioSettings.h"
#include "Presentation/CityBoardActor.h"
#include "Interaction/CityPaletteModel.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMusicClockTest, "CitySim.Audio.Music.ClockAndTransitions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityMusicClockTest::RunTest(const FString& Parameters)
{
	FCityMusicPlayback Clock;
	TestTrue(TEXT("Three varied tracks configure"), Clock.Configure({20, 30, 40}));
	Clock.Advance(18, false);
	TestEqual(TEXT("Outgoing offset follows wall time"), Clock.Voice(0).Offset, 18.0);
	TestEqual(TEXT("Incoming starts in the overlap"), Clock.Voice(1).Offset, 2.0);
	TestEqual(TEXT("Crossfade complementary gains"), Clock.Voice(0).Gain + Clock.Voice(1).Gain, 1.f);
	Clock.Advance(1000, true);
	TestEqual(TEXT("Suspended/muted playlist holds both offsets"), Clock.GetOffset(), 18.0);
	Clock.Advance(2, false);
	TestEqual(TEXT("Incoming continues without a restart"), Clock.Voice(1).Offset, 4.0);
	TestEqual(TEXT("Finished slot is released"), Clock.Voice(0).Track, INDEX_NONE);
	Clock.Advance(26, false);
	TestEqual(TEXT("Third track uses the freed player"), Clock.Voice(0).Track, 2);
	Clock.Advance(36, false);
	TestEqual(TEXT("Odd-length playlist wraps into the other slot"), Clock.Voice(1).Track, 0);
	TestEqual(TEXT("Playlist wrap keeps overlap seek position"), Clock.Voice(1).Offset, 4.0);
	Clock.Advance(156000000, false);
	TestEqual(TEXT("Long wall-time jump wraps with slot parity preserved"), Clock.Voice(1).Track, 0);
	Clock.Advance(std::numeric_limits<double>::quiet_NaN(), false);
	Clock.Advance(-10, false);
	TestEqual(TEXT("Invalid clock input cannot corrupt position"), Clock.GetOffset(), 4.0);
	TestFalse(TEXT("Unusable short track cannot create an endless rollover"), Clock.Configure({3, 20}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMusicNineTrackTest, "CitySim.Audio.Music.NineTrackWrap",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityMusicNineTrackTest::RunTest(const FString& Parameters)
{
	const TArray<double> Durations = {120, 130, 140, 150, 160, 170, 180, 190, 200};
	FCityMusicPlayback Clock;
	TestTrue(TEXT("Nine varied durations configure"), Clock.Configure(Durations));
	Clock.Advance(Durations[0] - 2, false);
	TestEqual(TEXT("Incoming overlap starts at its actual seek position"), Clock.Voice(1).Offset, 2.0);
	TestEqual(TEXT("Only complementary two-slot gains are active"), Clock.Voice(0).Gain + Clock.Voice(1).Gain, 1.f);
	Clock.Advance(2, false);
	for (int32 Index = 1; Index < Durations.Num(); ++Index)
	{
		TestEqual(TEXT("Every selected track is visited in order"), Clock.GetTrack(), Index);
		Clock.Advance(Durations[Index] - FCityMusicPlayback::CrossfadeSeconds, false);
	}
	TestEqual(TEXT("Nine-track wrap returns to the first recording"), Clock.GetTrack(), 0);
	TestEqual(TEXT("Odd nine-track wrap preserves opposite player-slot parity"), Clock.Voice(1).Track, 0);
	TestEqual(TEXT("Wrap continues from the completed overlap"), Clock.GetOffset(), 4.0);
	double Cycle = 0;
	for (double Duration : Durations)
		Cycle += Duration - FCityMusicPlayback::CrossfadeSeconds;
	Clock.Advance(Cycle * 2000000, false);
	TestEqual(TEXT("Long even-cycle jump preserves track and slot parity"), Clock.Voice(1).Track, 0);
	TestEqual(TEXT("Long jump preserves the current seek"), Clock.GetOffset(), 4.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMusicRuntimeTest, "CitySim.Audio.Music.AssetsPoolAndSuspend",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityMusicRuntimeTest::RunTest(const FString& Parameters)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Board = World->SpawnActor<ACityBoardActor>();
	auto* Music = Board->FindComponentByClass<UCityMusicComponent>();
	if (TestNotNull(TEXT("One music owner on the shared board"), Music))
	{
		TestTrue(TEXT("All nine approved native waves are cook referenced"), Music->IsReady());
		TestEqual(TEXT("Approved bank contains exactly nine selected waves"), Music->Sources.Num(), 9);
		for (const auto& Source : Music->Sources)
		{
			if (!Source)
				continue;
			TestEqual(TEXT("Music remains stereo"), Source->NumChannels, 2);
			TestFalse(TEXT("Playlist controls transitions; sources do not loop"), static_cast<bool>(Source->bLooping));
			TestTrue(TEXT("Approved tracks retain requested two-to-four-minute durations"),
			         Source->Duration >= 120 && Source->Duration <= 240);
			TestEqual(TEXT("Seekable cross-platform codec"), Source->GetSoundAssetCompressionType(),
			          ESoundAssetCompressionType::BinkAudio);
		}
		Music->EnsurePlayers();
		Music->EnsurePlayers();
		Music->BeginPlay();
		TestEqual(TEXT("Repeated initialization keeps the fixed two-player pool"), Music->Players.Num(), 2);
		for (const auto& Player : Music->Players)
		{
			TestFalse(TEXT("No automatic playback on load"), Player->bAutoActivate);
			TestFalse(TEXT("Stereo is not position attenuated"), Player->bAllowSpatialization);
			TestFalse(TEXT("Player cannot overlap instances"), Player->bCanPlayMultipleInstances);
		}
		Music->Playback.Advance(Music->Sources[0]->Duration - FCityMusicPlayback::CrossfadeSeconds * .5, false);
		const double Before = Music->Playback.GetOffset();
		Music->Suspend();
		Music->LastWallSeconds -= 300;
		Music->Update(false, .35f);
		TestEqual(TEXT("Foreground resume never counts time spent backgrounded"), Music->Playback.GetOffset(), Before);
		TestEqual(TEXT("Long suspension resumes from silence for the volume ramp"), Music->MixVolume, 0.f);
		TestTrue(TEXT("Resume never creates more than two voices"), Music->GetPlayingVoiceCount() <= 2);
		Music->Update(true, 1);
		TestEqual(TEXT("Background cancels all voices"), Music->GetPlayingVoiceCount(), 0);
		TestEqual(TEXT("Background preserves track offset"), Music->Playback.GetOffset(), Before);
		Music->EndPlay(EEndPlayReason::Destroyed);
		TestEqual(TEXT("Shutdown stops all voices"), Music->GetPlayingVoiceCount(), 0);
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityMusicControlsTest, "CitySim.Audio.Music.SharedControlGeometry",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
                                     EAutomationTestFlags::EngineFilter)
bool FCityMusicControlsTest::RunTest(const FString& Parameters)
{
	for (bool bHand : {false, true})
	{
		const auto Entries = FCityPaletteModel::SurfaceEntries(ECityPalettePage::Audio, bHand);
		TestTrue(TEXT("Entire audio page fits the runtime button pool"),
		         Entries.Num() <= FCityPaletteModel::MaximumEntryCount);
		int32 MusicCount = 0;
		TArray<FCityPaletteRect> Rects;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const auto Action = Entries[Index].Action;
			MusicCount += Action >= ECityPaletteAction::MusicMute && Action <= ECityPaletteAction::Music100;
			const auto Rect = FCityPaletteModel::ButtonRect(ECityPalettePage::Audio, Index, Action, bHand);
			for (const auto& Prior : Rects)
				TestTrue(TEXT("Audio touch targets remain separate"),
				         FMath::Abs(Rect.Center.Y - Prior.Center.Y) >= Rect.HalfSize.X + Prior.HalfSize.X ||
				             FMath::Abs(Rect.Center.Z - Prior.Center.Z) >= Rect.HalfSize.Y + Prior.HalfSize.Y);
			Rects.Add(Rect);
		}
		TestEqual(TEXT("All five saved music levels available on both XR surfaces"), MusicCount, 5);
	}
	return true;
}
#endif
