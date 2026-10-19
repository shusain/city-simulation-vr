#pragma once
#include "Commandlets/Commandlet.h"
#include "CityMetalReflectionAssetsCommandlet.generated.h"

UCLASS()
class CITYSIMVR_API UCityMetalReflectionAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCityMetalReflectionAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
	UFUNCTION(BlueprintCallable, Category = "CitySim|Editor")
	static int32 BakeRoomReflection();
};
