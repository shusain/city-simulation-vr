#pragma once

#include "Commandlets/Commandlet.h"
#include "CityXRPawnConfiguratorCommandlet.generated.h"

UCLASS()
class CITYSIMVR_API UCityXRPawnConfiguratorCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UCityXRPawnConfiguratorCommandlet();

	virtual int32 Main(const FString& Params) override;
};
