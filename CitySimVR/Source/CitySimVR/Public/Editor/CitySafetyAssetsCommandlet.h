#pragma once
#include "Commandlets/Commandlet.h"
#include "CitySafetyAssetsCommandlet.generated.h"
UCLASS()
class CITYSIMVR_API UCitySafetyAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCitySafetyAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
