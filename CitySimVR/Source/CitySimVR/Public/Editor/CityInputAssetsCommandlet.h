#pragma once

#include "Commandlets/Commandlet.h"
#include "CityInputAssetsCommandlet.generated.h"

UCLASS()
class CITYSIMVR_API UCityInputAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCityInputAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
