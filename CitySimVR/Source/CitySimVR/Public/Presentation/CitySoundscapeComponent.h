#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presentation/CitySoundscapePolicy.h"
#include "CitySoundscapeComponent.generated.h"
class ACityBoardActor;
class UCitySimulationSubsystem;
class UAudioComponent;
class USoundBase;

enum class ECityAudioChannel : uint8
{
	Vehicle,
	Town,
	Bed,
	Traffic,
	Shot
};
struct FCityAudioVoiceKey
{
	ECityAudioChannel Channel = ECityAudioChannel::Shot;
	uint64 Id = 0;
	bool operator==(const FCityAudioVoiceKey&) const = default;
};
inline uint32 GetTypeHash(const FCityAudioVoiceKey& Key)
{
	return HashCombine(GetTypeHash(Key.Id), static_cast<uint32>(Key.Channel));
}
struct FCityAudioVoice
{
	FCityAudioVoiceKey Key;
	ECityAudioCue Cue = ECityAudioCue::Count;
	FVector GridPosition = FVector::ZeroVector;
	float Gain = 0;
	float DuckGain = 1, DuckTarget = 1, DuckRate = 0;
	float DistanceGain = 1;
	uint64 ResponseId = 0;
	bool bSpatial = true, bAmbience = false, bRetiring = false;
	double StartSeconds = 0, EndSeconds = 0;
};
struct FCityAudioArea
{
	uint64 Id = 0;
	FName Definition;
	FVector GridPosition = FVector::ZeroVector;
	bool bPark = false, bIndustry = false, bMarket = false;
};
// One bounded pool driven by the board; no per-building/vehicle/citizen audio Actors or ticks.
UCLASS()
class CITYSIMVR_API UCitySoundscapeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCitySoundscapeComponent();
	void Update(ACityBoardActor& Board, UCitySimulationSubsystem& Simulation, float Seconds, bool bBackgrounded);
	void StopAll();
	float GetMusicDuckGain() const;
	bool IsReady() const;
	USoundBase* GetCueSound(ECityAudioCue Cue) const;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(BlueprintPure, Category = "City Audio")
	int32 GetPlayingVoiceCount() const;

private:
	friend class FCitySoundscapeRoutingTest;
	void EnsurePool();
	void RebuildAreas(const UCitySimulationSubsystem& Simulation);
	void Synchronize(FCityAudioVoice Request, bool bIntro = false);
	bool StartVoice(FCityAudioVoice Request, float FadeSeconds);
	void Retire(int32 Index, float FadeSeconds);
	float VoiceDistanceGain(const FCityAudioVoice& Voice) const;
	void PlayShot(ECityAudioCue Cue, const FVector& Position, bool bSpatial, bool bAmbience, float Gain = 1.f,
	              uint64 ResponseId = 0);
	int32 FindVoice(FCityAudioVoiceKey Key) const;
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> Sources;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Players;
	TArray<FCityAudioVoice> Voices;
	TArray<FCityAudioArea> Areas;
	TSet<FCityAudioVoiceKey> Eligible;
	TSet<FCityAudioVoiceKey> Wanted;
	TMap<uint64, double> AccentTimes;
	TMap<uint64, FVector> KnownEntities;
	uint64 Session = MAX_uint64, AreaRevision = MAX_uint64, ShotSequence = 0;
	double WallSeconds = 0, NextAmbienceEvent = 0, NextThunder = 0, NextNotification = 0, NextActivity = 0;
	double NextAreaCheck = 0;
	float TargetPark = 0, TargetIndustry = 0, TargetMarket = 0;
	bool bWasBlocked = true;
	FRandomStream Random{0x44535838};
	CitySoundscape::FTransitionGate IncidentGate, ServiceGate, ObjectiveGate, QuakeGate;
	int64 PriorIncidents = 0, PriorObjectiveCount = 0;
	FVector Focus = FVector::ZeroVector, Listener = FVector::ZeroVector;
	FTransform BoardTransform;
	bool bHeadTracked = false;
	float Detail = 0, Daylight = 1, ParkWeight = 0, IndustryWeight = 0, MarketWeight = 0;
	FVector ParkPosition = FVector::ZeroVector, IndustryPosition = FVector::ZeroVector,
	        MarketPosition = FVector::ZeroVector;
};
