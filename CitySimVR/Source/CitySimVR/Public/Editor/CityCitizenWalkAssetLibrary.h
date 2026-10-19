#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CityCitizenWalkAssetLibrary.generated.h"
class UAnimSequence;
class UBlendSpace1D;

UCLASS()
class CITYSIMVR_API UCityCitizenWalkAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="CitySim|Editor", meta=(DevelopmentOnly))
	static UBlendSpace1D* CreateWalkBlend(UAnimSequence* Idle, UAnimSequence* Walk);
	/** Restore only the idle sample reference, preserving the existing blend and sample settings. */
	UFUNCTION(BlueprintCallable, Category="CitySim|Editor", meta=(DevelopmentOnly))
	static bool RepairWalkBlendReferences();
};
