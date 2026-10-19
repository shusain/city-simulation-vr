#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CityParkAssetLibrary.generated.h"

class USkeletalMesh;
class USkeleton;
class ACityBoardActor;
class UAnimSequence;

UCLASS()
class CITYSIMVR_API UCityParkAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// Python does not expose USkeletalMesh's BlueprintSetter in this engine.
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool BindMatchingParkSkeleton(USkeletalMesh* Mesh, USkeleton* Skeleton);
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool BindMatchingParkAnimation(UAnimSequence* Sequence, USkeleton* Skeleton);
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor", meta = (DevelopmentOnly))
	static bool SetParkCaptureClock(ACityBoardActor* Board, double Seconds);
};
