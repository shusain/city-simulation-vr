#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presentation/CityMusicPlayback.h"
#include "CityMusicComponent.generated.h"
class UAudioComponent;
class USoundWave;

// One board-owned playlist, two fixed stereo players. No per-city/entity audio actors.
UCLASS()
class CITYSIMVR_API UCityMusicComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCityMusicComponent();
	void Update(bool bBackgrounded, float WarningGain);
	void Suspend();
	bool IsReady() const;
	int32 GetPlayingVoiceCount() const;
	const FCityMusicPlayback& GetPlayback() const
	{
		return Playback;
	}
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	friend class FCityMusicRuntimeTest;
	friend class FCityDesktopPackagedSmokeCommand;
	void EnsurePlayers();
	UPROPERTY()
	TArray<TObjectPtr<USoundWave>> Sources;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Players;
	FCityMusicPlayback Playback;
	int32 ActiveTracks[2] = {INDEX_NONE, INDEX_NONE};
	double LastWallSeconds = 0;
	float MixVolume = 0, DuckGain = 1;
	bool bWasSuspended = true;
};
