#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "Sandbox/CityDesktopCamera.h"
#include "CitySandboxDesktopPawn.generated.h"

UCLASS()
class CITYSIMVR_API ACitySandboxDesktopPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	ACitySandboxDesktopPawn();

	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	bool IsCameraGestureActive() const;
	void FocusCondition(const FVector& Point)
	{
		if (bCameraInitialized)
			CameraState.Focus = Point;
	}

private:
	friend class FCityDesktopConditionsCommand;
	friend class FCityTutorialWalkthroughCommand;
	friend class FCityBeachCaptureCommand;
	friend class FCityWaterCaptureCommand;
	friend class FCityShorelineCaptureCommand;
	UPROPERTY()
	TObjectPtr<class UCameraComponent> DesktopCamera;
	FCityDesktopCamera CameraState;
	bool bDesktopGameplay = false;
	bool bCameraInitialized = false;
	uint64 ObservedCitySession = MAX_uint64;
};
