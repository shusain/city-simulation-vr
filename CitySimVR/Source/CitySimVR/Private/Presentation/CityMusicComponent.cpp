#include "Presentation/CityMusicComponent.h"
#include "Presentation/CityAudioSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/PlatformTime.h"

UCityMusicComponent::UCityMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	for (const TCHAR* Name :
	     {TEXT("Afterglow"), TEXT("GlassCanal"), TEXT("NeonCourtyard"), TEXT("LanternWalk"), TEXT("NightTransit"),
	      TEXT("RooftopRain"), TEXT("PaperLanterns"), TEXT("TidalCircuit"), TEXT("BlueHour")})
	{
		const FString Path =
		    FString::Printf(TEXT("/Game/CitySim/Audio/Music/Approved/SW_Music_%s.SW_Music_%s"), Name, Name);
		ConstructorHelpers::FObjectFinder<USoundWave> Source(*Path);
		Sources.Add(Source.Object);
	}
}
bool UCityMusicComponent::IsReady() const
{
	return Sources.Num() == 9 && !Sources.Contains(nullptr);
}
void UCityMusicComponent::EnsurePlayers()
{
	if (!Players.IsEmpty() || !IsReady())
		return;
	TArray<double> Durations;
	for (const auto& Source : Sources)
		Durations.Add(Source->Duration);
	if (!Playback.Configure(Durations))
		return;
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		auto* Player = NewObject<UAudioComponent>(GetOwner());
		Player->bAutoActivate = false;
		Player->bAutoDestroy = false;
		Player->bStopWhenOwnerDestroyed = true;
		Player->bCanPlayMultipleInstances = false;
		Player->bAllowSpatialization = false;
		Player->bIsUISound = true;
		Player->bAlwaysPlay = true;
		Player->RegisterComponent();
		Players.Add(Player);
	}
}
void UCityMusicComponent::Suspend()
{
	for (const auto& Player : Players)
		Player->Stop();
	ActiveTracks[0] = ActiveTracks[1] = INDEX_NONE;
	MixVolume = 0;
	bWasSuspended = true;
	LastWallSeconds = FPlatformTime::Seconds();
}
void UCityMusicComponent::Update(bool bBackgrounded, float WarningGain)
{
	EnsurePlayers();
	const double Now = FPlatformTime::Seconds();
	const double Seconds = !bWasSuspended && LastWallSeconds > 0 ? FMath::Max(0.0, Now - LastWallSeconds) : 0;
	LastWallSeconds = Now;
	const float Volume = GetDefault<UCityAudioSettings>()->GetMusicVolume();
	if (bBackgrounded || Volume <= 0 || Players.Num() != 2)
	{
		Suspend();
		return;
	}
	Playback.Advance(Seconds, false);
	bWasSuspended = false;
	const float DuckTarget = FMath::IsFinite(WarningGain) ? FMath::Clamp(WarningGain, 0.f, 1.f) : 1.f;
	DuckGain = FMath::Lerp(DuckGain, DuckTarget,
	                       1.f - static_cast<float>(FMath::Exp(-Seconds / (DuckTarget < DuckGain ? .1 : 1.0))));
	MixVolume = FMath::Lerp(MixVolume, Volume, 1.f - static_cast<float>(FMath::Exp(-Seconds / .25)));
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		const FCityMusicVoice Voice = Playback.Voice(Slot);
		auto* Player = Players[Slot].Get();
		if (Voice.Track != ActiveTracks[Slot])
		{
			Player->Stop();
			ActiveTracks[Slot] = Voice.Track;
			if (Voice.Track != INDEX_NONE)
			{
				Player->SetSound(Sources[Voice.Track]);
				Player->SetVolumeMultiplier(MixVolume * DuckGain * Voice.Gain);
				Player->Play(static_cast<float>(Voice.Offset));
			}
		}
		Player->SetVolumeMultiplier(MixVolume * DuckGain * Voice.Gain);
	}
}
int32 UCityMusicComponent::GetPlayingVoiceCount() const
{
	int32 Count = 0;
	for (const auto& Player : Players)
		Count += Player->IsPlaying();
	return Count;
}
void UCityMusicComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Suspend();
	Super::EndPlay(Reason);
}
