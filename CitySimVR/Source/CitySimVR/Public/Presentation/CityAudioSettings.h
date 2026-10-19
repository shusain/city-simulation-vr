#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CityAudioSettings.generated.h"
UCLASS(Config = GameUserSettings)
class CITYSIMVR_API UCityAudioSettings : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "City Audio")
	float GetAlarmVolume() const
	{
		return FMath::IsFinite(AlarmVolume) ? FMath::Clamp(AlarmVolume, 0.f, 1.f) : .75f;
	}
	UFUNCTION(BlueprintCallable, Category = "City Audio")
	void SetAlarmVolume(float Value)
	{
		AlarmVolume = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : .75f;
		SaveConfig();
	}
	UFUNCTION(BlueprintPure, Category = "City Audio")
	float GetAmbienceVolume() const
	{
		return FMath::IsFinite(AmbienceVolume) ? FMath::Clamp(AmbienceVolume, 0.f, 1.f) : .5f;
	}
	UFUNCTION(BlueprintCallable, Category = "City Audio")
	void SetAmbienceVolume(float Value)
	{
		AmbienceVolume = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : .5f;
		SaveConfig();
	}
	UFUNCTION(BlueprintPure, Category = "City Audio")
	float GetMusicVolume() const
	{
		return FMath::IsFinite(MusicVolume) ? FMath::Clamp(MusicVolume, 0.f, 1.f) : .25f;
	}
	UFUNCTION(BlueprintCallable, Category = "City Audio")
	void SetMusicVolume(float Value)
	{
		MusicVolume = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 1.f) : .25f;
		SaveConfig();
	}

private:
	friend class FCityAudioPreferenceTest;
	UPROPERTY(Config)
	float AlarmVolume = .75f;
	UPROPERTY(Config)
	float AmbienceVolume = .5f;
	UPROPERTY(Config)
	float MusicVolume = .25f;
};
