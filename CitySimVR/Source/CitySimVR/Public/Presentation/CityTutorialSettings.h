#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CityTutorialSettings.generated.h"

// Player preference only: saved city milestones remain authoritative and untouched.
UCLASS(Config = GameUserSettings)
class CITYSIMVR_API UCityTutorialSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Config) bool bEnabled = true;
	void SetEnabled(bool bValue) { bEnabled = bValue; SaveConfig(); }
};

namespace CityTutorialAttention
{
	// A three-second breathing cycle, with no blinking or zero-brightness phase.
	inline float Pulse(double Seconds) { return .5f + .5f * FMath::Sin(static_cast<float>(Seconds * 2.0 * PI / 3.0)); }
	inline bool HasCompletedOpening(const TArray<FName>& Ids)
	{
		for (const FName Id : {FName(TEXT("entrance_road")), FName(TEXT("power")), FName(TEXT("water")),
		    FName(TEXT("homes_zone")), FName(TEXT("first_residents")), FName(TEXT("first_jobs"))})
			if (!Ids.Contains(Id)) return false;
		return true;
	}
	inline bool IsOpeningStep(FName Id)
	{
		return Id == TEXT("entrance_road") || Id == TEXT("power") || Id == TEXT("water") ||
		       Id == TEXT("homes_zone") || Id == TEXT("first_residents") || Id == TEXT("first_jobs");
	}
}
