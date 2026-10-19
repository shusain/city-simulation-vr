#pragma once
#include "Commandlets/Commandlet.h"
#include "CityDesktopUiAssetsCommandlet.generated.h"

UCLASS()
class CITYSIMVR_API UCityDesktopUiAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UCityDesktopUiAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};
