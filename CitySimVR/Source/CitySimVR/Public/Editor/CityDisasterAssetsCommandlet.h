#pragma once
#include "Commandlets/Commandlet.h"
#include "CityDisasterAssetsCommandlet.generated.h"
UCLASS()
class CITYSIMVR_API UCityDisasterAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCityDisasterAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
