#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CitySoundscapeAssetLibrary.generated.h"

/** Focused maintenance of the existing soundscape graphs; no graph reauthoring. */
UCLASS()
class CITYSIMVR_API UCitySoundscapeAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Restore only Wave Player object literals from the authoritative cue table. Returns changed count, or -1. */
	UFUNCTION(BlueprintCallable, Category="CitySim|Editor", meta=(DevelopmentOnly))
	static int32 RepairWaveReferences();
};
