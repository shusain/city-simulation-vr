#pragma once
#include "Commandlets/Commandlet.h"
#include "CitySoundscapeAssetsCommandlet.generated.h"
UCLASS()
class CITYSIMVR_API UCitySoundscapeAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCitySoundscapeAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
