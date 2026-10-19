#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CitySandboxGameMode.generated.h"

UCLASS()
class CITYSIMVR_API ACitySandboxGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACitySandboxGameMode();

	virtual void BeginPlay() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

private:
	void LogNavigationCoverage() const;

	UPROPERTY()
	TSubclassOf<APawn> XrPawnClass;
};
