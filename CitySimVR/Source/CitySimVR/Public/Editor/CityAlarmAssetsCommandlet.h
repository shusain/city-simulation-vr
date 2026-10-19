#pragma once
#include "Commandlets/Commandlet.h"
#include "CityAlarmAssetsCommandlet.generated.h"
UCLASS()
class CITYSIMVR_API UCityAlarmAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCityAlarmAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
